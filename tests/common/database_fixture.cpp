#include <boost/test/unit_test.hpp>
#include <boost/range/algorithm.hpp>

#include <graphene/account_history/account_history_plugin.hpp>
#include <graphene/market_history/market_history_plugin.hpp>
#include <graphene/grouped_orders/grouped_orders_plugin.hpp>
#include <graphene/elasticsearch/elasticsearch_plugin.hpp>
#include <graphene/api_helper_indexes/api_helper_indexes.hpp>
#include <graphene/es_objects/es_objects.hpp>
#include <graphene/custom_operations/custom_operations_plugin.hpp>
#include <graphene/debug_validator/debug_validator.hpp>

#include <graphene/chain/balance_object.hpp>
#include <graphene/chain/delegate_object.hpp>
#include <graphene/chain/fba_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/vesting_balance_object.hpp>
#include <graphene/chain/validator_object.hpp>
#include <graphene/chain/worker_object.hpp>
#include <graphene/chain/htlc_object.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/hardfork_visitor.hpp>

#include <fc/crypto/digest.hpp>

#include <iomanip>

#include "database_fixture.hpp"
#include "elasticsearch.hpp"

using namespace graphene::chain::test;
using namespace graphene::tests::utils;

extern uint32_t    GRAPHENE_TESTING_GENESIS_TIMESTAMP;
extern std::string GRAPHENE_TESTING_ES_URL;

namespace graphene { namespace chain {

using std::cout;
using std::cerr;

namespace buf = boost::unit_test::framework;

void clearable_block::clear()
{
   _calculated_merkle_root = checksum_type();
   _signee = fc::ecc::public_key();
   _block_id = block_id_type();
}

database_fixture_base::database_fixture_base()
   : app(), db( *app.chain_database() ),
     private_key( fc::ecc::private_key::generate() ),
     init_account_priv_key( fc::ecc::private_key::regenerate(fc::sha256::hash(string("null_key")) ) ),
     init_account_pub_key( init_account_priv_key.get_public_key() ),
     current_test_name( buf::current_test_case().p_name.value ),
     current_suite_name( buf::get<boost::unit_test::test_suite>(buf::current_test_case().p_parent_id).p_name
                                                                                                     .value )
{ try {
   int argc = buf::master_test_suite().argc;
   char** argv = buf::master_test_suite().argv;
   for( int i=1; i<argc; i++ )
   {
      const std::string arg = argv[i];
      if( arg == "--record-assert-trip" )
         fc::enable_record_assert_trip = true;
      if( arg == "--show-test-names" )
         std::cout << "running test " << current_test_name << std::endl;
   }
} FC_LOG_AND_RETHROW() }

database_fixture_base::~database_fixture_base()
{
   // cleanup data in ES
   if( !es_index_prefix.empty() || !es_obj_index_prefix.empty() )
   {
      CURL *curl; // curl handler
      curl = curl_easy_init();
      curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);

      graphene::utilities::ES es;
      es.curl = curl;
      es.elasticsearch_url = GRAPHENE_TESTING_ES_URL;

      if( !es_index_prefix.empty() )
      {
         es.index_prefix = es_index_prefix;
         // delete all
         try {
            graphene::utilities::deleteAll(es);
         } catch (...) {
            // nothing to do
         }
      }

      if( !es_obj_index_prefix.empty() )
      {
         es.index_prefix = es_obj_index_prefix;
         // delete all
         try {
            graphene::utilities::deleteAll(es);
         } catch (...) {
            // nothing to do
         }
      }
   }

