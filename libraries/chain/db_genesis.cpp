#include <graphene/chain/database.hpp>
#include <graphene/chain/fba_accumulator_id.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/balance_object.hpp>
#include <graphene/chain/block_summary_object.hpp>
#include <graphene/chain/chain_property_object.hpp>
#include <graphene/chain/delegate_object.hpp>
#include <graphene/chain/fba_object.hpp>
#include <graphene/chain/global_property_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/vesting_balance_object.hpp>
#include <graphene/chain/validator_object.hpp>
#include <graphene/chain/producer_schedule_object.hpp>
#include <graphene/chain/worker_object.hpp>

#include <fc/crypto/digest.hpp>

#include <boost/algorithm/string.hpp>

namespace graphene { namespace chain {

void database::init_genesis(const genesis_state_type& genesis_state)
{ try {
   FC_ASSERT( genesis_state.initial_timestamp != time_point_sec(), "Must initialize genesis timestamp." );
   FC_ASSERT( genesis_state.initial_timestamp.sec_since_epoch() % GRAPHENE_DEFAULT_BLOCK_INTERVAL == 0,
              "Genesis timestamp must be divisible by GRAPHENE_DEFAULT_BLOCK_INTERVAL." );
   FC_ASSERT(genesis_state.initial_validator_candidates.size() > 0,
             "Cannot start a chain with zero validators.");
   FC_ASSERT(genesis_state.initial_block_producers <= genesis_state.initial_validator_candidates.size(),
             "initial_block_producers is larger than the number of candidate validators.");

   _undo_db.disable();
   struct auth_inhibitor {
      explicit auth_inhibitor(database& db) : db(db), old_flags(db.node_properties().skip_flags)
      { db.node_properties().skip_flags |= skip_transaction_signatures; }
      ~auth_inhibitor()
      { db.node_properties().skip_flags = old_flags; }
      auth_inhibitor(const auth_inhibitor&) = delete;
   private:
      database& db;
      uint32_t old_flags;
   };
   auth_inhibitor inhibitor(*this);

   transaction_evaluation_state genesis_eval_state(this);

   _current_block_time = genesis_state.initial_timestamp;

   // Create blockchain accounts
   fc::ecc::private_key null_private_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("null_key")));
   create<account_balance_object>([](account_balance_object& b) {
      b.balance = GRAPHENE_MAX_SHARE_SUPPLY;
   });
   const account_object& council_account =
      create<account_object>( [this](account_object& n) {
         n.membership_expiration_date = time_point_sec::maximum();
         n.network_fee_percentage = GRAPHENE_DEFAULT_NETWORK_PERCENT_OF_FEE;
         n.lifetime_referrer_fee_percentage = GRAPHENE_100_PERCENT - GRAPHENE_DEFAULT_NETWORK_PERCENT_OF_FEE;
         n.owner.weight_threshold = 1;
         n.active.weight_threshold = 1;
         n.name = "council-account";
         n.statistics = create<account_statistics_object>( [&n](account_statistics_object& s){
                           s.owner = n.id;
                           s.name = n.name;
                           s.core_in_balance = GRAPHENE_MAX_SHARE_SUPPLY;
                        }).id;
         n.creation_block_num = 0;
         n.creation_time = _current_block_time;
      });
   FC_ASSERT(council_account.get_id() == GRAPHENE_COUNCIL_ACCOUNT);
   FC_ASSERT(create<account_object>([this](account_object& a) {
       a.name = "validator-account";
       a.statistics = create<account_statistics_object>([&a](account_statistics_object& s){
                         s.owner = a.id;
                         s.name = a.name;
                      }).id;
       a.owner.weight_threshold = 1;
       a.active.weight_threshold = 1;
       a.registrar = GRAPHENE_PRODUCERS_ACCOUNT;
       a.referrer = a.registrar;
       a.lifetime_referrer = a.registrar;
       a.membership_expiration_date = time_point_sec::maximum();
       a.network_fee_percentage = GRAPHENE_DEFAULT_NETWORK_PERCENT_OF_FEE;
       a.lifetime_referrer_fee_percentage = GRAPHENE_100_PERCENT - GRAPHENE_DEFAULT_NETWORK_PERCENT_OF_FEE;
       a.creation_block_num = 0;
       a.creation_time = _current_block_time;
   }).get_id() == GRAPHENE_PRODUCERS_ACCOUNT);
   FC_ASSERT(create<account_object>([this](account_object& a) {
       a.name = "relaxed-council-account";
       a.statistics = create<account_statistics_object>([&a](account_statistics_object& s){
                         s.owner = a.id;
                         s.name = a.name;
                      }).id;
       a.owner.weight_threshold = 1;
       a.active.weight_threshold = 1;
       a.registrar = GRAPHENE_RELAXED_COUNCIL_ACCOUNT;
       a.referrer = a.registrar;
       a.lifetime_referrer = a.registrar;
       a.membership_expiration_date = time_point_sec::maximum();
       a.network_fee_percentage = GRAPHENE_DEFAULT_NETWORK_PERCENT_OF_FEE;
       a.lifetime_referrer_fee_percentage = GRAPHENE_100_PERCENT - GRAPHENE_DEFAULT_NETWORK_PERCENT_OF_FEE;
       a.creation_block_num = 0;
       a.creation_time = _current_block_time;
   }).get_id() == GRAPHENE_RELAXED_COUNCIL_ACCOUNT);
   // The same data set is assigned to more than one account
   auto init_account_data_as_null = [this](account_object& a) {
       a.statistics = create<account_statistics_object>([&a](account_statistics_object& s){
                         s.owner = a.id;
                         s.name = a.name;
                      }).id;
       a.owner.weight_threshold = 1;
       a.active.weight_threshold = 1;
       a.registrar = GRAPHENE_NULL_ACCOUNT;
       a.referrer = a.registrar;
       a.lifetime_referrer = a.registrar;
       a.membership_expiration_date = time_point_sec::maximum();
       a.network_fee_percentage = 0;
       a.lifetime_referrer_fee_percentage = GRAPHENE_100_PERCENT;
       a.creation_block_num = 0;
       a.creation_time = _current_block_time;
   };
   FC_ASSERT(create<account_object>([&init_account_data_as_null](account_object& a) {
       a.name = "null-account";
       init_account_data_as_null(a);
   }).get_id() == GRAPHENE_NULL_ACCOUNT);
   FC_ASSERT(create<account_object>([this](account_object& a) {
       a.name = "temp-account";
       a.statistics = create<account_statistics_object>([&a](account_statistics_object& s){
                         s.owner = a.id;
                         s.name = a.name;
                      }).id;
       a.owner.weight_threshold = 0;
       a.active.weight_threshold = 0;
       a.registrar = GRAPHENE_TEMP_ACCOUNT;
       a.referrer = a.registrar;
       a.lifetime_referrer = a.registrar;
       a.membership_expiration_date = time_point_sec::maximum();
       a.network_fee_percentage = GRAPHENE_DEFAULT_NETWORK_PERCENT_OF_FEE;
       a.lifetime_referrer_fee_percentage = GRAPHENE_100_PERCENT - GRAPHENE_DEFAULT_NETWORK_PERCENT_OF_FEE;
       a.creation_block_num = 0;
       a.creation_time = _current_block_time;
   }).get_id() == GRAPHENE_TEMP_ACCOUNT);
   FC_ASSERT(create<account_object>([&init_account_data_as_null](account_object& a) {
       a.name = "proxy-to-self";
       init_account_data_as_null(a);
   }).get_id() == GRAPHENE_PROXY_TO_SELF_ACCOUNT);

