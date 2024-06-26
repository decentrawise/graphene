#include <fc/uint128.hpp>

#include <graphene/protocol/market.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/fba_accumulator_id.hpp>
#include <graphene/chain/hardfork.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/budget_record_object.hpp>
#include <graphene/chain/buyback_object.hpp>
#include <graphene/chain/chain_property_object.hpp>
#include <graphene/chain/delegate_object.hpp>
#include <graphene/chain/fba_object.hpp>
#include <graphene/chain/global_property_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/special_authority_object.hpp>
#include <graphene/chain/vesting_balance_object.hpp>
#include <graphene/chain/vote_count.hpp>
#include <graphene/chain/validator_object.hpp>
#include <graphene/chain/worker_object.hpp>

namespace graphene { namespace chain {

template<class Index>
vector<std::reference_wrapper<const typename Index::object_type>> database::sort_votable_objects(size_t count) const
{
   using ObjectType = typename Index::object_type;
   const auto& all_objects = get_index_type<Index>().indices();
   count = std::min(count, all_objects.size());
   vector<std::reference_wrapper<const ObjectType>> refs;
   refs.reserve(all_objects.size());
   std::transform(all_objects.begin(), all_objects.end(),
                  std::back_inserter(refs),
                  [](const ObjectType& o) { return std::cref(o); });
   std::partial_sort(refs.begin(), refs.begin() + count, refs.end(),
                   [this](const ObjectType& a, const ObjectType& b)->bool {
      share_type oa_vote = _vote_tally_buffer[a.vote_id];
      share_type ob_vote = _vote_tally_buffer[b.vote_id];
      if( oa_vote != ob_vote )
         return oa_vote > ob_vote;
      return a.vote_id < b.vote_id;
   });

   refs.resize(count, refs.front());
   return refs;
}

template<class Type>
void database::perform_account_maintenance(Type tally_helper)
{
   const auto& bal_idx = get_index_type< account_balance_index >().indices().get< by_maintenance_flag >();
   if( bal_idx.begin() != bal_idx.end() )
   {
      auto bal_itr = bal_idx.rbegin();
      while( bal_itr->maintenance_flag )
      {
         const account_balance_object& bal_obj = *bal_itr;

         modify( get_account_stats_by_owner( bal_obj.owner ), [&bal_obj](account_statistics_object& aso) {
            aso.core_in_balance = bal_obj.balance;
         });

         modify( bal_obj, []( account_balance_object& abo ) {
            abo.maintenance_flag = false;
         });

         bal_itr = bal_idx.rbegin();
      }
   }

   const auto& stats_idx = get_index_type< account_stats_index >().indices().get< by_maintenance_seq >();
   auto stats_itr = stats_idx.lower_bound( true );

   while( stats_itr != stats_idx.end() )
   {
      const account_statistics_object& acc_stat = *stats_itr;
      const account_object& acc_obj = acc_stat.owner( *this );
      ++stats_itr;

      if( acc_stat.has_some_core_voting() )
         tally_helper( acc_obj, acc_stat );

      if( acc_stat.has_pending_fees() )
         acc_stat.process_fees( acc_obj, *this );
   }

}

/// @brief A visitor for @ref worker_type which calls pay_worker on the worker within
struct worker_pay_visitor
{
   private:
      share_type pay;
      database& db;

   public:
      worker_pay_visitor(share_type pay, database& db)
         : pay(pay), db(db) {}

