#include <boost/test/unit_test.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/market_object.hpp>

#include <graphene/app/database_api.hpp>

#include <fc/crypto/digest.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

namespace graphene { namespace chain {

struct swan_fixture : database_fixture {
    limit_order_id_type init_standard_swan(share_type amount = 1000) {
        standard_users();
        standard_asset();
        return trigger_swan(amount, amount);
    }

    void standard_users() {
        set_expiration( db, trx );
        ACTORS((borrower)(borrower2)(feedproducer));
        _borrower = borrower_id;
        _borrower2 = borrower2_id;
        _feedproducer = feedproducer_id;

        transfer(council_account, borrower_id, asset(init_balance));
        transfer(council_account, borrower2_id, asset(init_balance));
    }

    void standard_asset() {
        set_expiration( db, trx );
        const auto& bitusd = create_backed_asset("USDBIT", _feedproducer);
        _swan = bitusd.id;
        _back = asset_id_type();
        update_feed_producers(swan(), {_feedproducer});
    }

    limit_order_id_type trigger_swan(share_type amount1, share_type amount2) {
        set_expiration( db, trx );
        // starting out with price 1:1
        set_feed( 1, 1 );
        // start out with 2:1 collateral
        borrow(borrower(), swan().amount(amount1), back().amount(2*amount1));
        borrow(borrower2(), swan().amount(amount2), back().amount(4*amount2));

        FC_ASSERT( get_balance(borrower(),  swan()) == amount1 );
        FC_ASSERT( get_balance(borrower2(), swan()) == amount2 );
        FC_ASSERT( get_balance(borrower() , back()) == init_balance - 2*amount1 );
        FC_ASSERT( get_balance(borrower2(), back()) == init_balance - 4*amount2 );

        set_feed( 1, 2 );
        // this sell order is designed to trigger a black swan
        limit_order_id_type oid = create_sell_order( borrower2(), swan().amount(1), back().amount(3) )->get_id();

        FC_ASSERT( get_balance(borrower(),  swan()) == amount1 );
        FC_ASSERT( get_balance(borrower2(), swan()) == amount2 - 1 );
        FC_ASSERT( get_balance(borrower() , back()) == init_balance - 2*amount1 );
        FC_ASSERT( get_balance(borrower2(), back()) == init_balance - 2*amount2 );

        BOOST_CHECK( swan().backed_asset_data(db).has_settlement() );

        return oid;
    }

    void set_feed(share_type usd, share_type core) {
        price_feed feed;
        feed.maintenance_collateral_ratio = 1750; // need to set this explicitly, testnet has a different default
        feed.settlement_price = swan().amount(usd) / back().amount(core);
        publish_feed(swan(), feedproducer(), feed);
    }

    void expire_feed() {
      generate_blocks(db.head_block_time() + GRAPHENE_DEFAULT_PRICE_FEED_LIFETIME);
      generate_block();
      FC_ASSERT( swan().backed_asset_data(db).current_feed.settlement_price.is_null() );
    }

    void wait_for_maintenance() {
      generate_blocks( db.get_dynamic_global_properties().next_maintenance_time );
      generate_block();
    }

    const account_object& borrower() { return _borrower(db); }
    const account_object& borrower2() { return _borrower2(db); }
    const account_object& feedproducer() { return _feedproducer(db); }
    const asset_object& swan() { return _swan(db); }
    const asset_object& back() { return _back(db); }

    int64_t init_balance = 1000000;
    account_id_type _borrower, _borrower2, _feedproducer;
    asset_id_type _swan, _back;
};

}}

BOOST_FIXTURE_TEST_SUITE( swan_tests, swan_fixture )

/**
 *  This test sets up the minimum condition for a black swan to occur but does
 *  not test the full range of cases that may be possible during a black swan.
 */
