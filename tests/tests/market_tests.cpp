#include <boost/test/unit_test.hpp>

#include <graphene/chain/hardfork.hpp>

#include <graphene/protocol/market.hpp>
#include <graphene/chain/market_object.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE(market_tests, database_fixture)

/***
 * Undercollateralized short positions call test cases
 */
BOOST_AUTO_TEST_CASE(short_positions_called)
{ try {

   auto mi = db.get_global_properties().parameters.maintenance_interval;

   if(hf1270)
      generate_blocks(HARDFORK_CORE_1270_TIME - mi);

   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

   set_expiration( db, trx );

   ACTORS((buyer)(seller)(borrower)(borrower2)(borrower3)(feedproducer));

   const auto& bitusd = create_bitasset("USDBIT", feedproducer_id);
   const auto& core   = asset_id_type()(db);
   asset_id_type usd_id = bitusd.get_id();
   asset_id_type core_id = core.get_id();

   int64_t init_balance(1000000);

   transfer(committee_account, buyer_id, asset(init_balance));
   transfer(committee_account, borrower_id, asset(init_balance));
   transfer(committee_account, borrower2_id, asset(init_balance));
   transfer(committee_account, borrower3_id, asset(init_balance));
   update_feed_producers( bitusd, {feedproducer.get_id()} );

   price_feed current_feed;
   current_feed.maintenance_collateral_ratio = 1750;
   current_feed.maximum_short_squeeze_ratio = 1100;
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(5);
   publish_feed( bitusd, feedproducer, current_feed );
   // start out with 300% collateral, call price is 15/1.75 CORE/USD = 60/7
   const call_order_object& call = *borrow( borrower, bitusd.amount(1000), asset(15000));
   call_order_id_type call_id = call.get_id();
   // create another position with 310% collateral, call price is 15.5/1.75 CORE/USD = 62/7
   const call_order_object& call2 = *borrow( borrower2, bitusd.amount(1000), asset(15500));
   call_order_id_type call2_id = call2.get_id();
   // create yet another position with 320% collateral, call price is 16/1.75 CORE/USD = 64/7
   const call_order_object& call3 = *borrow( borrower3, bitusd.amount(1000), asset(16000));
   call_order_id_type call3_id = call3.get_id();
   transfer(borrower, seller, bitusd.amount(1000));
   transfer(borrower2, seller, bitusd.amount(1000));
   transfer(borrower3, seller, bitusd.amount(1000));

   BOOST_CHECK_EQUAL( 1000, call.debt.value );
   BOOST_CHECK_EQUAL( 15000, call.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call2.debt.value );
   BOOST_CHECK_EQUAL( 15500, call2.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 16000, call3.collateral.value );
   BOOST_CHECK_EQUAL( 3000, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(seller, core) );

   // adjust price feed to get call_order into margin call territory
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(10);
   publish_feed( bitusd, feedproducer, current_feed );
   // settlement price = 1/10, mssp = 1/11

   // This sell order above MSSP will not be matched with a call
   BOOST_CHECK( create_sell_order(seller, bitusd.amount(7), core.amount(78)) != nullptr );

   BOOST_CHECK_EQUAL( 2993, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(seller, core) );

   // This buy order is too low will not be matched with a sell order
   limit_order_id_type buy_low = create_sell_order(buyer, asset(90), bitusd.amount(10))->get_id();
   // This buy order at MSSP will be matched only if no margin call (margin call takes precedence)
   limit_order_id_type buy_med = create_sell_order(buyer, asset(110), bitusd.amount(10))->get_id();
   // This buy order above MSSP will be matched with a sell order (limit order with better price takes precedence)
   limit_order_id_type buy_high = create_sell_order(buyer, asset(111), bitusd.amount(10))->get_id();

   BOOST_CHECK_EQUAL( 0, get_balance(buyer, bitusd) );
   BOOST_CHECK_EQUAL( init_balance - 90 - 110 - 111, get_balance(buyer, core) );

   // This order slightly below the call price will be matched
   BOOST_CHECK( !create_sell_order(seller, bitusd.amount(700), core.amount(5900) ) );

   // firstly it will match with buy_high, at buy_high's price
   BOOST_CHECK( !db.find( buy_high ) );
   BOOST_CHECK_EQUAL( db.find( buy_med )->for_sale.value, 110 );
   BOOST_CHECK_EQUAL( db.find( buy_low )->for_sale.value, 90 );

   // buy_high pays 111 CORE, receives 10 USD goes to buyer's balance
   BOOST_CHECK_EQUAL( 10, get_balance(buyer, bitusd) );
   BOOST_CHECK_EQUAL( init_balance - 90 - 110 - 111, get_balance(buyer, core) );
   // sell order pays 10 USD, receives 111 CORE, remaining 690 USD for sale, still at price 7/59

   // then it will match with call, at mssp: 1/11 = 690/7590
   BOOST_CHECK_EQUAL( 2293, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 7701, get_balance(seller, core) );
   BOOST_CHECK_EQUAL( 310, call.debt.value );
   BOOST_CHECK_EQUAL( 7410, call.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call2.debt.value );
   BOOST_CHECK_EQUAL( 15500, call2.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 16000, call3.collateral.value );

   // call's call_price will be updated after the match, to 741/31/1.75 CORE/USD = 2964/217
   // it's above settlement price (10/1) so won't be margin called again
   if(!hf1270) // can use call price only if we are before hf1270
      BOOST_CHECK( price(asset(2964),asset(217,usd_id)) == call.call_price );

   // This would match with call2
   BOOST_CHECK( !create_sell_order(seller, bitusd.amount(700), core.amount(6000) ) );
   BOOST_CHECK_EQUAL( db.find( buy_med )->for_sale.value, 110 );
   BOOST_CHECK_EQUAL( db.find( buy_low )->for_sale.value, 90 );

   // fill price would be mssp: 1/11 = 700/7700
   BOOST_CHECK_EQUAL( 1593, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 15401, get_balance(seller, core) );
   BOOST_CHECK_EQUAL( 310, call.debt.value );
   BOOST_CHECK_EQUAL( 7410, call.collateral.value );
   BOOST_CHECK_EQUAL( 300, call2.debt.value );
   BOOST_CHECK_EQUAL( 7800, call2.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 16000, call3.collateral.value );
   // call2's call_price will be updated after the match, to 78/3/1.75 CORE/USD = 312/21
   if(!hf1270) // can use call price only if we are before hf1270
      BOOST_CHECK( price(asset(312),asset(21,usd_id)) == call2.call_price );
   // it's above settlement price (10/1) so won't be margin called

   // at this moment, collateralization of call is 7410 / 310 = 23.9
   // collateralization of call2 is 7800 / 300 = 26
   // collateralization of call3 is 16000 / 1000 = 16

   // force settle
   force_settle( seller, bitusd.amount(10) );

   BOOST_CHECK_EQUAL( 1583, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 15401, get_balance(seller, core) );
   BOOST_CHECK_EQUAL( 310, call.debt.value );
   BOOST_CHECK_EQUAL( 7410, call.collateral.value );
   BOOST_CHECK_EQUAL( 300, call2.debt.value );
   BOOST_CHECK_EQUAL( 7800, call2.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 16000, call3.collateral.value );

   // generate blocks to let the settle order execute (price feed will expire after it)
   generate_block();
   generate_blocks( db.head_block_time() + fc::hours(24) );

   // call3 get settled, at settlement price 1/10
   BOOST_CHECK_EQUAL( 1583, get_balance(seller_id, usd_id) );
   BOOST_CHECK_EQUAL( 15501, get_balance(seller_id, core_id) );
   BOOST_CHECK_EQUAL( 310, call_id(db).debt.value );
   BOOST_CHECK_EQUAL( 7410, call_id(db).collateral.value );
   BOOST_CHECK_EQUAL( 300, call2_id(db).debt.value );
   BOOST_CHECK_EQUAL( 7800, call2_id(db).collateral.value );
   BOOST_CHECK_EQUAL( 990, call3_id(db).debt.value );
   BOOST_CHECK_EQUAL( 15900, call3_id(db).collateral.value );

   set_expiration( db, trx );
   update_feed_producers( usd_id(db), {feedproducer_id} );

   // at this moment, collateralization of call is 7410 / 310 = 23.9
   // collateralization of call2 is 7800 / 300 = 26
   // collateralization of call3 is 15900 / 990 = 16.06

   // adjust price feed to get call3 into black swan territory, but not the other call orders
   // Note: black swan should occur when callateralization < mssp, but not at < feed
   current_feed.settlement_price = asset(1, usd_id) / asset(16, core_id);
   publish_feed( usd_id(db), feedproducer_id(db), current_feed );
   // settlement price = 1/16, mssp = 10/176

   // black swan event will occur: #649 fixed
   BOOST_CHECK( usd_id(db).bitasset_data(db).has_settlement() );
   // short positions will be closed
   BOOST_CHECK( !db.find( call_id ) );
   BOOST_CHECK( !db.find( call2_id ) );
   BOOST_CHECK( !db.find( call3_id ) );

   // generate a block
   generate_block();


} FC_LOG_AND_RETHROW() }