   try {
      // If we're unwinding due to an exception, don't do any more checks.
      // This way, boost test's last checkpoint tells us approximately where the error was.
      if( !std::uncaught_exception() )
      {
         verify_asset_supplies(db);
         BOOST_CHECK( db.get_node_properties().skip_flags == database::skip_nothing );
      }
   } catch (fc::exception& ex) {
      BOOST_FAIL( std::string("fc::exception in ~database_fixture: ") + ex.to_detail_string() );
   } catch (std::exception& e) {
      BOOST_FAIL( std::string("std::exception in ~database_fixture:") + e.what() );
   } catch (...) {
      BOOST_FAIL( "Uncaught exception in ~database_fixture" );
   }

}

void database_fixture_base::init_genesis( database_fixture_base& fixture )
{
   fixture.genesis_state.initial_timestamp = fc::time_point_sec(GRAPHENE_TESTING_GENESIS_TIMESTAMP);

   fixture.genesis_state.initial_block_producers = 10;
   fixture.genesis_state.immutable_parameters.min_council_count = INITIAL_COUNCIL_COUNT;
   fixture.genesis_state.immutable_parameters.min_producer_count = INITIAL_PRODUCER_COUNT;

   for( unsigned int i = 0; i < fixture.genesis_state.initial_block_producers; ++i )
   {
      auto name = "init"+fc::to_string(i);
      fixture.genesis_state.initial_accounts.emplace_back( name, fixture.init_account_pub_key,
                                                           fixture.init_account_pub_key, true);
      fixture.genesis_state.initial_delegate_candidates.push_back({name});
      fixture.genesis_state.initial_validator_candidates.push_back({ name, fixture.init_account_pub_key });
   }
   fixture.genesis_state.initial_parameters.get_mutable_fees().zero_all_fees();

   genesis_state_type::initial_asset_type init_ba1;
   init_ba1.symbol = "INITBA";
   init_ba1.issuer_name = "council-account";
   init_ba1.description = "Initial BA";
   init_ba1.precision = 4;
   init_ba1.max_supply = GRAPHENE_MAX_SHARE_SUPPLY;
   init_ba1.accumulated_fees = 0;
   init_ba1.is_backed = true;
   fixture.genesis_state.initial_assets.push_back( init_ba1 );
   // TODO add initial UA's; add initial short positions; test non-zero accumulated_fees
}

std::shared_ptr<boost::program_options::variables_map> database_fixture_base::init_options(
      database_fixture_base& fixture )
{
   auto sharable_options = std::make_shared<boost::program_options::variables_map>();
   auto& options = *sharable_options;
   set_option( options, "seed-nodes", std::string("[]") ); // Do not connect to default seed nodes
   /**
    * Test specific settings
    */
   if (fixture.current_test_name == "broadcast_transaction_with_callback_test")
      set_option( options, "enable-p2p-network", true );
   else if (fixture.current_test_name == "broadcast_transaction_disabled_p2p_test")
      set_option( options, "enable-p2p-network", false );
   else if( rand() % 100 >= 50 ) // Disable P2P network randomly for test cases
      set_option( options, "enable-p2p-network", false );
   else
   {
      if( rand() % 100 >= 50 ) // this should lead to no change
      {
         set_option( options, "enable-p2p-network", true );
      }
      fc::ip::endpoint ep;
      ep.set_port( rand() % 20000 + 5000 );
      idump( (ep)(std::string(ep)) );
      set_option( options, "p2p-endpoint", std::string( ep ) );
   }

   if (fixture.current_test_name == "min_blocks_to_keep_test")
   {
      set_option( options, "partial-operations", true );
      set_option( options, "max-ops-per-account", (uint64_t)2 );
      set_option( options, "min-blocks-to-keep", (uint32_t)3 );
      set_option( options, "max-ops-per-acc-by-min-blocks", (uint64_t)5 );
   }
   if (fixture.current_test_name == "get_account_history_operations")
   {
      set_option( options, "max-ops-per-account", (uint64_t)75 );
      set_option( options, "min-blocks-to-keep", (uint32_t)0 );
   }
   if (fixture.current_test_name == "api_limit_get_account_history_operations")
   {
      set_option( options, "max-ops-per-account", (uint64_t)125 );
      set_option( options, "min-blocks-to-keep", (uint32_t)0 );
      set_option( options, "api-limit-get-account-history-operations", (uint32_t)300 );
   }
   if(fixture.current_test_name =="api_limit_get_account_history")
   {
      set_option( options, "max-ops-per-account", (uint64_t)125 );
      set_option( options, "min-blocks-to-keep", (uint32_t)0 );
      set_option( options, "api-limit-get-account-history", (uint32_t)250 );
   }
   if(fixture.current_test_name =="api_limit_get_grouped_limit_orders")
   {
      set_option( options, "api-limit-get-grouped-limit-orders", (uint32_t)250 );
   }
   if(fixture.current_test_name =="api_limit_get_relative_account_history")
   {
      set_option( options, "max-ops-per-account", (uint64_t)125 );
      set_option( options, "min-blocks-to-keep", (uint32_t)0 );
      set_option( options, "api-limit-get-relative-account-history", (uint32_t)250 );
   }
   if(fixture.current_test_name =="api_limit_get_account_history_by_operations")
   {
      set_option( options, "api-limit-get-account-history-by-operations", (uint32_t)250 );
      set_option( options, "api-limit-get-relative-account-history", (uint32_t)250 );
   }
   if(fixture.current_test_name =="api_limit_get_asset_holders")
   {
      set_option( options, "api-limit-get-asset-holders", (uint32_t)250 );
   }
   if(fixture.current_test_name =="api_limit_get_key_references")
   {
      set_option( options, "api-limit-get-key-references", (uint32_t)200 );
   }
   if(fixture.current_test_name =="api_limit_get_limit_orders")
   {
      set_option( options, "api-limit-get-limit-orders", (uint32_t)350 );
   }
   if(fixture.current_test_name =="api_limit_get_limit_orders_by_account")
   {
      set_option( options, "api-limit-get-limit-orders-by-account", (uint32_t)150 );
   }
   if(fixture.current_test_name =="api_limit_get_call_orders")
   {
      set_option( options, "api-limit-get-call-orders", (uint32_t)350 );
   }
   if(fixture.current_test_name =="api_limit_get_settle_orders")
   {
      set_option( options, "api-limit-get-settle-orders", (uint32_t)350 );
   }
   if(fixture.current_test_name =="api_limit_get_order_book")
   {
      set_option( options, "api-limit-get-order-book", (uint32_t)80 );
   }
   if(fixture.current_test_name =="api_limit_lookup_accounts")
   {
      set_option( options, "api-limit-lookup-accounts", (uint32_t)200 );
   }
   if(fixture.current_test_name =="api_limit_lookup_validator_accounts")
   {
      set_option( options, "api-limit-lookup-validator-accounts", (uint32_t)200 );
   }
   if(fixture.current_test_name =="api_limit_lookup_delegate_accounts")
   {
      set_option( options, "api-limit-lookup-delegate-accounts", (uint32_t)200 );
   }
   if(fixture.current_test_name =="api_limit_lookup_delegate_accounts")
   {
      set_option( options, "api-limit-lookup-delegate-accounts", (uint32_t)200 );
   }
   if(fixture.current_test_name =="api_limit_lookup_vote_ids")
   {
      set_option( options, "api-limit-lookup-vote-ids", (uint32_t)2 );
   }
   if(fixture.current_test_name =="api_limit_get_account_limit_orders")
   {
      set_option( options, "api-limit-get-account-limit-orders", (uint32_t)250 );
   }
   if(fixture.current_test_name =="api_limit_get_collateral_bids")
   {
      set_option( options, "api-limit-get-collateral-bids", (uint32_t)250 );
   }
   if(fixture.current_test_name =="api_limit_get_top_markets")
   {
      set_option( options, "api-limit-get-top-markets", (uint32_t)250 );
   }
   if(fixture.current_test_name =="api_limit_get_trade_history")
   {
      set_option( options, "api-limit-get-trade-history", (uint32_t)250 );
   }
   if(fixture.current_test_name =="api_limit_get_trade_history_by_sequence")
   {
      set_option( options, "api-limit-get-trade-history-by-sequence", (uint32_t)250 );
   }
   if(fixture.current_test_name =="api_limit_get_withdraw_permissions_by_giver")
   {
      set_option( options, "api-limit-get-withdraw-permissions-by-giver", (uint32_t)250 );
   }
   if(fixture.current_test_name =="api_limit_get_withdraw_permissions_by_recipient")
   {
      set_option( options, "api-limit-get-withdraw-permissions-by-recipient", (uint32_t)250 );
   }
   if(fixture.current_test_name =="api_limit_get_full_accounts2")
   {
      set_option( options, "api-limit-get-full-accounts", (uint32_t)200 );
      set_option( options, "api-limit-get-full-accounts-lists", (uint32_t)120 );
   }

   if( fixture.current_suite_name == "login_api_tests" )
   {
      if( fixture.current_test_name =="get_config_test" )
      {
         set_option( options, "api-node-info", string("Test API node") );
         set_option( options, "api-limit-get-full-accounts-subscribe", (uint32_t)120 );
      }
      if( fixture.current_test_name =="login_test" )
      {
         // bytemaster/supersecret, user2/superpassword2
         string api_access_config = R"(
         {
            "permission_map" :
            [
               [
                  "bytemaster",
                  {
                     "password_hash_b64" : "9e9GF7ooXVb9k4BoSfNIPTelXeGOZ5DrgOYMj94elaY=",
                     "password_salt_b64" : "INDdM6iCi/8=",
                     "allowed_apis" : ["database_api", "network_broadcast_api", "history_api", "network_node_api",
                                       "asset_api", "crypto_api", "block_api", "orders_api", "custom_operations_api"
                                       "debug_api"]
                  }
               ],
               [
                  "user2",
                  {
                     "password_hash_b64" : "myadjRISnFOWn2TTd91zqbY50q0w2j/oJGlcdQkUB0Y=",
                     "password_salt_b64" : "Zb8JrQDKNIQ=",
                     "allowed_apis" : ["history_api"]
                  }
               ],
               [
                  "*",
                  {
                     "password_hash_b64" : "*",
                     "password_salt_b64" : "*",
                     "allowed_apis" : ["database_api", "network_broadcast_api", "history_api"]
                  }
               ]
            ]
         }
         )";

         fc::json::save_to_file( fc::json::from_string( api_access_config ),
                                 fixture.data_dir.path() / "api-access.json" );
         set_option( options, "api-access",
                         boost::filesystem::path(fixture.data_dir.path() / "api-access.json") );

         fixture.app.register_plugin<graphene::debug_validator_plugin::debug_validator_plugin>(true);
         fixture.app.register_plugin<graphene::custom_operations::custom_operations_plugin>(true);
         set_option( options, "custom-operations-start-block", uint32_t(1) );
      }
   }

   // add account tracking for ahplugin for special test case with track-account enabled
   if( !options.count("track-account") && fixture.current_test_name == "track_account") {
      std::vector<std::string> track_account;
      std::string track = "\"1.2.17\"";
      track_account.push_back(track);
      set_option( options, "track-account", track_account );
      set_option( options, "partial-operations", true );
   }
   // account tracking 2 accounts
   if( !options.count("track-account") && fixture.current_test_name == "track_account2") {
      std::vector<std::string> track_account;
      std::string track = "\"1.2.0\"";
      track_account.push_back(track);
      track = "\"1.2.16\"";
      track_account.push_back(track);
      set_option( options, "track-account", track_account );
   }
   // standby votes tracking
   if( fixture.current_test_name == "track_votes_validators_disabled"
          || fixture.current_test_name == "track_votes_council_disabled") {
      fixture.app.chain_database()->enable_standby_votes_tracking( false );
   }
   // load ES or AH, but not both
   if(fixture.current_test_name == "elasticsearch_account_history" ||
         fixture.current_test_name == "elasticsearch_history_api") {
      fixture.app.register_plugin<graphene::elasticsearch::elasticsearch_plugin>(true);

      set_option( options, "elasticsearch-node-url", GRAPHENE_TESTING_ES_URL );
      set_option( options, "elasticsearch-bulk-replay", uint32_t(2) );
      set_option( options, "elasticsearch-bulk-sync", uint32_t(2) );
      set_option( options, "elasticsearch-start-es-after-block", uint32_t(0) );
      set_option( options, "elasticsearch-visitor", false );
      set_option( options, "elasticsearch-operation-object", true );
      set_option( options, "elasticsearch-operation-string", true );
      set_option( options, "elasticsearch-mode", uint16_t(2) );

      fixture.es_index_prefix = string("graphene-") + fc::to_string(uint64_t(rand())) + "-";
      BOOST_TEST_MESSAGE( string("ES index prefix is ") + fixture.es_index_prefix );
      set_option( options, "elasticsearch-index-prefix", fixture.es_index_prefix );
   }
   else if( fixture.current_suite_name != "performance_tests" )
   {
      fixture.app.register_plugin<graphene::account_history::account_history_plugin>(true);
   }