   // Create more special accounts and remove them, reserve the IDs
   while( true )
   {
      uint64_t id = get_index<account_object>().get_next_id().instance();
      if( id >= genesis_state.immutable_parameters.num_special_accounts )
         break;
      const account_object& acct = create<account_object>([this,id](account_object& a) {
          a.name = "special-account-" + std::to_string(id);
          a.statistics = create<account_statistics_object>([&a](account_statistics_object& s){
                            s.owner = a.id;
                            s.name = a.name;
                         }).id;
          a.owner.weight_threshold = 1;
          a.active.weight_threshold = 1;
          a.registrar = account_id_type(id);
          a.referrer = a.registrar;
          a.lifetime_referrer = a.registrar;
          a.membership_expiration_date = time_point_sec::maximum();
          a.network_fee_percentage = GRAPHENE_DEFAULT_NETWORK_PERCENT_OF_FEE;
          a.lifetime_referrer_fee_percentage = GRAPHENE_100_PERCENT - GRAPHENE_DEFAULT_NETWORK_PERCENT_OF_FEE;
          a.creation_block_num = 0;
          a.creation_time = _current_block_time;
      });
      FC_ASSERT( acct.get_id() == account_id_type(id) );
      remove( acct.statistics(*this) );
      remove( acct );
   }

