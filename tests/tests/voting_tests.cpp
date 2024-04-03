#include <boost/test/unit_test.hpp>

#include <graphene/app/database_api.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>

#include <iostream>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;


BOOST_FIXTURE_TEST_SUITE(voting_tests, database_fixture)

BOOST_FIXTURE_TEST_CASE( council_account_initialization_test, database_fixture )
{ try {
   // Check current default council
   // By default chain is configured with INITIAL_COUNCIL_COUNT=9 members
   const auto &delegates = db.get_global_properties().council_delegates;
   const auto &council = council_account(db);

   BOOST_CHECK_EQUAL(delegates.size(), INITIAL_COUNCIL_COUNT);
   BOOST_CHECK_EQUAL(council.active.num_auths(), 0);

   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
   generate_block();
   set_expiration(db, trx);

   // Check that council not changed, votes absent
   const auto &delegates_after_maint = db.get_global_properties().council_delegates;
   const auto &council_after_maint = council_account(db);
   BOOST_CHECK_EQUAL(delegates_after_maint.size(), INITIAL_COUNCIL_COUNT);
   BOOST_CHECK_EQUAL(council_after_maint.active.num_auths(), 0);

   // You can't use uninitialized council
   // when any user with stake created (create_account method automatically set up votes for council)
   // council is incomplete and consist of random active members
   ACTOR(alice);
   fund(alice);
   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

   const auto &council_after_maint_with_stake = council_account(db);
   BOOST_CHECK_LT(council_after_maint_with_stake.active.num_auths(), INITIAL_COUNCIL_COUNT);

   // Initialize council by voting for each member and for desired count
   vote_for_delegates_and_validators(INITIAL_COUNCIL_COUNT, INITIAL_PRODUCER_COUNT);
   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);

   const auto &delegates_after_maint_and_init = db.get_global_properties().council_delegates;
   const auto &council_after_maint_and_init = council_account(db);
   BOOST_CHECK_EQUAL(delegates_after_maint_and_init.size(), INITIAL_COUNCIL_COUNT);
   BOOST_CHECK_EQUAL(council_after_maint_and_init.active.num_auths(), INITIAL_COUNCIL_COUNT);
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(put_my_validators)
{
   try
   {
      ACTORS( (validator0)
              (validator1)
              (validator2)
              (validator3)
              (validator4)
              (validator5)
              (validator6)
              (validator7)
              (validator8)
              (validator9)
              (validator10)
              (validator11)
              (validator12)
              (validator13) );

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
      upgrade_to_lifetime_member(validator12_id);
      upgrade_to_lifetime_member(validator13_id);

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
      const validator_id_type validator12_validator_id = create_validator(validator12_id, validator12_private_key).get_id();
      const validator_id_type validator13_validator_id = create_validator(validator13_id, validator13_private_key).get_id();

      // Create a vector with private key of all validators, will be used to activate 9 validators at a time
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
            validator10_private_key,
            validator11_private_key,
            validator12_private_key,
            validator13_private_key

      };

      // create a map with account id and validator id
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
            {validator10_id, validator10_validator_id},
            {validator11_id, validator11_validator_id},
            {validator12_id, validator12_validator_id},
            {validator13_id, validator13_validator_id}
      };

      // Check current default validators, default chain is configured with 9 validators
      auto validators = db.get_global_properties().block_producers;
      BOOST_CHECK_EQUAL(validators.size(), INITIAL_PRODUCER_COUNT);
      BOOST_CHECK_EQUAL(validators.begin()[0].instance.value, 1u);
      BOOST_CHECK_EQUAL(validators.begin()[1].instance.value, 2u);
      BOOST_CHECK_EQUAL(validators.begin()[2].instance.value, 3u);
      BOOST_CHECK_EQUAL(validators.begin()[3].instance.value, 4u);
      BOOST_CHECK_EQUAL(validators.begin()[4].instance.value, 5u);
      BOOST_CHECK_EQUAL(validators.begin()[5].instance.value, 6u);
      BOOST_CHECK_EQUAL(validators.begin()[6].instance.value, 7u);
      BOOST_CHECK_EQUAL(validators.begin()[7].instance.value, 8u);
      BOOST_CHECK_EQUAL(validators.begin()[8].instance.value, 9u);

      // Activate all validators
      // Each validator is voted with incremental stake so last validator created will be the ones with more votes
      int c = 0;
      for (auto l : validator_map) {
         int stake = 100 + c + 10;
         transfer(council_account, l.first, asset(stake));
         {
            set_expiration(db, trx);
            account_update_operation op;
            op.account = l.first;
            op.new_options = l.first(db).options;
            op.new_options->votes.insert(l.second(db).vote_id);

            trx.operations.push_back(op);
            sign(trx, private_keys.at(c));
            PUSH_TX(db, trx);
            trx.clear();
         }
         ++c;
      }

      // Trigger the new validators
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      // Check my validators are now in control of the system
      validators = db.get_global_properties().block_producers;
      BOOST_CHECK_EQUAL(validators.size(), INITIAL_PRODUCER_COUNT);
      BOOST_CHECK_EQUAL(validators.begin()[0].instance.value, 16u);
      BOOST_CHECK_EQUAL(validators.begin()[1].instance.value, 17u);
      BOOST_CHECK_EQUAL(validators.begin()[2].instance.value, 18u);
      BOOST_CHECK_EQUAL(validators.begin()[3].instance.value, 19u);
      BOOST_CHECK_EQUAL(validators.begin()[4].instance.value, 20u);
      BOOST_CHECK_EQUAL(validators.begin()[5].instance.value, 21u);
      BOOST_CHECK_EQUAL(validators.begin()[6].instance.value, 22u);
      BOOST_CHECK_EQUAL(validators.begin()[7].instance.value, 23u);
      BOOST_CHECK_EQUAL(validators.begin()[8].instance.value, 24u);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(track_votes_validators_enabled)
{
   try
   {
      graphene::app::database_api db_api1(db);

      INVOKE(put_my_validators);

      const account_id_type validator1_id= get_account("validator1").get_id();
      auto validator1_object = db_api1.get_validator_by_account(validator1_id(db).name);
      BOOST_CHECK_EQUAL(validator1_object->total_votes, 111u);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(track_votes_validators_disabled)
{
   try
   {
      graphene::app::database_api db_api1(db);

      INVOKE(put_my_validators);

      const account_id_type validator1_id= get_account("validator1").get_id();
      auto validator1_object = db_api1.get_validator_by_account(validator1_id(db).name);
      BOOST_CHECK_EQUAL(validator1_object->total_votes, 0u);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(put_my_delegates)
{
   try
   {
      ACTORS( (delegate0)
              (delegate1)
              (delegate2)
              (delegate3)
              (delegate4)
              (delegate5)
              (delegate6)
              (delegate7)
              (delegate8)
              (delegate9)
              (delegate10)
              (delegate11)
              (delegate12)
              (delegate13) );

      // Upgrade all accounts to LTM
      upgrade_to_lifetime_member(delegate0_id);
      upgrade_to_lifetime_member(delegate1_id);
      upgrade_to_lifetime_member(delegate2_id);
      upgrade_to_lifetime_member(delegate3_id);
      upgrade_to_lifetime_member(delegate4_id);
      upgrade_to_lifetime_member(delegate5_id);
      upgrade_to_lifetime_member(delegate6_id);
      upgrade_to_lifetime_member(delegate7_id);
      upgrade_to_lifetime_member(delegate8_id);
      upgrade_to_lifetime_member(delegate9_id);
      upgrade_to_lifetime_member(delegate10_id);
      upgrade_to_lifetime_member(delegate11_id);
      upgrade_to_lifetime_member(delegate12_id);
      upgrade_to_lifetime_member(delegate13_id);

      // Create all the delegates
      const delegate_id_type delegate0_delegate_id = create_delegate(delegate0_id(db)).get_id();
      const delegate_id_type delegate1_delegate_id = create_delegate(delegate1_id(db)).get_id();
      const delegate_id_type delegate2_delegate_id = create_delegate(delegate2_id(db)).get_id();
      const delegate_id_type delegate3_delegate_id = create_delegate(delegate3_id(db)).get_id();
      const delegate_id_type delegate4_delegate_id = create_delegate(delegate4_id(db)).get_id();
      const delegate_id_type delegate5_delegate_id = create_delegate(delegate5_id(db)).get_id();
      const delegate_id_type delegate6_delegate_id = create_delegate(delegate6_id(db)).get_id();
      const delegate_id_type delegate7_delegate_id = create_delegate(delegate7_id(db)).get_id();
      const delegate_id_type delegate8_delegate_id = create_delegate(delegate8_id(db)).get_id();
      const delegate_id_type delegate9_delegate_id = create_delegate(delegate9_id(db)).get_id();
      const delegate_id_type delegate10_delegate_id = create_delegate(delegate10_id(db)).get_id();
      const delegate_id_type delegate11_delegate_id = create_delegate(delegate11_id(db)).get_id();
      const delegate_id_type delegate12_delegate_id = create_delegate(delegate12_id(db)).get_id();
      const delegate_id_type delegate13_delegate_id = create_delegate(delegate13_id(db)).get_id();

      // Create a vector with private key of all delegates, will be used to activate 9 members at a time
      const vector <fc::ecc::private_key> private_keys = {
            delegate0_private_key,
            delegate1_private_key,
            delegate2_private_key,
            delegate3_private_key,
            delegate4_private_key,
            delegate5_private_key,
            delegate6_private_key,
            delegate7_private_key,
            delegate8_private_key,
            delegate9_private_key,
            delegate10_private_key,
            delegate11_private_key,
            delegate12_private_key,
            delegate13_private_key
      };

      // create a map with account id and delegate id
      const flat_map <account_id_type, delegate_id_type> delegate_map = {
            {delegate0_id, delegate0_delegate_id},
            {delegate1_id, delegate1_delegate_id},
            {delegate2_id, delegate2_delegate_id},
            {delegate3_id, delegate3_delegate_id},
            {delegate4_id, delegate4_delegate_id},
            {delegate5_id, delegate5_delegate_id},
            {delegate6_id, delegate6_delegate_id},
            {delegate7_id, delegate7_delegate_id},
            {delegate8_id, delegate8_delegate_id},
            {delegate9_id, delegate9_delegate_id},
            {delegate10_id, delegate10_delegate_id},
            {delegate11_id, delegate11_delegate_id},
            {delegate12_id, delegate12_delegate_id},
            {delegate13_id, delegate13_delegate_id}
      };

      // Check current default council, default chain is configured with 9 delegates
      auto delegates = db.get_global_properties().council_delegates;

      BOOST_CHECK_EQUAL(delegates.size(), INITIAL_COUNCIL_COUNT);
      BOOST_CHECK_EQUAL(delegates.begin()[0].instance.value, 0u);
      BOOST_CHECK_EQUAL(delegates.begin()[1].instance.value, 1u);
      BOOST_CHECK_EQUAL(delegates.begin()[2].instance.value, 2u);
      BOOST_CHECK_EQUAL(delegates.begin()[3].instance.value, 3u);
      BOOST_CHECK_EQUAL(delegates.begin()[4].instance.value, 4u);
      BOOST_CHECK_EQUAL(delegates.begin()[5].instance.value, 5u);
      BOOST_CHECK_EQUAL(delegates.begin()[6].instance.value, 6u);
      BOOST_CHECK_EQUAL(delegates.begin()[7].instance.value, 7u);
      BOOST_CHECK_EQUAL(delegates.begin()[8].instance.value, 8u);

      // Activate all delegates
      // Each delegate is voted with incremental stake so last member created will be the ones with more votes
      int c = 0;
      for (auto delegate : delegate_map) {
         int stake = 100 + c + 10;
         transfer(council_account, delegate.first, asset(stake));
         {
            set_expiration(db, trx);
            account_update_operation op;
            op.account = delegate.first;
            op.new_options = delegate.first(db).options;

            op.new_options->votes.clear();
            op.new_options->votes.insert(delegate.second(db).vote_id);
            op.new_options->num_delegates = 1;

            trx.operations.push_back(op);
            sign(trx, private_keys.at(c));
            PUSH_TX(db, trx);
            trx.clear();
         }
         ++c;
      }

      // Trigger the new council
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();

      // Check my validators are now in control of the system
      delegates = db.get_global_properties().council_delegates;
      std::sort(delegates.begin(), delegates.end());

      BOOST_CHECK_EQUAL(delegates.size(), INITIAL_COUNCIL_COUNT);

      // Check my delegates are now in control of the system
      BOOST_CHECK_EQUAL(delegates.begin()[0].instance.value, 15);
      BOOST_CHECK_EQUAL(delegates.begin()[1].instance.value, 16);
      BOOST_CHECK_EQUAL(delegates.begin()[2].instance.value, 17);
      BOOST_CHECK_EQUAL(delegates.begin()[3].instance.value, 18);
      BOOST_CHECK_EQUAL(delegates.begin()[4].instance.value, 19);
      BOOST_CHECK_EQUAL(delegates.begin()[5].instance.value, 20);
      BOOST_CHECK_EQUAL(delegates.begin()[6].instance.value, 21);
      BOOST_CHECK_EQUAL(delegates.begin()[7].instance.value, 22);
      BOOST_CHECK_EQUAL(delegates.begin()[8].instance.value, 23);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(track_votes_council_enabled)
{
   try
   {
      graphene::app::database_api db_api1(db);

      INVOKE(put_my_delegates);

      const account_id_type delegate1_id= get_account("delegate1").get_id();
      auto delegate1_object = db_api1.get_delegate_by_account(delegate1_id(db).name);
      BOOST_CHECK_EQUAL(delegate1_object->total_votes, 111u);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(track_votes_council_disabled)
{
   try
   {
      graphene::app::database_api db_api1(db);

      INVOKE(put_my_delegates);

      const account_id_type delegate1_id= get_account("delegate1").get_id();
      auto delegate1_object = db_api1.get_delegate_by_account(delegate1_id(db).name);
      BOOST_CHECK_EQUAL(delegate1_object->total_votes, 0u);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(invalid_voting_account)
{
   try
   {
      ACTORS((alice));

      account_id_type invalid_account_id( (uint64_t)999999 );

      BOOST_CHECK( !db.find( invalid_account_id ) );

      graphene::chain::account_update_operation op;
      op.account = alice_id;
      op.new_options = alice.options;
      op.new_options->voting_account = invalid_account_id;
      trx.operations.push_back(op);
      sign(trx, alice_private_key);

      GRAPHENE_REQUIRE_THROW( PUSH_TX( db, trx, ~0 ), fc::exception );

   } FC_LOG_AND_RETHROW()
}
BOOST_AUTO_TEST_CASE(last_voting_date)
{
   try
   {
      ACTORS((alice));

      transfer(council_account, alice_id, asset(100));

      // we are going to vote for this validator
      auto validator1 = validator_id_type(1)(db);

      auto stats_obj = db.get_account_stats_by_owner(alice_id);
      BOOST_CHECK_EQUAL(stats_obj.last_vote_time.sec_since_epoch(), 0u);

      // alice votes
      graphene::chain::account_update_operation op;
      op.account = alice_id;
      op.new_options = alice.options;
      op.new_options->votes.insert(validator1.vote_id);
      trx.operations.push_back(op);
      sign(trx, alice_private_key);
      PUSH_TX( db, trx, ~0 );

      auto now = db.head_block_time().sec_since_epoch();

      // last_vote_time is updated for alice
      stats_obj = db.get_account_stats_by_owner(alice_id);
      BOOST_CHECK_EQUAL(stats_obj.last_vote_time.sec_since_epoch(), now);

   } FC_LOG_AND_RETHROW()
}
BOOST_AUTO_TEST_CASE(last_voting_date_proxy)
{
   try
   {
      ACTORS((alice)(proxy)(bob));

      transfer(council_account, alice_id, asset(100));
      transfer(council_account, bob_id, asset(200));
      transfer(council_account, proxy_id, asset(300));

      generate_block();

      // validator to vote for
      auto validator1 = validator_id_type(1)(db);

      // round1: alice changes proxy, this is voting activity
      {
         graphene::chain::account_update_operation op;
         op.account = alice_id;
         op.new_options = alice_id(db).options;
         op.new_options->voting_account = proxy_id;
         trx.operations.push_back(op);
         sign(trx, alice_private_key);
         PUSH_TX( db, trx, ~0 );
      }
      // alice last_vote_time is updated
      auto alice_stats_obj = db.get_account_stats_by_owner(alice_id);
      auto round1 = db.head_block_time().sec_since_epoch();
      BOOST_CHECK_EQUAL(alice_stats_obj.last_vote_time.sec_since_epoch(), round1);

      generate_block();

      // round 2: alice update account but no proxy or voting changes are done
      {
         graphene::chain::account_update_operation op;
         op.account = alice_id;
         op.new_options = alice_id(db).options;
         trx.operations.push_back(op);
         sign(trx, alice_private_key);
         set_expiration( db, trx );
         PUSH_TX( db, trx, ~0 );
      }
      // last_vote_time is not updated
      alice_stats_obj = db.get_account_stats_by_owner(alice_id);
      BOOST_CHECK_EQUAL(alice_stats_obj.last_vote_time.sec_since_epoch(), round1);

      generate_block();

      // round 3: bob votes
      {
         graphene::chain::account_update_operation op;
         op.account = bob_id;
         op.new_options = bob_id(db).options;
         op.new_options->votes.insert(validator1.vote_id);
         trx.operations.push_back(op);
         sign(trx, bob_private_key);
         set_expiration( db, trx );
         PUSH_TX(db, trx, ~0);
      }

      // last_vote_time for bob is updated as he voted
      auto round3 = db.head_block_time().sec_since_epoch();
      auto bob_stats_obj = db.get_account_stats_by_owner(bob_id);
      BOOST_CHECK_EQUAL(bob_stats_obj.last_vote_time.sec_since_epoch(), round3);

      generate_block();

      // round 4: proxy votes
      {
         graphene::chain::account_update_operation op;
         op.account = proxy_id;
         op.new_options = proxy_id(db).options;
         op.new_options->votes.insert(validator1.vote_id);
         trx.operations.push_back(op);
         sign(trx, proxy_private_key);
         PUSH_TX(db, trx, ~0);
      }

      // proxy just voted so the last_vote_time is updated
      auto round4 = db.head_block_time().sec_since_epoch();
      auto proxy_stats_obj = db.get_account_stats_by_owner(proxy_id);
      BOOST_CHECK_EQUAL(proxy_stats_obj.last_vote_time.sec_since_epoch(), round4);

      // alice haves proxy, proxy votes but last_vote_time is not updated for alice
      alice_stats_obj = db.get_account_stats_by_owner(alice_id);
      BOOST_CHECK_EQUAL(alice_stats_obj.last_vote_time.sec_since_epoch(), round1);

      // bob haves nothing to do with proxy so last_vote_time is not updated
      bob_stats_obj = db.get_account_stats_by_owner(bob_id);
      BOOST_CHECK_EQUAL(bob_stats_obj.last_vote_time.sec_since_epoch(), round3);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