   if( fixture.current_test_name == "elasticsearch_objects" ) {
      fixture.app.register_plugin<graphene::es_objects::es_objects_plugin>(true);

      set_option( options, "es-objects-elasticsearch-url", GRAPHENE_TESTING_ES_URL );
      set_option( options, "es-objects-bulk-replay", uint32_t(1) );
      set_option( options, "es-objects-bulk-sync", uint32_t(1) );
      set_option( options, "es-objects-proposals", true );
      set_option( options, "es-objects-accounts", true );
      set_option( options, "es-objects-assets", true );
      set_option( options, "es-objects-balances", true );
      set_option( options, "es-objects-limit-orders", true );
      set_option( options, "es-objects-backed-assets", true );

      fixture.es_obj_index_prefix = string("objects-") + fc::to_string(uint64_t(rand())) + "-";
      BOOST_TEST_MESSAGE( string("ES_OBJ index prefix is ") + fixture.es_obj_index_prefix );
      set_option( options, "es-objects-index-prefix", fixture.es_obj_index_prefix );
   }

   if( fixture.current_test_name == "asset_in_collateral"
            || fixture.current_test_name == "htlc_database_api"
            || fixture.current_suite_name == "database_api_tests"
            || fixture.current_suite_name == "api_limit_tests" )
   {
      fixture.app.register_plugin<graphene::api_helper_indexes::api_helper_indexes>(true);
   }

   if(fixture.current_test_name == "custom_operations_account_storage_map_test" ||
      fixture.current_test_name == "custom_operations_account_storage_list_test") {
      fixture.app.register_plugin<graphene::custom_operations::custom_operations_plugin>(true);
      set_option( options, "custom-operations-start-block", uint32_t(1) );
      if( fixture.current_test_name == "custom_operations_account_storage_map_test" )
         // Set a small limit
         set_option( options, "api-limit-get-storage-info", uint32_t(6) );
   }

   set_option( options, "bucket-size", string("[15]") );

   fixture.app.register_plugin<graphene::market_history::market_history_plugin>(true);
   fixture.app.register_plugin<graphene::grouped_orders::grouped_orders_plugin>(true);

   return sharable_options;
}

void database_fixture_base::vote_for_delegates_and_validators(uint16_t num_delegates, uint16_t num_producers)
{ try {

   auto &init0 = get_account("init0");
   fund(init0, asset(10));

   flat_set<vote_id_type> votes;

   const auto& wits = db.get_index_type<validator_index>().indices().get<by_id>();
   num_producers = std::min(num_producers, (uint16_t) wits.size());
   auto wit_end = wits.begin();
   std::advance(wit_end, num_producers);
   std::transform(wits.begin(), wit_end,
                  std::inserter(votes, votes.end()),
                  [](const validator_object& w) { return w.vote_id; });

   const auto& comms = db.get_index_type<delegate_index>().indices().get<by_id>();
   num_delegates = std::min(num_delegates, (uint16_t) comms.size());
   auto comm_end = comms.begin();
   std::advance(comm_end, num_delegates);
   std::transform(comms.begin(), comm_end,
                  std::inserter(votes, votes.end()),
                  [](const delegate_object& cm) { return cm.vote_id; });

   account_update_operation op;
   op.account = init0.get_id();
   op.new_options = init0.options;
   op.new_options->votes = votes;
   op.new_options->num_producers = num_producers;
   op.new_options->num_delegates = num_delegates;

   op.fee = db.current_fee_schedule().calculate_fee( op );

   trx.operations.clear();
   trx.operations.push_back(op);
   trx.validate();
   PUSH_TX(db, trx, ~0);
   trx.operations.clear();

} FC_CAPTURE_AND_RETHROW() } // GCOVR_EXCL_LINE

fc::ecc::private_key database_fixture_base::generate_private_key(string seed)
{
   static const fc::ecc::private_key council = fc::ecc::private_key::regenerate(fc::sha256::hash(string("null_key")));
   if( seed == "null_key" )
      return council;
   return fc::ecc::private_key::regenerate(fc::sha256::hash(seed));
}

string database_fixture_base::generate_anon_acct_name()
{
   // names of the form "anon-acct-x123" ; the "x" is a necessary workaround
   return "anon-acct-x" + std::to_string( anon_acct_count++ );
}

void database_fixture_base::verify_asset_supplies( const database& db )
{
   // wlog("*** Begin asset supply verification ***");
   const asset_dynamic_data_object &core_asset_data = db.get_core_asset().dynamic_asset_data_id(db);
   BOOST_CHECK(core_asset_data.fee_pool == 0);

   const auto &statistics_index = db.get_index_type<account_stats_index>().indices();
   const auto &acct_balance_index = db.get_index_type<account_balance_index>().indices();
   const auto &settle_index = db.get_index_type<force_settlement_index>().indices();
   const auto &bids = db.get_index_type<collateral_bid_index>().indices();
   map<asset_id_type, share_type> total_balances;
   map<asset_id_type, share_type> total_debts;
   share_type core_in_orders;
   share_type reported_core_in_orders;

   for (const account_balance_object &b : acct_balance_index)
      total_balances[b.asset_type] += b.balance;
   for (const force_settlement_object &s : settle_index)
      total_balances[s.balance.asset_id] += s.balance.amount;
   for (const collateral_bid_object &b : bids)
      total_balances[b.inv_swan_price.base.asset_id] += b.inv_swan_price.base.amount;
   for (const account_statistics_object &a : statistics_index)
   {
      reported_core_in_orders += a.total_core_in_orders;
      total_balances[asset_id_type()] += a.pending_fees + a.pending_vested_fees;
   }
   for (const limit_order_object &o : db.get_index_type<limit_order_index>().indices())
   {
      asset for_sale = o.amount_for_sale();
      if (for_sale.asset_id == asset_id_type())
         core_in_orders += for_sale.amount;
      total_balances[for_sale.asset_id] += for_sale.amount;
      total_balances[asset_id_type()] += o.deferred_fee;
      total_balances[o.deferred_paid_fee.asset_id] += o.deferred_paid_fee.amount;
   }
   for (const call_order_object &o : db.get_index_type<call_order_index>().indices())
   {
      asset col = o.get_collateral();
      if (col.asset_id == asset_id_type())
         core_in_orders += col.amount;
      total_balances[col.asset_id] += col.amount;
      total_debts[o.get_debt().asset_id] += o.get_debt().amount;
   }
   for (const asset_object &asset_obj : db.get_index_type<asset_index>().indices())
   {
      const auto &dasset_obj = asset_obj.dynamic_asset_data_id(db);
      total_balances[asset_obj.get_id()] += dasset_obj.accumulated_fees;
      total_balances[asset_id_type()] += dasset_obj.fee_pool;
      if (asset_obj.is_backed())
      {
         const auto &bad = asset_obj.backed_asset_data(db);
         total_balances[bad.options.short_backing_asset] += bad.settlement_fund;
      }
      total_balances[asset_obj.get_id()] += dasset_obj.confidential_supply.value;
   }
   for (const vesting_balance_object &vbo : db.get_index_type<vesting_balance_index>().indices())
      total_balances[vbo.balance.asset_id] += vbo.balance.amount;
   for (const fba_accumulator_object &fba : db.get_index_type<simple_index<fba_accumulator_object>>())
      total_balances[asset_id_type()] += fba.accumulated_fba_fees;
   for (const balance_object &bo : db.get_index_type<balance_index>().indices())
      total_balances[bo.balance.asset_id] += bo.balance.amount;

   total_balances[asset_id_type()] += db.get_dynamic_global_properties().validator_budget;

   for (const auto &item : total_debts)
   {
      BOOST_CHECK_EQUAL(item.first(db).dynamic_asset_data_id(db).current_supply.value, item.second.value);
   }

   // htlc
   const auto &htlc_idx = db.get_index_type<htlc_index>().indices().get<by_id>();
   for (auto itr = htlc_idx.begin(); itr != htlc_idx.end(); ++itr)
   {
      total_balances[itr->transfer.asset_id] += itr->transfer.amount;
   }

   for (const asset_object &asset_obj : db.get_index_type<asset_index>().indices())
   {
      BOOST_CHECK_EQUAL(total_balances[asset_obj.get_id()].value, asset_obj.dynamic_asset_data_id(db).current_supply.value);
   }

   BOOST_CHECK_EQUAL(core_in_orders.value, reported_core_in_orders.value);
   //   wlog("***  End  asset supply verification ***");
}