   // Create core asset
   const asset_dynamic_data_object& core_dyn_asset =
      create<asset_dynamic_data_object>([](asset_dynamic_data_object& a) {
         a.current_supply = GRAPHENE_MAX_SHARE_SUPPLY;
      });
   const asset_object& core_asset =
     create<asset_object>( [&genesis_state,&core_dyn_asset,this]( asset_object& a ) {
         a.symbol = GRAPHENE_SYMBOL;
         a.options.max_supply = genesis_state.max_core_supply;
         a.precision = GRAPHENE_BLOCKCHAIN_PRECISION_DIGITS;
         a.options.flags = 0;
         a.options.issuer_permissions = 0;
         a.issuer = GRAPHENE_NULL_ACCOUNT;
         a.options.core_exchange_rate.base.amount = 1;
         a.options.core_exchange_rate.base.asset_id = asset_id_type(0);
         a.options.core_exchange_rate.quote.amount = 1;
         a.options.core_exchange_rate.quote.asset_id = asset_id_type(0);
         a.dynamic_asset_data_id = core_dyn_asset.id;
         a.creation_block_num = 0;
         a.creation_time = _current_block_time;
      });
   FC_ASSERT( core_dyn_asset.id == asset_dynamic_data_id_type() );
   FC_ASSERT( asset_id_type(core_asset.id) == asset().asset_id );
   FC_ASSERT( get_balance(account_id_type(), asset_id_type()) == asset(core_dyn_asset.current_supply) );
   _p_core_asset_obj = &core_asset;
   _p_core_dynamic_data_obj = &core_dyn_asset;
   // Create more special assets and remove them, reserve the IDs
   while( true )
   {
      uint64_t id = get_index<asset_object>().get_next_id().instance();
      if( id >= genesis_state.immutable_parameters.num_special_assets )
         break;
      const asset_dynamic_data_object& dyn_asset =
         create<asset_dynamic_data_object>([](asset_dynamic_data_object& a) {
            a.current_supply = 0;
         });
      const asset_object& asset_obj = create<asset_object>( [id,&dyn_asset,this]( asset_object& a ) {
         a.symbol = "SPECIAL" + std::to_string( id );
         a.options.max_supply = 0;
         a.precision = GRAPHENE_BLOCKCHAIN_PRECISION_DIGITS;
         a.options.flags = 0;
         a.options.issuer_permissions = 0;
         a.issuer = GRAPHENE_NULL_ACCOUNT;
         a.options.core_exchange_rate.base.amount = 1;
         a.options.core_exchange_rate.base.asset_id = asset_id_type(0);
         a.options.core_exchange_rate.quote.amount = 1;
         a.options.core_exchange_rate.quote.asset_id = asset_id_type(0);
         a.dynamic_asset_data_id = dyn_asset.id;
         a.creation_block_num = 0;
         a.creation_time = _current_block_time;
      });
      FC_ASSERT( asset_obj.get_id() == asset_id_type(id) );
      remove( dyn_asset );
      remove( asset_obj );
   }

   chain_id_type chain_id = genesis_state.compute_chain_id();

   // Create global properties
   _p_global_prop_obj = & create<global_property_object>([&genesis_state](global_property_object& p) {
       p.parameters = genesis_state.initial_parameters;
       // Set fees to zero initially, so that genesis initialization needs not pay them
       // We'll fix it at the end of the function
       p.parameters.get_mutable_fees().zero_all_fees();

   });
   _p_dyn_global_prop_obj = & create<dynamic_global_property_object>(
                                 [&genesis_state](dynamic_global_property_object& p) {
      p.time = genesis_state.initial_timestamp;
      p.dynamic_flags = 0;
      p.validator_budget = 0;
      p.recent_slots_filled = std::numeric_limits<fc::uint128_t>::max();
   });

   FC_ASSERT( (genesis_state.immutable_parameters.min_producer_count & 1) == 1, "min_producer_count must be odd" );
   FC_ASSERT( (genesis_state.immutable_parameters.min_council_count & 1) == 1,
              "min_council_count must be odd" );