      typedef void result_type;
      template<typename W>
      void operator()(W& worker)const
      {
         worker.pay_worker(pay, db);
      }
};

void database::update_worker_votes()
{
   const auto& idx = get_index_type<worker_index>().indices().get<by_account>();
   auto itr = idx.begin();
   auto itr_end = idx.end();
   while( itr != itr_end )
   {
      modify( *itr, [this]( worker_object& obj )
      {
         obj.total_votes = _vote_tally_buffer[obj.vote_id];
      });
      ++itr;
   }
}

void database::pay_workers( share_type& budget )
{
   const auto head_time = head_block_time();
//   ilog("Processing payroll! Available budget is ${b}", ("b", budget));
   vector<std::reference_wrapper<const worker_object>> active_workers;
   // TODO optimization: add by_expiration index to avoid iterating through all objects
   get_index_type<worker_index>().inspect_all_objects([head_time, &active_workers](const object& o) {
      const worker_object& w = static_cast<const worker_object&>(o);
      if( w.is_active(head_time) && w.total_votes > 0 )
         active_workers.emplace_back(w);
   });

   // worker with more votes is preferred
   // if two workers exactly tie for votes, worker with lower ID is preferred
   std::sort(active_workers.begin(), active_workers.end(), [](const worker_object& wa, const worker_object& wb) {
      share_type wa_vote = wa.total_votes;
      share_type wb_vote = wb.total_votes;
      if( wa_vote != wb_vote )
         return wa_vote > wb_vote;
      return wa.id < wb.id;
   });

   const auto last_budget_time = get_dynamic_global_properties().last_budget_time;
   const auto passed_time_ms = head_time - last_budget_time;
   const auto passed_time_count = passed_time_ms.count();
   const auto day_count = fc::days(1).count();
   for( uint32_t i = 0; i < active_workers.size() && budget > 0; ++i )
   {
      const worker_object& active_worker = active_workers[i];
      share_type requested_pay = active_worker.daily_pay;

      // Note: if there is a good chance that passed_time_count == day_count,
      //       for better performance, can avoid the 128 bit calculation by adding a check.
      //       Since it's not the case, we're not using a check here.
      fc::uint128_t pay = requested_pay.value;
      pay *= passed_time_count;
      pay /= day_count;
      requested_pay = static_cast<uint64_t>(pay);

      share_type actual_pay = std::min(budget, requested_pay);
      //ilog(" ==> Paying ${a} to worker ${w}", ("w", active_worker.id)("a", actual_pay));
      modify(active_worker, [&](worker_object& w) {
         w.worker.visit(worker_pay_visitor(actual_pay, *this));
      });

      budget -= actual_pay;
   }
}

void database::update_block_producers()
{ try {
   assert( !_validator_count_histogram_buffer.empty() );
   share_type stake_target = (_total_voting_stake-_validator_count_histogram_buffer[0]) / 2;

   /// accounts that vote for 0 or 1 validator do not get to express an opinion on
   /// the number of validators to have (they abstain and are non-voting accounts)

   share_type stake_tally = 0;

   size_t validator_count = 0;
   if( stake_target > 0 )
   {
      while( (validator_count < _validator_count_histogram_buffer.size() - 1)
             && (stake_tally <= stake_target) )
      {
         stake_tally += _validator_count_histogram_buffer[++validator_count];
      }
   }

   const chain_property_object& cpo = get_chain_properties();

   validator_count = std::max( (validator_count * 2) + 1,
                             (size_t)cpo.immutable_parameters.min_producer_count );
   auto wits = sort_votable_objects<validator_index>( validator_count );

   const global_property_object& gpo = get_global_properties();

   auto update_validator_total_votes = [this]( const validator_object& wit ) {
      modify( wit, [this]( validator_object& obj )
      {
         obj.total_votes = _vote_tally_buffer[obj.vote_id];
      });
   };

   if( _track_standby_votes )
   {
      const auto& all_validators = get_index_type<validator_index>().indices();
      for( const validator_object& wit : all_validators )
      {
         update_validator_total_votes( wit );
      }
   }
   else
   {
      for( const validator_object& wit : wits )
      {
         update_validator_total_votes( wit );
      }
   }

   // Update validator authority
   modify( get(GRAPHENE_PRODUCERS_ACCOUNT), [this,&wits]( account_object& a )
   {
      vote_counter vc;
      for( const validator_object& wit : wits )
         vc.add( wit.validator_account, _vote_tally_buffer[wit.vote_id] );
      vc.finish( a.active );
   } );

   modify( gpo, [&wits]( global_property_object& gp )
   {
      gp.block_producers.clear();
      gp.block_producers.reserve(wits.size());
      std::transform(wits.begin(), wits.end(),
                     std::inserter(gp.block_producers, gp.block_producers.end()),
                     [](const validator_object& w) {
         return w.get_id();
      });
   });

} FC_CAPTURE_AND_RETHROW() } // GCOVR_EXCL_LINE

void database::update_council_delegates()
{ try {
   assert( !_council_count_histogram_buffer.empty() );
   share_type stake_target = (_total_voting_stake-_council_count_histogram_buffer[0]) / 2;

   /// accounts that vote for 0 or 1 delegate do not get to express an opinion on
   /// the number of delegates to have (they abstain and are non-voting accounts)
   share_type stake_tally = 0;
   size_t delegate_count = 0;
   if( stake_target > 0 )
   {
      while( (delegate_count < _council_count_histogram_buffer.size() - 1)
             && (stake_tally <= stake_target.value) )
      {
         stake_tally += _council_count_histogram_buffer[++delegate_count];
      }
   }

   const chain_property_object& cpo = get_chain_properties();

   delegate_count = std::max( (delegate_count * 2) + 1,
                                      (size_t)cpo.immutable_parameters.min_council_count );
   auto delegates = sort_votable_objects<delegate_index>( delegate_count );

   auto update_delegate_total_votes = [this]( const delegate_object& cm ) {
      modify( cm, [this]( delegate_object& obj )
      {
         obj.total_votes = _vote_tally_buffer[obj.vote_id];
      });
   };

   if( _track_standby_votes )
   {
      const auto& all_delegates = get_index_type<delegate_index>().indices();
      for( const delegate_object& cm : all_delegates )
      {
         update_delegate_total_votes( cm );
      }
   }
   else
   {
      for( const delegate_object& cm : delegates )
      {
         update_delegate_total_votes( cm );
      }
   }

   // Update council authorities
   if( !delegates.empty() )
   {
      const account_object& council_account = get(GRAPHENE_COUNCIL_ACCOUNT);
      modify( council_account, [this,&delegates](account_object& a)
      {
         vote_counter vc;
         for( const delegate_object& cm : delegates )
            vc.add( cm.delegate_account, _vote_tally_buffer[cm.vote_id] );
         vc.finish( a.active );
      });
      modify( get(GRAPHENE_RELAXED_COUNCIL_ACCOUNT), [&council_account](account_object& a)
      {
         a.active = council_account.active;
      });
   }
   modify( get_global_properties(), [&delegates](global_property_object& gp)
   {
      gp.council_delegates.clear();
      std::transform(delegates.begin(), delegates.end(),
                     std::inserter(gp.council_delegates, gp.council_delegates.begin()),
                     [](const delegate_object& d) { return d.get_id(); });
   });
} FC_CAPTURE_AND_RETHROW() } // GCOVR_EXCL_LINE

void database::initialize_budget_record( fc::time_point_sec now, budget_record& rec )const
{
   const dynamic_global_property_object& dpo = get_dynamic_global_properties();
   const asset_object& core = get_core_asset();
   const asset_dynamic_data_object& core_dd = get_core_dynamic_data();

   rec.from_initial_reserve = core.reserved(*this);
   rec.from_accumulated_fees = core_dd.accumulated_fees;
   rec.from_unused_validator_budget = dpo.validator_budget;

   if(    (dpo.last_budget_time == fc::time_point_sec())
       || (now <= dpo.last_budget_time) )
   {
      rec.time_since_last_budget = 0;
      return;
   }

   int64_t dt = (now - dpo.last_budget_time).to_seconds();
   rec.time_since_last_budget = uint64_t( dt );

   // We'll consider accumulated_fees to be reserved at the BEGINNING
   // of the maintenance interval.  However, for speed we only
   // call modify() on the asset_dynamic_data_object once at the
   // end of the maintenance interval.  Thus the accumulated_fees
   // are available for the budget at this point, but not included
   // in core.reserved().
   share_type reserve = rec.from_initial_reserve + core_dd.accumulated_fees;
   // Similarly, we consider leftover validator_budget to be burned
   // at the BEGINNING of the maintenance interval.
   reserve += dpo.validator_budget;

   fc::uint128_t budget_u128 = reserve.value;
   budget_u128 *= uint64_t(dt);
   budget_u128 *= GRAPHENE_CORE_ASSET_CYCLE_RATE;
   //round up to the nearest satoshi -- this is necessary to ensure
   //   there isn't an "untouchable" reserve, and we will eventually
   //   be able to use the entire reserve
   budget_u128 += ((uint64_t(1) << GRAPHENE_CORE_ASSET_CYCLE_RATE_BITS) - 1);
   budget_u128 >>= GRAPHENE_CORE_ASSET_CYCLE_RATE_BITS;
   if( budget_u128 < static_cast<fc::uint128_t>(reserve.value) )
      rec.total_budget = share_type(static_cast<uint64_t>(budget_u128));
   else
      rec.total_budget = reserve;

   return;
}

/**
 * Update the budget for validators and workers.
 */
void database::process_budget()
{
   try
   {
      const global_property_object& gpo = get_global_properties();
      const dynamic_global_property_object& dpo = get_dynamic_global_properties();
      const asset_dynamic_data_object& core = get_core_dynamic_data();
      fc::time_point_sec now = head_block_time();

      int64_t time_to_maint = (dpo.next_maintenance_time - now).to_seconds();
      //
      // The code that generates the next maintenance time should
      //    only produce a result in the future.  If this assert
      //    fails, then the next maintenance time algorithm is buggy.
      //
      assert( time_to_maint > 0 );
      //
      // Code for setting chain parameters should validate
      //    block_interval > 0 (as well as the humans proposing /
      //    voting on changes to block interval).
      //
      assert( gpo.parameters.block_interval > 0 );
      uint64_t blocks_to_maint = (uint64_t(time_to_maint) + gpo.parameters.block_interval - 1)
                                 / gpo.parameters.block_interval;

      // blocks_to_maint > 0 because time_to_maint > 0,
      // which means numerator is at least equal to block_interval

      budget_record rec;
      initialize_budget_record( now, rec );
      share_type available_funds = rec.total_budget;

      share_type validator_budget = gpo.parameters.producer_pay_per_block.value * blocks_to_maint;
      rec.requested_validator_budget = validator_budget;
      validator_budget = std::min(validator_budget, available_funds);
      rec.validator_budget = validator_budget;
      available_funds -= validator_budget;

      fc::uint128_t worker_budget_u128 = gpo.parameters.worker_budget_per_day.value;
      worker_budget_u128 *= uint64_t(time_to_maint);
      constexpr uint64_t seconds_per_day = 86400;
      worker_budget_u128 /= seconds_per_day;

      share_type worker_budget;
      if( worker_budget_u128 >= static_cast<fc::uint128_t>(available_funds.value) )
         worker_budget = available_funds;
      else
         worker_budget = static_cast<uint64_t>(worker_budget_u128);
      rec.worker_budget = worker_budget;
      available_funds -= worker_budget;

      share_type leftover_worker_funds = worker_budget;
      pay_workers(leftover_worker_funds);
      rec.leftover_worker_funds = leftover_worker_funds;
      available_funds += leftover_worker_funds;

      rec.supply_delta = rec.validator_budget
         + rec.worker_budget
         - rec.leftover_worker_funds
         - rec.from_accumulated_fees
         - rec.from_unused_validator_budget;

      modify(core, [&rec,&validator_budget,&worker_budget,&leftover_worker_funds,&dpo]
                   ( asset_dynamic_data_object& _core )
      {
         _core.current_supply = (_core.current_supply + rec.supply_delta );

         assert( rec.supply_delta ==
                                   validator_budget
                                 + worker_budget
                                 - leftover_worker_funds
                                 - _core.accumulated_fees
                                 - dpo.validator_budget
                                );
         _core.accumulated_fees = 0;
      });

      modify(dpo, [&validator_budget, &now]( dynamic_global_property_object& _dpo )
      {
         // Since initial validator_budget was rolled into
         // available_funds, we replace it with validator_budget
         // instead of adding it.
         _dpo.validator_budget = validator_budget;
         _dpo.last_budget_time = now;
      });

      create< budget_record_object >( [this,&rec]( budget_record_object& _rec )
      {
         _rec.time = head_block_time();
         _rec.record = rec;
      });

      // available_funds is money we could spend, but don't want to.
      // we simply let it evaporate back into the reserve.
   }
   FC_CAPTURE_AND_RETHROW()
}

template< typename Visitor >
void visit_special_authorities( const database& db, Visitor visit )
{
   const auto& sa_idx = db.get_index_type< special_authority_index >().indices().get<by_id>();

   for( const special_authority_object& sao : sa_idx )
   {
      const account_object& acct = sao.account(db);
      if( !acct.owner_special_authority.is_type< no_special_authority >() )
      {
         visit( acct, true, acct.owner_special_authority );
      }
      if( !acct.active_special_authority.is_type< no_special_authority >() )
      {
         visit( acct, false, acct.active_special_authority );
      }
   }
}

void update_top_n_authorities( database& db )
{
   visit_special_authorities( db, [&db]( const account_object& acct, bool is_owner, const special_authority& auth )
   {
      if( auth.is_type< top_holders_special_authority >() )
      {
         // use index to grab the top N holders of the asset and vote_counter to obtain the weights

         const top_holders_special_authority& tha = auth.get< top_holders_special_authority >();
         vote_counter vc;
         const auto& bal_idx = db.get_index_type< account_balance_index >().indices().get< by_asset_balance >();
         uint8_t num_needed = tha.num_top_holders;
         if( num_needed == 0 )
            return;

         // find accounts
         const auto range = bal_idx.equal_range( boost::make_tuple( tha.asset ) );
         for( const account_balance_object& bal : boost::make_iterator_range( range.first, range.second ) )
         {
             assert( bal.asset_type == tha.asset );
             if( bal.owner == acct.id )
                continue;
             vc.add( bal.owner, bal.balance.value );
             --num_needed;
             if( num_needed == 0 )
                break;
         }

         db.modify( acct, [&vc,&is_owner]( account_object& a )
         {
            vc.finish( is_owner ? a.owner : a.active );
            if( !vc.is_empty() )
               a.top_n_control_flags |= (is_owner ? account_object::top_n_control_owner
                                                  : account_object::top_n_control_active);
         } );
      }
   } );
}

void split_fba_balance(
   database& db,
   uint64_t fba_id,
   uint16_t network_pct,
   uint16_t designated_asset_buyback_pct,
   uint16_t designated_asset_issuer_pct
)
{
   FC_ASSERT( uint32_t(network_pct) + designated_asset_buyback_pct + designated_asset_issuer_pct
              == GRAPHENE_100_PERCENT );
   const fba_accumulator_object& fba = fba_accumulator_id_type( fba_id )(db);
   if( fba.accumulated_fba_fees == 0 )
      return;

   const asset_dynamic_data_object& core_dd = db.get_core_dynamic_data();

   if( !fba.is_configured(db) )
   {
      ilog( "${n} core given to network at block ${b} due to non-configured FBA",
            ("n", fba.accumulated_fba_fees)("b", db.head_block_time()) );
      db.modify( core_dd, [&fba]( asset_dynamic_data_object& _core_dd )
      {
         _core_dd.current_supply -= fba.accumulated_fba_fees;
      } );
      db.modify( fba, []( fba_accumulator_object& _fba )
      {
         _fba.accumulated_fba_fees = 0;
      } );
      return;
   }

   fc::uint128_t buyback_amount_128 = fba.accumulated_fba_fees.value;
   buyback_amount_128 *= designated_asset_buyback_pct;
   buyback_amount_128 /= GRAPHENE_100_PERCENT;
   share_type buyback_amount = static_cast<uint64_t>(buyback_amount_128);

   fc::uint128_t issuer_amount_128 = fba.accumulated_fba_fees.value;
   issuer_amount_128 *= designated_asset_issuer_pct;
   issuer_amount_128 /= GRAPHENE_100_PERCENT;
   share_type issuer_amount = static_cast<uint64_t>(issuer_amount_128);

   // this assert should never fail
   FC_ASSERT( buyback_amount + issuer_amount <= fba.accumulated_fba_fees );

   share_type network_amount = fba.accumulated_fba_fees - (buyback_amount + issuer_amount);

   const asset_object& designated_asset = (*fba.designated_asset)(db);

   if( network_amount != 0 )
   {
      db.modify( core_dd, [&]( asset_dynamic_data_object& _core_dd )
      {
         _core_dd.current_supply -= network_amount;
      } );
   }

   fba_distribute_operation vop;
   vop.account_id = *designated_asset.buyback_account;
   vop.fba_id = fba.id;
   vop.amount = buyback_amount;
   if( vop.amount != 0 )
   {
      db.adjust_balance( *designated_asset.buyback_account, asset(buyback_amount) );
      db.push_applied_operation(vop);
   }

   vop.account_id = designated_asset.issuer;
   vop.fba_id = fba.id;
   vop.amount = issuer_amount;
   if( vop.amount != 0 )
   {
      db.adjust_balance( designated_asset.issuer, asset(issuer_amount) );
      db.push_applied_operation(vop);
   }

   db.modify( fba, []( fba_accumulator_object& _fba )
   {
      _fba.accumulated_fba_fees = 0;
   } );
}

void distribute_fba_balances( database& db )
{
   constexpr uint16_t twenty_percent = 20 * GRAPHENE_1_PERCENT;
   constexpr uint16_t sixty_percent  = 60 * GRAPHENE_1_PERCENT;
   split_fba_balance( db, fba_accumulator_id_transfer_to_blind  , twenty_percent, sixty_percent, twenty_percent );
   split_fba_balance( db, fba_accumulator_id_blind_transfer     , twenty_percent, sixty_percent, twenty_percent );
   split_fba_balance( db, fba_accumulator_id_transfer_from_blind, twenty_percent, sixty_percent, twenty_percent );
}

void create_buyback_orders( database& db )
{
   const auto& bbo_idx = db.get_index_type< buyback_index >().indices().get<by_id>();
   const auto& bal_idx = db.get_index_type< primary_index< account_balance_index > >()
                           .get_secondary_index< balances_by_account_index >();

   for( const buyback_object& bbo : bbo_idx )
   {
      const asset_object& asset_to_buy = bbo.asset_to_buy(db);
      assert( asset_to_buy.buyback_account.valid() );

      const account_object& buyback_account = (*(asset_to_buy.buyback_account))(db);

      if( !buyback_account.allowed_assets.valid() )
      {
         wlog( "skipping buyback account ${b} at block ${n} because allowed_assets does not exist",
               ("b", buyback_account)("n", db.head_block_num()) );
         continue;
      }

      for( const auto& entry : bal_idx.get_account_balances( buyback_account.get_id() ) )
      {
         const auto* it = entry.second;
         asset_id_type asset_to_sell = it->asset_type;
         share_type amount_to_sell = it->balance;
         if( asset_to_sell == asset_to_buy.id )
            continue;
         if( amount_to_sell == 0 )
            continue;
         if( buyback_account.allowed_assets->find( asset_to_sell ) == buyback_account.allowed_assets->end() )
         {
            wlog( "buyback account ${b} not selling disallowed holdings of asset ${a} at block ${n}",
                  ("b", buyback_account)("a", asset_to_sell)("n", db.head_block_num()) );
            continue;
         }

         try
         {
            transaction_evaluation_state buyback_context(&db);
            buyback_context.skip_fee_schedule_check = true;

            limit_order_create_operation create_vop;
            create_vop.fee = asset( 0, asset_id_type() );
            create_vop.seller = buyback_account.id;
            create_vop.amount_to_sell = asset( amount_to_sell, asset_to_sell );
            create_vop.min_to_receive = asset( 1, asset_to_buy.get_id() );
            create_vop.expiration = time_point_sec::maximum();
            create_vop.fill_or_kill = false;

            limit_order_id_type order_id{ db.apply_operation( buyback_context, create_vop ).get< object_id_type >() };

            if( db.find( order_id ) != nullptr )
            {
               limit_order_cancel_operation cancel_vop;
               cancel_vop.fee = asset( 0, asset_id_type() );
               cancel_vop.order = order_id;
               cancel_vop.fee_paying_account = buyback_account.id;

               db.apply_operation( buyback_context, cancel_vop );
            }
         }
         catch( const fc::exception& e )
         {
            // we can in fact get here,
            // e.g. if asset issuer of buy/sell asset blacklists/whitelists the buyback account
            wlog( "Skipping buyback processing selling ${as} for ${ab} for buyback account ${b} at block ${n}; "
                  "exception was ${e}",
                  ("as", asset_to_sell)("ab", asset_to_buy)("b", buyback_account)
                  ("n", db.head_block_num())("e", e.to_detail_string()) );
            continue;
         }
      }
   }
   return;
}

void database::process_bids( const backed_asset_data_object& bad )
{
   if( bad.is_prediction_market || bad.current_feed.settlement_price.is_null() )
      return;

   asset_id_type to_revive_id = bad.asset_id;
   const asset_object& to_revive = to_revive_id( *this );
   const asset_dynamic_data_object& bdd = to_revive.dynamic_data( *this );

   if( bdd.current_supply == 0 ) // shortcut
   {
      _cancel_bids_and_revive_backed_asset( to_revive, bad );
      return;
   }

   const auto& bid_idx = get_index_type< collateral_bid_index >().indices().get<by_price>();
   const auto start = bid_idx.lower_bound( to_revive_id );
   auto end = bid_idx.upper_bound( to_revive_id );

   share_type covered = 0;
   auto itr = start;
   auto revive_ratio = bad.current_feed.maintenance_collateral_ratio;
   while( covered < bdd.current_supply && itr != end )
   {
      const collateral_bid_object& bid = *itr;
      asset debt_in_bid = bid.inv_swan_price.quote;
      if( debt_in_bid.amount > bdd.current_supply )
         debt_in_bid.amount = bdd.current_supply;
      asset total_collateral = debt_in_bid * bad.settlement_price;
      total_collateral += bid.inv_swan_price.base;
      price call_price = price::call_price( debt_in_bid, total_collateral, revive_ratio );
      if( ~call_price >= bad.current_feed.settlement_price ) break;
      covered += debt_in_bid.amount;
      ++itr;
   }
   if( covered < bdd.current_supply ) return;

   end = itr;
   share_type to_cover = bdd.current_supply;
   share_type remaining_fund = bad.settlement_fund;
   itr = start;
   while( itr != end )
   {
      const collateral_bid_object& bid = *itr;
      ++itr;
      asset debt_in_bid = bid.inv_swan_price.quote;
      if( debt_in_bid.amount > bdd.current_supply )
         debt_in_bid.amount = bdd.current_supply;
      share_type debt = debt_in_bid.amount;
      share_type collateral = (debt_in_bid * bad.settlement_price).amount;
      if( debt >= to_cover )
      {
         debt = to_cover;
         collateral = remaining_fund;
      }
      to_cover -= debt;
      remaining_fund -= collateral;
      execute_bid( bid, debt, collateral, bad.current_feed );
   }
   FC_ASSERT( remaining_fund == 0 );
   FC_ASSERT( to_cover == 0 );

   _cancel_bids_and_revive_backed_asset( to_revive, bad );
}

void database::process_backed_assets()
{
   time_point_sec head_time = head_block_time();
   uint32_t head_epoch_seconds = head_time.sec_since_epoch();

   const auto& update_backed_asset = [this,&head_time,head_epoch_seconds]
                                 ( backed_asset_data_object &o )
   {
      o.force_settled_volume = 0; // Reset all Backed Asset force settlement volumes to zero

      // clear expired feeds if non-user asset (validator_fed or delegate_fed) && check overflow
      if( o.options.feed_lifetime_sec < head_epoch_seconds
            && ( 0 != ( o.asset_id(*this).options.flags & ( validator_fed_asset | delegate_fed_asset ) ) ) )
      {
         fc::time_point_sec calculated = head_time - o.options.feed_lifetime_sec;
         auto itr = o.feeds.rbegin();
         auto end = o.feeds.rend();
         while( itr != end ) // loop feeds
         {
            auto feed_time = itr->second.first;
            std::advance( itr, 1 );
            if( feed_time < calculated )
               o.feeds.erase( itr.base() ); // delete expired feed
         }
         // Note: we don't update current_feed here, and the update_expired_feeds() call is a bit too late,
         //       so theoretically there could be an inconsistency between active feeds and current_feed.
         //       And note that the next step "process_bids()" is based on current_feed.
      }
   };

   for( const auto& d : get_index_type<backed_asset_data_index>().indices() )
   {
      modify( d, update_backed_asset );
      if( d.has_settlement() )
         process_bids(d);
   }
}

void database::perform_chain_maintenance(const signed_block& next_block)
{
   const auto& gpo = get_global_properties();
   const auto& dgpo = get_dynamic_global_properties();

   distribute_fba_balances(*this);
   create_buyback_orders(*this);

   struct vote_tally_helper {
      database& d;
      const global_property_object& props;

      vote_tally_helper(database& d, const global_property_object& gpo)
         : d(d), props(gpo)
      {
         d._vote_tally_buffer.resize(props.next_available_vote_id);
         d._validator_count_histogram_buffer.resize(props.parameters.maximum_producer_count / 2 + 1);
         d._council_count_histogram_buffer.resize(props.parameters.maximum_council_count / 2 + 1);
         d._total_voting_stake = 0;
      }

      void operator()( const account_object& stake_account, const account_statistics_object& stats )
      {
         if( props.parameters.count_non_member_votes || stake_account.is_member(d.head_block_time()) )
         {
            // There may be a difference between the account whose stake is voting and the one specifying opinions.
            // Usually they're the same, but if the stake account has specified a voting_account, that account is the one
            // specifying the opinions.
            const account_object& opinion_account =
                  (stake_account.options.voting_account ==
                   GRAPHENE_PROXY_TO_SELF_ACCOUNT)? stake_account
                                     : d.get(stake_account.options.voting_account);

            uint64_t voting_stake = stats.total_core_in_orders.value
                  + (stake_account.cashback_vb.valid() ? (*stake_account.cashback_vb)(d).balance.amount.value: 0)
                  + stats.core_in_balance.value;

            for( vote_id_type id : opinion_account.options.votes )
            {
               uint32_t offset = id.instance();
               // if they somehow managed to specify an illegal offset, ignore it.
               if( offset < d._vote_tally_buffer.size() )
                  d._vote_tally_buffer[offset] += voting_stake;
            }

            if( opinion_account.options.num_producers <= props.parameters.maximum_producer_count )
            {
               uint16_t offset = std::min(size_t(opinion_account.options.num_producers/2),
                                          d._validator_count_histogram_buffer.size() - 1);
               // votes for a number greater than maximum_producer_count
               // are turned into votes for maximum_producer_count.
               //
               // in particular, this takes care of the case where a
               // member was voting for a high number, then the
               // parameter was lowered.
               d._validator_count_histogram_buffer[offset] += voting_stake;
            }
            if( opinion_account.options.num_delegates <= props.parameters.maximum_council_count )
            {
               uint16_t offset = std::min(size_t(opinion_account.options.num_delegates/2),
                                          d._council_count_histogram_buffer.size() - 1);
               // votes for a number greater than maximum_council_count
               // are turned into votes for maximum_council_count.
               //
               // same rationale as for validators
               d._council_count_histogram_buffer[offset] += voting_stake;
            }

            d._total_voting_stake += voting_stake;
         }
      }
   };
   
   vote_tally_helper tally_helper(*this, gpo);

   perform_account_maintenance( tally_helper );

   struct clear_canary {
      clear_canary(vector<uint64_t>& target): target(target){}
      ~clear_canary() { target.clear(); }
   private:
      vector<uint64_t>& target;
   };
   clear_canary a(_validator_count_histogram_buffer);
   clear_canary b(_council_count_histogram_buffer);
   clear_canary c(_vote_tally_buffer);

   update_top_n_authorities(*this);
   update_block_producers();
   update_council_delegates();
   update_worker_votes();

   modify(gpo, [&dgpo](global_property_object& p) {
      // Remove scaling of account registration fee
      p.parameters.get_mutable_fees().get<account_create_operation>().basic_fee >>=
            p.parameters.account_fee_scale_bitshifts *
            (dgpo.accounts_registered_this_interval / p.parameters.accounts_per_fee_scale);

      if( p.pending_parameters )
      {
         p.parameters = std::move(*p.pending_parameters);
         p.pending_parameters.reset();
      }
   });

   auto next_maintenance_time = dgpo.next_maintenance_time;
   auto maintenance_interval = gpo.parameters.maintenance_interval;

   if( next_maintenance_time <= next_block.timestamp )
   {
      if( next_block.block_num() == 1 )
         next_maintenance_time = time_point_sec() +
               (((next_block.timestamp.sec_since_epoch() / maintenance_interval) + 1) * maintenance_interval);
      else
      {
         // We want to find the smallest k such that next_maintenance_time + k * maintenance_interval > head_block_time()
         //  This implies k > ( head_block_time() - next_maintenance_time ) / maintenance_interval
         //
         // Let y be the right-hand side of this inequality, i.e.
         // y = ( head_block_time() - next_maintenance_time ) / maintenance_interval
         //
         // and let the fractional part f be y-floor(y).  Clearly 0 <= f < 1.
         // We can rewrite f = y-floor(y) as floor(y) = y-f.
         //
         // Clearly k = floor(y)+1 has k > y as desired.  Now we must
         // show that this is the least such k, i.e. k-1 <= y.
         //
         // But k-1 = floor(y)+1-1 = floor(y) = y-f <= y.
         // So this k suffices.
         //
         auto y = (head_block_time() - next_maintenance_time).to_seconds() / maintenance_interval;
         next_maintenance_time += (uint32_t)( (y+1) * maintenance_interval );
      }
   }

   modify(dgpo, [next_maintenance_time](dynamic_global_property_object& d) {
      d.next_maintenance_time = next_maintenance_time;
      d.accounts_registered_this_interval = 0;
   });

   process_backed_assets();

   // process_budget needs to run at the bottom because
   //   it needs to know the next_maintenance_time
   process_budget();
}

} }