signed_block database_fixture_base::generate_block(uint32_t skip, const fc::ecc::private_key& key, int miss_blocks)
{
   skip |= database::skip_undo_history_check;
   // skip == ~0 will skip checks specified in database::validation_steps
   auto block = db.generate_block(db.get_slot_time(miss_blocks + 1),
                            db.get_scheduled_producer(miss_blocks + 1),
                            key, skip);
   db.clear_pending();
   verify_asset_supplies(db);
   return block;
}

void database_fixture_base::generate_blocks( uint32_t block_count )
{
   for( uint32_t i = 0; i < block_count; ++i )
      generate_block();
}

uint32_t database_fixture_base::generate_blocks(fc::time_point_sec timestamp, bool miss_intermediate_blocks, uint32_t skip)
{
   if( miss_intermediate_blocks )
   {
      generate_block(skip);
      auto slots_to_miss = db.get_slot_at_time(timestamp);
      if( slots_to_miss <= 1 )
         return 1;
      --slots_to_miss;
      generate_block(skip, init_account_priv_key, slots_to_miss);
      return 2;
   }
   uint32_t blocks = 0;
   while( db.head_block_time() < timestamp )
   {
      generate_block(skip);
      ++blocks;
   }
   return blocks;
}

account_create_operation database_fixture_base::make_account(
   const std::string& name /* = "nathan" */,
   public_key_type key /* = key_id_type() */
   )
{ try {
   account_create_operation create_account;
   create_account.registrar = account_id_type();

   create_account.name = name;
   create_account.owner = authority(123, key, 123);
   create_account.active = authority(321, key, 321);
   create_account.options.memo_key = key;
   create_account.options.voting_account = GRAPHENE_PROXY_TO_SELF_ACCOUNT;

   auto& council_delegates = db.get_global_properties().council_delegates;
   if( council_delegates.size() > 0 )
   {
      set<vote_id_type> votes;
      votes.insert(council_delegates[rand() % council_delegates.size()](db).vote_id);
      votes.insert(council_delegates[rand() % council_delegates.size()](db).vote_id);
      votes.insert(council_delegates[rand() % council_delegates.size()](db).vote_id);
      votes.insert(council_delegates[rand() % council_delegates.size()](db).vote_id);
      votes.insert(council_delegates[rand() % council_delegates.size()](db).vote_id);
      create_account.options.votes = flat_set<vote_id_type>(votes.begin(), votes.end());
   }
   create_account.options.num_delegates = create_account.options.votes.size();

   create_account.fee = db.current_fee_schedule().calculate_fee( create_account );
   return create_account;
} FC_CAPTURE_AND_RETHROW() } // GCOVR_EXCL_LINE

account_create_operation database_fixture_base::make_account(
   const std::string& name,
   const account_object& registrar,
   const account_object& referrer,
   uint16_t referrer_percent /* = 100 */,
   public_key_type key /* = public_key_type() */
   )
{
   try
   {
      account_create_operation          create_account;

      create_account.registrar          = registrar.id;
      create_account.referrer           = referrer.id;
      create_account.referrer_percent   = referrer_percent;

      create_account.name = name;
      create_account.owner = authority(123, key, 123);
      create_account.active = authority(321, key, 321);
      create_account.options.memo_key = key;
      create_account.options.voting_account = GRAPHENE_PROXY_TO_SELF_ACCOUNT;

      const vector<delegate_id_type>& council_delegates = db.get_global_properties().council_delegates;
      if( council_delegates.size() > 0 )
      {
         set<vote_id_type> votes;
         votes.insert(council_delegates[rand() % council_delegates.size()](db).vote_id);
         votes.insert(council_delegates[rand() % council_delegates.size()](db).vote_id);
         votes.insert(council_delegates[rand() % council_delegates.size()](db).vote_id);
         votes.insert(council_delegates[rand() % council_delegates.size()](db).vote_id);
         votes.insert(council_delegates[rand() % council_delegates.size()](db).vote_id);
         create_account.options.votes = flat_set<vote_id_type>(votes.begin(), votes.end());
      }
      create_account.options.num_delegates = create_account.options.votes.size();

      create_account.fee = db.current_fee_schedule().calculate_fee( create_account );
      return create_account;
   }
   FC_CAPTURE_AND_RETHROW((name)(referrer_percent))
}

const asset_object& database_fixture_base::get_asset( const string& symbol )const
{
   const auto& idx = db.get_index_type<asset_index>().indices().get<by_symbol>();
   const auto itr = idx.find(symbol);
   assert( itr != idx.end() );
   return *itr;
}

const account_object& database_fixture_base::get_account( const string& name )const
{
   const auto& idx = db.get_index_type<account_index>().indices().get<by_name>();
   const auto itr = idx.find(name);
   assert( itr != idx.end() );
   return *itr;
}

asset_create_operation database_fixture_base::make_backed_asset(
   const string& name,
   account_id_type issuer /* = GRAPHENE_PRODUCERS_ACCOUNT */,
   uint16_t market_fee_percent /* = 100 */ /* 1% */,
   uint16_t flags /* = charge_market_fee */,
   uint16_t precision /* = GRAPHENE_BLOCKCHAIN_PRECISION_DIGITS */,
   asset_id_type backing_asset /* = CORE */,
   share_type max_supply,  /* = GRAPHENE_MAX_SHARE_SUPPLY */
   optional<uint16_t> initial_cr, /* = {} */
   optional<uint16_t> margin_call_fee_ratio /* = {} */
   )
{
   asset_create_operation creator;
   creator.issuer = issuer;
   creator.fee = asset();
   creator.symbol = name;
   creator.common_options.max_supply = max_supply;
   creator.precision = precision;
   creator.common_options.market_fee_percent = market_fee_percent;
   if( issuer == GRAPHENE_PRODUCERS_ACCOUNT )
      flags |= validator_fed_asset;
   creator.common_options.issuer_permissions = flags;
   creator.common_options.flags = flags & ~global_settle;
   creator.common_options.core_exchange_rate = price(asset(1,asset_id_type(1)),asset(1));
   creator.backed_options = backed_asset_options();
   creator.backed_options->short_backing_asset = backing_asset;
   return creator;
}

const asset_object& database_fixture_base::create_backed_asset(
   const string& name,
   account_id_type issuer /* = GRAPHENE_PRODUCERS_ACCOUNT */,
   uint16_t market_fee_percent /* = 100 */ /* 1% */,
   uint16_t flags /* = charge_market_fee */,
   uint16_t precision /* = GRAPHENE_BLOCKCHAIN_PRECISION_DIGITS */,
   asset_id_type backing_asset /* = CORE */,
   share_type max_supply,  /* = GRAPHENE_MAX_SHARE_SUPPLY */
   optional<uint16_t> initial_cr, /* = {} */
   optional<uint16_t> margin_call_fee_ratio /* = {} */
   )
{ try {
   asset_create_operation creator = make_backed_asset( name, issuer, market_fee_percent, flags,
                                                   precision, backing_asset, max_supply, initial_cr,
                                                   margin_call_fee_ratio );
   trx.operations.clear();
   trx.operations.push_back(std::move(creator));
   trx.validate();
   processed_transaction ptx = PUSH_TX(db, trx, ~0);
   trx.operations.clear();
   return db.get<asset_object>(ptx.operation_results[0].get<object_id_type>());
} FC_CAPTURE_AND_RETHROW( (name)(flags) ) } // GCOVR_EXCL_LINE