   _p_chain_property_obj = & create<chain_property_object>([chain_id,&genesis_state](chain_property_object& p)
   {
      p.chain_id = chain_id;
      p.immutable_parameters = genesis_state.immutable_parameters;
   } );

   constexpr uint32_t block_summary_object_count = 0x10000;
   for (uint32_t i = 0; i <= block_summary_object_count; ++i)
   {
      create<block_summary_object>( [](const block_summary_object&) {
         // Nothing to do
      } );
   }

   // Create initial accounts
   for( const auto& account : genesis_state.initial_accounts )
   {
      account_create_operation cop;
      cop.name = account.name;
      cop.registrar = GRAPHENE_TEMP_ACCOUNT;
      cop.owner = authority(1, account.owner_key, 1);
      if( account.active_key == public_key_type() )
      {
         cop.active = cop.owner;
         cop.options.memo_key = account.owner_key;
      }
      else
      {
         cop.active = authority(1, account.active_key, 1);
         cop.options.memo_key = account.active_key;
      }
      account_id_type account_id(apply_operation(genesis_eval_state, cop).get<object_id_type>());

      if( account.is_lifetime_member )
      {
          account_upgrade_operation op;
          op.account_to_upgrade = account_id;
          op.upgrade_to_lifetime_member = true;
          apply_operation(genesis_eval_state, op);
      }
   }

   // Helper function to get account ID by name
   const auto& accounts_by_name = get_index_type<account_index>().indices().get<by_name>();
   auto get_account_id = [&accounts_by_name](const string& name) {
      auto itr = accounts_by_name.find(name);
      FC_ASSERT(itr != accounts_by_name.end(),
                "Unable to find account '${acct}'. Did you forget to add a record for it to initial_accounts?",
                ("acct", name));
      return itr->get_id();
   };

   // Helper function to get asset ID by symbol
   const auto& assets_by_symbol = get_index_type<asset_index>().indices().get<by_symbol>();
   const auto get_asset_id = [&assets_by_symbol](const string& symbol) {
      auto itr = assets_by_symbol.find(symbol);
      FC_ASSERT(itr != assets_by_symbol.end(),
                "Unable to find asset '${sym}'. Did you forget to add a record for it to initial_assets?",
                ("sym", symbol));
      return itr->get_id();
   };

   map<asset_id_type, share_type> total_supplies;
   map<asset_id_type, share_type> total_debts;

   // Create initial assets
   for( const genesis_state_type::initial_asset_type& asst : genesis_state.initial_assets )
   {
      asset_id_type new_asset_id { get_index_type<asset_index>().get_next_id() };
      total_supplies[ new_asset_id ] = 0;

      asset_dynamic_data_id_type dynamic_data_id;
      optional<asset_bitasset_data_id_type> bitasset_data_id;
      if( asst.is_bitasset )
      {
         size_t collateral_holder_number = 0;
         total_debts[ new_asset_id ] = 0;
         for( const auto& collateral_rec : asst.collateral_records )
         {
            account_create_operation cop;
            cop.name = asst.symbol + "-collateral-holder-" + std::to_string(collateral_holder_number);
            boost::algorithm::to_lower(cop.name);
            cop.registrar = GRAPHENE_TEMP_ACCOUNT;
            cop.owner = authority(1, collateral_rec.owner, 1);
            cop.active = cop.owner;
            account_id_type owner_account_id { apply_operation(genesis_eval_state, cop).get<object_id_type>() };

            modify( owner_account_id(*this).statistics(*this), [&collateral_rec]( account_statistics_object& o ) {
               o.total_core_in_orders = collateral_rec.collateral;
            });

            create<call_order_object>(
                     [&owner_account_id,&collateral_rec,&new_asset_id,&core_asset](call_order_object& c) {
               c.borrower = owner_account_id;
               c.collateral = collateral_rec.collateral;
               c.debt = collateral_rec.debt;
               c.call_price = price::call_price(chain::asset(c.debt, new_asset_id),
                                                chain::asset(c.collateral, core_asset.get_id()),
                                                GRAPHENE_DEFAULT_MAINTENANCE_COLLATERAL_RATIO);
            });

            total_supplies[ asset_id_type(0) ] += collateral_rec.collateral;
            total_debts[ new_asset_id ] += collateral_rec.debt;
            ++collateral_holder_number;
         }

         bitasset_data_id = create<asset_bitasset_data_object>([&core_asset,new_asset_id](asset_bitasset_data_object& b) {
            b.options.short_backing_asset = core_asset.id;
            b.options.minimum_feeds = GRAPHENE_DEFAULT_MINIMUM_FEEDS;
            b.asset_id = new_asset_id;
         }).id;
      }

      dynamic_data_id = create<asset_dynamic_data_object>([&asst](asset_dynamic_data_object& d) {
         d.accumulated_fees = asst.accumulated_fees;
      }).id;

      total_supplies[ new_asset_id ] += asst.accumulated_fees;

      create<asset_object>([&asst,&get_account_id,&dynamic_data_id,&bitasset_data_id,this](asset_object& a)
      {
         a.symbol = asst.symbol;
         a.options.description = asst.description;
         a.precision = asst.precision;
         string issuer_name = asst.issuer_name;
         a.issuer = get_account_id(issuer_name);
         a.options.max_supply = asst.max_supply;
         a.options.flags = validator_fed_asset;
         a.options.issuer_permissions = ( asst.is_bitasset ? ASSET_ISSUER_PERMISSION_MASK
                                                           : UIA_ASSET_ISSUER_PERMISSION_MASK );
         a.dynamic_asset_data_id = dynamic_data_id;
         a.bitasset_data_id = bitasset_data_id;
         a.creation_block_num = 0;
         a.creation_time = _current_block_time;
      });
   }