/***
 * Multiple limit order filling issue
 */
BOOST_AUTO_TEST_CASE(multiple_limit_order_filling)
{ try {

   auto mi = db.get_global_properties().parameters.maintenance_interval;

   if(hf1270)
      generate_blocks(HARDFORK_CORE_1270_TIME - mi);

   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

   set_expiration( db, trx );

   ACTORS((buyer)(seller)(borrower)(borrower2)(borrower3)(feedproducer));

   const auto& bitusd = create_bitasset("USDBIT", feedproducer_id);
   const auto& core   = asset_id_type()(db);
   asset_id_type usd_id = bitusd.get_id();

   int64_t init_balance(1000000);

   transfer(committee_account, buyer_id, asset(init_balance));
   transfer(committee_account, borrower_id, asset(init_balance));
   transfer(committee_account, borrower2_id, asset(init_balance));
   transfer(committee_account, borrower3_id, asset(init_balance));
   update_feed_producers( bitusd, {feedproducer.get_id()} );

   price_feed current_feed;
   current_feed.maintenance_collateral_ratio = 1750;
   current_feed.maximum_short_squeeze_ratio = 1100;
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(5);
   publish_feed( bitusd, feedproducer, current_feed );
   // start out with 300% collateral, call price is 15/1.75 CORE/USD = 60/7
   const call_order_object& call = *borrow( borrower, bitusd.amount(1000), asset(15000));
   call_order_id_type call_id = call.get_id();
   // create another position with 310% collateral, call price is 15.5/1.75 CORE/USD = 62/7
   const call_order_object& call2 = *borrow( borrower2, bitusd.amount(1000), asset(15500));
   call_order_id_type call2_id = call2.get_id();
   // create yet another position with 320% collateral, call price is 16/1.75 CORE/USD = 64/7
   const call_order_object& call3 = *borrow( borrower3, bitusd.amount(1000), asset(16000));
   call_order_id_type call3_id = call3.get_id();
   transfer(borrower, seller, bitusd.amount(1000));
   transfer(borrower2, seller, bitusd.amount(1000));
   transfer(borrower3, seller, bitusd.amount(1000));

   BOOST_CHECK_EQUAL( 1000, call.debt.value );
   BOOST_CHECK_EQUAL( 15000, call.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call2.debt.value );
   BOOST_CHECK_EQUAL( 15500, call2.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 16000, call3.collateral.value );
   BOOST_CHECK_EQUAL( 3000, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(seller, core) );

   // no margin call so far

   // This order would match call when it's margin called, it has an amount same as call's debt which will be full filled later
   limit_order_id_type sell_med = create_sell_order(seller_id(db), asset(1000, usd_id), asset(10000))->get_id(); // 1/10
   // Another big order above sell_med, amount bigger than call2's debt
   limit_order_id_type sell_med2 = create_sell_order(seller_id(db), asset(1200, usd_id), asset(12120))->get_id(); // 1/10.1
   // Another small order above sell_med2
   limit_order_id_type sell_med3 = create_sell_order(seller_id(db), asset(120, usd_id), asset(1224))->get_id(); // 1/10.2

   // adjust price feed to get the call orders  into margin call territory
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(10);
   publish_feed( bitusd, feedproducer, current_feed );
   // settlement price = 1/10, mssp = 1/11

   // Multiple order matching check
   BOOST_CHECK( !db.find( sell_med ) ); // sell_med get filled
   BOOST_CHECK( !db.find( sell_med2 ) ); // sell_med2 get filled
   BOOST_CHECK( !db.find( sell_med3 ) ); // sell_med3 get filled
   BOOST_CHECK( !db.find( call_id ) ); // the first call order get filled
   BOOST_CHECK( !db.find( call2_id ) ); // the second call order get filled
   BOOST_CHECK( db.find( call3_id ) ); // the third call order is still there

   // generate a block
   generate_block();

} FC_LOG_AND_RETHROW() }