const asset_object& database_fixture_base::create_prediction_market(
   const string& name,
   account_id_type issuer /* = GRAPHENE_PRODUCERS_ACCOUNT */,
   uint16_t market_fee_percent /* = 100 */ /* 1% */,
   uint16_t flags /* = charge_market_fee */,
   uint16_t precision /* = 2, which seems arbitrary, but historically chosen */,
   asset_id_type backing_asset /* = CORE */
   )
{ try {
   asset_create_operation creator;
   creator.issuer = issuer;
   creator.fee = asset();
   creator.symbol = name;
   creator.common_options.max_supply = GRAPHENE_MAX_SHARE_SUPPLY;
   creator.precision = precision;
   creator.common_options.market_fee_percent = market_fee_percent;
   creator.common_options.issuer_permissions = flags | global_settle;
   creator.common_options.flags = flags & ~global_settle;
   if( issuer == GRAPHENE_PRODUCERS_ACCOUNT )
      creator.common_options.flags |= validator_fed_asset;
   creator.common_options.core_exchange_rate = price(asset(1,asset_id_type(1)),asset(1));
   creator.backed_options = backed_asset_options();
   creator.backed_options->short_backing_asset = backing_asset;
   creator.is_prediction_market = true;
   trx.operations.clear();
   trx.operations.push_back(std::move(creator));
   trx.validate();
   processed_transaction ptx = PUSH_TX(db, trx, ~0);
   trx.operations.clear();
   return db.get<asset_object>(ptx.operation_results[0].get<object_id_type>());
} FC_CAPTURE_AND_RETHROW( (name)(flags) ) } // GCOVR_EXCL_LINE


const asset_object& database_fixture_base::create_user_asset( const string& name )
{
   asset_create_operation creator;
   creator.issuer = account_id_type();
   creator.fee = asset();
   creator.symbol = name;
   creator.common_options.max_supply = 0;
   creator.precision = 2;
   creator.common_options.core_exchange_rate = price(asset(1,asset_id_type(1)),asset(1));
   creator.common_options.max_supply = GRAPHENE_MAX_SHARE_SUPPLY;
   creator.common_options.flags = charge_market_fee;
   creator.common_options.issuer_permissions = charge_market_fee;
   trx.operations.clear();
   trx.operations.push_back(std::move(creator));
   trx.validate();
   processed_transaction ptx = PUSH_TX(db, trx, ~0);
   trx.operations.clear();
   return db.get<asset_object>(ptx.operation_results[0].get<object_id_type>());
}

const asset_object& database_fixture_base::create_user_asset( const string& name, const account_object& issuer,
                                                               uint16_t flags, const price& core_exchange_rate,
                                                               uint8_t precision, uint16_t market_fee_percent,
                                                               additional_asset_options_t additional_options)
{
   asset_create_operation creator;
   creator.issuer = issuer.id;
   creator.fee = asset();
   creator.symbol = name;
   creator.common_options.max_supply = 0;
   creator.precision = precision;
   creator.common_options.core_exchange_rate = core_exchange_rate;
   creator.common_options.max_supply = GRAPHENE_MAX_SHARE_SUPPLY;
   creator.common_options.flags = flags;
   creator.common_options.issuer_permissions = flags;
   creator.common_options.market_fee_percent = market_fee_percent;
   creator.common_options.extensions = std::move(additional_options);
   trx.operations.clear();
   trx.operations.push_back(std::move(creator));
   set_expiration( db, trx );
   trx.validate();
   processed_transaction ptx = PUSH_TX(db, trx, ~0);
   trx.operations.clear();
   return db.get<asset_object>(ptx.operation_results[0].get<object_id_type>());
}

void database_fixture_base::issue_ua( const account_object& recipient, asset amount )
{
   BOOST_TEST_MESSAGE( "Issuing UA" );
   asset_issue_operation op;
   op.issuer = amount.asset_id(db).issuer;
   op.asset_to_issue = amount;
   op.issue_to_account = recipient.id;
   trx.operations.clear();
   trx.operations.push_back(op);
   PUSH_TX( db, trx, ~0 );
   trx.operations.clear();
}

void database_fixture_base::issue_ua( account_id_type recipient_id, asset amount )
{
   issue_ua( recipient_id(db), amount );
}

void database_fixture_base::reserve_asset( account_id_type account, asset amount )
{
   BOOST_TEST_MESSAGE( "Reserving asset" );
   asset_reserve_operation op;
   op.payer = account;
   op.amount_to_reserve = amount;
   trx.operations.clear();
   trx.operations.push_back(op);
   set_expiration( db, trx );
   trx.validate();
   PUSH_TX( db, trx, ~0 );
   trx.operations.clear();
}

void database_fixture_base::change_fees(
   const fee_parameters::flat_set_type& new_params,
   uint32_t new_scale /* = 0 */
   )
{
   const chain_parameters& current_chain_params = db.get_global_properties().parameters;
   const fee_schedule& current_fees = current_chain_params.get_current_fees();

   flat_map< int, fee_parameters > fee_map;
   fee_map.reserve( current_fees.parameters.size() );
   for( const fee_parameters& op_fee : current_fees.parameters )
      fee_map[ op_fee.which() ] = op_fee;
   for( const fee_parameters& new_fee : new_params )
      fee_map[ new_fee.which() ] = new_fee;

   fee_schedule_type new_fees;

   for( const std::pair< int, fee_parameters >& item : fee_map )
      new_fees.parameters.insert( item.second );
   if( new_scale != 0 )
      new_fees.scale = new_scale;

   chain_parameters new_chain_params = current_chain_params;
   new_chain_params.get_mutable_fees() = new_fees;

   db.modify(db.get_global_properties(), [&](global_property_object& p) {
      p.parameters = new_chain_params;
   });
}

const account_object& database_fixture_base::create_account(
   const string& name,
   const public_key_type& key /* = public_key_type() */
   )
{
   trx.operations.clear();
   trx.operations.push_back(make_account(name, key));
   trx.validate();
   processed_transaction ptx = PUSH_TX(db, trx, ~0);
   auto& result = db.get<account_object>(ptx.operation_results[0].get<object_id_type>());
   trx.operations.clear();
   return result;
}

const account_object& database_fixture_base::create_account(
   const string& name,
   const account_object& registrar,
   const account_object& referrer,
   uint16_t referrer_percent /* = 100 (1%)*/,
   const public_key_type& key /*= public_key_type()*/
   )
{
   try
   {
      trx.operations.resize(1);
      trx.operations.back() = (make_account(name, registrar, referrer, referrer_percent, key));
      trx.validate();
      auto r = PUSH_TX(db, trx, ~0);
      const auto& result = db.get<account_object>(r.operation_results[0].get<object_id_type>());
      trx.operations.clear();
      return result;
   }
   FC_CAPTURE_AND_RETHROW( (name)(registrar)(referrer) )
}

const account_object& database_fixture_base::create_account(
   const string& name,
   const private_key_type& key,
   const account_id_type& registrar_id /* = account_id_type() */,
   const account_id_type& referrer_id /* = account_id_type() */,
   uint16_t referrer_percent /* = 100 (1%)*/
   )
{
   try
   {
      trx.operations.clear();

      account_create_operation account_create_op;

      account_create_op.registrar = registrar_id;
      account_create_op.referrer = referrer_id;
      account_create_op.referrer_percent = referrer_percent;
      account_create_op.name = name;
      account_create_op.owner = authority(1234, public_key_type(key.get_public_key()), 1234);
      account_create_op.active = authority(5678, public_key_type(key.get_public_key()), 5678);
      account_create_op.options.memo_key = key.get_public_key();
      account_create_op.options.voting_account = GRAPHENE_PROXY_TO_SELF_ACCOUNT;
      trx.operations.push_back( account_create_op );

      trx.validate();

      processed_transaction ptx = PUSH_TX(db, trx, ~0);
      const account_object& result = db.get<account_object>(ptx.operation_results[0].get<object_id_type>());
      trx.operations.clear();
      return result;
   }
   FC_CAPTURE_AND_RETHROW( (name)(registrar_id)(referrer_id) )
}

const delegate_object& database_fixture_base::create_delegate( const account_object& owner )
{
   delegate_create_operation op;
   op.delegate_account = owner.id;
   trx.operations.clear();
   trx.operations.push_back(op);
   trx.validate();
   processed_transaction ptx = PUSH_TX(db, trx, ~0);
   trx.operations.clear();
   return db.get<delegate_object>(ptx.operation_results[0].get<object_id_type>());
}