   // Create initial balances
   for( const auto& handout : genesis_state.initial_balances )
   {
      const auto asset_id = get_asset_id(handout.asset_symbol);
      create<balance_object>([&handout, asset_id](balance_object& b) {
         b.balance = asset(handout.amount, asset_id);
         b.owner = handout.owner;
      });

      total_supplies[ asset_id ] += handout.amount;
   }

   // Create initial vesting balances
   for( const genesis_state_type::initial_vesting_balance_type& vest : genesis_state.initial_vesting_balances )
   {
      const auto asset_id = get_asset_id(vest.asset_symbol);
      create<balance_object>([&vest,&asset_id](balance_object& b) {
         b.owner = vest.owner;
         b.balance = asset(vest.amount, asset_id);

         linear_vesting_policy policy;
         policy.begin_timestamp = vest.begin_timestamp;
         policy.vesting_cliff_seconds = 0;
         policy.vesting_duration_seconds = vest.vesting_duration_seconds;
         policy.begin_balance = vest.begin_balance;

         b.vesting_policy = std::move(policy);
      });

      total_supplies[ asset_id ] += vest.amount;
   }

   if( total_supplies[ asset_id_type(0) ] > 0 )
   {
       adjust_balance(GRAPHENE_COUNCIL_ACCOUNT, -get_balance(GRAPHENE_COUNCIL_ACCOUNT,{}));
   }
   else
   {
       total_supplies[ asset_id_type(0) ] = GRAPHENE_MAX_SHARE_SUPPLY;
   }

   const auto& idx = get_index_type<asset_index>().indices().get<by_symbol>();
   auto it = idx.begin();
   bool has_imbalanced_assets = false;

   while( it != idx.end() )
   {
      if( it->bitasset_data_id.valid() )
      {
         auto supply_itr = total_supplies.find( it->get_id() );
         auto debt_itr = total_debts.find( it->get_id() );
         FC_ASSERT( supply_itr != total_supplies.end() );
         FC_ASSERT( debt_itr != total_debts.end() );
         if( supply_itr->second != debt_itr->second )
         {
            has_imbalanced_assets = true;
            elog( "Genesis for asset ${aname} is not balanced\n"
                  "   Debt is ${debt}\n"
                  "   Supply is ${supply}\n",
                  ("aname", it->symbol)
                  ("debt", debt_itr->second)
                  ("supply", supply_itr->second)
                );
         }
      }
      ++it;
   }
   FC_ASSERT( !has_imbalanced_assets );

   // Save tallied supplies
   for( const auto& item : total_supplies )
   {
      const auto& asset_id = item.first;
      const auto& total_supply = item.second;

      modify( get(get(asset_id).dynamic_asset_data_id), [&total_supply]( asset_dynamic_data_object& asset_data ) {
         asset_data.current_supply = total_supply;
      } );
   }