BOOST_AUTO_TEST_CASE( black_swan )
{ try {
      init_standard_swan();

      force_settle( borrower(), swan().amount(100) );

      expire_feed();

      force_settle( borrower(), swan().amount(100) );

      set_feed( 100, 150 );

      BOOST_TEST_MESSAGE( "Verify that we cannot borrow after black swan" );
      GRAPHENE_REQUIRE_THROW( borrow(borrower(), swan().amount(1000), back().amount(2000)), fc::exception )
      trx.operations.clear();

      generate_block();

} catch( const fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

/**
 * Black swan occurs when price feed falls, triggered by settlement
 * order.
 */
BOOST_AUTO_TEST_CASE( black_swan_by_settlement )
{ try {

      ACTORS((buyer)(seller)(borrower)(borrower2)(settler)(feeder));

      const asset_object& core = asset_id_type()(db);

      int trial = 0;

      vector< const account_object* > actors{ &buyer, &seller, &borrower, &borrower2, &settler, &feeder };

      auto top_up = [&]()
      {
         for( const account_object* actor : actors )
         {
            int64_t bal = get_balance( *actor, core );
            if( bal < init_balance )
               transfer( council_account, actor->get_id(), asset(init_balance - bal) );
            else if( bal > init_balance )
               transfer( actor->get_id(), council_account, asset(bal - init_balance) );
         }
      };

      auto setup_asset = [&]() -> const asset_object&
      {
         const asset_object& bitusd = create_backed_asset("USDBIT"+fc::to_string(trial)+"X", feeder_id);
         update_feed_producers( bitusd, {feeder.get_id()} );
         BOOST_CHECK( !bitusd.backed_asset_data(db).has_settlement() );
         trial++;
         return bitusd;
      };

      /*
       * GRAPHENE_COLLATERAL_RATIO_DENOM
      uint16_t maintenance_collateral_ratio = GRAPHENE_DEFAULT_MAINTENANCE_COLLATERAL_RATIO;
      uint16_t maximum_short_squeeze_ratio = GRAPHENE_DEFAULT_MAX_SHORT_SQUEEZE_RATIO;
      */

      // situations to test:
      // 1. minus short squeeze protection would be black swan, otherwise no
      // 2. price feed drops followed by force settle, drop should trigger BS
      // 3. feed price < D/C of least collateralized short < call price < highest bid

      auto set_price = [&](
         const asset_object& bitusd,
         const price& settlement_price
         )
      {
         price_feed feed;
         feed.settlement_price = settlement_price;
         feed.core_exchange_rate = settlement_price;
         wdump( (feed.max_short_squeeze_price()) );
         publish_feed( bitusd, feeder, feed );
      };

      auto wait_for_settlement = [&]()
      {
         const auto& idx = db.get_index_type<force_settlement_index>().indices().get<by_expiration>();
         const auto& itr = idx.rbegin();
         if( itr == idx.rend() )
            return;
         generate_blocks( itr->settlement_date );
         BOOST_CHECK( !idx.empty() );
         generate_block();
         BOOST_CHECK( idx.empty() );
      };

      {
         const asset_object& bitusd = setup_asset();
         top_up();
         set_price( bitusd, bitusd.amount(1) / core.amount(5) );  // $0.20
         borrow(borrower, bitusd.amount(100), asset(1000));       // 2x collat
         transfer( borrower, settler, bitusd.amount(100) );

         // drop to $0.02 and settle
         BOOST_CHECK( !bitusd.backed_asset_data(db).has_settlement() );
         set_price( bitusd, bitusd.amount(1) / core.amount(50) ); // $0.02
         BOOST_CHECK( bitusd.backed_asset_data(db).has_settlement() );
         GRAPHENE_REQUIRE_THROW( borrow( borrower2, bitusd.amount(100), asset(10000) ), fc::exception );
         force_settle( settler, bitusd.amount(100) );

         // wait for forced settlement to execute
         wait_for_settlement();
      }

   } catch( const fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

/** Creates a black swan, recover price feed - asset should be revived
 */
BOOST_AUTO_TEST_CASE( revive_recovered )
{ try {
      init_standard_swan( 700 );

      // revive after price recovers
      set_feed( 700, 800 );
      BOOST_CHECK( swan().backed_asset_data(db).has_settlement() );
      set_feed( 701, 800 );
      BOOST_CHECK( !swan().backed_asset_data(db).has_settlement() );

      graphene::app::database_api db_api( db, &( app.get_options() ));
      auto swan_symbol = _swan(db).symbol;
      vector<call_order_object> calls = db_api.get_call_orders(swan_symbol, 100);
      BOOST_REQUIRE_EQUAL( 1u, calls.size() );
      BOOST_CHECK( calls[0].borrower == swan().issuer );
      BOOST_CHECK_EQUAL( calls[0].debt.value, 1400 );
      BOOST_CHECK_EQUAL( calls[0].collateral.value, 2800 );

      generate_block();

} catch( const fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

/** Creates a black swan, recover price feed - asset should be revived
 */
BOOST_AUTO_TEST_CASE( recollateralize )
{ try {
      init_standard_swan( 700 );

      expire_feed();

      int64_t b2_balance = get_balance( borrower2(), back() );
      bid_collateral( borrower2(), back().amount(1000), swan().amount(100) );
      BOOST_CHECK_EQUAL( get_balance( borrower2(), back() ), b2_balance - 1000 );
      bid_collateral( borrower2(), back().amount(2000), swan().amount(200) );
      BOOST_CHECK_EQUAL( get_balance( borrower2(), back() ), b2_balance - 2000 );
      bid_collateral( borrower2(), back().amount(1000), swan().amount(0) );
      BOOST_CHECK_EQUAL( get_balance( borrower2(), back() ), b2_balance );

      // can't bid for non-backed assets
      GRAPHENE_REQUIRE_THROW( bid_collateral( borrower2(), swan().amount(100), asset(100) ), fc::exception );
      // can't cancel a non-existant bid
      GRAPHENE_REQUIRE_THROW( bid_collateral( borrower2(), back().amount(0), swan().amount(0) ), fc::exception );
      // can't bid zero collateral
      GRAPHENE_REQUIRE_THROW( bid_collateral( borrower2(), back().amount(0), swan().amount(100) ), fc::exception );
      // can't bid more than we have
      GRAPHENE_REQUIRE_THROW( bid_collateral( borrower2(), back().amount(b2_balance + 100), swan().amount(100) ), fc::exception );
      trx.operations.clear();

      // can't bid on a live backed asset
      const asset_object& bitcny = create_backed_asset("CNYBIT", _feedproducer);
      GRAPHENE_REQUIRE_THROW( bid_collateral( borrower2(), asset(100), bitcny.amount(100) ), fc::exception );
      update_feed_producers(bitcny, {_feedproducer});
      price_feed feed;
      feed.settlement_price = bitcny.amount(1) / asset(1);
      publish_feed( bitcny.get_id(), _feedproducer, feed );
      borrow( borrower2(), bitcny.amount(100), asset(1000) );

      // can't bid wrong collateral type
      GRAPHENE_REQUIRE_THROW( bid_collateral( borrower2(), bitcny.amount(100), swan().amount(100) ), fc::exception );

      BOOST_CHECK( swan().dynamic_data(db).current_supply == 1400 );
      BOOST_CHECK( swan().backed_asset_data(db).settlement_fund == 2800 );
      BOOST_CHECK( swan().backed_asset_data(db).has_settlement() );
      BOOST_CHECK( swan().backed_asset_data(db).current_feed.settlement_price.is_null() );

      // doesn't happen without price feed
      bid_collateral( borrower(),  back().amount(1400), swan().amount(700) );
      bid_collateral( borrower2(), back().amount(1400), swan().amount(700) );
      wait_for_maintenance();
      BOOST_CHECK( swan().backed_asset_data(db).has_settlement() );

      set_feed(1, 2);
      // doesn't happen if cover is insufficient
      bid_collateral( borrower2(), back().amount(1400), swan().amount(600) );
      wait_for_maintenance();
      BOOST_CHECK( swan().backed_asset_data(db).has_settlement() );

      set_feed(1, 2);
      // doesn't happen if some bids have a bad swan price
      bid_collateral( borrower2(), back().amount(1050), swan().amount(700) );
      wait_for_maintenance();
      BOOST_CHECK( swan().backed_asset_data(db).has_settlement() );

      set_feed(1, 2);
      // works
      bid_collateral( borrower(),  back().amount(1051), swan().amount(700) );
      bid_collateral( borrower2(), back().amount(2100), swan().amount(1399) );

      // check get_collateral_bids
      graphene::app::database_api db_api(db, &(app.get_options()));
      GRAPHENE_REQUIRE_THROW( db_api.get_collateral_bids(back().symbol, 100, 0), fc::assert_exception );
      auto swan_symbol = _swan(db).symbol;
      vector<collateral_bid_object> bids = db_api.get_collateral_bids(swan_symbol, 100, 1);
      BOOST_CHECK_EQUAL( 1u, bids.size() );
      FC_ASSERT( _borrower2 == bids[0].bidder );
      bids = db_api.get_collateral_bids(swan_symbol, 1, 0);
      BOOST_CHECK_EQUAL( 1u, bids.size() );
      FC_ASSERT( _borrower == bids[0].bidder );
      bids = db_api.get_collateral_bids(swan_symbol, 100, 0);
      BOOST_CHECK_EQUAL( 2u, bids.size() );
      FC_ASSERT( _borrower == bids[0].bidder );
      FC_ASSERT( _borrower2 == bids[1].bidder );

      BOOST_CHECK( swan().backed_asset_data(db).has_settlement() );
      // revive
      wait_for_maintenance();
      BOOST_CHECK( !swan().backed_asset_data(db).has_settlement() );
      bids = db_api.get_collateral_bids(swan_symbol, 100, 0);
      BOOST_CHECK( bids.empty() );
} catch( const fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

/** Creates a black swan, settles all debts, recovers price feed - asset should be revived
 */
BOOST_AUTO_TEST_CASE( revive_empty_recovered )
{ try {
      limit_order_id_type oid = init_standard_swan( 1000 );

      set_expiration( db, trx );
      cancel_limit_order( oid(db) );
      force_settle( borrower(), swan().amount(1000) );
      force_settle( borrower2(), swan().amount(1000) );
      BOOST_CHECK_EQUAL( 0, swan().dynamic_data(db).current_supply.value );
      BOOST_CHECK_EQUAL( 0, swan().backed_asset_data(db).settlement_fund.value );
      BOOST_CHECK( swan().backed_asset_data(db).has_settlement() );

      // revive after price recovers
      set_feed( 1, 1 );
      BOOST_CHECK( !swan().backed_asset_data(db).has_settlement() );

      auto& call_idx = db.get_index_type<call_order_index>().indices().get<by_account>();
      auto itr = call_idx.find( boost::make_tuple(_feedproducer, _swan) );
      BOOST_CHECK( itr == call_idx.end() );
} catch( const fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

/** Creates a black swan, settles all debts - asset should be revived in next maintenance
 */
BOOST_AUTO_TEST_CASE( revive_empty )
{ try {
      limit_order_id_type oid = init_standard_swan( 1000 );

      cancel_limit_order( oid(db) );
      force_settle( borrower(), swan().amount(1000) );
      force_settle( borrower2(), swan().amount(1000) );
      BOOST_CHECK_EQUAL( 0, swan().dynamic_data(db).current_supply.value );

      BOOST_CHECK( swan().backed_asset_data(db).has_settlement() );

      // revive
      wait_for_maintenance();
      BOOST_CHECK( !swan().backed_asset_data(db).has_settlement() );
} catch( const fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

/** Creates a black swan, settles all debts - asset should be revived in next maintenance
 */
BOOST_AUTO_TEST_CASE( revive_empty_with_bid )
{ try {
      standard_users();
      standard_asset();

      set_feed( 1, 1 );
      borrow(borrower(), swan().amount(1000), back().amount(2000));
      borrow(borrower2(), swan().amount(1000), back().amount(1967));

      set_feed( 1, 2 );
      // this sell order is designed to trigger a black swan
      limit_order_id_type oid = create_sell_order(borrower2(), swan().amount(1), back().amount(3))->get_id();
      BOOST_CHECK( swan().backed_asset_data(db).has_settlement() );

      cancel_limit_order( oid(db) );
      force_settle( borrower(), swan().amount(500) );
      force_settle( borrower(), swan().amount(500) );
      force_settle( borrower2(), swan().amount(667) );
      force_settle( borrower2(), swan().amount(333) );
      BOOST_CHECK_EQUAL( 0, swan().dynamic_data(db).current_supply.value );
      BOOST_CHECK_EQUAL( 0, swan().backed_asset_data(db).settlement_fund.value );

      bid_collateral( borrower(), back().amount(3000), swan().amount(700) );

      BOOST_CHECK( swan().backed_asset_data(db).has_settlement() );

      // revive
      wait_for_maintenance();
      BOOST_CHECK( !swan().backed_asset_data(db).has_settlement() );
      graphene::app::database_api db_api(db, &(app.get_options()));
      auto swan_symbol = _swan(db).symbol;
      vector<collateral_bid_object> bids = db_api.get_collateral_bids(swan_symbol, 100, 0);
      BOOST_CHECK( bids.empty() );

      auto& call_idx = db.get_index_type<call_order_index>().indices().get<by_account>();
      auto itr = call_idx.find( boost::make_tuple(_borrower, _swan) );
      BOOST_CHECK( itr == call_idx.end() );
      itr = call_idx.find( boost::make_tuple(_feedproducer, _swan) );
      BOOST_CHECK( itr == call_idx.end() );
} catch( const fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

/** Creates a black swan, bids on more than outstanding debt
 */
BOOST_AUTO_TEST_CASE( overflow )
{ try {
   init_standard_swan( 700 );

   bid_collateral( borrower(),  back().amount(2200), swan().amount(GRAPHENE_MAX_SHARE_SUPPLY - 1) );
   bid_collateral( borrower2(), back().amount(2100), swan().amount(1399) );
   set_feed(1, 2);
   wait_for_maintenance();

   auto& call_idx = db.get_index_type<call_order_index>().indices().get<by_account>();
   auto itr = call_idx.find( boost::make_tuple(_borrower, _swan) );
   BOOST_REQUIRE( itr != call_idx.end() );
   BOOST_CHECK_EQUAL( 1, itr->debt.value );
   itr = call_idx.find( boost::make_tuple(_borrower2, _swan) );
   BOOST_REQUIRE( itr != call_idx.end() );
   BOOST_CHECK_EQUAL( 1399, itr->debt.value );

   BOOST_CHECK( !swan().backed_asset_data(db).has_settlement() );
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()