const validator_object& database_fixture_base::create_validator(account_id_type owner,
                                                        const fc::ecc::private_key& signing_private_key,
                                                        uint32_t skip_flags )
{
   return create_validator(owner(db), signing_private_key, skip_flags );
}

const validator_object& database_fixture_base::create_validator( const account_object& owner,
                                                        const fc::ecc::private_key& signing_private_key,
                                                        uint32_t skip_flags )
{ try {
   validator_create_operation op;
   op.validator_account = owner.id;
   op.block_producer_key = signing_private_key.get_public_key();
   trx.operations.clear();
   trx.operations.push_back(op);
   trx.validate();
   processed_transaction ptx = PUSH_TX(db, trx, skip_flags );
   trx.clear();
   return db.get<validator_object>(ptx.operation_results[0].get<object_id_type>());
} FC_CAPTURE_AND_RETHROW() } // GCOVR_EXCL_LINE

const worker_object& database_fixture_base::create_worker( const account_id_type owner, const share_type daily_pay, const fc::microseconds& duration )
{ try {
   worker_create_operation op;
   op.owner = owner;
   op.daily_pay = daily_pay;
   op.initializer = burn_worker_initializer();
   op.work_begin_date = db.head_block_time();
   op.work_end_date = op.work_begin_date + duration;
   trx.operations.clear();
   trx.operations.push_back(op);
   trx.validate();
   processed_transaction ptx = PUSH_TX(db, trx, ~0);
   trx.clear();
   return db.get<worker_object>(ptx.operation_results[0].get<object_id_type>());
} FC_CAPTURE_AND_RETHROW() } // GCOVR_EXCL_LINE

uint64_t database_fixture_base::fund(
   const account_object& account,
   const asset& amount /* = asset(500000) */
   )
{
   transfer(account_id_type()(db), account, amount);
   return get_balance(account, amount.asset_id(db));
}

void database_fixture_base::sign(signed_transaction& trx, const fc::ecc::private_key& key)
{
   trx.sign( key, db.get_chain_id() );
}

digest_type database_fixture_base::digest( const transaction& tx )
{
   return tx.digest();
}

limit_order_create_operation database_fixture_base::make_limit_order_create_op(
                                                const account_id_type& user, const asset& amount, const asset& recv,
                                                const time_point_sec& order_expiration ) const
{
   limit_order_create_operation buy_order;
   buy_order.seller = user;
   buy_order.amount_to_sell = amount;
   buy_order.min_to_receive = recv;
   buy_order.expiration = order_expiration;
   return buy_order;
}

const limit_order_object* database_fixture_base::create_sell_order(
                                                const account_id_type& user, const asset& amount, const asset& recv,
                                                const time_point_sec& order_expiration,
                                                const price& fee_core_exchange_rate )
{
   set_expiration( db, trx );
   trx.operations.clear();

   limit_order_create_operation buy_order = make_limit_order_create_op( user, amount, recv, order_expiration );
   trx.operations = {buy_order};
   for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op, fee_core_exchange_rate);
   trx.validate();
   auto processed = PUSH_TX(db, trx, ~0);
   trx.operations.clear();
   verify_asset_supplies(db);
   return db.find<limit_order_object>( processed.operation_results[0].get<object_id_type>() );
}

const limit_order_object* database_fixture_base::create_sell_order(
                                                const account_object& user, const asset& amount, const asset& recv,
                                                const time_point_sec& order_expiration,
                                                const price& fee_core_exchange_rate )
{
   return create_sell_order( user.get_id(), amount, recv, order_expiration, fee_core_exchange_rate );
}

asset database_fixture_base::cancel_limit_order( const limit_order_object& order )
{
  limit_order_cancel_operation cancel_order;
  cancel_order.fee_paying_account = order.seller;
  cancel_order.order = order.id;
  trx.operations.clear();
  trx.operations.push_back(cancel_order);
  for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);
  trx.validate();
  auto processed = PUSH_TX(db, trx, ~0);
  trx.operations.clear();
  verify_asset_supplies(db);
  return processed.operation_results[0].get<asset>();
}

void database_fixture_base::transfer(
   account_id_type from,
   account_id_type to,
   const asset& amount,
   const asset& fee /* = asset() */
   )
{
   transfer(from(db), to(db), amount, fee);
}

void database_fixture_base::transfer(
   const account_object& from,
   const account_object& to,
   const asset& amount,
   const asset& fee /* = asset() */ )
{
   try
   {
      set_expiration( db, trx );
      transfer_operation trans;
      trans.from = from.id;
      trans.to   = to.id;
      trans.amount = amount;
      trx.operations.clear();
      trx.operations.push_back(trans);

      if( fee == asset() )
      {
         for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);
      }
      trx.validate();
      PUSH_TX(db, trx, ~0);
      verify_asset_supplies(db);
      trx.operations.clear();
   } FC_CAPTURE_AND_RETHROW( (from.id)(to.id)(amount)(fee) )
}

void database_fixture_base::update_feed_producers( const asset_object& mia, flat_set<account_id_type> producers )
{ try {
   set_expiration( db, trx );
   trx.operations.clear();
   asset_update_feed_producers_operation op;
   op.asset_to_update = mia.id;
   op.issuer = mia.issuer;
   op.new_feed_producers = std::move(producers);
   trx.operations = {std::move(op)};

   for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);
   trx.validate();
   PUSH_TX(db, trx, ~0);
   trx.operations.clear();
   verify_asset_supplies(db);
} FC_CAPTURE_AND_RETHROW( (mia)(producers) ) } // GCOVR_EXCL_LINE

void database_fixture_base::publish_feed( const asset_object& mia, const account_object& by, const price_feed& f )
{
   set_expiration( db, trx );
   trx.operations.clear();

   asset_publish_feed_operation op;
   op.publisher = by.id;
   op.asset_id = mia.id;
   op.feed = f;
   if( op.feed.core_exchange_rate.is_null() )
   {
      op.feed.core_exchange_rate = op.feed.settlement_price;
      op.feed.core_exchange_rate.quote.asset_id = asset_id_type();
   }
   trx.operations.emplace_back( std::move(op) );

   for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);
   trx.validate();
   PUSH_TX(db, trx, ~0);
   trx.operations.clear();
   verify_asset_supplies(db);
}

void database_fixture_base::publish_feed(const account_id_type& publisher,
      const asset_id_type& asset1, int64_t amount1,
      const asset_id_type& asset2, int64_t amount2,
      const asset_id_type& core_id)
{
   const asset_object& a1 = asset1(db);
   const asset_object& a2 = asset2(db);
   const asset_object& core = core_id(db);
   asset_publish_feed_operation op;
   op.publisher = publisher;
   op.asset_id = asset2;
   op.feed.settlement_price = ~price(a1.amount(amount1),a2.amount(amount2));
   op.feed.core_exchange_rate = ~price(core.amount(amount1), a2.amount(amount2));
   trx.operations.clear();
   trx.operations.emplace_back(std::move(op));
   for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);
   set_expiration( db, trx );
   PUSH_TX( db, trx, ~0);
   verify_asset_supplies(db);
   generate_block();
   trx.clear();
}

void database_fixture_base::force_global_settle( const asset_object& what, const price& p )
{ try {
   set_expiration( db, trx );
   trx.operations.clear();
   asset_global_settle_operation sop;
   sop.issuer = what.issuer;
   sop.asset_to_settle = what.id;
   sop.settle_price = p;
   trx.operations.push_back(sop);
   for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);
   trx.validate();
   PUSH_TX(db, trx, ~0);
   trx.operations.clear();
   verify_asset_supplies(db);
} FC_CAPTURE_AND_RETHROW( (what)(p) ) } // GCOVR_EXCL_LINE