   // Create special validator account and remove it, reserve the id
   const validator_object& wit = create<validator_object>([](const validator_object&) {
      // Nothing to do
   });
   FC_ASSERT( wit.id == GRAPHENE_NULL_VALIDATOR );
   remove(wit);

   // Create initial validators
   std::for_each( genesis_state.initial_validator_candidates.begin(),
                  genesis_state.initial_validator_candidates.end(),
                  [this,&get_account_id,&genesis_eval_state](const auto& validator) {
      validator_create_operation op;
      op.validator_account = get_account_id(validator.owner_name);
      op.block_signing_key = validator.block_signing_key;
      this->apply_operation(genesis_eval_state, op);
   });

   // Create initial delegates
   std::for_each( genesis_state.initial_council_candidates.begin(),
                  genesis_state.initial_council_candidates.end(),
                  [this,&get_account_id,&genesis_eval_state](const auto& member) {
      delegate_create_operation op;
      op.delegate_account = get_account_id(member.owner_name);
      this->apply_operation(genesis_eval_state, op);
   });

   // Create initial workers
   std::for_each( genesis_state.initial_worker_candidates.begin(),
                  genesis_state.initial_worker_candidates.end(),
                  [this,&get_account_id,&genesis_state,&genesis_eval_state](const auto& worker)
   {
       worker_create_operation op;
       op.owner = get_account_id(worker.owner_name);
       op.work_begin_date = genesis_state.initial_timestamp;
       op.work_end_date = time_point_sec::maximum();
       op.daily_pay = worker.daily_pay;
       op.name = "Genesis-Worker-" + worker.owner_name;
       op.initializer = vesting_balance_worker_initializer{uint16_t(0)};

       this->apply_operation(genesis_eval_state, std::move(op));
   });

   // Set block producers
   modify(get_global_properties(), [&genesis_state](global_property_object& p) {
      for( uint32_t i = 1; i <= genesis_state.initial_block_producers; ++i )
      {
         p.block_producers.insert(validator_id_type(i));
      }
   });

   // Enable fees
   modify(get_global_properties(), [&genesis_state](global_property_object& p) {
      p.parameters.get_mutable_fees() = genesis_state.initial_parameters.get_current_fees();
   });

   // Create validator scheduler
   _p_producer_schedule_obj = & create<producer_schedule_object>([this]( producer_schedule_object& wso )
   {
      for( const validator_id_type& wid : get_global_properties().block_producers )
         wso.current_shuffled_producers.push_back( wid );
   });

   // Create FBA counters
   create<fba_accumulator_object>([]( fba_accumulator_object& acc )
   {
      FC_ASSERT( acc.id == fba_accumulator_id_type( fba_accumulator_id_transfer_to_blind ) );
      acc.accumulated_fba_fees = 0;
#ifdef GRAPHENE_FBA_STEALTH_DESIGNATED_ASSET
      acc.designated_asset = GRAPHENE_FBA_STEALTH_DESIGNATED_ASSET;
#endif
   });

   create<fba_accumulator_object>([]( fba_accumulator_object& acc )
   {
      FC_ASSERT( acc.id == fba_accumulator_id_type( fba_accumulator_id_blind_transfer ) );
      acc.accumulated_fba_fees = 0;
#ifdef GRAPHENE_FBA_STEALTH_DESIGNATED_ASSET
      acc.designated_asset = GRAPHENE_FBA_STEALTH_DESIGNATED_ASSET;
#endif
   });

   create<fba_accumulator_object>([]( fba_accumulator_object& acc )
   {
      FC_ASSERT( acc.id == fba_accumulator_id_type( fba_accumulator_id_transfer_from_blind ) );
      acc.accumulated_fba_fees = 0;
#ifdef GRAPHENE_FBA_STEALTH_DESIGNATED_ASSET
      acc.designated_asset = GRAPHENE_FBA_STEALTH_DESIGNATED_ASSET;
#endif
   });

   FC_ASSERT( get_index<fba_accumulator_object>().get_next_id() == fba_accumulator_id_type( fba_accumulator_id_count ) );

   //debug_dump(); // for debug

   _undo_db.enable();
} FC_CAPTURE_AND_RETHROW() } // GCOVR_EXCL_LINE

} }
