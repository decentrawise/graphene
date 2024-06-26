#include <graphene/chain/asset_evaluator.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>

#include <functional>

#include <locale>

namespace graphene { namespace chain {

void_result asset_create_evaluator::do_evaluate( const asset_create_operation& op )
{ try {

   database& d = db();

   const auto& chain_parameters = d.get_global_properties().parameters;
   FC_ASSERT( op.common_options.whitelist_authorities.size() <= chain_parameters.maximum_asset_whitelist_authorities );
   FC_ASSERT( op.common_options.blacklist_authorities.size() <= chain_parameters.maximum_asset_whitelist_authorities );

   // Check that all authorities do exist
   for( auto id : op.common_options.whitelist_authorities )
      d.get(id);
   for( auto id : op.common_options.blacklist_authorities )
      d.get(id);

   auto& asset_indx = d.get_index_type<asset_index>().indices().get<by_symbol>();
   auto asset_symbol_itr = asset_indx.find( op.symbol );
   FC_ASSERT( asset_symbol_itr == asset_indx.end() );

   // Check Sub-Asset issuer
   auto dotpos = op.symbol.rfind( '.' );
   if( dotpos != std::string::npos )
   {
      auto prefix = op.symbol.substr( 0, dotpos );
      auto asset_symbol_itr = asset_indx.find( prefix );
      FC_ASSERT( asset_symbol_itr != asset_indx.end(), "Asset ${s} may only be created by issuer of ${p}, but ${p} has not been registered",
                  ("s",op.symbol)("p",prefix) );
      FC_ASSERT( asset_symbol_itr->issuer == op.issuer, "Asset ${s} may only be created by issuer of ${p}, ${i}",
                  ("s",op.symbol)("p",prefix)("i", op.issuer(d).name) );
   }

   if( op.backed_options )
   {
      const asset_object& backing = op.backed_options->short_backing_asset(d);
      if( backing.is_backed() )
      {
         const backed_asset_data_object& backing_basset_data = backing.backed_asset_data(d);
         const asset_object& backing_backing = backing_basset_data.options.short_backing_asset(d);
         FC_ASSERT( !backing_backing.is_backed(),
                    "May not create an asset backed by an asset backed by an asset." );
         FC_ASSERT( op.issuer != GRAPHENE_COUNCIL_ACCOUNT || backing_backing.get_id() == asset_id_type(),
                    "May not create a blockchain-controlled backed asset which is not backed by CORE.");
      } else
         FC_ASSERT( op.issuer != GRAPHENE_COUNCIL_ACCOUNT || backing.get_id() == asset_id_type(),
                    "May not create a blockchain-controlled backed asset which is not backed by CORE.");
      FC_ASSERT( op.backed_options->feed_lifetime_sec > chain_parameters.block_interval &&
                 op.backed_options->force_settlement_delay_sec > chain_parameters.block_interval );
   }
   if( op.is_prediction_market )
   {
      FC_ASSERT( op.backed_options );
      FC_ASSERT( op.precision == op.backed_options->short_backing_asset(d).precision );
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) } // GCOVR_EXCL_LINE

void asset_create_evaluator::pay_fee()
{
   fee_is_odd = core_fee_paid.value & 1;
   core_fee_paid -= core_fee_paid.value / (int64_t)2;
   generic_evaluator::pay_fee();
}

object_id_type asset_create_evaluator::do_apply( const asset_create_operation& op )
{ try {
   database& d = db();

   const asset_dynamic_data_object& dyn_asset =
      d.create<asset_dynamic_data_object>( [this]( asset_dynamic_data_object& a ) {
         a.current_supply = 0;
         a.fee_pool = core_fee_paid - (fee_is_odd ? 1 : 0);
      });

   backed_asset_data_id_type backed_asset_id;

   auto next_asset_id = d.get_index_type<asset_index>().get_next_id();

   if( op.backed_options.valid() )
      backed_asset_id = d.create<backed_asset_data_object>( [&op,next_asset_id]( backed_asset_data_object& a ) {
            a.options = *op.backed_options;
            a.is_prediction_market = op.is_prediction_market;
            a.asset_id = next_asset_id;
         }).id;

   const asset_object& new_asset =
     d.create<asset_object>( [&op,next_asset_id,&dyn_asset,backed_asset_id,&d]( asset_object& a ) {
         a.issuer = op.issuer;
         a.symbol = op.symbol;
         a.precision = op.precision;
         a.options = op.common_options;
         if( a.options.core_exchange_rate.base.asset_id.instance.value == 0 )
            a.options.core_exchange_rate.quote.asset_id = next_asset_id;
         else
            a.options.core_exchange_rate.base.asset_id = next_asset_id;
         a.dynamic_asset_data_id = dyn_asset.id;
         if( op.backed_options.valid() )
            a.backed_asset_data_id = backed_asset_id;
         a.creation_block_num = d._current_block_num;
         a.creation_time      = d._current_block_time;
      });
   FC_ASSERT( new_asset.id == next_asset_id, "Unexpected object database error, object id mismatch" );

   return new_asset.id;
} FC_CAPTURE_AND_RETHROW( (op) ) } // GCOVR_EXCL_LINE

void_result asset_issue_evaluator::do_evaluate( const asset_issue_operation& o )
{ try {
   const database& d = db();

   const asset_object& a = o.asset_to_issue.asset_id(d);
   FC_ASSERT( o.issuer == a.issuer );
   FC_ASSERT( !a.is_backed(), "Cannot manually issue a backed asset." );

   to_account = &o.issue_to_account(d);
   FC_ASSERT( is_authorized_asset( d, *to_account, a ) );

   asset_dyn_data = &a.dynamic_asset_data_id(d);
   FC_ASSERT( (asset_dyn_data->current_supply + o.asset_to_issue.amount) <= a.options.max_supply );

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) } // GCOVR_EXCL_LINE

void_result asset_issue_evaluator::do_apply( const asset_issue_operation& o )
{ try {
   db().adjust_balance( o.issue_to_account, o.asset_to_issue );

   db().modify( *asset_dyn_data, [&o]( asset_dynamic_data_object& data ){
        data.current_supply += o.asset_to_issue.amount;
   });

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) } // GCOVR_EXCL_LINE

void_result asset_reserve_evaluator::do_evaluate( const asset_reserve_operation& o )
{ try {
   const database& d = db();

   const asset_object& a = o.amount_to_reserve.asset_id(d);
   GRAPHENE_ASSERT(
      !a.is_backed(),
      asset_reserve_invalid_on_mia,
      "Cannot reserve ${sym} because it is a backed asset",
      ("sym", a.symbol)
   );

   from_account = &o.payer(d);
   FC_ASSERT( is_authorized_asset( d, *from_account, a ) );

   asset_dyn_data = &a.dynamic_asset_data_id(d);
   FC_ASSERT( (asset_dyn_data->current_supply - o.amount_to_reserve.amount) >= 0 );

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) } // GCOVR_EXCL_LINE

void_result asset_reserve_evaluator::do_apply( const asset_reserve_operation& o )
{ try {
   db().adjust_balance( o.payer, -o.amount_to_reserve );

   db().modify( *asset_dyn_data, [&o]( asset_dynamic_data_object& data ){
        data.current_supply -= o.amount_to_reserve.amount;
   });

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) } // GCOVR_EXCL_LINE

void_result asset_fund_fee_pool_evaluator::do_evaluate(const asset_fund_fee_pool_operation& o)
{ try {
   database& d = db();

   const asset_object& a = o.asset_id(d);

   asset_dyn_data = &a.dynamic_asset_data_id(d);

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) } // GCOVR_EXCL_LINE

void_result asset_fund_fee_pool_evaluator::do_apply(const asset_fund_fee_pool_operation& o)
{ try {
   db().adjust_balance(o.from_account, -o.amount);

   db().modify( *asset_dyn_data, [&o]( asset_dynamic_data_object& data ) {
      data.fee_pool += o.amount;
   });

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) } // GCOVR_EXCL_LINE

static void validate_new_issuer( const database& d, const asset_object& a, account_id_type new_issuer )
{ try {
   FC_ASSERT(d.find(new_issuer), "New issuer account does not exist");
   if( a.is_backed() && new_issuer == GRAPHENE_COUNCIL_ACCOUNT )
   {
      const asset_object& backing = a.backed_asset_data(d).options.short_backing_asset(d);
      if( backing.is_backed() )
      {
         const asset_object& backing_backing = backing.backed_asset_data(d).options.short_backing_asset(d);
         FC_ASSERT( backing_backing.get_id() == asset_id_type(),
                    "May not create a blockchain-controlled backed asset which is not backed by CORE.");
      } else
         FC_ASSERT( backing.get_id() == asset_id_type(),
                    "May not create a blockchain-controlled backed asset which is not backed by CORE.");
   }
} FC_CAPTURE_AND_RETHROW( (a)(new_issuer) ) } // GCOVR_EXCL_LINE

void_result asset_update_evaluator::do_evaluate(const asset_update_operation& o)
{ try {
   database& d = db();

   const asset_object& a = o.asset_to_update(d);
   auto a_copy = a;
   a_copy.options = o.new_options;
   a_copy.validate();

   if( a.dynamic_asset_data_id(d).current_supply != 0 )
   {
      // new issuer_permissions must be subset of old issuer permissions
      FC_ASSERT(!(o.new_options.issuer_permissions & ~a.options.issuer_permissions),
                "Cannot reinstate previously revoked issuer permissions on an asset.");
   }

   // changed flags must be subset of old issuer permissions
   FC_ASSERT(!((o.new_options.flags ^ a.options.flags) & ~a.options.issuer_permissions),
             "Flag change is forbidden by issuer permissions");

   asset_to_update = &a;
   FC_ASSERT( o.issuer == a.issuer,
              "Incorrect issuer for asset! (${o.issuer} != ${a.issuer})",
              ("o.issuer", o.issuer)("a.issuer", a.issuer) );

   const auto& chain_parameters = d.get_global_properties().parameters;

   FC_ASSERT( o.new_options.whitelist_authorities.size() <= chain_parameters.maximum_asset_whitelist_authorities );
   for( auto id : o.new_options.whitelist_authorities )
      d.get(id);
   FC_ASSERT( o.new_options.blacklist_authorities.size() <= chain_parameters.maximum_asset_whitelist_authorities );
   for( auto id : o.new_options.blacklist_authorities )
      d.get(id);

   return void_result();
} FC_CAPTURE_AND_RETHROW((o)) } // GCOVR_EXCL_LINE

void_result asset_update_evaluator::do_apply(const asset_update_operation& o)
{ try {
   database& d = db();

   // If we are now disabling force settlements, cancel all open force settlement orders
   if( (o.new_options.flags & disable_force_settle) && asset_to_update->can_force_settle() )
   {
      const auto& idx = d.get_index_type<force_settlement_index>().indices().get<by_expiration>();
      // Funky iteration code because we're removing objects as we go. We have to re-initialize itr every loop instead
      // of simply incrementing it.
      for( auto itr = idx.lower_bound(o.asset_to_update);
           itr != idx.end() && itr->settlement_asset_id() == o.asset_to_update;
           itr = idx.lower_bound(o.asset_to_update) )
         d.cancel_settle_order(*itr);
   }

   // For backed assets, if core change rate changed, update flag in backed asset data
   if( asset_to_update->is_backed()
          && asset_to_update->options.core_exchange_rate != o.new_options.core_exchange_rate )
   {
      const auto& ba = asset_to_update->backed_asset_data(d);
      if( !ba.asset_cer_updated )
      {
         d.modify( ba, [](backed_asset_data_object& b)
         {
            b.asset_cer_updated = true;
         });
      }
   }

   d.modify(*asset_to_update, [&o](asset_object& a) {
      a.options = o.new_options;
   });

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) } // GCOVR_EXCL_LINE

void_result asset_update_issuer_evaluator::do_evaluate(const asset_update_issuer_operation& o)
{ try {
   database& d = db();

   const asset_object& a = o.asset_to_update(d);

   validate_new_issuer( d, a, o.new_issuer );

   asset_to_update = &a;
   FC_ASSERT( o.issuer == a.issuer,
              "Incorrect issuer for asset! (${o.issuer} != ${a.issuer})",
              ("o.issuer", o.issuer)("a.issuer", a.issuer) );

   return void_result();
} FC_CAPTURE_AND_RETHROW((o)) } // GCOVR_EXCL_LINE

void_result asset_update_issuer_evaluator::do_apply(const asset_update_issuer_operation& o)
{ try {
   database& d = db();
   d.modify(*asset_to_update, [&](asset_object& a) {
      a.issuer = o.new_issuer;
   });

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) } // GCOVR_EXCL_LINE

/****************
 * Loop through assets, looking for ones that are backed by the asset being changed. When found,
 * perform checks to verify validity
 *
 * @param d the database
 * @param op the backed asset update operation being performed
 * @param new_backing_asset the new asset that backs this backed asset
 */
void check_children_of_backed_asset(database& d, const asset_update_backed_asset_operation& op,
      const asset_object& new_backing_asset)
{
   // no need to do these checks if the new backing asset is CORE
   if ( new_backing_asset.get_id() == asset_id_type() )
      return;

   // loop through all assets that have this asset as a backing asset
   const auto& idx = d.get_index_type<graphene::chain::backed_asset_data_index>()
         .indices()
         .get<by_short_backing_asset>();
   auto backed_range = idx.equal_range(op.asset_to_update);
   std::for_each( backed_range.first, backed_range.second,
         [&new_backing_asset, &d, &op](const backed_asset_data_object& backed_asset_data)
         {
            const auto& child = backed_asset_data.asset_id(d);

            FC_ASSERT( child.get_id() != op.new_options.short_backing_asset,
                  "A Backed Asset would be invalidated by changing this backing asset ('A' backed by 'B' backed by 'A')." );

            FC_ASSERT( child.issuer != GRAPHENE_COUNCIL_ACCOUNT,
                  "A blockchain-controlled backed asset would be invalidated by changing this backing asset." );

            FC_ASSERT( !new_backing_asset.is_backed(),
                  "A non-blockchain controlled Backed Asset would be invalidated by changing this backing asset.");
         } ); // end of lambda and std::for_each()
} // check_children_of_backed_asset

void_result asset_update_backed_asset_evaluator::do_evaluate(const asset_update_backed_asset_operation& op)
{ try {
   database& d = db();

   const asset_object& asset_obj = op.asset_to_update(d);

   FC_ASSERT( asset_obj.is_backed(), "Cannot update Backed Asset specific settings on a non-Backed Asset." );

   FC_ASSERT( op.issuer == asset_obj.issuer, "Only asset issuer can update backed_asset_data of the asset." );

   const backed_asset_data_object& current_backed_asset_data = asset_obj.backed_asset_data(d);

   FC_ASSERT( !current_backed_asset_data.has_settlement(), "Cannot update a backed asset after a global settlement has executed" );

   // Are we changing the backing asset?
   if( op.new_options.short_backing_asset != current_backed_asset_data.options.short_backing_asset )
   {
      FC_ASSERT( asset_obj.dynamic_asset_data_id(d).current_supply == 0,
                 "Cannot update a backed asset if there is already a current supply." );

      const asset_object& new_backing_asset = op.new_options.short_backing_asset(d); // check if the asset exists

      FC_ASSERT( op.new_options.short_backing_asset != asset_obj.get_id(),
                  "Cannot update an asset to be backed by itself." );

      if( current_backed_asset_data.is_prediction_market )
      {
         FC_ASSERT( asset_obj.precision == new_backing_asset.precision,
                     "The precision of the asset and backing asset must be equal." );
      }

      if( asset_obj.issuer == GRAPHENE_COUNCIL_ACCOUNT )
      {
         if( new_backing_asset.is_backed() )
         {
            FC_ASSERT( new_backing_asset.backed_asset_data(d).options.short_backing_asset == asset_id_type(),
                        "May not modify a blockchain-controlled backed asset to be backed by an asset which is not "
                        "backed by CORE." );

            check_children_of_backed_asset( d, op, new_backing_asset );
         }
         else
         {
            FC_ASSERT( new_backing_asset.get_id() == asset_id_type(),
                        "May not modify a blockchain-controlled backed asset to be backed by an asset which is not "
                        "backed asset nor CORE." );
         }
      }
      else
      {
         // not a council issued asset

         // If we're changing to a backing_asset that is not CORE, we need to look at any
         // asset ( "CHILD" ) that has this one as a backing asset. If CHILD is council-owned,
         // the change is not allowed. If CHILD is user-owned, then this asset's backing
         // asset must be either CORE or a UA.
         if ( new_backing_asset.get_id() != asset_id_type() ) // not backed by CORE
         {
            check_children_of_backed_asset( d, op, new_backing_asset );
         }

      }

      // Check if the new backing asset is itself backed by something. It must be CORE or a UA
      if ( new_backing_asset.is_backed() )
      {
         asset_id_type backing_backing_asset_id = new_backing_asset.backed_asset_data(d).options.short_backing_asset;
         FC_ASSERT( (backing_backing_asset_id == asset_id_type() || !backing_backing_asset_id(d).is_backed()),
               "An Asset cannot be backed by an Asset that itself is backed by another Asset.");
      }
   }

   const auto& chain_parameters = d.get_global_properties().parameters;

   FC_ASSERT( op.new_options.feed_lifetime_sec > chain_parameters.block_interval,
         "Feed lifetime must exceed block interval." );
   FC_ASSERT( op.new_options.force_settlement_delay_sec > chain_parameters.block_interval,
         "Force settlement delay must exceed block interval." );

   backed_asset_to_update = &current_backed_asset_data;
   asset_to_update = &asset_obj;

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) } // GCOVR_EXCL_LINE

/*******
 * @brief Apply requested changes to backed asset options
 *
 * This applies the requested changes to the backed asset object. It also cleans up the
 * related feeds
 *
 * @param op the requested operation
 * @param db the database
 * @param bdo the actual database object
 * @param asset_to_update the asset_object related to this backed_asset_data_object
 * @returns true if the feed price is changed
 */
static bool update_backed_asset_object_options(
      const asset_update_backed_asset_operation& op, database& db,
      backed_asset_data_object& bdo, const asset_object& asset_to_update )
{
   const fc::time_point_sec next_maint_time = db.get_dynamic_global_properties().next_maintenance_time;

   // If the minimum number of feeds to calculate a median has changed, we need to recalculate the median
   bool should_update_feeds = false;
   if( op.new_options.minimum_feeds != bdo.options.minimum_feeds )
      should_update_feeds = true;

   // we also should call update_median_feeds if the feed_lifetime_sec changed
   if( op.new_options.feed_lifetime_sec != bdo.options.feed_lifetime_sec )
   {
      should_update_feeds = true;
   }

   // feeds must be reset if the backing asset is changed
   bool backing_asset_changed = false;
   bool is_validator_or_delegate_fed = false;
   if( op.new_options.short_backing_asset != bdo.options.short_backing_asset )
   {
      backing_asset_changed = true;
      should_update_feeds = true;
      if( asset_to_update.options.flags & ( validator_fed_asset | delegate_fed_asset ) )
         is_validator_or_delegate_fed = true;
   }

   bdo.options = op.new_options;

   // are we modifying the underlying? If so, reset the feeds
   if( backing_asset_changed )
   {
      if( is_validator_or_delegate_fed )
      {
         bdo.feeds.clear();
      }
      else
      {
         // for non-validator-feeding and non-delegate-feeding assets, modify all feeds
         // published by producers to nothing, since we can't simply remove them.
         for( auto& current_feed : bdo.feeds )
         {
            current_feed.second.second.settlement_price = price();
         }
      }
   }

   if( should_update_feeds )
   {
      const auto old_feed = bdo.current_feed;
      bdo.update_median_feeds( db.head_block_time(), next_maint_time );

      // We need to call check_call_orders if the price feed changes
      return ( !( old_feed == bdo.current_feed ) );
   }

   return false;
}

void_result asset_update_backed_asset_evaluator::do_apply(const asset_update_backed_asset_operation& op)
{
   try
   {
      auto& db_conn = db();
      const auto& asset_being_updated = (*asset_to_update);
      bool to_check_call_orders = false;

      db_conn.modify( *backed_asset_to_update,
                      [&op, &asset_being_updated, &to_check_call_orders, &db_conn]( backed_asset_data_object& bdo )
      {
         to_check_call_orders = update_backed_asset_object_options( op, db_conn, bdo, asset_being_updated );
      });

      if( to_check_call_orders )
         // Process margin calls, allow black swan, not for a new limit order
         db_conn.check_call_orders( asset_being_updated, true, backed_asset_to_update );

      return void_result();

   } FC_CAPTURE_AND_RETHROW( (op) )
}

void_result asset_update_feed_producers_evaluator::do_evaluate(const asset_update_feed_producers_evaluator::operation_type& o)
{ try {
   database& d = db();

   FC_ASSERT( o.new_feed_producers.size() <= d.get_global_properties().parameters.maximum_asset_feed_publishers,
              "Cannot specify more feed producers than maximum allowed" );

   const asset_object& a = o.asset_to_update(d);

   FC_ASSERT(a.is_backed(), "Cannot update feed producers on a non-Backed Asset.");
   FC_ASSERT(!(a.options.flags & delegate_fed_asset), "Cannot set feed producers on a delegate-fed asset.");
   FC_ASSERT(!(a.options.flags & validator_fed_asset), "Cannot set feed producers on a validator-fed asset.");

   FC_ASSERT( a.issuer == o.issuer, "Only asset issuer can update feed producers of an asset" );

   asset_to_update = &a;

   // Make sure all producers exist. Check these after asset because account lookup is more expensive
   for( auto id : o.new_feed_producers )
      d.get(id);

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) } // GCOVR_EXCL_LINE

void_result asset_update_feed_producers_evaluator::do_apply(const asset_update_feed_producers_evaluator::operation_type& o)
{ try {
   database& d = db();
   const auto head_time = d.head_block_time();
   const auto next_maint_time = d.get_dynamic_global_properties().next_maintenance_time;
   const backed_asset_data_object& backed_asset_to_update = asset_to_update->backed_asset_data(d);
   d.modify( backed_asset_to_update, [&o,head_time,next_maint_time](backed_asset_data_object& a) {
      //This is tricky because I have a set of publishers coming in, but a map of publisher to feed is stored.
      //I need to update the map such that the keys match the new publishers, but not munge the old price feeds from
      //publishers who are being kept.

      // TODO possible performance optimization:
      //      Since both the map and the set are ordered by account already, we can iterate through them only once
      //      and avoid lookups while iterating by maintaining two iterators at same time.
      //      However, this operation is not used much, and both the set and the map are small,
      //      so likely we won't gain much with the optimization.

      //First, remove any old publishers who are no longer publishers
      for( auto itr = a.feeds.begin(); itr != a.feeds.end(); )
      {
         if( !o.new_feed_producers.count(itr->first) )
            itr = a.feeds.erase(itr);
         else
            ++itr;
      }
      //Now, add any new publishers
      for( const account_id_type acc : o.new_feed_producers )
      {
         a.feeds[acc];
      }
      a.update_median_feeds( head_time, next_maint_time );
   });
   // Process margin calls, allow black swan, not for a new limit order
   d.check_call_orders( *asset_to_update, true, &backed_asset_to_update );

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) } // GCOVR_EXCL_LINE

void_result asset_global_settle_evaluator::do_evaluate(const asset_global_settle_evaluator::operation_type& op)
{ try {
   const database& d = db();
   asset_to_settle = &op.asset_to_settle(d);
   FC_ASSERT( asset_to_settle->is_backed(), "Can only globally settle backed assets" );
   FC_ASSERT( asset_to_settle->can_global_settle(), "The global_settle permission of this asset is disabled" );
   FC_ASSERT( asset_to_settle->issuer == op.issuer, "Only asset issuer can globally settle an asset" );
   FC_ASSERT( asset_to_settle->dynamic_data(d).current_supply > 0, "Can not globally settle an asset with zero supply" );

   const backed_asset_data_object& _backed_asset_data  = asset_to_settle->backed_asset_data(d);
   // if there is a settlement for this asset, then no further global settle may be taken
   FC_ASSERT( !_backed_asset_data.has_settlement(), "This asset has settlement, cannot global settle again" );

   const auto& idx = d.get_index_type<call_order_index>().indices().get<by_collateral>();
   FC_ASSERT( !idx.empty(), "Internal error: no debt position found" );
   auto itr = idx.lower_bound( price::min( _backed_asset_data.options.short_backing_asset, op.asset_to_settle ) );
   FC_ASSERT( itr != idx.end() && itr->debt_type() == op.asset_to_settle, "Internal error: no debt position found" );
   const call_order_object& least_collateralized_short = *itr;
   FC_ASSERT(least_collateralized_short.get_debt() * op.settle_price <= least_collateralized_short.get_collateral(),
             "Cannot force settle at supplied price: least collateralized short lacks sufficient collateral to settle.");

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) } // GCOVR_EXCL_LINE

void_result asset_global_settle_evaluator::do_apply(const asset_global_settle_evaluator::operation_type& op)
{ try {
   database& d = db();
   d.globally_settle_asset( *asset_to_settle, op.settle_price );
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) } // GCOVR_EXCL_LINE

void_result asset_settle_evaluator::do_evaluate(const asset_settle_evaluator::operation_type& op)
{ try {
   const database& d = db();
   asset_to_settle = &op.amount.asset_id(d);
   FC_ASSERT(asset_to_settle->is_backed());
   const auto& ba = asset_to_settle->backed_asset_data(d);
   FC_ASSERT(asset_to_settle->can_force_settle() || ba.has_settlement() );
   if( ba.is_prediction_market )
      FC_ASSERT( ba.has_settlement(), "global settlement must occur before force settling a prediction market"  );
   else if( ba.current_feed.settlement_price.is_null() && !ba.has_settlement() )
      FC_THROW_EXCEPTION(insufficient_feeds, "Cannot force settle with no price feed.");
   FC_ASSERT(d.get_balance(d.get(op.account), *asset_to_settle) >= op.amount);

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) } // GCOVR_EXCL_LINE

operation_result asset_settle_evaluator::do_apply(const asset_settle_evaluator::operation_type& op)
{ try {
   database& d = db();

   const auto& ba = asset_to_settle->backed_asset_data(d);
   if( ba.has_settlement() )
   {
      const auto& mia_dyn = asset_to_settle->dynamic_asset_data_id(d);

      auto settled_amount = op.amount * ba.settlement_price; // round down, in favor of global settlement fund
      if( op.amount.amount == mia_dyn.current_supply )
         settled_amount.amount = ba.settlement_fund; // avoid rounding problems
      else
         FC_ASSERT( settled_amount.amount <= ba.settlement_fund ); // should be strictly < except for PM with zero outcome

      if( settled_amount.amount == 0 && !ba.is_prediction_market )
      {
         FC_THROW( "Settle amount is too small to receive anything due to rounding" );
      }

      asset pays = op.amount;
      if( op.amount.amount != mia_dyn.current_supply && settled_amount.amount != 0 )
      {
         pays = settled_amount.multiply_and_round_up( ba.settlement_price );
      }

      d.adjust_balance( op.account, -pays );

      if( settled_amount.amount > 0 )
      {
         d.modify( ba, [&]( backed_asset_data_object& obj ){
            obj.settlement_fund -= settled_amount.amount;
         });

         d.adjust_balance( op.account, settled_amount );
      }

      d.modify( mia_dyn, [&]( asset_dynamic_data_object& obj ){
         obj.current_supply -= pays.amount;
      });

      return settled_amount;
   }
   else
   {
      d.adjust_balance( op.account, -op.amount );
      return d.create<force_settlement_object>([&](force_settlement_object& s) {
         s.owner = op.account;
         s.balance = op.amount;
         s.settlement_date = d.head_block_time() + asset_to_settle->backed_asset_data(d).options.force_settlement_delay_sec;
      }).id;
   }
} FC_CAPTURE_AND_RETHROW( (op) ) } // GCOVR_EXCL_LINE

void_result asset_publish_feeds_evaluator::do_evaluate(const asset_publish_feed_operation& o)
{ try {
   database& d = db();

   const asset_object& base = o.asset_id(d);
   //Verify that this feed is for a backed asset and that asset is backed by the base
   FC_ASSERT( base.is_backed(), "Can only publish price feeds for backed assets" );

   const backed_asset_data_object& ba = base.backed_asset_data(d);
   if( ba.is_prediction_market )
   {
      FC_ASSERT( !ba.has_settlement(), "No further feeds may be published after a settlement event" );
   }

   // the settlement price must be quoted in terms of the backing asset
   FC_ASSERT( o.feed.settlement_price.quote.asset_id == ba.options.short_backing_asset,
              "Quote asset type in settlement price should be same as backing asset of this asset" );

   if( !o.feed.core_exchange_rate.is_null() )
   {
      FC_ASSERT( o.feed.core_exchange_rate.quote.asset_id == asset_id_type(),
                  "Quote asset in core exchange rate should be CORE asset" );
   }

   //Verify that the publisher is authoritative to publish a feed
   if( base.options.flags & validator_fed_asset )
   {
      FC_ASSERT( d.get(GRAPHENE_PRODUCERS_ACCOUNT).active.account_auths.count(o.publisher),
                 "Only block producers are allowed to publish price feeds for this asset" );
   }
   else if( base.options.flags & delegate_fed_asset )
   {
      FC_ASSERT( d.get(GRAPHENE_COUNCIL_ACCOUNT).active.account_auths.count(o.publisher),
                 "Only active delegates are allowed to publish price feeds for this asset" );
   }
   else
   {
      FC_ASSERT( ba.feeds.count(o.publisher),
                 "The account is not in the set of allowed price feed producers of this asset" );
   }

   asset_ptr = &base;
   backed_asset_ptr = &ba;

   return void_result();
} FC_CAPTURE_AND_RETHROW((o)) } // GCOVR_EXCL_LINE

void_result asset_publish_feeds_evaluator::do_apply(const asset_publish_feed_operation& o)
{ try {

   database& d = db();
   const auto head_time = d.head_block_time();
   const auto next_maint_time = d.get_dynamic_global_properties().next_maintenance_time;

   const asset_object& base = *asset_ptr;
   const backed_asset_data_object& bad = *backed_asset_ptr;

   auto old_feed =  bad.current_feed;
   // Store medians for this asset
   d.modify( bad , [&o,head_time,next_maint_time](backed_asset_data_object& a) {
      a.feeds[o.publisher] = make_pair( head_time, o.feed );
      a.update_median_feeds( head_time, next_maint_time );
   });

   if( !(old_feed == bad.current_feed) )
   {
      // Check whether need to revive the asset and proceed if need
      if( bad.has_settlement() // has globally settled
          && !bad.current_feed.settlement_price.is_null() ) // has a valid feed
      {
         bool should_revive = false;
         const auto& mia_dyn = base.dynamic_asset_data_id(d);
         if( mia_dyn.current_supply == 0 ) // if current supply is zero, revive the asset
            should_revive = true;
         else // if current supply is not zero, when collateral ratio of settlement fund is greater than MCR, revive the asset
         {
            // calculate collateralization and compare to maintenance_collateralization
            if( price( asset( bad.settlement_fund, bad.options.short_backing_asset ),
                        asset( mia_dyn.current_supply, o.asset_id ) ) > bad.current_maintenance_collateralization )
               should_revive = true;
         }
         if( should_revive )
            d.revive_backed_asset(base);
      }
      // Process margin calls, allow black swan, not for a new limit order
      d.check_call_orders( base, true, backed_asset_ptr );
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW((o)) } // GCOVR_EXCL_LINE

void_result asset_claim_fees_evaluator::do_evaluate( const asset_claim_fees_operation& o )
{ try {
   FC_ASSERT( o.amount_to_claim.asset_id(db()).issuer == o.issuer, "Asset fees may only be claimed by the issuer" );
   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) } // GCOVR_EXCL_LINE

void_result asset_claim_fees_evaluator::do_apply( const asset_claim_fees_operation& o )
{ try {
   database& d = db();

   const asset_object& a = o.amount_to_claim.asset_id(d);
   const asset_dynamic_data_object& addo = a.dynamic_asset_data_id(d);
   FC_ASSERT( o.amount_to_claim.amount <= addo.accumulated_fees, "Attempt to claim more fees than have accumulated", ("addo",addo) );

   d.modify( addo, [&]( asset_dynamic_data_object& _addo  ) {
     _addo.accumulated_fees -= o.amount_to_claim.amount;
   });

   d.adjust_balance( o.issuer, o.amount_to_claim );

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) } // GCOVR_EXCL_LINE

void_result asset_claim_pool_evaluator::do_evaluate( const asset_claim_pool_operation& o )
{ try {
    FC_ASSERT( o.asset_id(db()).issuer == o.issuer, "Asset fee pool may only be claimed by the issuer" );

    return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) } // GCOVR_EXCL_LINE

void_result asset_claim_pool_evaluator::do_apply( const asset_claim_pool_operation& o )
{ try {
    database& d = db();

    const asset_object& a = o.asset_id(d);
    const asset_dynamic_data_object& addo = a.dynamic_asset_data_id(d);
    FC_ASSERT( o.amount_to_claim.amount <= addo.fee_pool, "Attempt to claim more fees than is available", ("addo",addo) );

    d.modify( addo, [&o]( asset_dynamic_data_object& _addo  ) {
        _addo.fee_pool -= o.amount_to_claim.amount;
    });

    d.adjust_balance( o.issuer, o.amount_to_claim );

    return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) } // GCOVR_EXCL_LINE

} } // graphene::chain