/***
 * Tests (big) limit order matching logic
 */
BOOST_AUTO_TEST_CASE(big_limit_order_test)
{ try {

   auto mi = db.get_global_properties().parameters.maintenance_interval;

   if(hf1270)
      generate_blocks(HARDFORK_CORE_1270_TIME - mi);

   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

   set_expiration( db, trx );

   ACTORS((buyer)(buyer2)(buyer3)(seller)(borrower)(borrower2)(borrower3)(feedproducer));

   const auto& bitusd = create_bitasset("USDBIT", feedproducer_id);
   const auto& core   = asset_id_type()(db);

   int64_t init_balance(1000000);

   transfer(committee_account, buyer_id, asset(init_balance));
   transfer(committee_account, buyer2_id, asset(init_balance));
   transfer(committee_account, buyer3_id, asset(init_balance));
   transfer(committee_account, borrower_id, asset(init_balance));
   transfer(committee_account, borrower2_id, asset(init_balance));
   transfer(committee_account, borrower3_id, asset(init_balance));
   update_feed_producers( bitusd, {feedproducer.get_id()} );

   price_feed current_feed;
   current_feed.maintenance_collateral_ratio = 1750;
   current_feed.maximum_short_squeeze_ratio = 1100;
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(5);
   publish_feed( bitusd, feedproducer, current_feed );
   // start out with 300% collateral, call price is 15/1.75 CORE/USD = 60/7
   const call_order_object& call = *borrow( borrower, bitusd.amount(1000), asset(15000));
   call_order_id_type call_id = call.get_id();
   // create another position with 310% collateral, call price is 15.5/1.75 CORE/USD = 62/7
   const call_order_object& call2 = *borrow( borrower2, bitusd.amount(1000), asset(15500));
   call_order_id_type call2_id = call2.get_id();
   // create yet another position with 500% collateral, call price is 25/1.75 CORE/USD = 100/7
   const call_order_object& call3 = *borrow( borrower3, bitusd.amount(1000), asset(25000));
   transfer(borrower, seller, bitusd.amount(1000));
   transfer(borrower2, seller, bitusd.amount(1000));
   transfer(borrower3, seller, bitusd.amount(1000));

   BOOST_CHECK_EQUAL( 1000, call.debt.value );
   BOOST_CHECK_EQUAL( 15000, call.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call2.debt.value );
   BOOST_CHECK_EQUAL( 15500, call2.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 25000, call3.collateral.value );
   BOOST_CHECK_EQUAL( 3000, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(seller, core) );
   BOOST_CHECK_EQUAL( 3000, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( init_balance - 15000, get_balance(borrower, core) );
   BOOST_CHECK_EQUAL( init_balance - 15500, get_balance(borrower2, core) );
   BOOST_CHECK_EQUAL( init_balance - 25000, get_balance(borrower3, core) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower2, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower3, bitusd) );

   // adjust price feed to get call and call2 (but not call3) into margin call territory
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(10);
   publish_feed( bitusd, feedproducer, current_feed );
   // settlement price = 1/10, mssp = 1/11

   // This sell order above MSSP will not be matched with a call
   limit_order_id_type sell_high = create_sell_order(seller, bitusd.amount(7), core.amount(78))->get_id();
   BOOST_CHECK_EQUAL( db.find( sell_high )->for_sale.value, 7 );

   BOOST_CHECK_EQUAL( 2993, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(seller, core) );

   // This buy order is too low will not be matched with a sell order
   limit_order_id_type buy_low = create_sell_order(buyer, asset(80), bitusd.amount(10))->get_id();
   // This buy order at MSSP will be matched only if no margin call (margin call takes precedence)
   limit_order_id_type buy_med = create_sell_order(buyer2, asset(11000), bitusd.amount(1000))->get_id();
   // This buy order above MSSP will be matched with a sell order (limit order with better price takes precedence)
   limit_order_id_type buy_high = create_sell_order(buyer3, asset(111), bitusd.amount(10))->get_id();

   BOOST_CHECK_EQUAL( 0, get_balance(buyer, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(buyer2, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(buyer3, bitusd) );
   BOOST_CHECK_EQUAL( init_balance - 80, get_balance(buyer, core) );
   BOOST_CHECK_EQUAL( init_balance - 11000, get_balance(buyer2, core) );
   BOOST_CHECK_EQUAL( init_balance - 111, get_balance(buyer3, core) );

   // Create a big sell order slightly below the call price, will be matched with several orders
   BOOST_CHECK( !create_sell_order(seller, bitusd.amount(700*4), core.amount(5900*4) ) );

   // firstly it will match with buy_high, at buy_high's price
   BOOST_CHECK( !db.find( buy_high ) );
   // buy_high pays 111 CORE, receives 10 USD goes to buyer3's balance
   BOOST_CHECK_EQUAL( 10, get_balance(buyer3, bitusd) );
   BOOST_CHECK_EQUAL( init_balance - 111, get_balance(buyer3, core) );

   // then it will match with call, at mssp: 1/11 = 1000/11000
   BOOST_CHECK( !db.find( call_id ) );
   // call pays 11000 CORE, receives 1000 USD to cover borrower's position, remaining CORE goes to borrower's balance
   BOOST_CHECK_EQUAL( init_balance - 11000, get_balance(borrower, core) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower, bitusd) );

   // then it will match with call2, at mssp: 1/11 = 1000/11000
   BOOST_CHECK( !db.find( call2_id ) );
   // call2 pays 11000 CORE, receives 1000 USD to cover borrower2's position, remaining CORE goes to borrower2's balance
   BOOST_CHECK_EQUAL( init_balance - 11000, get_balance(borrower2, core) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower2, bitusd) );

   // then it will match with buy_med, at buy_med's price. Since buy_med is too big, it's partially filled.
   // buy_med receives the remaining USD of sell order, minus market fees, goes to buyer2's balance
   BOOST_CHECK_EQUAL( 783, get_balance(buyer2, bitusd) ); // 700*4-10-1000-1000=790, minus 1% market fee 790*100/10000=7
   BOOST_CHECK_EQUAL( init_balance - 11000, get_balance(buyer2, core) );
   // buy_med pays at 1/11 = 790/8690
   BOOST_CHECK_EQUAL( db.find( buy_med )->for_sale.value, 11000-8690 );

   // call3 is not in margin call territory so won't be matched
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 25000, call3.collateral.value );

   // buy_low's price is too low that won't be matched
   BOOST_CHECK_EQUAL( db.find( buy_low )->for_sale.value, 80 );

   // check seller balance
   BOOST_CHECK_EQUAL( 193, get_balance(seller, bitusd) ); // 3000 - 7 - 700*4
   BOOST_CHECK_EQUAL( 30801, get_balance(seller, core) ); // 111 + 11000 + 11000 + 8690

   // Cancel buy_med
   cancel_limit_order( buy_med(db) );
   BOOST_CHECK( !db.find( buy_med ) );
   BOOST_CHECK_EQUAL( 783, get_balance(buyer2, bitusd) );
   BOOST_CHECK_EQUAL( init_balance - 8690, get_balance(buyer2, core) );

   // Create another sell order slightly below the call price, won't fill
   limit_order_id_type sell_med = create_sell_order( seller, bitusd.amount(7), core.amount(59) )->get_id();
   BOOST_CHECK_EQUAL( db.find( sell_med )->for_sale.value, 7 );
   // check seller balance
   BOOST_CHECK_EQUAL( 193-7, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 30801, get_balance(seller, core) );

   // call3 is not in margin call territory so won't be matched
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 25000, call3.collateral.value );

   // buy_low's price is too low that won't be matched
   BOOST_CHECK_EQUAL( db.find( buy_low )->for_sale.value, 80 );

   // generate a block
   generate_block();

} FC_LOG_AND_RETHROW() }

