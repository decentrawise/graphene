#include <boost/test/unit_test.hpp>

#include <graphene/app/database_api.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/exceptions.hpp>

#include <iostream>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;


BOOST_FIXTURE_TEST_SUITE(smartcoin_tests, database_fixture)


BOOST_AUTO_TEST_CASE(maintenance_feed_cleanup)
{
   try
   {
      // Create 12 accounts to be validators under our control
      ACTORS( (validator0)(validator1)(validator2)(validator3)(validator4)(validator5)
                   (validator6)(validator7)(validator8)(validator9)(validator10)(validator11) );

      // Upgrade all accounts to LTM
      upgrade_to_lifetime_member(validator0_id);
      upgrade_to_lifetime_member(validator1_id);
      upgrade_to_lifetime_member(validator2_id);
      upgrade_to_lifetime_member(validator3_id);
      upgrade_to_lifetime_member(validator4_id);
      upgrade_to_lifetime_member(validator5_id);
      upgrade_to_lifetime_member(validator6_id);
      upgrade_to_lifetime_member(validator7_id);
      upgrade_to_lifetime_member(validator8_id);
      upgrade_to_lifetime_member(validator9_id);
      upgrade_to_lifetime_member(validator10_id);
      upgrade_to_lifetime_member(validator11_id);

      // Create all the validators
      const validator_id_type validator0_validator_id = create_validator(validator0_id, validator0_private_key).get_id();
      const validator_id_type validator1_validator_id = create_validator(validator1_id, validator1_private_key).get_id();
      const validator_id_type validator2_validator_id = create_validator(validator2_id, validator2_private_key).get_id();
      const validator_id_type validator3_validator_id = create_validator(validator3_id, validator3_private_key).get_id();
      const validator_id_type validator4_validator_id = create_validator(validator4_id, validator4_private_key).get_id();
      const validator_id_type validator5_validator_id = create_validator(validator5_id, validator5_private_key).get_id();
      const validator_id_type validator6_validator_id = create_validator(validator6_id, validator6_private_key).get_id();
      const validator_id_type validator7_validator_id = create_validator(validator7_id, validator7_private_key).get_id();
      const validator_id_type validator8_validator_id = create_validator(validator8_id, validator8_private_key).get_id();
      const validator_id_type validator9_validator_id = create_validator(validator9_id, validator9_private_key).get_id();
      const validator_id_type validator10_validator_id = create_validator(validator10_id, validator10_private_key).get_id();
      const validator_id_type validator11_validator_id = create_validator(validator11_id, validator11_private_key).get_id();

      // Create a vector with private key of all validators, will be used to activate 11 validators at a time
      const vector <fc::ecc::private_key> private_keys = {
            validator0_private_key,
            validator1_private_key,
            validator2_private_key,
            validator3_private_key,
            validator4_private_key,
            validator5_private_key,
            validator6_private_key,
            validator7_private_key,
            validator8_private_key,
            validator9_private_key,
            validator10_private_key
      };

      // create a map with account id and validator id of the first 11 validators
      const flat_map <account_id_type, validator_id_type> validator_map = {
         {validator0_id, validator0_validator_id},
         {validator1_id, validator1_validator_id},
         {validator2_id, validator2_validator_id},
         {validator3_id, validator3_validator_id},
         {validator4_id, validator4_validator_id},
         {validator5_id, validator5_validator_id},
         {validator6_id, validator6_validator_id},
         {validator7_id, validator7_validator_id},
         {validator8_id, validator8_validator_id},
         {validator9_id, validator9_validator_id},
         {validator10_id, validator10_validator_id}
      };

      // Create the asset
      const asset_id_type bit_usd_id = create_bitasset("USDBIT").get_id();

      // Update the asset to be fed by system validators
      asset_update_operation op;
      const asset_object &asset_obj = bit_usd_id(db);
      op.asset_to_update = bit_usd_id;
      op.issuer = asset_obj.issuer;
      op.new_options = asset_obj.options;
      op.new_options.flags &= validator_fed_asset;
      op.new_options.issuer_permissions &= validator_fed_asset;
      trx.operations.push_back(op);
      PUSH_TX(db, trx, ~0);
      generate_block();
      trx.clear();

      // Check current default validators, default chain is configured with 10 validators
      auto validators = db.get_global_properties().block_producers;
      BOOST_CHECK_EQUAL(validators.size(), INITIAL_VALIDATOR_COUNT);
      BOOST_CHECK_EQUAL(validators.begin()[0].instance.value, 1u);
      BOOST_CHECK_EQUAL(validators.begin()[1].instance.value, 2u);
      BOOST_CHECK_EQUAL(validators.begin()[2].instance.value, 3u);
      BOOST_CHECK_EQUAL(validators.begin()[3].instance.value, 4u);
      BOOST_CHECK_EQUAL(validators.begin()[4].instance.value, 5u);
      BOOST_CHECK_EQUAL(validators.begin()[5].instance.value, 6u);
      BOOST_CHECK_EQUAL(validators.begin()[6].instance.value, 7u);
      BOOST_CHECK_EQUAL(validators.begin()[7].instance.value, 8u);
      BOOST_CHECK_EQUAL(validators.begin()[8].instance.value, 9u);

      // We need to activate 11 validators by voting for each of them.
      // Each validator is voted with incremental stake so last validator created will be the ones with more votes

      // by default we have 9 validators, we need to vote for desired validator count (11) to increase them
      vote_for_delegates_and_validators(9, 11);

      int c = 0;
      for (auto l : validator_map) {
         // voting stake have step of 100
         // so vote_for_delegates_and_validators() with stake=10 does not affect the expected result
         int stake = 100 * (c + 1);
         transfer(council_account, l.first, asset(stake));
         {
            account_update_operation op;
            op.account = l.first;
            op.new_options = l.first(db).options;
            op.new_options->votes.insert(l.second(db).vote_id);
            op.new_options->num_validator = std::count_if(op.new_options->votes.begin(), op.new_options->votes.end(),
                                                        [](vote_id_type id) {
                                                           return id.type() == vote_id_type::validator;
                                                        });
            trx.operations.push_back(op);
            sign(trx, private_keys.at(c));
            PUSH_TX(db, trx);
            trx.clear();
         }
         ++c;
      }

      // Trigger the new validators
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

      // Check my validators are now in control of the system
      validators = db.get_global_properties().block_producers;
      BOOST_CHECK_EQUAL(validators.size(), 11u);
      BOOST_CHECK_EQUAL(validators.begin()[0].instance.value, 11u);
      BOOST_CHECK_EQUAL(validators.begin()[1].instance.value, 12u);
      BOOST_CHECK_EQUAL(validators.begin()[2].instance.value, 13u);
      BOOST_CHECK_EQUAL(validators.begin()[3].instance.value, 14u);
      BOOST_CHECK_EQUAL(validators.begin()[4].instance.value, 15u);
      BOOST_CHECK_EQUAL(validators.begin()[5].instance.value, 16u);
      BOOST_CHECK_EQUAL(validators.begin()[6].instance.value, 17u);
      BOOST_CHECK_EQUAL(validators.begin()[7].instance.value, 18u);
      BOOST_CHECK_EQUAL(validators.begin()[8].instance.value, 19u);
      BOOST_CHECK_EQUAL(validators.begin()[9].instance.value, 20u);
      BOOST_CHECK_EQUAL(validators.begin()[10].instance.value, 21u);

      // Adding 2 feeds with validators 0 and 1, checking if they get inserted
      const asset_object &core = asset_id_type()(db);
      price_feed feed;
      feed.settlement_price = bit_usd_id(db).amount(1) / core.amount(5);
      publish_feed(bit_usd_id(db), validator0_id(db), feed);

      asset_bitasset_data_object bitasset_data = bit_usd_id(db).bitasset_data(db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 1u);
      auto itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(itr[0].first.instance.value, 16u);

      feed.settlement_price = bit_usd_id(db).amount(2) / core.amount(5);
      publish_feed(bit_usd_id(db), validator1_id(db), feed);

      bitasset_data = bit_usd_id(db).bitasset_data(db);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 2u);
      BOOST_CHECK_EQUAL(itr[0].first.instance.value, 16u);
      BOOST_CHECK_EQUAL(itr[1].first.instance.value, 17u);

      // Activate validator11 with voting stake, will kick the validator with less votes(validator0) out of the active list
      transfer(council_account, validator11_id, asset(1200));
      set_expiration(db, trx);
      {
         account_update_operation op;
         op.account = validator11_id;
         op.new_options = validator11_id(db).options;
         op.new_options->votes.insert(validator11_validator_id(db).vote_id);
         op.new_options->num_validator = std::count_if(op.new_options->votes.begin(), op.new_options->votes.end(),
                                                     [](vote_id_type id) {
                                                        return id.type() == vote_id_type::validator;
                                                     });
         trx.operations.push_back(op);
         sign(trx, validator11_private_key);
         PUSH_TX(db, trx);
         trx.clear();
      }

      // Trigger new validator
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

      // Check block producer list now
      validators = db.get_global_properties().block_producers;
      BOOST_CHECK_EQUAL(validators.begin()[0].instance.value, 12u);
      BOOST_CHECK_EQUAL(validators.begin()[1].instance.value, 13u);
      BOOST_CHECK_EQUAL(validators.begin()[2].instance.value, 14u);
      BOOST_CHECK_EQUAL(validators.begin()[3].instance.value, 15u);
      BOOST_CHECK_EQUAL(validators.begin()[4].instance.value, 16u);
      BOOST_CHECK_EQUAL(validators.begin()[5].instance.value, 17u);
      BOOST_CHECK_EQUAL(validators.begin()[6].instance.value, 18u);
      BOOST_CHECK_EQUAL(validators.begin()[7].instance.value, 19u);
      BOOST_CHECK_EQUAL(validators.begin()[8].instance.value, 20u);
      BOOST_CHECK_EQUAL(validators.begin()[9].instance.value, 21u);
      BOOST_CHECK_EQUAL(validators.begin()[10].instance.value, 22u);

      // validator0 has been removed but it was a feeder before
      // Feed persist in the blockchain
      bitasset_data = bit_usd_id(db).bitasset_data(db);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 2u);
      BOOST_CHECK_EQUAL(itr[0].first.instance.value, 16u);

      // Expire feeds
      const auto feed_lifetime = bit_usd_id(db).bitasset_data(db).options.feed_lifetime_sec;
      generate_blocks(db.head_block_time() + feed_lifetime + 1);

      // Other validators add more feeds
      feed.settlement_price = bit_usd_id(db).amount(4) / core.amount(5);
      publish_feed(bit_usd_id(db), validator2_id(db), feed);
      feed.settlement_price = bit_usd_id(db).amount(3) / core.amount(5);
      publish_feed(bit_usd_id(db), validator3_id(db), feed);

      // Advancing to next maint
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

      //  All expired feeds are deleted
      bitasset_data = bit_usd_id(db).bitasset_data(db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 2u);

      // validator1 start feed producing again
      feed.settlement_price = bit_usd_id(db).amount(1) / core.amount(5);
      publish_feed(bit_usd_id(db), validator1_id(db), feed);
      bitasset_data = bit_usd_id(db).bitasset_data(db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 3u);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(itr[0].first.instance.value, 17u);

      // generate some blocks up to expiration but feed will not be deleted yet as need next maint time
      generate_blocks(itr[0].second.first + feed_lifetime + 1);

      // add another feed with validator2
      feed.settlement_price = bit_usd_id(db).amount(1) / core.amount(5);
      publish_feed(bit_usd_id(db), validator2_id(db), feed);
      bitasset_data = bit_usd_id(db).bitasset_data(db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 2u);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(itr[0].first.instance.value, 17u);
      BOOST_CHECK_EQUAL(itr[1].first.instance.value, 18u);

      // make the first feed expire
      generate_blocks(itr[0].second.first + feed_lifetime + 1);
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

      // feed from validator0 expires and gets deleted, feed from validator is on time so persist
      bitasset_data = bit_usd_id(db).bitasset_data(db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 1u);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(itr[0].first.instance.value, 18u);

      // expire everything
      generate_blocks(itr[0].second.first + feed_lifetime + 1);
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      bitasset_data = bit_usd_id(db).bitasset_data(db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 0u);

      // add new feed with validator1
      feed.settlement_price = bit_usd_id(db).amount(1) / core.amount(5);
      publish_feed(bit_usd_id(db), validator1_id(db), feed);
      bitasset_data = bit_usd_id(db).bitasset_data(db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 1u);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(itr[0].first.instance.value, 17u);

      // Reactivate validator0
      transfer(council_account, validator0_id, asset(1000));
      set_expiration(db, trx);
      {
         account_update_operation op;
         op.account = validator0_id;
         op.new_options = validator0_id(db).options;
         op.new_options->votes.insert(validator0_validator_id(db).vote_id);
         op.new_options->num_validator = std::count_if(op.new_options->votes.begin(), op.new_options->votes.end(),
                                                     [](vote_id_type id) {
                                                        return id.type() == vote_id_type::validator;
                                                     });
         trx.operations.push_back(op);
         sign(trx, validator0_private_key);
         PUSH_TX(db, trx);
         trx.clear();
      }

      // This will deactivate validator1 as it is the one with less votes
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

      // Checking
      validators = db.get_global_properties().block_producers;
      BOOST_CHECK_EQUAL(validators.begin()[0].instance.value, 11u);
      BOOST_CHECK_EQUAL(validators.begin()[1].instance.value, 13u);
      BOOST_CHECK_EQUAL(validators.begin()[2].instance.value, 14u);
      BOOST_CHECK_EQUAL(validators.begin()[3].instance.value, 15u);
      BOOST_CHECK_EQUAL(validators.begin()[4].instance.value, 16u);
      BOOST_CHECK_EQUAL(validators.begin()[5].instance.value, 17u);
      BOOST_CHECK_EQUAL(validators.begin()[6].instance.value, 18u);
      BOOST_CHECK_EQUAL(validators.begin()[7].instance.value, 19u);
      BOOST_CHECK_EQUAL(validators.begin()[8].instance.value, 20u);
      BOOST_CHECK_EQUAL(validators.begin()[9].instance.value, 21u);
      BOOST_CHECK_EQUAL(validators.begin()[10].instance.value, 22u);

      // feed from validator1 is still here as the validator is no longer a producer but the feed is not yet expired
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 1u);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(itr[0].first.instance.value, 17u);

      // make feed from validator1 expire
      generate_blocks(itr[0].second.first + feed_lifetime + 1);
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

      bitasset_data = bit_usd_id(db).bitasset_data(db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 0u);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(update_feed_producers)
{
   try
   {
      /* For MPA fed by non validators or non delegates but by feed producers changes should do nothing */
      ACTORS( (sam)(alice)(paul)(bob) );

      // Create the asset
      const asset_id_type bit_usd_id = create_bitasset("USDBIT").get_id();

      // Update asset issuer
      {
         asset_update_issuer_operation op;
         op.asset_to_update = bit_usd_id;
         op.issuer = bit_usd_id(db).issuer;
         op.new_issuer = bob_id;
         trx.operations.push_back(op);
         PUSH_TX(db, trx, ~0);
         generate_block();
         trx.clear();
      }
      // Update asset options
      {
         asset_update_operation op;
         op.asset_to_update = bit_usd_id;
         op.issuer = bob_id;
         op.new_options = bit_usd_id(db).options;
         op.new_options.flags &= ~validator_fed_asset;
         trx.operations.push_back(op);
         PUSH_TX(db, trx, ~0);
         generate_block();
         trx.clear();
      }

      // Add 3 feed producers for asset
      {
         asset_update_feed_producers_operation op;
         op.asset_to_update = bit_usd_id;
         op.issuer = bob_id;
         op.new_feed_producers = {sam_id, alice_id, paul_id};
         trx.operations.push_back(op);
         sign(trx, bob_private_key);
         PUSH_TX(db, trx);
         generate_block();
         trx.clear();
      }

      // Graphene will create entries in the field feed after feed producers are added
      auto bitasset_data = bit_usd_id(db).bitasset_data(db);

      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 3u);
      auto itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(itr[0].first.instance.value, 16u);
      BOOST_CHECK_EQUAL(itr[1].first.instance.value, 17u);
      BOOST_CHECK_EQUAL(itr[2].first.instance.value, 18u);

      // Removing a feed producer
      {
         asset_update_feed_producers_operation op;
         op.asset_to_update = bit_usd_id;
         op.issuer = bob_id;
         op.new_feed_producers = {alice_id, paul_id};
         trx.operations.push_back(op);
         sign(trx, bob_private_key);
         PUSH_TX(db, trx);
         generate_block();
         trx.clear();
      }

      // Feed for removed producer is removed
      bitasset_data = bit_usd_id(db).bitasset_data(db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 2u);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(itr[0].first.instance.value, 17u);
      BOOST_CHECK_EQUAL(itr[1].first.instance.value, 18u);

      // Feed persist after expiration
      const auto feed_lifetime = bit_usd_id(db).bitasset_data(db).options.feed_lifetime_sec;
      generate_blocks(db.head_block_time() + feed_lifetime + 1);
      bitasset_data = bit_usd_id(db).bitasset_data(db);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 2u);
      BOOST_CHECK_EQUAL(itr[0].first.instance.value, 17u);
      BOOST_CHECK_EQUAL(itr[1].first.instance.value, 18u);

      // Advancing to next maint
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

      // Expired feeds persist, no changes
      bitasset_data = bit_usd_id(db).bitasset_data(db);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 2u);
      BOOST_CHECK_EQUAL(itr[0].first.instance.value, 17u);
      BOOST_CHECK_EQUAL(itr[1].first.instance.value, 18u);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(maintenance_multiple_feed_cleanup)
{
   try
   {
      /* Check impact with multiple feeds */
      INVOKE( maintenance_feed_cleanup );

      // get the stuff needed from invoked test
      const asset_id_type bit_usd_id = get_asset("USDBIT").get_id();
      const asset_id_type core_id = asset_id_type();
      const account_id_type validator5_id= get_account("validator5").get_id();
      const account_id_type validator6_id= get_account("validator6").get_id();
      const account_id_type validator7_id= get_account("validator7").get_id();
      const account_id_type validator8_id= get_account("validator8").get_id();
      const account_id_type validator9_id= get_account("validator9").get_id();
      const account_id_type validator10_id= get_account("validator10").get_id();


      set_expiration( db, trx );

      // changing lifetime feed to 5 days
      // maint interval default is every 1 day
      {
         asset_update_bitasset_operation op;
         op.new_options.minimum_feeds = 3;
         op.new_options.feed_lifetime_sec = 86400 * 5;
         op.asset_to_update = bit_usd_id;
         op.issuer = bit_usd_id(db).issuer;
         trx.operations.push_back(op);
         PUSH_TX(db, trx, ~0);
         generate_block();
         trx.clear();
      }

      price_feed feed;
      feed.settlement_price = bit_usd_id(db).amount(1) / core_id(db).amount(5);
      publish_feed(bit_usd_id(db), validator5_id(db), feed);
      auto bitasset_data = bit_usd_id(db).bitasset_data(db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 1u);
      auto itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(itr[0].first.instance.value, 21u);

      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      feed.settlement_price = bit_usd_id(db).amount(1) / core_id(db).amount(5);
      publish_feed(bit_usd_id(db), validator6_id(db), feed);
      bitasset_data = bit_usd_id(db).bitasset_data(db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 2u);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(itr[0].first.instance.value, 21u);
      BOOST_CHECK_EQUAL(itr[1].first.instance.value, 22u);

      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      feed.settlement_price = bit_usd_id(db).amount(1) / core_id(db).amount(5);
      publish_feed(bit_usd_id(db), validator7_id(db), feed);
      bitasset_data = bit_usd_id(db).bitasset_data(db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 3u);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(itr[0].first.instance.value, 21u);
      BOOST_CHECK_EQUAL(itr[1].first.instance.value, 22u);
      BOOST_CHECK_EQUAL(itr[2].first.instance.value, 23u);

      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      feed.settlement_price = bit_usd_id(db).amount(1) / core_id(db).amount(5);
      publish_feed(bit_usd_id(db), validator8_id(db), feed);
      bitasset_data = bit_usd_id(db).bitasset_data(db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 4u);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(itr[0].first.instance.value, 21u);
      BOOST_CHECK_EQUAL(itr[1].first.instance.value, 22u);
      BOOST_CHECK_EQUAL(itr[2].first.instance.value, 23u);
      BOOST_CHECK_EQUAL(itr[3].first.instance.value, 24u);

      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      feed.settlement_price = bit_usd_id(db).amount(1) / core_id(db).amount(5);
      publish_feed(bit_usd_id(db), validator9_id(db), feed);
      bitasset_data = bit_usd_id(db).bitasset_data(db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 5u);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(itr[0].first.instance.value, 21u);
      BOOST_CHECK_EQUAL(itr[1].first.instance.value, 22u);
      BOOST_CHECK_EQUAL(itr[2].first.instance.value, 23u);
      BOOST_CHECK_EQUAL(itr[3].first.instance.value, 24u);
      BOOST_CHECK_EQUAL(itr[4].first.instance.value, 25u);

      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      feed.settlement_price = bit_usd_id(db).amount(1) / core_id(db).amount(5);
      publish_feed(bit_usd_id(db), validator10_id(db), feed);
      bitasset_data = bit_usd_id(db).bitasset_data(db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 6u);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(itr[0].first.instance.value, 21u);
      BOOST_CHECK_EQUAL(itr[1].first.instance.value, 22u);
      BOOST_CHECK_EQUAL(itr[2].first.instance.value, 23u);
      BOOST_CHECK_EQUAL(itr[3].first.instance.value, 24u);
      BOOST_CHECK_EQUAL(itr[4].first.instance.value, 25u);
      BOOST_CHECK_EQUAL(itr[5].first.instance.value, 26u);

      // make the older feed expire
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      bitasset_data = bit_usd_id(db).bitasset_data(db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 5u);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(itr[0].first.instance.value, 22u);
      BOOST_CHECK_EQUAL(itr[1].first.instance.value, 23u);
      BOOST_CHECK_EQUAL(itr[2].first.instance.value, 24u);
      BOOST_CHECK_EQUAL(itr[3].first.instance.value, 25u);
      BOOST_CHECK_EQUAL(itr[4].first.instance.value, 26u);

      // make older 2 feeds expire
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      bitasset_data = bit_usd_id(db).bitasset_data(db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 3u);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(itr[0].first.instance.value, 24u);
      BOOST_CHECK_EQUAL(itr[1].first.instance.value, 25u);
      BOOST_CHECK_EQUAL(itr[2].first.instance.value, 26u);

      // validator5 add new feed, feeds are sorted by validator_id not by feed_time
      feed.settlement_price = bit_usd_id(db).amount(1) / core_id(db).amount(5);
      publish_feed(bit_usd_id(db), validator5_id(db), feed);
      bitasset_data = bit_usd_id(db).bitasset_data(db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 4u);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(itr[0].first.instance.value, 21u);
      BOOST_CHECK_EQUAL(itr[1].first.instance.value, 24u);
      BOOST_CHECK_EQUAL(itr[2].first.instance.value, 25u);
      BOOST_CHECK_EQUAL(itr[3].first.instance.value, 26u);

      // another feed expires
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();
      bitasset_data = bit_usd_id(db).bitasset_data(db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 3u);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(itr[0].first.instance.value, 21u);
      BOOST_CHECK_EQUAL(itr[1].first.instance.value, 25u);
      BOOST_CHECK_EQUAL(itr[2].first.instance.value, 26u);

      // another feed expires
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();
      bitasset_data = bit_usd_id(db).bitasset_data(db);
      BOOST_CHECK_EQUAL(bitasset_data.feeds.size(), 2u);
      itr = bitasset_data.feeds.begin();
      BOOST_CHECK_EQUAL(itr[0].first.instance.value, 21u);
      BOOST_CHECK_EQUAL(itr[2].first.instance.value, 26u);

      // and so on

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