operation_result database_fixture_base::force_settle( const account_object& who, asset what )
{ try {
   set_expiration( db, trx );
   trx.operations.clear();
   asset_settle_operation sop;
   sop.account = who.id;
   sop.amount = what;
   trx.operations.push_back(sop);
   for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);
   trx.validate();
   processed_transaction ptx = PUSH_TX(db, trx, ~0);
   const operation_result& op_result = ptx.operation_results.front();
   trx.operations.clear();
   verify_asset_supplies(db);
   return op_result;
} FC_CAPTURE_AND_RETHROW( (who)(what) ) } // GCOVR_EXCL_LINE

const call_order_object* database_fixture_base::borrow( const account_object& who, asset what, asset collateral,
                                                   optional<uint16_t> target_cr )
{ try {
   set_expiration( db, trx );
   trx.operations.clear();
   call_order_update_operation update = {};
   update.funding_account = who.id;
   update.delta_collateral = collateral;
   update.delta_debt = what;
   update.extensions.value.target_collateral_ratio = target_cr;
   trx.operations.push_back(update);
   for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);
   trx.validate();
   PUSH_TX(db, trx, ~0);
   trx.operations.clear();
   verify_asset_supplies(db);

   auto& call_idx = db.get_index_type<call_order_index>().indices().get<by_account>();
   auto itr = call_idx.find( boost::make_tuple(who.get_id(), what.asset_id) );
   const call_order_object* call_obj = nullptr;

   if( itr != call_idx.end() )
      call_obj = &*itr;
   return call_obj;
} FC_CAPTURE_AND_RETHROW( (who.name)(what)(collateral)(target_cr) ) } // GCOVR_EXCL_LINE

void database_fixture_base::cover(const account_object& who, asset what, asset collateral, optional<uint16_t> target_cr)
{ try {
   set_expiration( db, trx );
   trx.operations.clear();
   call_order_update_operation update = {};
   update.funding_account = who.id;
   update.delta_collateral = -collateral;
   update.delta_debt = -what;
   update.extensions.value.target_collateral_ratio = target_cr;
   trx.operations.push_back(update);
   for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);
   trx.validate();
   PUSH_TX(db, trx, ~0);
   trx.operations.clear();
   verify_asset_supplies(db);
} FC_CAPTURE_AND_RETHROW( (who.name)(what)(collateral)(target_cr) ) } // GCOVR_EXCL_LINE

void database_fixture_base::bid_collateral(const account_object& who, const asset& to_bid, const asset& to_cover)
{ try {
   set_expiration( db, trx );
   trx.operations.clear();
   bid_collateral_operation bid;
   bid.bidder = who.id;
   bid.additional_collateral = to_bid;
   bid.debt_covered = to_cover;
   trx.operations.push_back(bid);
   for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);
   trx.validate();
   PUSH_TX(db, trx, ~0);
   trx.operations.clear();
   verify_asset_supplies(db);
} FC_CAPTURE_AND_RETHROW( (who.name)(to_bid)(to_cover) ) } // GCOVR_EXCL_LINE

void database_fixture_base::fund_fee_pool( const account_object& from, const asset_object& asset_to_fund, const share_type amount )
{
   asset_fund_fee_pool_operation fund;
   fund.from_account = from.id;
   fund.asset_id = asset_to_fund.id;
   fund.amount = amount;
   trx.operations.clear();
   trx.operations.push_back( fund );

   for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);
   trx.validate();
   set_expiration( db, trx );
   PUSH_TX(db, trx, ~0);
   trx.operations.clear();
   verify_asset_supplies(db);
}

void database_fixture_base::enable_fees()
{
   db.modify(global_property_id_type()(db), [](global_property_object& gpo)
   {
      gpo.parameters.get_mutable_fees() = fee_schedule::get_default();
   });
}

void database_fixture_base::upgrade_to_lifetime_member(account_id_type account)
{
   upgrade_to_lifetime_member(account(db));
}

void database_fixture_base::upgrade_to_lifetime_member( const account_object& account )
{
   try
   {
      account_upgrade_operation op;
      op.account_to_upgrade = account.get_id();
      op.upgrade_to_lifetime_member = true;
      op.fee = db.get_global_properties().parameters.get_current_fees().calculate_fee(op);
      trx.operations = {op};
      PUSH_TX(db, trx, ~0);
      FC_ASSERT( op.account_to_upgrade(db).is_lifetime_member() );
      trx.clear();
      verify_asset_supplies(db);
   }
   FC_CAPTURE_AND_RETHROW((account))
}

void database_fixture_base::upgrade_to_annual_member(account_id_type account)
{
   upgrade_to_annual_member(account(db));
}

void database_fixture_base::upgrade_to_annual_member(const account_object& account)
{
   try {
      account_upgrade_operation op;
      op.account_to_upgrade = account.get_id();
      op.fee = db.get_global_properties().parameters.get_current_fees().calculate_fee(op);
      trx.operations = {op};
      PUSH_TX(db, trx, ~0);
      FC_ASSERT( op.account_to_upgrade(db).is_member(db.head_block_time()) );
      trx.clear();
      verify_asset_supplies(db);
   } FC_CAPTURE_AND_RETHROW((account))
}

void database_fixture_base::print_market( const string& syma, const string& symb )const
{
   const auto& limit_idx = db.get_index_type<limit_order_index>();
   const auto& price_idx = limit_idx.indices().get<by_price>();

   cerr << std::fixed;
   cerr.precision(5);
   cerr << std::setw(10) << std::left  << "NAME"      << " ";
   cerr << std::setw(16) << std::right << "FOR SALE"  << " ";
   cerr << std::setw(16) << std::right << "FOR WHAT"  << " ";
   cerr << std::setw(10) << std::right << "PRICE (S/W)"   << " ";
   cerr << std::setw(10) << std::right << "1/PRICE (W/S)" << "\n";
   cerr << string(70, '=') << std::endl;
   auto cur = price_idx.begin();
   while( cur != price_idx.end() )
   {
      cerr << std::setw( 10 ) << std::left   << cur->seller(db).name << " ";
      cerr << std::setw( 10 ) << std::right  << cur->for_sale.value << " ";
      cerr << std::setw( 5 )  << std::left   << cur->amount_for_sale().asset_id(db).symbol << " ";
      cerr << std::setw( 10 ) << std::right  << cur->amount_to_receive().amount.value << " ";
      cerr << std::setw( 5 )  << std::left   << cur->amount_to_receive().asset_id(db).symbol << " ";
      cerr << std::setw( 10 ) << std::right  << cur->sell_price.to_real() << " ";
      cerr << std::setw( 10 ) << std::right  << (~cur->sell_price).to_real() << " ";
      cerr << "\n";
      ++cur;
   }
}

string database_fixture_base::pretty( const asset& a )const
{
  std::stringstream ss;
  ss << a.amount.value << " ";
  ss << a.asset_id(db).symbol;
  return ss.str();
}

void database_fixture_base::print_limit_order( const limit_order_object& cur )const
{
  std::cout << std::setw(10) << cur.seller(db).name << " ";
  std::cout << std::setw(10) << "LIMIT" << " ";
  std::cout << std::setw(16) << pretty( cur.amount_for_sale() ) << " ";
  std::cout << std::setw(16) << pretty( cur.amount_to_receive() ) << " ";
  std::cout << std::setw(16) << cur.sell_price.to_real() << " ";
}

void database_fixture_base::print_call_orders()const
{
  cout << std::fixed;
  cout.precision(5);
  cout << std::setw(10) << std::left  << "NAME"      << " ";
  cout << std::setw(10) << std::right << "TYPE"      << " ";
  cout << std::setw(16) << std::right << "DEBT"  << " ";
  cout << std::setw(16) << std::right << "COLLAT"  << " ";
  cout << std::setw(16) << std::right << "CALL PRICE(D/C)"     << " ";
  cout << std::setw(16) << std::right << "~CALL PRICE(C/D)"     << " ";
  cout << std::setw(16) << std::right << "SWAN(D/C)"     << " ";
  cout << std::setw(16) << std::right << "SWAN(C/D)"     << "\n";
  cout << string(70, '=');

  for( const call_order_object& o : db.get_index_type<call_order_index>().indices() )
  {
     std::cout << "\n";
     cout << std::setw( 10 ) << std::left   << o.borrower(db).name << " ";
     cout << std::setw( 16 ) << std::right  << pretty( o.get_debt() ) << " ";
     cout << std::setw( 16 ) << std::right  << pretty( o.get_collateral() ) << " ";
     cout << std::setw( 16 ) << std::right  << o.call_price.to_real() << " ";
     cout << std::setw( 16 ) << std::right  << (~o.call_price).to_real() << " ";
     cout << std::setw( 16 ) << std::right  << (o.get_debt()/o.get_collateral()).to_real() << " ";
     cout << std::setw( 16 ) << std::right  << (~(o.get_debt()/o.get_collateral())).to_real() << " ";
  }
     std::cout << "\n";
}