/***
 * BSIP38 "target_collateral_ratio" test: matching a taker limit order with multiple maker call orders
 */
BOOST_AUTO_TEST_CASE(target_cr_test_limit_call)
{ try {

   auto mi = db.get_global_properties().parameters.maintenance_interval;

   if(hf1270)
      generate_blocks(HARDFORK_CORE_1270_TIME - mi);

   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

   set_expiration( db, trx );

   ACTORS((buyer)(buyer2)(buyer3)(seller)(borrower)(borrower2)(borrower3)(feedproducer));

   const auto& bitusd = create_bitasset("USDBIT", feedproducer_id);
   const auto& core   = asset_id_type()(db);

   int64_t init_balance(1000000);

   transfer(committee_account, buyer_id, asset(init_balance));
   transfer(committee_account, buyer2_id, asset(init_balance));
   transfer(committee_account, buyer3_id, asset(init_balance));
   transfer(committee_account, borrower_id, asset(init_balance));
   transfer(committee_account, borrower2_id, asset(init_balance));
   transfer(committee_account, borrower3_id, asset(init_balance));
   update_feed_producers( bitusd, {feedproducer.get_id()} );

   price_feed current_feed;
   current_feed.maintenance_collateral_ratio = 1750;
   current_feed.maximum_short_squeeze_ratio = 1100;
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(5);
   publish_feed( bitusd, feedproducer, current_feed );
   // start out with 300% collateral, call price is 15/1.75 CORE/USD = 60/7, tcr 170% is lower than 175%
   const call_order_object& call = *borrow( borrower, bitusd.amount(1000), asset(15000), 1700);
   call_order_id_type call_id = call.get_id();
   // create another position with 310% collateral, call price is 15.5/1.75 CORE/USD = 62/7, tcr 200% is higher than 175%
   const call_order_object& call2 = *borrow( borrower2, bitusd.amount(1000), asset(15500), 2000);
   call_order_id_type call2_id = call2.get_id();
   // create yet another position with 500% collateral, call price is 25/1.75 CORE/USD = 100/7, no tcr
   const call_order_object& call3 = *borrow( borrower3, bitusd.amount(1000), asset(25000));
   transfer(borrower, seller, bitusd.amount(1000));
   transfer(borrower2, seller, bitusd.amount(1000));
   transfer(borrower3, seller, bitusd.amount(1000));

   BOOST_CHECK_EQUAL( 1000, call.debt.value );
   BOOST_CHECK_EQUAL( 15000, call.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call2.debt.value );
   BOOST_CHECK_EQUAL( 15500, call2.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 25000, call3.collateral.value );
   BOOST_CHECK_EQUAL( 3000, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(seller, core) );
   BOOST_CHECK_EQUAL( 3000, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( init_balance - 15000, get_balance(borrower, core) );
   BOOST_CHECK_EQUAL( init_balance - 15500, get_balance(borrower2, core) );
   BOOST_CHECK_EQUAL( init_balance - 25000, get_balance(borrower3, core) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower2, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower3, bitusd) );

   // adjust price feed to get call and call2 (but not call3) into margin call territory
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(10);
   publish_feed( bitusd, feedproducer, current_feed );
   // settlement price = 1/10, mssp = 1/11

   // This sell order above MSSP will not be matched with a call
   limit_order_id_type sell_high = create_sell_order(seller, bitusd.amount(7), core.amount(78))->get_id();
   BOOST_CHECK_EQUAL( db.find( sell_high )->for_sale.value, 7 );

   BOOST_CHECK_EQUAL( 2993, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(seller, core) );

   // This buy order is too low will not be matched with a sell order
   limit_order_id_type buy_low = create_sell_order(buyer, asset(80), bitusd.amount(10))->get_id();
   // This buy order at MSSP will be matched only if no margin call (margin call takes precedence)
   limit_order_id_type buy_med = create_sell_order(buyer2, asset(33000), bitusd.amount(3000))->get_id();
   // This buy order above MSSP will be matched with a sell order (limit order with better price takes precedence)
   limit_order_id_type buy_high = create_sell_order(buyer3, asset(111), bitusd.amount(10))->get_id();

   BOOST_CHECK_EQUAL( 0, get_balance(buyer, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(buyer2, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(buyer3, bitusd) );
   BOOST_CHECK_EQUAL( init_balance - 80, get_balance(buyer, core) );
   BOOST_CHECK_EQUAL( init_balance - 33000, get_balance(buyer2, core) );
   BOOST_CHECK_EQUAL( init_balance - 111, get_balance(buyer3, core) );

   // call and call2's CR is quite high, and debt amount is quite a lot, assume neither of them will be completely filled
   price match_price( bitusd.amount(1) / core.amount(11) );
   share_type call_to_cover = call_id(db).get_max_debt_to_cover(match_price,current_feed.settlement_price,1750);
   share_type call2_to_cover = call2_id(db).get_max_debt_to_cover(match_price,current_feed.settlement_price,1750);
   BOOST_CHECK_LT( call_to_cover.value, call_id(db).debt.value );
   BOOST_CHECK_LT( call2_to_cover.value, call2_id(db).debt.value );
   // even though call2 has a higher CR, since call's TCR is less than call2's TCR, so we expect call will cover less when called
   BOOST_CHECK_LT( call_to_cover.value, call2_to_cover.value );

   // Create a big sell order slightly below the call price, will be matched with several orders
   BOOST_CHECK( !create_sell_order(seller, bitusd.amount(700*4), core.amount(5900*4) ) );

   // firstly it will match with buy_high, at buy_high's price
   BOOST_CHECK( !db.find( buy_high ) );
   // buy_high pays 111 CORE, receives 10 USD goes to buyer3's balance
   BOOST_CHECK_EQUAL( 10, get_balance(buyer3, bitusd) );
   BOOST_CHECK_EQUAL( init_balance - 111, get_balance(buyer3, core) );

   // then it will match with call, at mssp: 1/11 = 1000/11000
   const call_order_object* tmp_call = db.find( call_id );
   BOOST_CHECK( tmp_call != nullptr );

   // call will receive call_to_cover, pay 11*call_to_cover
   share_type call_to_pay = call_to_cover * 11;
   BOOST_CHECK_EQUAL( 1000 - call_to_cover.value, call.debt.value );
   BOOST_CHECK_EQUAL( 15000 - call_to_pay.value, call.collateral.value );
   // new collateral ratio should be higher than mcr as well as tcr
   BOOST_CHECK( call.debt.value * 10 * 1750 < call.collateral.value * 1000 );
   idump( (call) );
   // borrower's balance doesn't change
   BOOST_CHECK_EQUAL( init_balance - 15000, get_balance(borrower, core) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower, bitusd) );

   // the limit order then will match with call2, at mssp: 1/11 = 1000/11000
   const call_order_object* tmp_call2 = db.find( call2_id );
   BOOST_CHECK( tmp_call2 != nullptr );

   // call2 will receive call2_to_cover, pay 11*call2_to_cover
   share_type call2_to_pay = call2_to_cover * 11;
   BOOST_CHECK_EQUAL( 1000 - call2_to_cover.value, call2.debt.value );
   BOOST_CHECK_EQUAL( 15500 - call2_to_pay.value, call2.collateral.value );
   // new collateral ratio should be higher than mcr as well as tcr
   BOOST_CHECK( call2.debt.value * 10 * 2000 < call2.collateral.value * 1000 );
   idump( (call2) );
   // borrower2's balance doesn't change
   BOOST_CHECK_EQUAL( init_balance - 15500, get_balance(borrower2, core) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower2, bitusd) );

   // then it will match with buy_med, at buy_med's price. Since buy_med is too big, it's partially filled.
   // buy_med receives the remaining USD of sell order, minus market fees, goes to buyer2's balance
   share_type buy_med_get = 700*4 - 10 - call_to_cover - call2_to_cover;
   share_type buy_med_pay = buy_med_get * 11; // buy_med pays at 1/11
   buy_med_get -= (buy_med_get/100); // minus 1% market fee
   BOOST_CHECK_EQUAL( buy_med_get.value, get_balance(buyer2, bitusd) );
   BOOST_CHECK_EQUAL( init_balance - 33000, get_balance(buyer2, core) );
   BOOST_CHECK_EQUAL( db.find( buy_med )->for_sale.value, 33000-buy_med_pay.value );

   // call3 is not in margin call territory so won't be matched
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 25000, call3.collateral.value );

   // buy_low's price is too low that won't be matched
   BOOST_CHECK_EQUAL( db.find( buy_low )->for_sale.value, 80 );

   // check seller balance
   BOOST_CHECK_EQUAL( 193, get_balance(seller, bitusd) ); // 3000 - 7 - 700*4
   BOOST_CHECK_EQUAL( 30801, get_balance(seller, core) ); // 111 + (700*4-10)*11

   // Cancel buy_med
   cancel_limit_order( buy_med(db) );
   BOOST_CHECK( !db.find( buy_med ) );
   BOOST_CHECK_EQUAL( buy_med_get.value, get_balance(buyer2, bitusd) );
   BOOST_CHECK_EQUAL( init_balance - buy_med_pay.value, get_balance(buyer2, core) );

   // Create another sell order slightly below the call price, won't fill
   limit_order_id_type sell_med = create_sell_order( seller, bitusd.amount(7), core.amount(59) )->get_id();
   BOOST_CHECK_EQUAL( db.find( sell_med )->for_sale.value, 7 );
   // check seller balance
   BOOST_CHECK_EQUAL( 193-7, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 30801, get_balance(seller, core) );

   // call3 is not in margin call territory so won't be matched
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 25000, call3.collateral.value );

   // buy_low's price is too low that won't be matched
   BOOST_CHECK_EQUAL( db.find( buy_low )->for_sale.value, 80 );

   // generate a block
   generate_block();

} FC_LOG_AND_RETHROW() }

