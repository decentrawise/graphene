#include <graphene/chain/hardfork.hpp>

#include <graphene/chain/fba_accumulator_id.hpp>

#include <graphene/chain/fba_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/vesting_balance_object.hpp>
#include <graphene/chain/exceptions.hpp>

#include <fc/uint128.hpp>

#include <boost/test/unit_test.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE( fee_tests, database_fixture )

BOOST_AUTO_TEST_CASE( nonzero_fee_test )
{
   try
   {
      ACTORS((alice)(bob));

      const share_type prec = asset::scaled_precision( asset_id_type()(db).precision );

      // Return number of core shares (times precision)
      auto _core = [&]( int64_t x ) -> asset
      {  return asset( x*prec );    };

      transfer( council_account, alice_id, _core(1000000) );

      // make sure the database requires our fee to be nonzero
      enable_fees();

      signed_transaction tx;
      transfer_operation xfer_op;
      xfer_op.from = alice_id;
      xfer_op.to = bob_id;
      xfer_op.amount = _core(1000);
      xfer_op.fee = _core(0);
      tx.operations.push_back( xfer_op );
      set_expiration( db, tx );
      sign( tx, alice_private_key );
      GRAPHENE_REQUIRE_THROW( PUSH_TX( db, tx ), insufficient_fee );
   }
   catch( const fc::exception& e )
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(asset_claim_fees_test)
{
   try
   {
      ACTORS((alice)(bob)(izzy)(jill));
      // Izzy issues asset to Alice
      // Jill issues asset to Bob
      // Alice and Bob trade in the market and pay fees
      // Verify that Izzy and Jill can claim the fees

      const share_type core_prec = asset::scaled_precision( asset_id_type()(db).precision );

      // Return number of core shares (times precision)
      auto _core = [&]( int64_t x ) -> asset
      {  return asset( x*core_prec );    };

      transfer( council_account, alice_id, _core(1000000) );
      transfer( council_account,   bob_id, _core(1000000) );
      transfer( council_account,  izzy_id, _core(1000000) );
      transfer( council_account,  jill_id, _core(1000000) );

      asset_id_type izzycoin_id = create_backed_asset( "IZZYCOIN", izzy_id,   GRAPHENE_1_PERCENT, charge_market_fee ).get_id();
      asset_id_type jillcoin_id = create_backed_asset( "JILLCOIN", jill_id, 2*GRAPHENE_1_PERCENT, charge_market_fee ).get_id();

      const share_type izzy_prec = asset::scaled_precision( asset_id_type(izzycoin_id)(db).precision );
      const share_type jill_prec = asset::scaled_precision( asset_id_type(jillcoin_id)(db).precision );

      auto _izzy = [&]( int64_t x ) -> asset
      {   return asset( x*izzy_prec, izzycoin_id );   };
      auto _jill = [&]( int64_t x ) -> asset
      {   return asset( x*jill_prec, jillcoin_id );   };

      update_feed_producers( izzycoin_id(db), { izzy_id } );
      update_feed_producers( jillcoin_id(db), { jill_id } );

      const asset izzy_satoshi = asset(1, izzycoin_id);
      const asset jill_satoshi = asset(1, jillcoin_id);

      // Izzycoin is worth 100 CORE
      price_feed feed;
      feed.settlement_price = price( _izzy(1), _core(100) );
      feed.maintenance_collateral_ratio = 175 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
      feed.maximum_short_squeeze_ratio = 150 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
      publish_feed( izzycoin_id(db), izzy, feed );

      // Jillcoin is worth 30 CORE
      feed.settlement_price = price( _jill(1), _core(30) );
      feed.maintenance_collateral_ratio = 175 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
      feed.maximum_short_squeeze_ratio = 150 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
      publish_feed( jillcoin_id(db), jill, feed );

      enable_fees();

      // Alice and Bob create some coins
      borrow( alice_id, _izzy( 200), _core( 60000) );
      borrow(   bob_id, _jill(2000), _core(180000) );

      // Alice and Bob place orders which match
      create_sell_order( alice_id, _izzy(100), _jill(300) );   // Alice is willing to sell her Izzy's for 3 Jill
      create_sell_order(   bob_id, _jill(700), _izzy(200) );   // Bob is buying up to 200 Izzy's for up to 3.5 Jill

      // 100 Izzys and 300 Jills are matched, so the fees should be
      //   1 Izzy (1%) and 6 Jill (2%).

      auto claim_fees = [&]( account_id_type issuer, asset amount_to_claim )
      {
         asset_claim_fees_operation claim_op;
         claim_op.issuer = issuer;
         claim_op.amount_to_claim = amount_to_claim;
         signed_transaction tx;
         tx.operations.push_back( claim_op );
         db.current_fee_schedule().set_fee( tx.operations.back() );
         set_expiration( db, tx );
         fc::ecc::private_key   my_pk = (issuer == izzy_id) ? izzy_private_key : jill_private_key;
         fc::ecc::private_key your_pk = (issuer == izzy_id) ? jill_private_key : izzy_private_key;
         sign( tx, your_pk );
         GRAPHENE_REQUIRE_THROW( PUSH_TX( db, tx ), fc::exception );
         tx.clear_signatures();
         sign( tx, my_pk );
         PUSH_TX( db, tx );
      };

      {
         const asset_object& izzycoin = izzycoin_id(db);
         const asset_object& jillcoin = jillcoin_id(db);

         //wdump( (izzycoin)(izzycoin.dynamic_asset_data_id(db))((*izzycoin.backed_asset_data_id)(db)) );
         //wdump( (jillcoin)(jillcoin.dynamic_asset_data_id(db))((*jillcoin.backed_asset_data_id)(db)) );

         // check the correct amount of fees has been awarded
         BOOST_CHECK( izzycoin.dynamic_asset_data_id(db).accumulated_fees == _izzy(1).amount );
         BOOST_CHECK( jillcoin.dynamic_asset_data_id(db).accumulated_fees == _jill(6).amount );

      }

      {
         const asset_object& izzycoin = izzycoin_id(db);
         const asset_object& jillcoin = jillcoin_id(db);

         // can't claim more than balance
         GRAPHENE_REQUIRE_THROW( claim_fees( izzy_id, _izzy(1) + izzy_satoshi ), fc::exception );
         GRAPHENE_REQUIRE_THROW( claim_fees( jill_id, _jill(6) + jill_satoshi ), fc::exception );

         // can't claim asset that doesn't belong to you
         GRAPHENE_REQUIRE_THROW( claim_fees( jill_id, izzy_satoshi ), fc::exception );
         GRAPHENE_REQUIRE_THROW( claim_fees( izzy_id, jill_satoshi ), fc::exception );

         // can claim asset in one go
         claim_fees( izzy_id, _izzy(1) );
         GRAPHENE_REQUIRE_THROW( claim_fees( izzy_id, izzy_satoshi ), fc::exception );
         BOOST_CHECK( izzycoin.dynamic_asset_data_id(db).accumulated_fees == _izzy(0).amount );

         // can claim in multiple goes
         claim_fees( jill_id, _jill(4) );
         BOOST_CHECK( jillcoin.dynamic_asset_data_id(db).accumulated_fees == _jill(2).amount );
         GRAPHENE_REQUIRE_THROW( claim_fees( jill_id, _jill(2) + jill_satoshi ), fc::exception );
         claim_fees( jill_id, _jill(2) );
         BOOST_CHECK( jillcoin.dynamic_asset_data_id(db).accumulated_fees == _jill(0).amount );
      }
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(asset_claim_pool_test)
{
    try
    {
        ACTORS((alice)(bob));
        // Alice and Bob create some user assets
        // Alice deposits CORE to the fee pool
        // Alice claims fee pool of her asset and can't claim pool of Bob's asset

        const share_type core_prec = asset::scaled_precision( asset_id_type()(db).precision );

        // return number of core shares (times precision)
        auto _core = [&core_prec]( int64_t x ) -> asset
        {  return asset( x*core_prec );    };

        const asset_object& alicecoin = create_user_asset( "ALICECOIN", alice,  0 );
        const asset_object& aliceusd = create_user_asset( "ALICEUSD", alice, 0 );

        asset_id_type alicecoin_id = alicecoin.get_id();
        asset_id_type aliceusd_id = aliceusd.get_id();
        asset_id_type bobcoin_id = create_user_asset("BOBCOIN", bob, 0).get_id();

        // prepare users' balance
        issue_ua( alice, aliceusd.amount( 20000000 ) );
        issue_ua( alice, alicecoin.amount( 10000000 ) );

        transfer( council_account, alice_id, _core(1000) );
        transfer( council_account, bob_id, _core(1000) );

        enable_fees();

        auto claim_pool = [&]( const account_id_type issuer, const asset_id_type asset_to_claim,
                              const asset& amount_to_fund, const asset_object& fee_asset  )
        {
            asset_claim_pool_operation claim_op;
            claim_op.issuer = issuer;
            claim_op.asset_id = asset_to_claim;
            claim_op.amount_to_claim = amount_to_fund;

            signed_transaction tx;
            tx.operations.push_back( claim_op );
            db.current_fee_schedule().set_fee( tx.operations.back(), fee_asset.options.core_exchange_rate );
            set_expiration( db, tx );
            sign( tx, alice_private_key );
            PUSH_TX( db, tx );

        };

        auto claim_pool_proposal = [&]( const account_id_type issuer, const asset_id_type asset_to_claim,
                                        const asset& amount_to_fund, const asset_object& fee_asset  )
        {
            asset_claim_pool_operation claim_op;
            claim_op.issuer = issuer;
            claim_op.asset_id = asset_to_claim;
            claim_op.amount_to_claim = amount_to_fund;

            const auto& curfees = db.get_global_properties().parameters.get_current_fees();
            const auto& proposal_create_fees = curfees.get<proposal_create_operation>();
            proposal_create_operation prop;
            prop.fee_paying_account = alice_id;
            prop.proposed_ops.emplace_back( claim_op );
            prop.expiration_time =  db.head_block_time() + fc::days(1);
            prop.fee = asset( proposal_create_fees.fee + proposal_create_fees.price_per_kbyte );

            signed_transaction tx;
            tx.operations.push_back( prop );
            db.current_fee_schedule().set_fee( tx.operations.back(), fee_asset.options.core_exchange_rate );
            set_expiration( db, tx );
            sign( tx, alice_private_key );
            PUSH_TX( db, tx );

        };

        // deposit 100 CORE to the fee pool of ALICEUSD asset
        fund_fee_pool( alice_id(db), aliceusd_id(db), _core(100).amount );

        // Reference for core_asset
        const asset_object& core_asset = asset_id_type()(db);

        // can't claim pool because it is empty
        GRAPHENE_REQUIRE_THROW( claim_pool( alice_id, alicecoin_id, _core(1), core_asset), fc::exception );

        // deposit 300 CORE to the fee pool of ALICECOIN asset
        fund_fee_pool( alice_id(db), alicecoin_id(db), _core(300).amount );

        // Test amount of CORE in fee pools
        BOOST_CHECK( alicecoin_id(db).dynamic_asset_data_id(db).fee_pool == _core(300).amount );
        BOOST_CHECK( aliceusd_id(db).dynamic_asset_data_id(db).fee_pool == _core(100).amount );

        // can't claim pool of an asset that doesn't belong to you
        GRAPHENE_REQUIRE_THROW( claim_pool( alice_id, bobcoin_id, _core(200), core_asset), fc::exception );

        // can't claim more than is available in the fee pool
        GRAPHENE_REQUIRE_THROW( claim_pool( alice_id, alicecoin_id, _core(400), core_asset ), fc::exception );

        // can't pay fee in the same asset whose pool is being drained
        GRAPHENE_REQUIRE_THROW( claim_pool( alice_id, alicecoin_id, _core(200), alicecoin_id(db) ), fc::exception );

        // can claim CORE back from the fee pool
        claim_pool( alice_id, alicecoin_id, _core(200), core_asset );
        BOOST_CHECK( alicecoin_id(db).dynamic_asset_data_id(db).fee_pool == _core(100).amount );

        // can pay fee in the asset other than the one whose pool is being drained
        share_type balance_before_claim = get_balance( alice_id, asset_id_type() );
        claim_pool( alice_id, alicecoin_id, _core(100), aliceusd_id(db) );
        BOOST_CHECK( alicecoin_id(db).dynamic_asset_data_id(db).fee_pool == _core(0).amount );

        //check balance after claiming pool
        share_type current_balance = get_balance( alice_id, asset_id_type() );
        BOOST_CHECK( balance_before_claim + _core(100).amount == current_balance );

        // can create a proposal to claim claim pool
        claim_pool_proposal( alice_id, aliceusd_id, _core(1), core_asset);
    }
    FC_LOG_AND_RETHROW()
}

///////////////////////////////////////////////////////////////
// cashback_test infrastructure                              //
///////////////////////////////////////////////////////////////

#define CHECK_BALANCE( actor_name, amount ) \
   BOOST_CHECK_EQUAL( get_balance( actor_name ## _id, asset_id_type() ), amount )

#define CHECK_VESTED_CASHBACK( actor_name, amount ) \
   BOOST_CHECK_EQUAL( actor_name ## _id(db).statistics(db).pending_vested_fees.value, amount )

#define CHECK_UNVESTED_CASHBACK( actor_name, amount ) \
   BOOST_CHECK_EQUAL( actor_name ## _id(db).statistics(db).pending_fees.value, amount )

#define GET_CASHBACK_BALANCE( account ) \
   ( (account.cashback_vb.valid()) \
   ? account.cashback_balance(db).balance.amount.value \
   : 0 )

#define CHECK_CASHBACK_VBO( actor_name, _amount ) \
   BOOST_CHECK_EQUAL( GET_CASHBACK_BALANCE( actor_name ## _id(db) ), _amount )

#define P100 GRAPHENE_100_PERCENT
#define P1 GRAPHENE_1_PERCENT

uint64_t pct( uint64_t percentage, uint64_t val )
{
   fc::uint128_t x = percentage;
   x *= val;
   x /= GRAPHENE_100_PERCENT;
   return static_cast<uint64_t>(x);
}

uint64_t pct( uint64_t percentage0, uint64_t percentage1, uint64_t val )
{
   return pct( percentage1, pct( percentage0, val ) );
}

uint64_t pct( uint64_t percentage0, uint64_t percentage1, uint64_t percentage2, uint64_t val )
{
   return pct( percentage2, pct( percentage1, pct( percentage0, val ) ) );
}

struct actor_audit
{
   int64_t b0 = 0;      // starting balance parameter
   int64_t bal = 0;     // balance should be this
   int64_t ubal = 0;    // unvested balance (in VBO) should be this
   int64_t ucb = 0;     // unvested cashback in account_statistics should be this
   int64_t vcb = 0;     // vested cashback in account_statistics should be this
   int64_t ref_pct = 0; // referrer percentage should be this
};

BOOST_AUTO_TEST_CASE( cashback_test )
{ try {
   /*                        Account Structure used in this test                         *
    *                                                                                    *
    *               /-----------------\       /-------------------\                      *
    *               | life (Lifetime) |       |  rog (Lifetime)   |                      *
    *               \-----------------/       \-------------------/                      *
    *                  | Ref&Reg    | Refers     | Registers  | Registers                *
    *                  |            | 75         | 25         |                          *
    *                  v            v            v            |                          *
    *  /----------------\         /----------------\          |                          *
    *  |  ann (Annual)  |         |  dumy (basic)  |          |                          *
    *  \----------------/         \----------------/          |-------------.            *
    * 80 | Refers      L--------------------------------.     |             |            *
    *    v                     Refers                80 v     v 20          |            *
    *  /----------------\                         /----------------\        |            *
    *  |  scud (basic)  |<------------------------|  stud (basic)  |        |            *
    *  \----------------/ 20   Registers          | (Upgrades to   |        | 5          *
    *                                             |   Lifetime)    |        v            *
    *                                             \----------------/   /--------------\  *
    *                                                         L------->| pleb (Basic) |  *
    *                                                       95 Refers  \--------------/  *
    *                                                                                    *
    * Fee distribution chains (80-20 referral/net split, 50-30 referrer/LTM split)       *
    * life : 80% -> life, 20% -> net                                                     *
    * rog: 80% -> rog, 20% -> net                                                        *
    * ann (before upg): 80% -> life, 20% -> net                                          *
    * ann (after upg): 80% * 5/8 -> ann, 80% * 3/8 -> life, 20% -> net                   *
    * stud (before upg): 80% * 5/8 -> ann, 80% * 3/8 -> life, 20% * 80% -> rog,          *
    *                    20% -> net                                                      *
    * stud (after upg): 80% -> stud, 20% -> net                                          *
    * dumy : 75% * 80% -> life, 25% * 80% -> rog, 20% -> net                             *
    * scud : 80% * 5/8 -> ann, 80% * 3/8 -> life, 20% * 80% -> stud, 20% -> net          *
    * pleb : 95% * 80% -> stud, 5% * 80% -> rog, 20% -> net                              *
    */

   BOOST_TEST_MESSAGE("Creating actors");

   ACTOR(life);
   ACTOR(rog);
   PREP_ACTOR(ann);
   PREP_ACTOR(scud);
   PREP_ACTOR(dumy);
   PREP_ACTOR(stud);
   PREP_ACTOR(pleb);
   // use ##_public_key vars to silence unused variable warning
   BOOST_CHECK_GT(ann_public_key.key_data.size(), 0u);
   BOOST_CHECK_GT(scud_public_key.key_data.size(), 0u);
   BOOST_CHECK_GT(dumy_public_key.key_data.size(), 0u);
   BOOST_CHECK_GT(stud_public_key.key_data.size(), 0u);
   BOOST_CHECK_GT(pleb_public_key.key_data.size(), 0u);

   account_id_type ann_id, scud_id, dumy_id, stud_id, pleb_id;
   actor_audit alife, arog, aann, ascud, adumy, astud, apleb;

   alife.b0 = 100000000;
   arog.b0 = 100000000;
   aann.b0 = 1000000;
   astud.b0 = 1000000;
   astud.ref_pct = 80 * GRAPHENE_1_PERCENT;
   ascud.ref_pct = 80 * GRAPHENE_1_PERCENT;
   adumy.ref_pct = 75 * GRAPHENE_1_PERCENT;
   apleb.ref_pct = 95 * GRAPHENE_1_PERCENT;

   transfer(account_id_type(), life_id, asset(alife.b0));
   alife.bal += alife.b0;
   transfer(account_id_type(), rog_id, asset(arog.b0));
   arog.bal += arog.b0;
   upgrade_to_lifetime_member(life_id);
   upgrade_to_lifetime_member(rog_id);

   BOOST_TEST_MESSAGE("Enable fees");
   const auto& fees = db.get_global_properties().parameters.get_current_fees();

#define CustomRegisterActor(actor_name, registrar_name, referrer_name, referrer_rate) \
   { \
      account_create_operation op; \
      op.registrar = registrar_name ## _id; \
      op.referrer = referrer_name ## _id; \
      op.referrer_percent = referrer_rate*GRAPHENE_1_PERCENT; \
      op.name = BOOST_PP_STRINGIZE(actor_name); \
      op.options.memo_key = actor_name ## _private_key.get_public_key(); \
      op.active = authority(1, public_key_type(actor_name ## _private_key.get_public_key()), 1); \
      op.owner = op.active; \
      op.fee = fees.calculate_fee(op); \
      trx.operations = {op}; \
      sign( trx,  registrar_name ## _private_key ); \
      actor_name ## _id = PUSH_TX( db, trx ).operation_results.front().get<object_id_type>(); \
      trx.clear(); \
   }
#define CustomAuditActor(actor_name)                                \
   if( actor_name ## _id != account_id_type() )                     \
   {                                                                \
      CHECK_BALANCE( actor_name, a ## actor_name.bal );             \
      CHECK_VESTED_CASHBACK( actor_name, a ## actor_name.vcb );     \
      CHECK_UNVESTED_CASHBACK( actor_name, a ## actor_name.ucb );   \
      CHECK_CASHBACK_VBO( actor_name, a ## actor_name.ubal );       \
   }

#define CustomAudit()                                \
   {                                                 \
      CustomAuditActor( life );                      \
      CustomAuditActor( rog );                       \
      CustomAuditActor( ann );                       \
      CustomAuditActor( stud );                      \
      CustomAuditActor( dumy );                      \
      CustomAuditActor( scud );                      \
      CustomAuditActor( pleb );                      \
   }

   int64_t reg_fee    = fees.get< account_create_operation >().premium_fee;
   int64_t xfer_fee   = fees.get< transfer_operation >().fee;
   int64_t upg_an_fee = fees.get< account_upgrade_operation >().membership_annual_fee;
   int64_t upg_lt_fee = fees.get< account_upgrade_operation >().membership_lifetime_fee;
   // all percentages here are cut from whole pie!
   uint64_t network_pct = 20 * P1;
   uint64_t lt_pct = 375 * P100 / 1000;

   BOOST_TEST_MESSAGE("Register and upgrade Ann");
   {
      CustomRegisterActor(ann, life, life, 75);
      alife.vcb += reg_fee; alife.bal += -reg_fee;
      CustomAudit();

      transfer(life_id, ann_id, asset(aann.b0));
      alife.vcb += xfer_fee; alife.bal += -xfer_fee -aann.b0; aann.bal += aann.b0;
      CustomAudit();

      upgrade_to_annual_member(ann_id);
      aann.ucb += upg_an_fee; aann.bal += -upg_an_fee;

      // audit distribution of fees from Ann
      alife.ubal += pct( P100-network_pct, aann.ucb );
      alife.bal  += pct( P100-network_pct, aann.vcb );
      aann.ucb = 0; aann.vcb = 0;
      CustomAudit();
   }

   BOOST_TEST_MESSAGE("Register dumy and stud");
   CustomRegisterActor(dumy, rog, life, 75);
   arog.vcb += reg_fee; arog.bal += -reg_fee;
   CustomAudit();

   CustomRegisterActor(stud, rog, ann, 80);
   arog.vcb += reg_fee; arog.bal += -reg_fee;
   CustomAudit();

   BOOST_TEST_MESSAGE("Upgrade stud to lifetime member");

   transfer(life_id, stud_id, asset(astud.b0));
   alife.vcb += xfer_fee; alife.bal += -astud.b0 -xfer_fee; astud.bal += astud.b0;
   CustomAudit();

   upgrade_to_lifetime_member(stud_id);
   astud.ucb += upg_lt_fee; astud.bal -= upg_lt_fee;

/*
network_cut:   20000
referrer_cut:  40000 -> ann
registrar_cut: 10000 -> rog
lifetime_cut:  30000 -> life

NET : net
LTM : net' ltm
REF : net' ltm' ref
REG : net' ltm' ref'
*/

   // audit distribution of fees from stud
   alife.ubal += pct( P100-network_pct,      lt_pct,                     astud.ucb );
   aann.ubal  += pct( P100-network_pct, P100-lt_pct,      astud.ref_pct, astud.ucb );
   arog.ubal  += pct( P100-network_pct, P100-lt_pct, P100-astud.ref_pct, astud.ucb );
   astud.ucb  = 0;
   CustomAudit();

   BOOST_TEST_MESSAGE("Register pleb and scud");

   CustomRegisterActor(pleb, rog, stud, 95);
   arog.vcb += reg_fee; arog.bal += -reg_fee;
   CustomAudit();

   CustomRegisterActor(scud, stud, ann, 80);
   astud.vcb += reg_fee; astud.bal += -reg_fee;
   CustomAudit();

   generate_block();

   BOOST_TEST_MESSAGE("Wait for maintenance interval");

   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
   // audit distribution of fees from life
   alife.ubal += pct( P100-network_pct, alife.ucb +alife.vcb );
   alife.ucb = 0; alife.vcb = 0;

   // audit distribution of fees from rog
   arog.ubal += pct( P100-network_pct, arog.ucb + arog.vcb );
   arog.ucb = 0; arog.vcb = 0;

   // audit distribution of fees from ann
   alife.ubal += pct( P100-network_pct,      lt_pct,                    aann.ucb+aann.vcb );
   aann.ubal  += pct( P100-network_pct, P100-lt_pct,      aann.ref_pct, aann.ucb+aann.vcb );
   alife.ubal += pct( P100-network_pct, P100-lt_pct, P100-aann.ref_pct, aann.ucb+aann.vcb );
   aann.ucb = 0; aann.vcb = 0;

   // audit distribution of fees from stud
   astud.ubal += pct( P100-network_pct,                                  astud.ucb+astud.vcb );
   astud.ucb = 0; astud.vcb = 0;

   // audit distribution of fees from dumy
   alife.ubal += pct( P100-network_pct,      lt_pct,                     adumy.ucb+adumy.vcb );
   alife.ubal += pct( P100-network_pct, P100-lt_pct,      adumy.ref_pct, adumy.ucb+adumy.vcb );
   arog.ubal  += pct( P100-network_pct, P100-lt_pct, P100-adumy.ref_pct, adumy.ucb+adumy.vcb );
   adumy.ucb = 0; adumy.vcb = 0;

   // audit distribution of fees from scud
   alife.ubal += pct( P100-network_pct,      lt_pct,                     ascud.ucb+ascud.vcb );
   aann.ubal  += pct( P100-network_pct, P100-lt_pct,      ascud.ref_pct, ascud.ucb+ascud.vcb );
   astud.ubal += pct( P100-network_pct, P100-lt_pct, P100-ascud.ref_pct, ascud.ucb+ascud.vcb );
   ascud.ucb = 0; ascud.vcb = 0;

   // audit distribution of fees from pleb
   astud.ubal += pct( P100-network_pct,      lt_pct,                     apleb.ucb+apleb.vcb );
   astud.ubal += pct( P100-network_pct, P100-lt_pct,      apleb.ref_pct, apleb.ucb+apleb.vcb );
   arog.ubal  += pct( P100-network_pct, P100-lt_pct, P100-apleb.ref_pct, apleb.ucb+apleb.vcb );
   apleb.ucb = 0; apleb.vcb = 0;

   CustomAudit();

   BOOST_TEST_MESSAGE("Doing some transfers");

   transfer(stud_id, scud_id, asset(500000));
   astud.bal += -500000-xfer_fee; astud.vcb += xfer_fee; ascud.bal += 500000;
   CustomAudit();

   transfer(scud_id, pleb_id, asset(400000));
   ascud.bal += -400000-xfer_fee; ascud.vcb += xfer_fee; apleb.bal += 400000;
   CustomAudit();

   transfer(pleb_id, dumy_id, asset(300000));
   apleb.bal += -300000-xfer_fee; apleb.vcb += xfer_fee; adumy.bal += 300000;
   CustomAudit();

   transfer(dumy_id, rog_id, asset(200000));
   adumy.bal += -200000-xfer_fee; adumy.vcb += xfer_fee; arog.bal += 200000;
   CustomAudit();

   BOOST_TEST_MESSAGE("Waiting for maintenance time");

   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

   // audit distribution of fees from life
   alife.ubal += pct( P100-network_pct, alife.ucb +alife.vcb );
   alife.ucb = 0; alife.vcb = 0;

   // audit distribution of fees from rog
   arog.ubal += pct( P100-network_pct, arog.ucb + arog.vcb );
   arog.ucb = 0; arog.vcb = 0;

   // audit distribution of fees from ann
   alife.ubal += pct( P100-network_pct,      lt_pct,                    aann.ucb+aann.vcb );
   aann.ubal  += pct( P100-network_pct, P100-lt_pct,      aann.ref_pct, aann.ucb+aann.vcb );
   alife.ubal += pct( P100-network_pct, P100-lt_pct, P100-aann.ref_pct, aann.ucb+aann.vcb );
   aann.ucb = 0; aann.vcb = 0;

   // audit distribution of fees from stud
   astud.ubal += pct( P100-network_pct,                                  astud.ucb+astud.vcb );
   astud.ucb = 0; astud.vcb = 0;

   // audit distribution of fees from dumy
   alife.ubal += pct( P100-network_pct,      lt_pct,                     adumy.ucb+adumy.vcb );
   alife.ubal += pct( P100-network_pct, P100-lt_pct,      adumy.ref_pct, adumy.ucb+adumy.vcb );
   arog.ubal  += pct( P100-network_pct, P100-lt_pct, P100-adumy.ref_pct, adumy.ucb+adumy.vcb );
   adumy.ucb = 0; adumy.vcb = 0;

   // audit distribution of fees from scud
   alife.ubal += pct( P100-network_pct,      lt_pct,                     ascud.ucb+ascud.vcb );
   aann.ubal  += pct( P100-network_pct, P100-lt_pct,      ascud.ref_pct, ascud.ucb+ascud.vcb );
   astud.ubal += pct( P100-network_pct, P100-lt_pct, P100-ascud.ref_pct, ascud.ucb+ascud.vcb );
   ascud.ucb = 0; ascud.vcb = 0;

   // audit distribution of fees from pleb
   astud.ubal += pct( P100-network_pct,      lt_pct,                     apleb.ucb+apleb.vcb );
   astud.ubal += pct( P100-network_pct, P100-lt_pct,      apleb.ref_pct, apleb.ucb+apleb.vcb );
   arog.ubal  += pct( P100-network_pct, P100-lt_pct, P100-apleb.ref_pct, apleb.ucb+apleb.vcb );
   apleb.ucb = 0; apleb.vcb = 0;

   CustomAudit();

   BOOST_TEST_MESSAGE("Waiting for annual membership to expire");

   generate_blocks(ann_id(db).membership_expiration_date);
   generate_block();

   BOOST_TEST_MESSAGE("Transferring from scud to pleb");

   //ann's membership has expired, so scud's fee should go up to life instead.
   transfer(scud_id, pleb_id, asset(10));
   ascud.bal += -10-xfer_fee; ascud.vcb += xfer_fee; apleb.bal += 10;
   CustomAudit();

   BOOST_TEST_MESSAGE("Waiting for maint interval");

   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

   // audit distribution of fees from scud
   alife.ubal += pct( P100-network_pct,      lt_pct,                     ascud.ucb+ascud.vcb );
   alife.ubal += pct( P100-network_pct, P100-lt_pct,      ascud.ref_pct, ascud.ucb+ascud.vcb );
   astud.ubal += pct( P100-network_pct, P100-lt_pct, P100-ascud.ref_pct, ascud.ucb+ascud.vcb );
   ascud.ucb = 0; ascud.vcb = 0;

   CustomAudit();

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( account_create_fee_scaling )
{ try {
   auto accounts_per_scale = db.get_global_properties().parameters.accounts_per_fee_scale;
   db.modify(global_property_id_type()(db), [](global_property_object& gpo)
   {
      gpo.parameters.get_mutable_fees() = fee_schedule::get_default();
      gpo.parameters.get_mutable_fees().get<account_create_operation>().basic_fee = 1;
   });

   for( int i = db.get_dynamic_global_properties().accounts_registered_this_interval; i < accounts_per_scale; ++i )
   {
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.get_current_fees().get<account_create_operation>().basic_fee, 1u);
      create_account("shill" + fc::to_string(i));
   }
   for( int i = 0; i < accounts_per_scale; ++i )
   {
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.get_current_fees().get<account_create_operation>().basic_fee, 16u);
      create_account("moreshills" + fc::to_string(i));
   }
   for( int i = 0; i < accounts_per_scale; ++i )
   {
      BOOST_CHECK_EQUAL(db.get_global_properties().parameters.get_current_fees().get<account_create_operation>().basic_fee, 256u);
      create_account("moarshills" + fc::to_string(i));
   }
   BOOST_CHECK_EQUAL(db.get_global_properties().parameters.get_current_fees().get<account_create_operation>().basic_fee, 4096u);

   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
   BOOST_CHECK_EQUAL(db.get_global_properties().parameters.get_current_fees().get<account_create_operation>().basic_fee, 1u);
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( fee_refund_test )
{
   try
   {
      ACTORS((alice)(bob)(izzy));

      int64_t alice_b0 = 1000000, bob_b0 = 1000000;

      transfer( account_id_type(), alice_id, asset(alice_b0) );
      transfer( account_id_type(), bob_id, asset(bob_b0) );

      asset_id_type core_id = asset_id_type();
      asset_id_type usd_id = create_user_asset( "IZZYUSD", izzy_id(db), charge_market_fee ).get_id();
      issue_ua( alice_id, asset( alice_b0, usd_id ) );
      issue_ua( bob_id, asset( bob_b0, usd_id ) );

      int64_t order_create_fee = 537;
      int64_t order_cancel_fee = 129;

      uint32_t skip = database::skip_validator_signature
                    | database::skip_transaction_signatures
                    | database::skip_transaction_dupe_check
                    | database::skip_block_size_check
                    | database::skip_tapos_check
                    | database::skip_merkle_check
                    ;

      generate_block( skip );

      // enable_fees() and change_fees() modifies DB directly, and results will be overwritten by block generation
      // so we have to do it every time we stop generating/popping blocks and start doing tx's
      enable_fees();
      /*
      change_fees({
                     limit_order_create_operation::fee_parameters_type { order_create_fee },
                     limit_order_cancel_operation::fee_parameters_type { order_cancel_fee }
                  });
      */
      // C++ -- The above commented out statement doesn't work, I don't know why
      // so we will use the following rather lengthy initialization instead
      {
         fee_parameters::flat_set_type new_fees;
         {
            limit_order_create_operation::fee_parameters_type create_fee_params;
            create_fee_params.fee = order_create_fee;
            new_fees.insert( create_fee_params );
         }
         {
            limit_order_cancel_operation::fee_parameters_type cancel_fee_params;
            cancel_fee_params.fee = order_cancel_fee;
            new_fees.insert( cancel_fee_params );
         }
         change_fees( new_fees );
      }

      // Alice creates order
      // Bob creates order which doesn't match

      // AAAAGGHH create_sell_order reads trx.expiration #469
      set_expiration( db, trx );

      // Check non-overlapping

      limit_order_id_type ao1_id = create_sell_order( alice_id, asset(1000), asset(1000, usd_id) )->get_id();
      limit_order_id_type bo1_id = create_sell_order(   bob_id, asset(500, usd_id), asset(1000) )->get_id();

      BOOST_CHECK_EQUAL( get_balance( alice_id, core_id ), alice_b0 - 1000 - order_create_fee );
      BOOST_CHECK_EQUAL( get_balance( alice_id,  usd_id ), alice_b0 );
      BOOST_CHECK_EQUAL( get_balance(   bob_id, core_id ), bob_b0 - order_create_fee );
      BOOST_CHECK_EQUAL( get_balance(   bob_id,  usd_id ), bob_b0 - 500 );

      // Bob cancels order
      cancel_limit_order( bo1_id(db) );

      BOOST_CHECK_EQUAL( get_balance( alice_id, core_id ), alice_b0 - 1000 - order_create_fee );
      BOOST_CHECK_EQUAL( get_balance( alice_id,  usd_id ), alice_b0 );
      BOOST_CHECK_EQUAL( get_balance(   bob_id, core_id ), bob_b0 - order_cancel_fee );
      BOOST_CHECK_EQUAL( get_balance(   bob_id,  usd_id ), bob_b0 );

      // Alice cancels order
      cancel_limit_order( ao1_id(db) );

      BOOST_CHECK_EQUAL( get_balance( alice_id, core_id ), alice_b0 - order_cancel_fee );
      BOOST_CHECK_EQUAL( get_balance( alice_id,  usd_id ), alice_b0 );
      BOOST_CHECK_EQUAL( get_balance(   bob_id, core_id ), bob_b0 - order_cancel_fee );
      BOOST_CHECK_EQUAL( get_balance(   bob_id,  usd_id ), bob_b0 );

      // Check partial fill
      const limit_order_object* ao2 = create_sell_order( alice_id, asset(1000), asset(200, usd_id) );
      const limit_order_object* bo2 = create_sell_order(   bob_id, asset(100, usd_id), asset(500) );

      BOOST_CHECK( ao2 != nullptr );
      BOOST_CHECK( bo2 == nullptr );

      BOOST_CHECK_EQUAL( get_balance( alice_id, core_id ), alice_b0 - order_cancel_fee - order_create_fee - 1000 );
      BOOST_CHECK_EQUAL( get_balance( alice_id,  usd_id ), alice_b0 + 100 );
      BOOST_CHECK_EQUAL( get_balance(   bob_id, core_id ),   bob_b0 - order_cancel_fee - order_create_fee + 500 );
      BOOST_CHECK_EQUAL( get_balance(   bob_id,  usd_id ),   bob_b0 - 100 );

      // cancel Alice order, show that entire deferred_fee was consumed by partial match
      cancel_limit_order( *ao2 );

      BOOST_CHECK_EQUAL( get_balance( alice_id, core_id ), alice_b0 - order_cancel_fee - order_create_fee - 500 - order_cancel_fee );
      BOOST_CHECK_EQUAL( get_balance( alice_id,  usd_id ), alice_b0 + 100 );
      BOOST_CHECK_EQUAL( get_balance(   bob_id, core_id ),   bob_b0 - order_cancel_fee - order_create_fee + 500 );
      BOOST_CHECK_EQUAL( get_balance(   bob_id,  usd_id ),   bob_b0 - 100 );

      // TODO: Check multiple fill
      // there really should be a test case involving Alice creating multiple orders matched by single Bob order
      // but we'll save that for future cleanup

      // undo above tx's and reset
      generate_block( skip );
      db.pop_block();
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( fee_refund_original_asset )
{
   try
   {
      ACTORS((alice)(bob)(izzy));

      int64_t alice_b0 = 1000000, bob_b0 = 1000000;
      int64_t pool_0 = 1000000, accum_0 = 0;

      transfer( account_id_type(), alice_id, asset(alice_b0) );
      transfer( account_id_type(), bob_id, asset(bob_b0) );

      asset_id_type core_id = asset_id_type();
      int64_t cer_core_amount = 1801;
      int64_t cer_usd_amount = 3;
      price tmp_cer( asset( cer_core_amount ), asset( cer_usd_amount, asset_id_type(1) ) );
      const auto& usd_obj = create_user_asset( "IZZYUSD", izzy_id(db), charge_market_fee, tmp_cer );
      asset_id_type usd_id = usd_obj.get_id();
      issue_ua( alice_id, asset( alice_b0, usd_id ) );
      issue_ua( bob_id, asset( bob_b0, usd_id ) );

      fund_fee_pool( council_account( db ), usd_obj, pool_0 );

      int64_t order_create_fee = 547;
      int64_t order_cancel_fee;
      int64_t order_cancel_fee1 = 139;
      int64_t order_cancel_fee2 = 829;

      uint32_t skip = database::skip_validator_signature
                    | database::skip_transaction_signatures
                    | database::skip_transaction_dupe_check
                    | database::skip_block_size_check
                    | database::skip_tapos_check
                    | database::skip_merkle_check
                    ;

      generate_block( skip );

      fee_parameters::flat_set_type new_fees;
      fee_parameters::flat_set_type new_fees1;
      fee_parameters::flat_set_type new_fees2;
      {
         limit_order_create_operation::fee_parameters_type create_fee_params;
         create_fee_params.fee = order_create_fee;
         new_fees1.insert( create_fee_params );
         new_fees2.insert( create_fee_params );
      }
      {
         limit_order_cancel_operation::fee_parameters_type cancel_fee_params;
         cancel_fee_params.fee = order_cancel_fee1;
         new_fees1.insert( cancel_fee_params );
      }
      {
         limit_order_cancel_operation::fee_parameters_type cancel_fee_params;
         cancel_fee_params.fee = order_cancel_fee2;
         new_fees2.insert( cancel_fee_params );
      }
      {
         transfer_operation::fee_parameters_type transfer_fee_params;
         transfer_fee_params.fee = 0;
         transfer_fee_params.price_per_kbyte = 0;
         new_fees1.insert( transfer_fee_params );
         new_fees2.insert( transfer_fee_params );
      }

      for( int i=0; i<4; i++ )
      {
         bool expire_order = ( i % 2 != 0 );
         bool high_cancel_fee = ( i % 4 >= 2 );
         idump( (expire_order)(high_cancel_fee) );

         if( high_cancel_fee )
         {
            new_fees = new_fees2;
            order_cancel_fee = order_cancel_fee2;
         }
         else
         {
            new_fees = new_fees1;
            order_cancel_fee = order_cancel_fee1;
         }

         int64_t usd_create_fee = order_create_fee * cer_usd_amount / cer_core_amount;
         if( usd_create_fee * cer_core_amount != order_create_fee * cer_usd_amount ) usd_create_fee += 1;
         int64_t usd_cancel_fee = order_cancel_fee * cer_usd_amount / cer_core_amount;
         if( usd_cancel_fee * cer_core_amount != order_cancel_fee * cer_usd_amount ) usd_cancel_fee += 1;
         int64_t core_create_fee = usd_create_fee * cer_core_amount / cer_usd_amount;
         int64_t core_cancel_fee = usd_cancel_fee * cer_core_amount / cer_usd_amount;
         BOOST_CHECK( core_cancel_fee >= order_cancel_fee );

         BOOST_TEST_MESSAGE( "Start" );

         // enable_fees() and change_fees() modifies DB directly, and results will be overwritten by block generation
         // so we have to do it every time we stop generating/popping blocks and start doing tx's
         enable_fees();
         change_fees( new_fees );

         // AAAAGGHH create_sell_order reads trx.expiration #469
         set_expiration( db, trx );

         // prepare params
         uint32_t blocks_generated = 0;
         time_point_sec max_exp = time_point_sec::maximum();
         time_point_sec exp = db.head_block_time(); // order will be accepted when pushing trx then expired at current block
         price cer = usd_id( db ).options.core_exchange_rate;
         const auto* usd_stat = &usd_id( db ).dynamic_asset_data_id( db );

         // balance data
         int64_t alice_bc = alice_b0, bob_bc = bob_b0; // core balance
         int64_t alice_bu = alice_b0, bob_bu = bob_b0; // usd balance
         int64_t pool_b = pool_0, accum_b = accum_0;

         // refund data
         int64_t core_fee_refund_core = order_create_fee;
         int64_t core_fee_refund_usd = 0;
         int64_t usd_fee_refund_core = 0;
         int64_t usd_fee_refund_usd = usd_create_fee;
         int64_t accum_on_new = 0;
         int64_t accum_on_fill = usd_create_fee;
         int64_t pool_refund = core_create_fee;

         // Check non-overlapping
         // Alice creates order
         // Bob creates order which doesn't match
         BOOST_TEST_MESSAGE( "Creating non-overlapping orders" );
         BOOST_TEST_MESSAGE( "Creating ao1" );
         limit_order_id_type ao1_id = create_sell_order( alice_id, asset(1000), asset(1000, usd_id), exp )->get_id();

         alice_bc -= order_create_fee;
         alice_bc -= 1000;

         BOOST_CHECK_EQUAL( get_balance( alice_id, core_id ), alice_bc );
         BOOST_CHECK_EQUAL( get_balance( alice_id,  usd_id ), alice_bu );
         BOOST_CHECK_EQUAL( get_balance(   bob_id, core_id ), bob_bc );
         BOOST_CHECK_EQUAL( get_balance(   bob_id,  usd_id ), bob_bu );
         BOOST_CHECK_EQUAL( usd_stat->fee_pool.value,          pool_b );
         BOOST_CHECK_EQUAL( usd_stat->accumulated_fees.value,  accum_b );

         // Alice cancels order
         if( !expire_order )
         {
            BOOST_TEST_MESSAGE( "Cancel order ao1" );
            cancel_limit_order( ao1_id(db) );
         }
         else
         {
            BOOST_TEST_MESSAGE( "Order ao1 expired" );
            // empty accounts before generate block, to test if it will fail when charging order cancel fee
            transfer( alice_id, account_id_type(), asset(alice_bc, core_id) );
            transfer( alice_id, account_id_type(), asset(alice_bu,  usd_id) );
            transfer(   bob_id, account_id_type(), asset(  bob_bc, core_id) );
            transfer(   bob_id, account_id_type(), asset(  bob_bu,  usd_id) );
            // generate a new block so one or more order will expire
            generate_block( skip );
            ++blocks_generated;
            enable_fees();
            change_fees( new_fees );
            set_expiration( db, trx );
            exp = db.head_block_time();
            usd_stat = &usd_id( db ).dynamic_asset_data_id( db );
            // restore account balances
            transfer( account_id_type(), alice_id, asset(alice_bc, core_id) );
            transfer( account_id_type(), alice_id, asset(alice_bu,  usd_id) );
            transfer( account_id_type(),   bob_id, asset(  bob_bc, core_id) );
            transfer( account_id_type(),   bob_id, asset(  bob_bu,  usd_id) );
         }


         if( !expire_order )
            alice_bc -= order_cancel_fee; // manual cancellation always need a fee
         else
         {
            // charge a cancellation fee in core, capped by deffered_fee which is order_create_fee
            alice_bc -= std::min( order_cancel_fee, order_create_fee );
         }
         alice_bc += 1000;
         alice_bc += core_fee_refund_core;
         alice_bu += core_fee_refund_usd;

         BOOST_CHECK_EQUAL( get_balance( alice_id, core_id ), alice_bc );
         BOOST_CHECK_EQUAL( get_balance( alice_id,  usd_id ), alice_bu );
         BOOST_CHECK_EQUAL( get_balance(   bob_id, core_id ), bob_bc );
         BOOST_CHECK_EQUAL( get_balance(   bob_id,  usd_id ), bob_bu );
         BOOST_CHECK_EQUAL( usd_stat->fee_pool.value,          pool_b );
         BOOST_CHECK_EQUAL( usd_stat->accumulated_fees.value,  accum_b );

         BOOST_TEST_MESSAGE( "Creating bo1" );
         limit_order_id_type bo1_id = create_sell_order(   bob_id, asset(500, usd_id), asset(1000), exp, cer )->get_id();

         bob_bu -= usd_create_fee;
         bob_bu -= 500;
         pool_b -= core_create_fee;
         accum_b += accum_on_new;

         BOOST_CHECK_EQUAL( get_balance( alice_id, core_id ), alice_bc );
         BOOST_CHECK_EQUAL( get_balance( alice_id,  usd_id ), alice_bu );
         BOOST_CHECK_EQUAL( get_balance(   bob_id, core_id ), bob_bc );
         BOOST_CHECK_EQUAL( get_balance(   bob_id,  usd_id ), bob_bu );
         BOOST_CHECK_EQUAL( usd_stat->fee_pool.value,          pool_b );
         BOOST_CHECK_EQUAL( usd_stat->accumulated_fees.value,  accum_b );

         // Bob cancels order
         if( !expire_order )
         {
            BOOST_TEST_MESSAGE( "Cancel order bo1" );
            cancel_limit_order( bo1_id(db) );
         }
         else
         {
            BOOST_TEST_MESSAGE( "Order bo1 expired" );
            // empty accounts before generate block, to test if it will fail when charging order cancel fee
            transfer( alice_id, account_id_type(), asset(alice_bc, core_id) );
            transfer( alice_id, account_id_type(), asset(alice_bu,  usd_id) );
            transfer(   bob_id, account_id_type(), asset(  bob_bc, core_id) );
            transfer(   bob_id, account_id_type(), asset(  bob_bu,  usd_id) );
            // generate a new block so one or more order will expire
            generate_block( skip );
            ++blocks_generated;
            enable_fees();
            change_fees( new_fees );
            set_expiration( db, trx );
            exp = db.head_block_time();
            usd_stat = &usd_id( db ).dynamic_asset_data_id( db );
            // restore account balances
            transfer( account_id_type(), alice_id, asset(alice_bc, core_id) );
            transfer( account_id_type(), alice_id, asset(alice_bu,  usd_id) );
            transfer( account_id_type(),   bob_id, asset(  bob_bc, core_id) );
            transfer( account_id_type(),   bob_id, asset(  bob_bu,  usd_id) );
         }

         if( !expire_order )
            bob_bc -= order_cancel_fee; // manual cancellation always need a fee
         else
         {
            // when expired, should have core_create_fee in deferred, usd_create_fee in deferred_paid

            // charge a cancellation fee in core from fee_pool, capped by deffered
            int64_t capped_core_cancel_fee = std::min( order_cancel_fee, core_create_fee );
            pool_b -= capped_core_cancel_fee;

            // charge a coresponding cancellation fee in usd from deffered_paid, round up, capped
            int64_t capped_usd_cancel_fee = capped_core_cancel_fee * usd_create_fee / core_create_fee;
            if( capped_usd_cancel_fee * core_create_fee != capped_core_cancel_fee * usd_create_fee )
               capped_usd_cancel_fee += 1;
            if( capped_usd_cancel_fee > usd_create_fee )
               capped_usd_cancel_fee = usd_create_fee;
            bob_bu -= capped_usd_cancel_fee;

            // cancellation fee goes to accumulated fees
            accum_b += capped_usd_cancel_fee;
         }
         bob_bc += usd_fee_refund_core;
         bob_bu += 500;
         bob_bu += usd_fee_refund_usd;
         pool_b += pool_refund; // bo1

         BOOST_CHECK_EQUAL( get_balance( alice_id, core_id ), alice_bc );
         BOOST_CHECK_EQUAL( get_balance( alice_id,  usd_id ), alice_bu );
         BOOST_CHECK_EQUAL( get_balance(   bob_id, core_id ), bob_bc );
         BOOST_CHECK_EQUAL( get_balance(   bob_id,  usd_id ), bob_bu );
         BOOST_CHECK_EQUAL( usd_stat->fee_pool.value,          pool_b );
         BOOST_CHECK_EQUAL( usd_stat->accumulated_fees.value,  accum_b );


         // Check partial fill
         BOOST_TEST_MESSAGE( "Creating ao2, then be partially filled by bo2" );
         const limit_order_object* ao2 = create_sell_order( alice_id, asset(1000), asset(200, usd_id), exp, cer );
         const limit_order_id_type ao2id = ao2->get_id();
         const limit_order_object* bo2 = create_sell_order(   bob_id, asset(100, usd_id), asset(500) );

         BOOST_CHECK( db.find( ao2id ) != nullptr );
         BOOST_CHECK( bo2 == nullptr );

         // data after order created
         alice_bc -= 1000;
         alice_bu -= usd_create_fee;
         pool_b -= core_create_fee;
         accum_b += accum_on_new;
         bob_bc -= order_create_fee;
         bob_bu -= 100;

         // data after order filled
         alice_bu += 100;
         bob_bc += 500;
         accum_b += accum_on_fill; // ao2

         BOOST_CHECK_EQUAL( get_balance( alice_id, core_id ), alice_bc );
         BOOST_CHECK_EQUAL( get_balance( alice_id,  usd_id ), alice_bu );
         BOOST_CHECK_EQUAL( get_balance(   bob_id, core_id ), bob_bc );
         BOOST_CHECK_EQUAL( get_balance(   bob_id,  usd_id ), bob_bu );
         BOOST_CHECK_EQUAL( usd_stat->fee_pool.value,          pool_b );
         BOOST_CHECK_EQUAL( usd_stat->accumulated_fees.value,  accum_b );

         // cancel Alice order, show that entire deferred_fee was consumed by partial match
         if( !expire_order )
         {
            BOOST_TEST_MESSAGE( "Cancel order ao2" );
            cancel_limit_order( *ao2 );
         }
         else
         {
            BOOST_TEST_MESSAGE( "Order ao2 expired" );
            // empty accounts before generate block, to test if it will fail when charging order cancel fee
            transfer( alice_id, account_id_type(), asset(alice_bc, core_id) );
            transfer( alice_id, account_id_type(), asset(alice_bu,  usd_id) );
            transfer(   bob_id, account_id_type(), asset(  bob_bc, core_id) );
            transfer(   bob_id, account_id_type(), asset(  bob_bu,  usd_id) );
            // generate a new block so one or more order will expire
            generate_block( skip );
            ++blocks_generated;
            enable_fees();
            change_fees( new_fees );
            set_expiration( db, trx );
            exp = db.head_block_time();
            usd_stat = &usd_id( db ).dynamic_asset_data_id( db );
            // restore account balances
            transfer( account_id_type(), alice_id, asset(alice_bc, core_id) );
            transfer( account_id_type(), alice_id, asset(alice_bu,  usd_id) );
            transfer( account_id_type(),   bob_id, asset(  bob_bc, core_id) );
            transfer( account_id_type(),   bob_id, asset(  bob_bu,  usd_id) );
         }


         if( !expire_order )
            alice_bc -= order_cancel_fee;
         // else do nothing, when partially filled order expired, order cancel fee is capped at 0
         alice_bc += 500;

         BOOST_CHECK_EQUAL( get_balance( alice_id, core_id ), alice_bc );
         BOOST_CHECK_EQUAL( get_balance( alice_id,  usd_id ), alice_bu );
         BOOST_CHECK_EQUAL( get_balance(   bob_id, core_id ), bob_bc );
         BOOST_CHECK_EQUAL( get_balance(   bob_id,  usd_id ), bob_bu );
         BOOST_CHECK_EQUAL( usd_stat->fee_pool.value,          pool_b );
         BOOST_CHECK_EQUAL( usd_stat->accumulated_fees.value,  accum_b );

         // Check multiple fill
         // Alice creating multiple orders
         BOOST_TEST_MESSAGE( "Creating ao31-ao35" );
         const limit_order_object* ao31 = create_sell_order( alice_id, asset(1000), asset(200, usd_id), max_exp, cer );
         const limit_order_object* ao32 = create_sell_order( alice_id, asset(1000), asset(2000, usd_id), max_exp, cer );
         const limit_order_object* ao33 = create_sell_order( alice_id, asset(1000), asset(200, usd_id), max_exp, cer );
         const limit_order_object* ao34 = create_sell_order( alice_id, asset(1000), asset(200, usd_id), max_exp, cer );
         const limit_order_object* ao35 = create_sell_order( alice_id, asset(1000), asset(200, usd_id), max_exp, cer );

         const limit_order_id_type ao31id = ao31->get_id();
         const limit_order_id_type ao32id = ao32->get_id();
         const limit_order_id_type ao33id = ao33->get_id();
         const limit_order_id_type ao34id = ao34->get_id();
         const limit_order_id_type ao35id = ao35->get_id();

         alice_bc -= 1000 * 5;
         alice_bu -= usd_create_fee * 5;
         pool_b -= core_create_fee * 5;
         accum_b += accum_on_new * 5;

         BOOST_CHECK_EQUAL( get_balance( alice_id, core_id ), alice_bc );
         BOOST_CHECK_EQUAL( get_balance( alice_id,  usd_id ), alice_bu );
         BOOST_CHECK_EQUAL( get_balance(   bob_id, core_id ), bob_bc );
         BOOST_CHECK_EQUAL( get_balance(   bob_id,  usd_id ), bob_bu );
         BOOST_CHECK_EQUAL( usd_stat->fee_pool.value,          pool_b );
         BOOST_CHECK_EQUAL( usd_stat->accumulated_fees.value,  accum_b );

         // Bob creating an order matching multiple Alice's orders
         BOOST_TEST_MESSAGE( "Creating bo31, completely fill ao31 and ao33, partially fill ao34" );
         const limit_order_object* bo31 = create_sell_order(   bob_id, asset(500, usd_id), asset(2500), exp );

         BOOST_CHECK( db.find( ao31id ) == nullptr );
         BOOST_CHECK( db.find( ao32id ) != nullptr );
         BOOST_CHECK( db.find( ao33id ) == nullptr );
         BOOST_CHECK( db.find( ao34id ) != nullptr );
         BOOST_CHECK( db.find( ao35id ) != nullptr );
         BOOST_CHECK( bo31 == nullptr );

         // data after order created
         bob_bc -= order_create_fee;
         bob_bu -= 500;

         // data after order filled
         alice_bu += 500;
         bob_bc += 2500;
         accum_b += accum_on_fill * 3; // ao31, ao33, ao34

         BOOST_CHECK_EQUAL( get_balance( alice_id, core_id ), alice_bc );
         BOOST_CHECK_EQUAL( get_balance( alice_id,  usd_id ), alice_bu );
         BOOST_CHECK_EQUAL( get_balance(   bob_id, core_id ), bob_bc );
         BOOST_CHECK_EQUAL( get_balance(   bob_id,  usd_id ), bob_bu );
         BOOST_CHECK_EQUAL( usd_stat->fee_pool.value,          pool_b );
         BOOST_CHECK_EQUAL( usd_stat->accumulated_fees.value,  accum_b );

         // Bob creating an order matching multiple Alice's orders
         BOOST_TEST_MESSAGE( "Creating bo32, completely fill partially filled ao34 and new ao35, leave on market" );
         const limit_order_object* bo32 = create_sell_order(   bob_id, asset(500, usd_id), asset(2500), exp );

         BOOST_CHECK( db.find( ao31id ) == nullptr );
         BOOST_CHECK( db.find( ao32id ) != nullptr );
         BOOST_CHECK( db.find( ao33id ) == nullptr );
         BOOST_CHECK( db.find( ao34id ) == nullptr );
         BOOST_CHECK( db.find( ao35id ) == nullptr );
         BOOST_CHECK( bo32 != nullptr );

         // data after order created
         bob_bc -= order_create_fee;
         bob_bu -= 500;

         // data after order filled
         alice_bu += 300;
         bob_bc += 1500;
         accum_b += accum_on_fill; // ao35

         BOOST_CHECK_EQUAL( get_balance( alice_id, core_id ), alice_bc );
         BOOST_CHECK_EQUAL( get_balance( alice_id,  usd_id ), alice_bu );
         BOOST_CHECK_EQUAL( get_balance(   bob_id, core_id ), bob_bc );
         BOOST_CHECK_EQUAL( get_balance(   bob_id,  usd_id ), bob_bu );
         BOOST_CHECK_EQUAL( usd_stat->fee_pool.value,          pool_b );
         BOOST_CHECK_EQUAL( usd_stat->accumulated_fees.value,  accum_b );

         // cancel Bob order, show that entire deferred_fee was consumed by partial match
         if( !expire_order )
         {
            BOOST_TEST_MESSAGE( "Cancel order bo32" );
            cancel_limit_order( *bo32 );
         }
         else
         {
            BOOST_TEST_MESSAGE( "Order bo32 expired" );
            // empty accounts before generate block, to test if it will fail when charging order cancel fee
            transfer( alice_id, account_id_type(), asset(alice_bc, core_id) );
            transfer( alice_id, account_id_type(), asset(alice_bu,  usd_id) );
            transfer(   bob_id, account_id_type(), asset(  bob_bc, core_id) );
            transfer(   bob_id, account_id_type(), asset(  bob_bu,  usd_id) );
            // generate a new block so one or more order will expire
            generate_block( skip );
            ++blocks_generated;
            enable_fees();
            change_fees( new_fees );
            set_expiration( db, trx );
            exp = db.head_block_time();
            usd_stat = &usd_id( db ).dynamic_asset_data_id( db );
            // restore account balances
            transfer( account_id_type(), alice_id, asset(alice_bc, core_id) );
            transfer( account_id_type(), alice_id, asset(alice_bu,  usd_id) );
            transfer( account_id_type(),   bob_id, asset(  bob_bc, core_id) );
            transfer( account_id_type(),   bob_id, asset(  bob_bu,  usd_id) );
         }

         if( !expire_order )
            bob_bc -= order_cancel_fee;
         // else do nothing, when partially filled order expired, order cancel fee is capped at 0
         bob_bu += 200;

         BOOST_CHECK_EQUAL( get_balance( alice_id, core_id ), alice_bc );
         BOOST_CHECK_EQUAL( get_balance( alice_id,  usd_id ), alice_bu );
         BOOST_CHECK_EQUAL( get_balance(   bob_id, core_id ), bob_bc );
         BOOST_CHECK_EQUAL( get_balance(   bob_id,  usd_id ), bob_bu );
         BOOST_CHECK_EQUAL( usd_stat->fee_pool.value,          pool_b );
         BOOST_CHECK_EQUAL( usd_stat->accumulated_fees.value,  accum_b );

         // cancel Alice order, will refund
         BOOST_TEST_MESSAGE( "Cancel order ao32" );
         cancel_limit_order( ao32id( db ) );

         alice_bc -= order_cancel_fee;
         alice_bc += 1000;
         alice_bc += usd_fee_refund_core;
         alice_bu += usd_fee_refund_usd;
         pool_b += pool_refund;

         BOOST_CHECK_EQUAL( get_balance( alice_id, core_id ), alice_bc );
         BOOST_CHECK_EQUAL( get_balance( alice_id,  usd_id ), alice_bu );
         BOOST_CHECK_EQUAL( get_balance(   bob_id, core_id ), bob_bc );
         BOOST_CHECK_EQUAL( get_balance(   bob_id,  usd_id ), bob_bu );
         BOOST_CHECK_EQUAL( usd_stat->fee_pool.value,          pool_b );
         BOOST_CHECK_EQUAL( usd_stat->accumulated_fees.value,  accum_b );

         // undo above tx's and reset
         BOOST_TEST_MESSAGE( "Clean up" );
         generate_block( skip );
         ++blocks_generated;
         while( blocks_generated > 0 )
         {
            db.pop_block();
            --blocks_generated;
         }
      }
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( stealth_fba_test )
{
   try
   {
      ACTORS( (alice)(bob)(chloe)(dan)(izzy)(philbin)(tom) );
      upgrade_to_lifetime_member(philbin_id);

      // Philbin (registrar who registers Rex)

      // Izzy (initial issuer of stealth asset, will later transfer to Tom)
      // Alice, Bob, Chloe, Dan (ABCD)
      // Rex (recycler -- buyback account for stealth asset)
      // Tom (owner of stealth asset who will be set as top_n authority)

      // Izzy creates STEALTH
      asset_id_type stealth_id = create_user_asset( "STEALTH", izzy_id(db),
         disable_confidential | transfer_restricted | override_authority | white_list | charge_market_fee ).get_id();

      /*
      // this is disabled because it doesn't work, our modify() is probably being overwritten by undo

      //
      // Init blockchain with stealth ID's
      // On a real chain, this would be done with #define GRAPHENE_FBA_STEALTH_DESIGNATED_ASSET
      // causing the designated_asset fields of these objects to be set at genesis, but for
      // this test we modify the db directly.
      //
      auto set_fba_asset = [&]( uint64_t fba_acc_id, asset_id_type asset_id )
      {
         db.modify( fba_accumulator_id_type(fba_acc_id)(db), [&]( fba_accumulator_object& fba )
         {
            fba.designated_asset = asset_id;
         } );
      };

      set_fba_asset( fba_accumulator_id_transfer_to_blind  , stealth_id );
      set_fba_asset( fba_accumulator_id_blind_transfer     , stealth_id );
      set_fba_asset( fba_accumulator_id_transfer_from_blind, stealth_id );
      */

      // Izzy kills some permission bits (this somehow happened to the real STEALTH in production)
      {
         asset_update_operation update_op;
         update_op.issuer = izzy_id;
         update_op.asset_to_update = stealth_id;
         asset_options new_options;
         new_options = stealth_id(db).options;
         new_options.issuer_permissions = charge_market_fee;
         new_options.flags = disable_confidential | transfer_restricted | override_authority | white_list | charge_market_fee;
         // after fixing #579 you should be able to delete the following line
         new_options.core_exchange_rate = price( asset( 1, stealth_id ), asset( 1, asset_id_type() ) );
         update_op.new_options = new_options;
         signed_transaction tx;
         tx.operations.push_back( update_op );
         set_expiration( db, tx );
         sign( tx, izzy_private_key );
         PUSH_TX( db, tx );
      }

      // Izzy transfers issuer duty to Tom
      {
         asset_update_issuer_operation update_op;
         update_op.issuer = izzy_id;
         update_op.asset_to_update = stealth_id;
         update_op.new_issuer = tom_id;
         signed_transaction tx;
         tx.operations.push_back( update_op );
         set_expiration( db, tx );
         sign( tx, izzy_private_key );
         PUSH_TX( db, tx );
      }

      // Tom re-enables the permission bits to clear the flags, then clears them again
      // Allowed when current_supply == 0
      {
         asset_update_operation update_op;
         update_op.issuer = tom_id;
         update_op.asset_to_update = stealth_id;
         asset_options new_options;
         new_options = stealth_id(db).options;
         new_options.issuer_permissions = new_options.flags | charge_market_fee;
         update_op.new_options = new_options;
         signed_transaction tx;
         // enable perms is one op
         tx.operations.push_back( update_op );

         new_options.issuer_permissions = charge_market_fee;
         new_options.flags = charge_market_fee;
         update_op.new_options = new_options;
         // reset wrongly set flags and reset permissions can be done in a single op
         tx.operations.push_back( update_op );

         set_expiration( db, tx );
         sign( tx, tom_private_key );
         PUSH_TX( db, tx );
      }

      // Philbin registers Rex who will be the asset's buyback, including sig from the new issuer (Tom)
      account_id_type rex_id;
      {
         buyback_account_options bbo;
         bbo.asset_to_buy = stealth_id;
         bbo.asset_to_buy_issuer = tom_id;
         bbo.markets.emplace( asset_id_type() );
         account_create_operation create_op = make_account( "rex" );
         create_op.registrar = philbin_id;
         create_op.extensions.value.buyback_options = bbo;
         create_op.owner = authority::null_authority();
         create_op.active = authority::null_authority();

         signed_transaction tx;
         tx.operations.push_back( create_op );
         set_expiration( db, tx );
         sign( tx, philbin_private_key );
         sign( tx, tom_private_key );

         processed_transaction ptx = PUSH_TX( db, tx );
         rex_id = ptx.operation_results.back().get< object_id_type >();
      }

      // Tom issues some asset to Alice and Bob
      set_expiration( db, trx );  // #11
      issue_ua( alice_id, asset( 1000, stealth_id ) );
      issue_ua(   bob_id, asset( 1000, stealth_id ) );

      // Tom sets his authority to the top_n of the asset
      {
         top_holders_special_authority top2;
         top2.num_top_holders = 2;
         top2.asset = stealth_id;

         account_update_operation op;
         op.account = tom_id;
         op.extensions.value.active_special_authority = top2;
         op.extensions.value.owner_special_authority = top2;

         signed_transaction tx;
         tx.operations.push_back( op );

         set_expiration( db, tx );
         sign( tx, tom_private_key );

         PUSH_TX( db, tx );
      }

      // Wait until the next maintenance interval for top_n to take effect
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

      // Do a blind op to add some fees to the pool.
      fund( chloe_id(db), asset( 100000, asset_id_type() ) );

      auto create_transfer_to_blind = [&]( account_id_type account, asset amount, const std::string& key ) -> transfer_to_blind_operation
      {
         fc::ecc::private_key blind_key = fc::ecc::private_key::regenerate( fc::sha256::hash( key+"-privkey" ) );
         public_key_type blind_pub = blind_key.get_public_key();

         fc::sha256 secret = fc::sha256::hash( key+"-secret" );
         fc::sha256 nonce = fc::sha256::hash( key+"-nonce" );

         transfer_to_blind_operation op;
         blind_output blind_out;
         blind_out.owner = authority( 1, blind_pub, 1 );
         blind_out.commitment = fc::ecc::blind( secret, amount.amount.value );
         blind_out.range_proof = fc::ecc::range_proof_sign( 0, blind_out.commitment, secret, nonce, 0, 0, amount.amount.value );

         op.amount = amount;
         op.from = account;
         op.blinding_factor = fc::ecc::blind_sum( {secret}, 1 );
         op.outputs = {blind_out};

         return op;
      };

      {
         transfer_to_blind_operation op = create_transfer_to_blind( chloe_id, asset( 5000, asset_id_type() ), "chloe-key" );
         op.fee = asset( 1000, asset_id_type() );

         signed_transaction tx;
         tx.operations.push_back( op );
         set_expiration( db, tx );
         sign( tx, chloe_private_key );

         PUSH_TX( db, tx );
      }

      // wait until next maint interval
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

      idump( ( get_operation_history( chloe_id ) ) );
      idump( ( get_operation_history( rex_id ) ) );
      idump( ( get_operation_history( tom_id ) ) );
   }
   catch( const fc::exception& e )
   {
      elog( "caught exception ${e}", ("e", e.to_detail_string()) );
      throw;
   }
}

BOOST_AUTO_TEST_CASE( defaults_test )
{ try {
    fee_schedule schedule;
    const limit_order_create_operation::fee_parameters_type default_order_fee {};

    // no fees set yet -> default
    asset fee = schedule.calculate_fee( limit_order_create_operation() );
    BOOST_CHECK_EQUAL( (int64_t)default_order_fee.fee, fee.amount.value );

    limit_order_create_operation::fee_parameters_type new_order_fee; new_order_fee.fee = 123;
    // set fee + check
    schedule.parameters.insert( new_order_fee );
    fee = schedule.calculate_fee( limit_order_create_operation() );
    BOOST_CHECK_EQUAL( (int64_t)new_order_fee.fee, fee.amount.value );

    // bid_collateral fee defaults to call_order_update fee
    // call_order_update fee is unset -> default
    const call_order_update_operation::fee_parameters_type default_short_fee {};
    call_order_update_operation::fee_parameters_type new_short_fee; new_short_fee.fee = 123;
    fee = schedule.calculate_fee( bid_collateral_operation() );
    BOOST_CHECK_EQUAL( (int64_t)default_short_fee.fee, fee.amount.value );

    // set call_order_update fee + check bid_collateral fee
    schedule.parameters.insert( new_short_fee );
    fee = schedule.calculate_fee( bid_collateral_operation() );
    BOOST_CHECK_EQUAL( (int64_t)new_short_fee.fee, fee.amount.value );

    // set bid_collateral fee + check
    bid_collateral_operation::fee_parameters_type new_bid_fee; new_bid_fee.fee = 124;
    schedule.parameters.insert( new_bid_fee );
    fee = schedule.calculate_fee( bid_collateral_operation() );
    BOOST_CHECK_EQUAL( (int64_t)new_bid_fee.fee, fee.amount.value );
  }
  catch( const fc::exception& e )
  {
     elog( "caught exception ${e}", ("e", e.to_detail_string()) );
     throw;
  }
}

BOOST_AUTO_TEST_CASE( create_asset_fee_rounding )
{
   try
   {
      ACTORS((alice));

      transfer( council_account, alice_id, asset( 1000000 * asset::scaled_precision( asset_id_type()(db).precision ) ) );

      // make sure the database requires our fee to be nonzero
      enable_fees();

      const auto& fees = db.get_global_properties().parameters.get_current_fees();
      auto fees_to_pay = fees.get<asset_create_operation>();

      {
         signed_transaction tx;
         asset_create_operation op;
         op.issuer = alice_id;
         op.symbol = "ALICE";
         op.common_options.core_exchange_rate = asset( 1 ) / asset( 1, asset_id_type( 1 ) );
         op.fee = asset( (fees_to_pay.long_symbol + fees_to_pay.price_per_kbyte) & (~1) );
         tx.operations.push_back( op );
         set_expiration( db, tx );
         sign( tx, alice_private_key );
         PUSH_TX( db, tx );
      }

      verify_asset_supplies( db );

      {
         signed_transaction tx;
         asset_create_operation op;
         op.issuer = alice_id;
         op.symbol = "ALICE.ODD";
         op.common_options.core_exchange_rate = asset( 1 ) / asset( 1, asset_id_type( 1 ) );
         op.fee = asset((fees_to_pay.long_symbol + fees_to_pay.price_per_kbyte) | 1);
         tx.operations.push_back( op );
         set_expiration( db, tx );
         sign( tx, alice_private_key );
         PUSH_TX( db, tx );
      }

      verify_asset_supplies( db );
   }
   catch( const fc::exception& e )
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( issue_433_test )
{
   try
   {
      ACTORS((alice));

      auto& core = asset_id_type()(db);

      transfer( council_account, alice_id, asset( 1000000 * asset::scaled_precision( core.precision ) ) );

      const auto& myusd = create_user_asset( "MYUSD", alice, 0 );
      issue_ua( alice, myusd.amount( 2000000000 ) );

      // make sure the database requires our fee to be nonzero
      enable_fees();

      const auto& fees = db.get_global_properties().parameters.get_current_fees();
      const auto asset_create_fees = fees.get<asset_create_operation>();

      fund_fee_pool( alice, myusd, 5*asset_create_fees.long_symbol );

      asset_create_operation op;
      op.issuer = alice_id;
      op.symbol = "ALICE";
      op.common_options.core_exchange_rate = asset( 1 ) / asset( 1, asset_id_type( 1 ) );
      op.fee = myusd.amount( ((asset_create_fees.long_symbol + asset_create_fees.price_per_kbyte) & (~1)) );
      signed_transaction tx;
      tx.operations.push_back( op );
      set_expiration( db, tx );
      sign( tx, alice_private_key );
      PUSH_TX( db, tx );

      verify_asset_supplies( db );
   }
   catch( const fc::exception& e )
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( issue_433_indirect_test )
{
   try
   {
      ACTORS((alice));

      auto& core = asset_id_type()(db);

      transfer( council_account, alice_id, asset( 1000000 * asset::scaled_precision( core.precision ) ) );

      const auto& myusd = create_user_asset( "MYUSD", alice, 0 );
      issue_ua( alice, myusd.amount( 2000000000 ) );

      // make sure the database requires our fee to be nonzero
      enable_fees();

      const auto& fees = db.get_global_properties().parameters.get_current_fees();
      const auto asset_create_fees = fees.get<asset_create_operation>();

      fund_fee_pool( alice, myusd, 5*asset_create_fees.long_symbol );

      asset_create_operation op;
      op.issuer = alice_id;
      op.symbol = "ALICE";
      op.common_options.core_exchange_rate = asset( 1 ) / asset( 1, asset_id_type( 1 ) );
      op.fee = myusd.amount( ((asset_create_fees.long_symbol + asset_create_fees.price_per_kbyte) & (~1)) );

      const auto proposal_create_fees = fees.get<proposal_create_operation>();
      proposal_create_operation prop;
      prop.fee_paying_account = alice_id;
      prop.proposed_ops.emplace_back( op );
      prop.expiration_time =  db.head_block_time() + fc::days(1);
      prop.fee = asset( proposal_create_fees.fee + proposal_create_fees.price_per_kbyte );
      object_id_type proposal_id;
      {
         signed_transaction tx;
         tx.operations.push_back( prop );
         set_expiration( db, tx );
         sign( tx, alice_private_key );
         proposal_id = PUSH_TX( db, tx ).operation_results.front().get<object_id_type>();
      }
      const proposal_object& proposal = db.get<proposal_object>( proposal_id );

      const auto proposal_update_fees = fees.get<proposal_update_operation>();
      proposal_update_operation pup;
      pup.proposal = proposal.id;
      pup.fee_paying_account = alice_id;
      pup.active_approvals_to_add.insert(alice_id);
      pup.fee = asset( proposal_update_fees.fee + proposal_update_fees.price_per_kbyte );
      {
         signed_transaction tx;
         tx.operations.push_back( pup );
         set_expiration( db, tx );
         sign( tx, alice_private_key );
         PUSH_TX( db, tx );
      }

      verify_asset_supplies( db );
   }
   catch( const fc::exception& e )
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