void database_fixture_base::print_joint_market( const string& syma, const string& symb )const
{
  cout << std::fixed;
  cout.precision(5);

  cout << std::setw(10) << std::left  << "NAME"      << " ";
  cout << std::setw(10) << std::right << "TYPE"      << " ";
  cout << std::setw(16) << std::right << "FOR SALE"  << " ";
  cout << std::setw(16) << std::right << "FOR WHAT"  << " ";
  cout << std::setw(16) << std::right << "PRICE (S/W)" << "\n";
  cout << string(70, '=');

  const auto& limit_idx = db.get_index_type<limit_order_index>();
  const auto& limit_price_idx = limit_idx.indices().get<by_price>();

  auto limit_itr = limit_price_idx.begin();
  while( limit_itr != limit_price_idx.end() )
  {
     std::cout << std::endl;
     print_limit_order( *limit_itr );
     ++limit_itr;
  }
}

int64_t database_fixture_base::get_balance( account_id_type account, asset_id_type a )const
{
  return db.get_balance(account, a).amount.value;
}

int64_t database_fixture_base::get_balance( const account_object& account, const asset_object& a )const
{
  return db.get_balance(account.get_id(), a.get_id()).amount.value;
}

int64_t database_fixture_base::get_market_fee_reward( account_id_type account_id, asset_id_type asset_id)const
{
   return db.get_market_fee_vesting_balance(account_id, asset_id).amount.value;
}

int64_t database_fixture_base::get_market_fee_reward( const account_object& account, const asset_object& asset )const
{
  return get_market_fee_reward(account.get_id(), asset.get_id());
}

vector< operation_history_object > database_fixture_base::get_operation_history( account_id_type account_id )const
{
   vector< operation_history_object > result;
   const auto& stats = account_id(db).statistics(db);
   if(stats.most_recent_op == account_history_id_type())
      return result;

   const account_history_object* node = &stats.most_recent_op(db);
   while( true )
   {
      result.push_back( node->operation_id(db) );
      if(node->next == account_history_id_type())
         break;
      node = db.find(node->next);
   }
   return result;
}

vector< graphene::market_history::order_history_object > database_fixture_base::get_market_order_history( asset_id_type a, asset_id_type b )const
{
   const auto& history_idx = db.get_index_type<graphene::market_history::history_index>().indices().get<graphene::market_history::by_key>();
   graphene::market_history::history_key hkey;
   if( a > b ) std::swap(a,b);
   hkey.base = a;
   hkey.quote = b;
   hkey.sequence = std::numeric_limits<int64_t>::min();
   auto itr = history_idx.lower_bound( hkey );
   vector<graphene::market_history::order_history_object> result;
   while( itr != history_idx.end())
   {
       result.push_back( *itr );
       ++itr;
   }
   return result;
}

flat_map< uint64_t, graphene::chain::fee_parameters > database_fixture_base::get_htlc_fee_parameters()
{
   flat_map<uint64_t, graphene::chain::fee_parameters> ret_val;

   htlc_create_operation::fee_parameters_type create_param;
   create_param.fee_per_day = 2 * GRAPHENE_BLOCKCHAIN_PRECISION;
   create_param.fee = 2 * GRAPHENE_BLOCKCHAIN_PRECISION;
   ret_val[((operation)htlc_create_operation()).which()] = create_param;

   htlc_redeem_operation::fee_parameters_type redeem_param;
   redeem_param.fee = 2 * GRAPHENE_BLOCKCHAIN_PRECISION;
   redeem_param.price_per_kbyte = 2 * GRAPHENE_BLOCKCHAIN_PRECISION;
   ret_val[((operation)htlc_redeem_operation()).which()] = redeem_param;

   htlc_extend_operation::fee_parameters_type extend_param;
   extend_param.fee = 2 * GRAPHENE_BLOCKCHAIN_PRECISION;
   extend_param.fee_per_day = 2 * GRAPHENE_BLOCKCHAIN_PRECISION;
   ret_val[((operation)htlc_extend_operation()).which()] = extend_param;

   // set the transfer kb fee to something other than default, to verify we're looking
   // at the correct fee
   transfer_operation::fee_parameters_type transfer_param;
   transfer_param.price_per_kbyte *= 2;
   ret_val[ ((operation)transfer_operation()).which() ] = transfer_param;

   return ret_val;
}

void database_fixture_base::set_htlc_council_parameters()
{
   // htlc fees
   // get existing fee_schedule
   const chain_parameters& existing_params = db.get_global_properties().parameters;
   const fee_schedule_type& existing_fee_schedule = *(existing_params.current_fees);
   // create a new fee_shedule
   std::shared_ptr<fee_schedule_type> new_fee_schedule = std::make_shared<fee_schedule_type>();
   new_fee_schedule->scale = GRAPHENE_100_PERCENT;
   // replace the old with the new
   flat_map<uint64_t, graphene::chain::fee_parameters> htlc_fees = get_htlc_fee_parameters();
   for(auto param : existing_fee_schedule.parameters)
   {
      auto itr = htlc_fees.find(param.which());
      if (itr == htlc_fees.end()) {
         // Only define fees for operations which are already forked in!
         if (hardfork_visitor(db.head_block_time()).visit(param.which()))
            new_fee_schedule->parameters.insert(param);
      } else {
         new_fee_schedule->parameters.insert( (*itr).second);
      }
   }
   // htlc parameters
   proposal_create_operation cop = proposal_create_operation::council_proposal(
         db.get_global_properties().parameters, db.head_block_time());
   cop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
   cop.expiration_time = db.head_block_time() + *cop.review_period_seconds + 10;
   delegate_update_global_parameters_operation uop;
   graphene::chain::htlc_options new_params;
   new_params.max_preimage_size = 19200;
   new_params.max_timeout_secs = 60 * 60 * 24 * 28;
   uop.new_parameters.extensions.value.updatable_htlc_options = new_params;
   uop.new_parameters.current_fees = new_fee_schedule;
   cop.proposed_ops.emplace_back(uop);

   trx.operations.clear();
   trx.operations.push_back(cop);
   graphene::chain::processed_transaction proc_trx = db.push_transaction(trx);
   trx.clear();
   proposal_id_type good_proposal_id { proc_trx.operation_results[0].get<object_id_type>() };

   proposal_update_operation puo;
   puo.proposal = good_proposal_id;
   puo.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
   puo.key_approvals_to_add.emplace( init_account_priv_key.get_public_key() );
   trx.operations.push_back(puo);
   sign( trx, init_account_priv_key );
   db.push_transaction(trx);
   trx.clear();

   generate_blocks( good_proposal_id( db ).expiration_time + 5 );
   generate_blocks( db.get_dynamic_global_properties().next_maintenance_time );
   generate_block();   // get the maintenance skip slots out of the way

}

namespace test {

void set_expiration( const database& db, transaction& tx )
{
   const chain_parameters& params = db.get_global_properties().parameters;
   tx.set_reference_block(db.head_block_id());
   tx.set_expiration( db.head_block_time() + fc::seconds( params.block_interval * (params.maintenance_skip_slots + 1) * 3 ) );
   return;
}

bool _push_block( database& db, const signed_block& b, uint32_t skip_flags /* = 0 */ )
{
   return db.push_block( b, skip_flags);
}

processed_transaction _push_transaction( database& db, const signed_transaction& tx, uint32_t skip_flags /* = 0 */ )
{ try {
   auto pt = db.push_transaction( precomputable_transaction(tx), skip_flags );
   database_fixture_base::verify_asset_supplies(db);
   return pt;
} FC_CAPTURE_AND_RETHROW((tx)) } // GCOVR_EXCL_LINE

} // graphene::chain::test

} } // graphene::chain