/***
 * BSIP38 "target_collateral_ratio" test: matching a maker limit order with multiple taker call orders
 */
BOOST_AUTO_TEST_CASE(target_cr_test_call_limit)
{ try {

   auto mi = db.get_global_properties().parameters.maintenance_interval;

   if(hf1270)
      generate_blocks(HARDFORK_CORE_1270_TIME - mi);

   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

   set_expiration( db, trx );

   ACTORS((buyer)(seller)(borrower)(borrower2)(borrower3)(feedproducer));

   const auto& bitusd = create_bitasset("USDBIT", feedproducer_id);
   const auto& core   = asset_id_type()(db);

   int64_t init_balance(1000000);

   transfer(committee_account, buyer_id, asset(init_balance));
   transfer(committee_account, borrower_id, asset(init_balance));
   transfer(committee_account, borrower2_id, asset(init_balance));
   transfer(committee_account, borrower3_id, asset(init_balance));
   update_feed_producers( bitusd, {feedproducer.get_id()} );

   price_feed current_feed;
   current_feed.maintenance_collateral_ratio = 1750;
   current_feed.maximum_short_squeeze_ratio = 1100;
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(5);
   publish_feed( bitusd, feedproducer, current_feed );
   // start out with 300% collateral, call price is 15/1.75 CORE/USD = 60/7, tcr 170% is lower than 175%
   const call_order_object& call = *borrow( borrower, bitusd.amount(1000), asset(15000), 1700);
   call_order_id_type call_id = call.get_id();
   // create another position with 310% collateral, call price is 15.5/1.75 CORE/USD = 62/7, tcr 200% is higher than 175%
   const call_order_object& call2 = *borrow( borrower2, bitusd.amount(1000), asset(15500), 2000);
   call_order_id_type call2_id = call2.get_id();
   // create yet another position with 500% collateral, call price is 25/1.75 CORE/USD = 100/7, no tcr
   const call_order_object& call3 = *borrow( borrower3, bitusd.amount(1000), asset(25000));
   transfer(borrower, seller, bitusd.amount(1000));
   transfer(borrower2, seller, bitusd.amount(1000));
   transfer(borrower3, seller, bitusd.amount(1000));

   BOOST_CHECK_EQUAL( 1000, call.debt.value );
   BOOST_CHECK_EQUAL( 15000, call.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call2.debt.value );
   BOOST_CHECK_EQUAL( 15500, call2.collateral.value );
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 25000, call3.collateral.value );
   BOOST_CHECK_EQUAL( 3000, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(seller, core) );
   BOOST_CHECK_EQUAL( 3000, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( init_balance - 15000, get_balance(borrower, core) );
   BOOST_CHECK_EQUAL( init_balance - 15500, get_balance(borrower2, core) );
   BOOST_CHECK_EQUAL( init_balance - 25000, get_balance(borrower3, core) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower2, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower3, bitusd) );

   // This sell order above MSSP will not be matched with a call
   limit_order_id_type sell_high = create_sell_order(seller, bitusd.amount(7), core.amount(78))->get_id();
   BOOST_CHECK_EQUAL( db.find( sell_high )->for_sale.value, 7 );

   BOOST_CHECK_EQUAL( 2993, get_balance(seller, bitusd) );
   BOOST_CHECK_EQUAL( 0, get_balance(seller, core) );

   // This buy order is too low will not be matched with a sell order
   limit_order_id_type buy_low = create_sell_order(buyer, asset(80), bitusd.amount(10))->get_id();

   BOOST_CHECK_EQUAL( 0, get_balance(buyer, bitusd) );
   BOOST_CHECK_EQUAL( init_balance - 80, get_balance(buyer, core) );

   // Create a sell order which will be matched with several call orders later, price 1/9
   limit_order_id_type sell_id = create_sell_order(seller, bitusd.amount(500), core.amount(4500))->get_id();
   BOOST_CHECK_EQUAL( db.find( sell_id )->for_sale.value, 500 );

   // prepare price feed to get call and call2 (but not call3) into margin call territory
   current_feed.settlement_price = bitusd.amount( 1 ) / core.amount(10);

   // call and call2's CR is quite high, and debt amount is quite a lot, assume neither of them will be completely filled
   price match_price = sell_id(db).sell_price;
   share_type call_to_cover = call_id(db).get_max_debt_to_cover(match_price,current_feed.settlement_price,1750);
   share_type call2_to_cover = call2_id(db).get_max_debt_to_cover(match_price,current_feed.settlement_price,1750);
   BOOST_CHECK_LT( call_to_cover.value, call_id(db).debt.value );
   BOOST_CHECK_LT( call2_to_cover.value, call2_id(db).debt.value );
   // even though call2 has a higher CR, since call's TCR is less than call2's TCR, so we expect call will cover less when called
   BOOST_CHECK_LT( call_to_cover.value, call2_to_cover.value );

   // adjust price feed to get call and call2 (but not call3) into margin call territory
   publish_feed( bitusd, feedproducer, current_feed );
   // settlement price = 1/10, mssp = 1/11

   // firstly the limit order will match with call, at limit order's price: 1/9
   const call_order_object* tmp_call = db.find( call_id );
   BOOST_CHECK( tmp_call != nullptr );

   // call will receive call_to_cover, pay 9*call_to_cover
   share_type call_to_pay = call_to_cover * 9;
   BOOST_CHECK_EQUAL( 1000 - call_to_cover.value, call.debt.value );
   BOOST_CHECK_EQUAL( 15000 - call_to_pay.value, call.collateral.value );
   // new collateral ratio should be higher than mcr as well as tcr
   BOOST_CHECK( call.debt.value * 10 * 1750 < call.collateral.value * 1000 );
   idump( (call) );
   // borrower's balance doesn't change
   BOOST_CHECK_EQUAL( init_balance - 15000, get_balance(borrower, core) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower, bitusd) );

   // the limit order then will match with call2, at limit order's price: 1/9
   const call_order_object* tmp_call2 = db.find( call2_id );
   BOOST_CHECK( tmp_call2 != nullptr );

   // if the limit is big enough, call2 will receive call2_to_cover, pay 11*call2_to_cover
   // however it's not the case, so call2 will receive less
   call2_to_cover = 500 - call_to_cover;
   share_type call2_to_pay = call2_to_cover * 9;
   BOOST_CHECK_EQUAL( 1000 - call2_to_cover.value, call2.debt.value );
   BOOST_CHECK_EQUAL( 15500 - call2_to_pay.value, call2.collateral.value );
   idump( (call2) );
   // borrower2's balance doesn't change
   BOOST_CHECK_EQUAL( init_balance - 15500, get_balance(borrower2, core) );
   BOOST_CHECK_EQUAL( 0, get_balance(borrower2, bitusd) );

   // call3 is not in margin call territory so won't be matched
   BOOST_CHECK_EQUAL( 1000, call3.debt.value );
   BOOST_CHECK_EQUAL( 25000, call3.collateral.value );

   // sell_id is completely filled
   BOOST_CHECK( !db.find( sell_id ) );

   // check seller balance
   BOOST_CHECK_EQUAL( 2493, get_balance(seller, bitusd) ); // 3000 - 7 - 500
   BOOST_CHECK_EQUAL( 4500, get_balance(seller, core) ); // 500*9

   // buy_low's price is too low that won't be matched
   BOOST_CHECK_EQUAL( db.find( buy_low )->for_sale.value, 80 );

   // generate a block
   generate_block();

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(mcr_bug_increase_before1270)
{ try {

   generate_block();

   set_expiration( db, trx );

   ACTORS((seller)(borrower)(borrower2)(feedproducer));

   const auto& bitusd = create_bitasset("USDBIT", feedproducer_id);
   const auto& core   = asset_id_type()(db);

   int64_t init_balance(1000000);

   transfer(committee_account, borrower_id, asset(init_balance));
   transfer(committee_account, borrower2_id, asset(init_balance));
   update_feed_producers( bitusd, {feedproducer.get_id()} );

   price_feed current_feed;
   current_feed.settlement_price = bitusd.amount( 100 ) / core.amount(100);
   current_feed.maintenance_collateral_ratio = 1750;
   current_feed.maximum_short_squeeze_ratio  = 1100;
   publish_feed( bitusd, feedproducer, current_feed );

   const call_order_object& b1 = *borrow( borrower, bitusd.amount(1000), asset(1800));
   auto b1_id = b1.get_id();
   const call_order_object& b2 = *borrow( borrower2, bitusd.amount(1000), asset(2000) );
   auto b2_id = b2.get_id();

   BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower , core ), init_balance - 1800 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, core ), init_balance - 2000 );

   // move order to margin call territory with mcr only
   current_feed.maintenance_collateral_ratio = 2000;
   publish_feed( bitusd, feedproducer, current_feed );

   BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower , core ), 998200 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, core ), 998000 );

   BOOST_CHECK( db.find( b1_id ) );
   BOOST_CHECK( db.find( b2_id ) );

   // attempt to trade the margin call
   create_sell_order( borrower2, bitusd.amount(1000), core.amount(1100) );

   BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 0 );
   BOOST_CHECK_EQUAL( get_balance( borrower , core ), 998200 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, core ), 998000  );

   print_market(bitusd.symbol, core.symbol);

   // both calls are still there, no margin call, mcr bug
   BOOST_CHECK( db.find( b1_id ) );
   BOOST_CHECK( db.find( b2_id ) );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(mcr_bug_increase_after1270)
{ try {

   auto mi = db.get_global_properties().parameters.maintenance_interval;
   generate_blocks(HARDFORK_CORE_1270_TIME - mi);
   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
   generate_block();

   set_expiration( db, trx );

   ACTORS((seller)(borrower)(borrower2)(feedproducer));

   const auto& bitusd = create_bitasset("USDBIT", feedproducer_id);
   const auto& core   = asset_id_type()(db);

   int64_t init_balance(1000000);

   transfer(committee_account, borrower_id, asset(init_balance));
   transfer(committee_account, borrower2_id, asset(init_balance));
   update_feed_producers(bitusd, {feedproducer.get_id()});

   price_feed current_feed;
   current_feed.settlement_price = bitusd.amount( 100 ) / core.amount(100);
   current_feed.maintenance_collateral_ratio = 1750;
   current_feed.maximum_short_squeeze_ratio  = 1100;
   publish_feed( bitusd, feedproducer, current_feed );

   const call_order_object& b1 = *borrow( borrower, bitusd.amount(1000), asset(1800));
   auto b1_id = b1.get_id();
   const call_order_object& b2 = *borrow( borrower2, bitusd.amount(1000), asset(2000) );
   auto b2_id = b2.get_id();

   BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower , core ), init_balance - 1800 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, core ), init_balance - 2000 );

   // move order to margin call territory with mcr only
   current_feed.maintenance_collateral_ratio = 2000;
   publish_feed( bitusd, feedproducer, current_feed );

   BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower , core ), 998200 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, core ), 998000 );

   BOOST_CHECK( db.find( b1_id ) );
   BOOST_CHECK( db.find( b2_id ) );

   // attempt to trade the margin call
   create_sell_order( borrower2, bitusd.amount(1000), core.amount(1100) );

   BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 0 );
   BOOST_CHECK_EQUAL( get_balance( borrower , core ), 998900 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, core ), 999100  );

   print_market(bitusd.symbol, core.symbol);

   // b1 is margin called
   BOOST_CHECK( ! db.find( b1_id ) );
   BOOST_CHECK( db.find( b2_id ) );


} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(mcr_bug_decrease_before1270)
{ try {

   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
   generate_block();

   set_expiration( db, trx );

   ACTORS((seller)(borrower)(borrower2)(feedproducer));

   const auto& bitusd = create_bitasset("USDBIT", feedproducer_id);
   const auto& core   = asset_id_type()(db);

   int64_t init_balance(1000000);

   transfer(committee_account, borrower_id, asset(init_balance));
   transfer(committee_account, borrower2_id, asset(init_balance));
   update_feed_producers( bitusd, {feedproducer.get_id()} );

   price_feed current_feed;
   current_feed.settlement_price = bitusd.amount( 100 ) / core.amount(100);
   current_feed.maintenance_collateral_ratio = 1750;
   current_feed.maximum_short_squeeze_ratio  = 1100;
   publish_feed( bitusd, feedproducer, current_feed );

   const call_order_object& b1 = *borrow( borrower, bitusd.amount(1000), asset(1800));
   auto b1_id = b1.get_id();
   const call_order_object& b2 = *borrow( borrower2, bitusd.amount(1000), asset(2000) );
   auto b2_id = b2.get_id();

   BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower , core ), init_balance - 1800 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, core ), init_balance - 2000 );

   // move order to margin call territory with the feed
   current_feed.settlement_price = bitusd.amount( 100 ) / core.amount(150);
   publish_feed( bitusd, feedproducer, current_feed );

   // getting out of margin call territory with mcr change
   current_feed.maintenance_collateral_ratio = 1100;
   publish_feed( bitusd, feedproducer, current_feed );

   BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower , core ), 998200 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, core ), 998000 );

   BOOST_CHECK( db.find( b1_id ) );
   BOOST_CHECK( db.find( b2_id ) );

   // attempt to trade the margin call
   create_sell_order( borrower2, bitusd.amount(1000), core.amount(1100) );

   BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 0 );
   BOOST_CHECK_EQUAL( get_balance( borrower , core ), 998350 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, core ), 999650  );

   print_market(bitusd.symbol, core.symbol);

   // margin call at b1, mcr bug
   BOOST_CHECK( !db.find( b1_id ) );
   BOOST_CHECK( db.find( b2_id ) );


} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(mcr_bug_decrease_after1270)
{ try {

   auto mi = db.get_global_properties().parameters.maintenance_interval;
   generate_blocks(HARDFORK_CORE_1270_TIME - mi);
   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
   generate_block();

   set_expiration( db, trx );

   ACTORS((seller)(borrower)(borrower2)(feedproducer));

   const auto& bitusd = create_bitasset("USDBIT", feedproducer_id);
   const auto& core   = asset_id_type()(db);

   int64_t init_balance(1000000);

   transfer(committee_account, borrower_id, asset(init_balance));
   transfer(committee_account, borrower2_id, asset(init_balance));
   update_feed_producers( bitusd, {feedproducer.get_id()} );

   price_feed current_feed;
   current_feed.settlement_price = bitusd.amount( 100 ) / core.amount(100);
   current_feed.maintenance_collateral_ratio = 1750;
   current_feed.maximum_short_squeeze_ratio  = 1100;
   publish_feed( bitusd, feedproducer, current_feed );

   const call_order_object& b1 = *borrow( borrower, bitusd.amount(1000), asset(1800));
   auto b1_id = b1.get_id();
   const call_order_object& b2 = *borrow( borrower2, bitusd.amount(1000), asset(2000) );
   auto b2_id = b2.get_id();

   BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower , core ), init_balance - 1800 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, core ), init_balance - 2000 );

   // move order to margin call territory with the feed
   current_feed.settlement_price = bitusd.amount( 100 ) / core.amount(150);
   publish_feed( bitusd, feedproducer, current_feed );

   // getting out of margin call territory with mcr decrease
   current_feed.maintenance_collateral_ratio = 1100;
   publish_feed( bitusd, feedproducer, current_feed );

   BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower , core ), 998200 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, core ), 998000 );

   BOOST_CHECK( db.find( b1_id ) );
   BOOST_CHECK( db.find( b2_id ) );

   // attempt to trade the margin call
   create_sell_order( borrower2, bitusd.amount(1000), core.amount(1100) );

   BOOST_CHECK_EQUAL( get_balance( borrower, bitusd ), 1000 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, bitusd ), 0 );
   BOOST_CHECK_EQUAL( get_balance( borrower , core ), 998200 );
   BOOST_CHECK_EQUAL( get_balance( borrower2, core ), 998000  );

   print_market(bitusd.symbol, core.symbol);

   // both calls are there, no margin call, good
   BOOST_CHECK( db.find( b1_id ) );
   BOOST_CHECK( db.find( b2_id ) );


} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(mcr_bug_cross1270)
{ try {

   INVOKE(mcr_bug_increase_before1270);

   auto mi = db.get_global_properties().parameters.maintenance_interval;
   generate_blocks(HARDFORK_CORE_1270_TIME - mi);

   const asset_object& core = get_asset(GRAPHENE_SYMBOL);
   const asset_object& bitusd = get_asset("USDBIT");
   const asset_id_type bitusd_id = bitusd.get_id();
   const account_object& feedproducer = get_account("feedproducer");

   // feed is expired
   auto mcr = (*bitusd_id(db).bitasset_data_id)(db).current_feed.maintenance_collateral_ratio;
   BOOST_CHECK_EQUAL(mcr, GRAPHENE_DEFAULT_MAINTENANCE_COLLATERAL_RATIO);

   // make new feed
   price_feed current_feed;
   current_feed.settlement_price = bitusd.amount( 100 ) / core.amount(100);
   current_feed.maintenance_collateral_ratio = 2000;
   current_feed.maximum_short_squeeze_ratio  = 1100;
   publish_feed( bitusd, feedproducer, current_feed );

   mcr = (*bitusd_id(db).bitasset_data_id)(db).current_feed.maintenance_collateral_ratio;
   BOOST_CHECK_EQUAL(mcr, 2000);

   // pass hardfork
   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
   generate_block();

   // feed is still valid
   mcr = (*bitusd_id(db).bitasset_data_id)(db).current_feed.maintenance_collateral_ratio;
   BOOST_CHECK_EQUAL(mcr, 2000);

   // margin call is traded
   print_market(asset_id_type(1)(db).symbol, asset_id_type()(db).symbol);

   // call b1 not there anymore
   BOOST_CHECK( !db.find( call_order_id_type() ) );
   BOOST_CHECK( db.find( call_order_id_type(1) ) );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(multiple_limit_order_filling_after_hf1270)
{ try {
   hf1270 = true;
   INVOKE(multiple_limit_order_filling);

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(big_limit_order_test_after_hf1270)
{ try {
   hf1270 = true;
   INVOKE(big_limit_order_test);

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(target_cr_test_limit_call_after_hf1270)
{ try {
   hf1270 = true;
   INVOKE(target_cr_test_limit_call);

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(target_cr_test_call_limit_after_hf1270)
{ try {
   hf1270 = true;
   INVOKE(target_cr_test_call_limit);

} FC_LOG_AND_RETHROW() }


BOOST_AUTO_TEST_SUITE_END()
