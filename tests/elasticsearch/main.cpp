#include <graphene/chain/hardfork.hpp>
#include <graphene/app/api.hpp>
#include <graphene/utilities/tempdir.hpp>
#include <fc/crypto/digest.hpp>

#include <graphene/utilities/elasticsearch.hpp>
#include <graphene/elasticsearch/elasticsearch_plugin.hpp>

#include "../common/init_unit_test_suite.hpp"
#include "../common/database_fixture.hpp"
#include "../common/elasticsearch.hpp"
#include "../common/utils.hpp"

#define ES_WAIT_TIME (fc::milliseconds(10000))

using namespace graphene::chain;
using namespace graphene::chain::test;
using namespace graphene::app;
using namespace graphene::tests::utils;

extern std::string GRAPHENE_TESTING_ES_URL;

BOOST_FIXTURE_TEST_SUITE( elasticsearch_tests, database_fixture )

BOOST_AUTO_TEST_CASE(elasticsearch_account_history) {
   try {

      CURL *curl; // curl handler
      curl = curl_easy_init();
      curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);

      graphene::utilities::ES es;
      es.curl = curl;
      es.elasticsearch_url = GRAPHENE_TESTING_ES_URL;
      es.index_prefix = es_index_prefix;

      // delete all first
      auto delete_account_history = graphene::utilities::deleteAll(es);

      BOOST_REQUIRE(delete_account_history); // require successful deletion
      if(delete_account_history) { // all records deleted

         //account_id_type() do 3 ops
         create_backed_asset("USD", account_id_type());
         auto dan = create_account("dan");
         auto bob = create_account("bob");

         generate_block();

         string query = "{ \"query\" : { \"bool\" : { \"must\" : [{\"match_all\": {}}] } } }";
         es.endpoint = es.index_prefix + "*/_count";
         es.query = query;

         string res;
         variant j;
         string total;

         wait_for( ES_WAIT_TIME,  [&]() {
            res = graphene::utilities::simpleQuery(es);
            j = fc::json::from_string(res);
            total = j["count"].as_string();
            return (total == "5");
         });

         es.endpoint = es.index_prefix + "*/_search";
         res = graphene::utilities::simpleQuery(es);
         j = fc::json::from_string(res);
         auto first_id = j["hits"]["hits"][size_t(0)]["_id"].as_string();
         BOOST_CHECK_EQUAL(first_id, "2.9.0");

         generate_block();
         auto willie = create_account("willie");
         generate_block();

         es.endpoint = es.index_prefix + "*/_count";

         wait_for( ES_WAIT_TIME,  [&]() {
            res = graphene::utilities::simpleQuery(es);
            j = fc::json::from_string(res);
            total = j["count"].as_string();
            return (total == "7");
         });

         // do some transfers in 1 block
         transfer(account_id_type()(db), bob, asset(100));
         transfer(account_id_type()(db), bob, asset(200));
         transfer(account_id_type()(db), bob, asset(300));

         generate_block();

         wait_for( ES_WAIT_TIME,  [&]() {
            res = graphene::utilities::simpleQuery(es);
            j = fc::json::from_string(res);
            total = j["count"].as_string();
            return (total == "13");
         });

         // check the visitor data
         auto block_date = db.head_block_time();
         std::string index_name = es_index_prefix + block_date.to_iso_string().substr( 0, 7 ); // yyyy-MM

         es.endpoint = index_name + "/_doc/2.9.12"; // we know last op is a transfer of amount 300
         res = graphene::utilities::getEndPoint(es);
         j = fc::json::from_string(res);
         auto last_transfer_amount = j["_source"]["operation_history"]["op_object"]["amount_"]["amount"].as_string();
         BOOST_CHECK_EQUAL(last_transfer_amount, "300");
         auto last_transfer_payer = j["_source"]["operation_history"]["fee_payer"].as_string();
         BOOST_CHECK_EQUAL(last_transfer_payer, "1.2.0");
         auto is_virtual = j["_source"]["operation_history"]["is_virtual"].as_bool();
         BOOST_CHECK( !is_virtual );

      }
   }
   catch (fc::exception &e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(elasticsearch_objects) {
   try {

      CURL *curl; // curl handler
      curl = curl_easy_init();
      curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);

      graphene::utilities::ES es;
      es.curl = curl;
      es.elasticsearch_url = GRAPHENE_TESTING_ES_URL;
      es.index_prefix = es_obj_index_prefix;

      // The head block number is 1
      BOOST_CHECK_EQUAL( db.head_block_num(), 1u );

      generate_block();
      set_expiration( db, trx );

      // delete all first, this will delete genesis data and data inserted at block 1
      auto delete_objects = graphene::utilities::deleteAll(es);
      BOOST_REQUIRE(delete_objects); // require successful deletion

      generate_block();

      if(delete_objects) { // all records deleted

         // user asset and backed asset
         create_backed_asset("USD", account_id_type());
         generate_block();

         string query = "{ \"query\" : { \"bool\" : { \"must\" : [{\"match_all\": {}}] } } }";
         es.endpoint = es.index_prefix + "*/_count";
         es.query = query;

         string res;
         variant j;
         string total;

         wait_for( ES_WAIT_TIME,  [&]() {
            res = graphene::utilities::simpleQuery(es);
            j = fc::json::from_string(res);
            total = j["count"].as_string();
            return (total == "2");
         });

         es.endpoint = es.index_prefix + "asset/_search";
         res = graphene::utilities::simpleQuery(es);
         j = fc::json::from_string(res);
         auto first_id = j["hits"]["hits"][size_t(0)]["_source"]["symbol"].as_string();
         BOOST_CHECK_EQUAL(first_id, "USD");

         auto backed_asset_data_id = j["hits"]["hits"][size_t(0)]["_source"]["backed_asset_data_id"].as_string();
         es.endpoint = es.index_prefix + "backedasset/_search";
         es.query = "{ \"query\" : { \"bool\": { \"must\" : [{ \"term\": { \"object_id\": \""
                  + backed_asset_data_id + "\"}}] } } }";
         res = graphene::utilities::simpleQuery(es);
         j = fc::json::from_string(res);
         auto backedasset_object_id = j["hits"]["hits"][size_t(0)]["_source"]["object_id"].as_string();
         BOOST_CHECK_EQUAL(backedasset_object_id, backed_asset_data_id);
      }
   }
   catch (fc::exception &e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(elasticsearch_history_api) {
   try {
      CURL *curl; // curl handler
      curl = curl_easy_init();
      curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);

      graphene::utilities::ES es;
      es.curl = curl;
      es.elasticsearch_url = GRAPHENE_TESTING_ES_URL;
      es.index_prefix = es_index_prefix;

      generate_block();
      set_expiration( db, trx );

      auto delete_account_history = graphene::utilities::deleteAll(es);
      BOOST_REQUIRE(delete_account_history); // require successful deletion

      generate_block();

      if(delete_account_history) {

         create_backed_asset("USD", account_id_type()); // create op 0
         const account_object& dan = create_account("dan"); // create op 1
         create_backed_asset("CNY", dan.get_id()); // create op 2
         create_backed_asset("BTC", account_id_type()); // create op 3
         create_backed_asset("XMR", dan.get_id()); // create op 4
         create_backed_asset("EUR", account_id_type()); // create op 5
         create_backed_asset("OIL", dan.get_id()); // create op 6

         generate_block();

         // Test history APIs
         graphene::app::history_api hist_api(app);

         // f(A, 0, 4, 9) = { 5, 3, 1, 0 }
         auto histories = hist_api.get_account_history(
               "1.2.0", operation_history_id_type(), 4, operation_history_id_type(9));

         wait_for( ES_WAIT_TIME,  [&]() {
            histories = hist_api.get_account_history(
                  "1.2.0", operation_history_id_type(), 4, operation_history_id_type(9));
            return (histories.size() == 4u);
         });
         BOOST_REQUIRE_EQUAL(histories.size(), 4u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);
         BOOST_CHECK_EQUAL(histories[2].id.instance(), 1u);
         BOOST_CHECK_EQUAL(histories[3].id.instance(), 0u);

         BOOST_CHECK( !histories[0].is_virtual );
         BOOST_CHECK( histories[0].block_time == db.head_block_time() );

         // f(A, 0, 4, 6) = { 5, 3, 1, 0 }
         histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 4, operation_history_id_type(6));
         BOOST_REQUIRE_EQUAL(histories.size(), 4u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);
         BOOST_CHECK_EQUAL(histories[2].id.instance(), 1u);
         BOOST_CHECK_EQUAL(histories[3].id.instance(), 0u);

         // f(A, 0, 4, 5) = { 5, 3, 1, 0 }
         histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 4, operation_history_id_type(5));
         BOOST_REQUIRE_EQUAL(histories.size(), 4u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);
         BOOST_CHECK_EQUAL(histories[2].id.instance(), 1u);
         BOOST_CHECK_EQUAL(histories[3].id.instance(), 0u);

         // f(A, 0, 4, 4) = { 3, 1, 0 }
         histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 4, operation_history_id_type(4));
         BOOST_REQUIRE_EQUAL(histories.size(), 3u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 3u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 1u);
         BOOST_CHECK_EQUAL(histories[2].id.instance(), 0u);

         // f(A, 0, 4, 3) = { 3, 1, 0 }
         histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 4, operation_history_id_type(3));
         BOOST_REQUIRE_EQUAL(histories.size(), 3u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 3u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 1u);
         BOOST_CHECK_EQUAL(histories[2].id.instance(), 0u);

         // f(A, 0, 4, 2) = { 1, 0 }
         histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 4, operation_history_id_type(2));
         BOOST_REQUIRE_EQUAL(histories.size(), 2u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 1u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 0u);

         // f(A, 0, 4, 1) = { 1, 0 }
         histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 4, operation_history_id_type(1));
         BOOST_REQUIRE_EQUAL(histories.size(), 2u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 1u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 0u);

         // f(A, 0, 4, 0) = { 5, 3, 1, 0 }
         histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 4, operation_history_id_type());
         BOOST_REQUIRE_EQUAL(histories.size(), 4u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);
         BOOST_CHECK_EQUAL(histories[2].id.instance(), 1u);
         BOOST_CHECK_EQUAL(histories[3].id.instance(), 0u);

         // f(A, 1, 5, 9) = { 5, 3 }
         histories = hist_api.get_account_history("1.2.0", operation_history_id_type(1), 5, operation_history_id_type(9));
         BOOST_REQUIRE_EQUAL(histories.size(), 2u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);

         // f(A, 1, 5, 6) = { 5, 3 }
         histories = hist_api.get_account_history("1.2.0", operation_history_id_type(1), 5, operation_history_id_type(6));
         BOOST_REQUIRE_EQUAL(histories.size(), 2u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);

         // f(A, 1, 5, 5) = { 5, 3 }
         histories = hist_api.get_account_history("1.2.0", operation_history_id_type(1), 5, operation_history_id_type(5));
         BOOST_REQUIRE_EQUAL(histories.size(), 2u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);

         // f(A, 1, 5, 4) = { 3 }
         histories = hist_api.get_account_history("1.2.0", operation_history_id_type(1), 5, operation_history_id_type(4));
         BOOST_REQUIRE_EQUAL(histories.size(), 1u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 3u);

         // f(A, 1, 5, 3) = { 3 }
         histories = hist_api.get_account_history("1.2.0", operation_history_id_type(1), 5, operation_history_id_type(3));
         BOOST_REQUIRE_EQUAL(histories.size(), 1u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 3u);

         // f(A, 1, 5, 2) = { }
         histories = hist_api.get_account_history("1.2.0", operation_history_id_type(1), 5, operation_history_id_type(2));
         BOOST_CHECK_EQUAL(histories.size(), 0u);

         // f(A, 1, 5, 1) = { }
         histories = hist_api.get_account_history("1.2.0", operation_history_id_type(1), 5, operation_history_id_type(1));
         BOOST_CHECK_EQUAL(histories.size(), 0u);

         // f(A, 1, 5, 0) = { 5, 3 }
         histories = hist_api.get_account_history("1.2.0", operation_history_id_type(1), 5, operation_history_id_type(0));
         BOOST_REQUIRE_EQUAL(histories.size(), 2u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);

         // f(A, 0, 3, 9) = { 5, 3, 1 }
         histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 3, operation_history_id_type(9));
         BOOST_REQUIRE_EQUAL(histories.size(), 3u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);
         BOOST_CHECK_EQUAL(histories[2].id.instance(), 1u);

         // f(A, 0, 3, 6) = { 5, 3, 1 }
         histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 3, operation_history_id_type(6));
         BOOST_REQUIRE_EQUAL(histories.size(), 3u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);
         BOOST_CHECK_EQUAL(histories[2].id.instance(), 1u);

         // f(A, 0, 3, 5) = { 5, 3, 1 }
         histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 3, operation_history_id_type(5));
         BOOST_REQUIRE_EQUAL(histories.size(), 3u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);
         BOOST_CHECK_EQUAL(histories[2].id.instance(), 1u);

         // f(A, 0, 3, 4) = { 3, 1, 0 }
         histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 3, operation_history_id_type(4));
         BOOST_REQUIRE_EQUAL(histories.size(), 3u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 3u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 1u);
         BOOST_CHECK_EQUAL(histories[2].id.instance(), 0u);

         // f(A, 0, 3, 3) = { 3, 1, 0 }
         histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 3, operation_history_id_type(3));
         BOOST_REQUIRE_EQUAL(histories.size(), 3u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 3u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 1u);
         BOOST_CHECK_EQUAL(histories[2].id.instance(), 0u);

         // f(A, 0, 3, 2) = { 1, 0 }
         histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 3, operation_history_id_type(2));
         BOOST_REQUIRE_EQUAL(histories.size(), 2u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 1u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 0u);

         // f(A, 0, 3, 1) = { 1, 0 }
         histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 3, operation_history_id_type(1));
         BOOST_REQUIRE_EQUAL(histories.size(), 2u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 1u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 0u);

         // f(A, 0, 3, 0) = { 5, 3, 1 }
         histories = hist_api.get_account_history("1.2.0", operation_history_id_type(), 3, operation_history_id_type());
         BOOST_REQUIRE_EQUAL(histories.size(), 3u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 5u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 3u);
         BOOST_CHECK_EQUAL(histories[2].id.instance(), 1u);

         // f(B, 0, 4, 9) = { 6, 4, 2, 1 }
         histories = hist_api.get_account_history("dan", operation_history_id_type(), 4, operation_history_id_type(9));
         BOOST_REQUIRE_EQUAL(histories.size(), 4u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 6u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 4u);
         BOOST_CHECK_EQUAL(histories[2].id.instance(), 2u);
         BOOST_CHECK_EQUAL(histories[3].id.instance(), 1u);

         // f(B, 0, 4, 6) = { 6, 4, 2, 1 }
         histories = hist_api.get_account_history("dan", operation_history_id_type(), 4, operation_history_id_type(6));
         BOOST_REQUIRE_EQUAL(histories.size(), 4u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 6u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 4u);
         BOOST_CHECK_EQUAL(histories[2].id.instance(), 2u);
         BOOST_CHECK_EQUAL(histories[3].id.instance(), 1u);

         // f(B, 0, 4, 5) = { 4, 2, 1 }
         histories = hist_api.get_account_history("dan", operation_history_id_type(), 4, operation_history_id_type(5));
         BOOST_REQUIRE_EQUAL(histories.size(), 3u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 4u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 2u);
         BOOST_CHECK_EQUAL(histories[2].id.instance(), 1u);

         // f(B, 0, 4, 4) = { 4, 2, 1 }
         histories = hist_api.get_account_history("dan", operation_history_id_type(), 4, operation_history_id_type(4));
         BOOST_REQUIRE_EQUAL(histories.size(), 3u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 4u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 2u);
         BOOST_CHECK_EQUAL(histories[2].id.instance(), 1u);

         // f(B, 0, 4, 3) = { 2, 1 }
         histories = hist_api.get_account_history("dan", operation_history_id_type(), 4, operation_history_id_type(3));
         BOOST_REQUIRE_EQUAL(histories.size(), 2u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 2u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 1u);

         // f(B, 0, 4, 2) = { 2, 1 }
         histories = hist_api.get_account_history("dan", operation_history_id_type(), 4, operation_history_id_type(2));
         BOOST_REQUIRE_EQUAL(histories.size(), 2u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 2u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 1u);

         // f(B, 0, 4, 1) = { 1 }
         histories = hist_api.get_account_history("dan", operation_history_id_type(), 4, operation_history_id_type(1));
         BOOST_REQUIRE_EQUAL(histories.size(), 1u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 1u);

         // f(B, 0, 4, 0) = { 6, 4, 2, 1 }
         histories = hist_api.get_account_history("dan", operation_history_id_type(), 4, operation_history_id_type());
         BOOST_REQUIRE_EQUAL(histories.size(), 4u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 6u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 4u);
         BOOST_CHECK_EQUAL(histories[2].id.instance(), 2u);
         BOOST_CHECK_EQUAL(histories[3].id.instance(), 1u);

         // f(B, 2, 4, 9) = { 6, 4 }
         histories = hist_api.get_account_history("dan", operation_history_id_type(2), 4, operation_history_id_type(9));
         BOOST_REQUIRE_EQUAL(histories.size(), 2u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 6u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 4u);

         // f(B, 2, 4, 6) = { 6, 4 }
         histories = hist_api.get_account_history("dan", operation_history_id_type(2), 4, operation_history_id_type(6));
         BOOST_REQUIRE_EQUAL(histories.size(), 2u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 6u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 4u);

         // f(B, 2, 4, 5) = { 4 }
         histories = hist_api.get_account_history("dan", operation_history_id_type(2), 4, operation_history_id_type(5));
         BOOST_REQUIRE_EQUAL(histories.size(), 1u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 4u);

         // f(B, 2, 4, 4) = { 4 }
         histories = hist_api.get_account_history("dan", operation_history_id_type(2), 4, operation_history_id_type(4));
         BOOST_REQUIRE_EQUAL(histories.size(), 1u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 4u);

         // f(B, 2, 4, 3) = { }
         histories = hist_api.get_account_history("dan", operation_history_id_type(2), 4, operation_history_id_type(3));
         BOOST_CHECK_EQUAL(histories.size(), 0u);

         // f(B, 2, 4, 2) = { }
         histories = hist_api.get_account_history("dan", operation_history_id_type(2), 4, operation_history_id_type(2));
         BOOST_CHECK_EQUAL(histories.size(), 0u);

         // f(B, 2, 4, 1) = { }
         histories = hist_api.get_account_history("dan", operation_history_id_type(2), 4, operation_history_id_type(1));
         BOOST_CHECK_EQUAL(histories.size(), 0u);

         // f(B, 2, 4, 0) = { 6, 4 }
         histories = hist_api.get_account_history("dan", operation_history_id_type(2), 4, operation_history_id_type(0));
         BOOST_REQUIRE_EQUAL(histories.size(), 2u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 6u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 4u);

         // 0 limits
         histories = hist_api.get_account_history("dan", operation_history_id_type(0), 0, operation_history_id_type(0));
         BOOST_CHECK_EQUAL(histories.size(), 0u);
         histories = hist_api.get_account_history("1.2.0", operation_history_id_type(3), 0, operation_history_id_type(9));
         BOOST_CHECK_EQUAL(histories.size(), 0u);

         // non existent account
         histories = hist_api.get_account_history("1.2.18", operation_history_id_type(0), 4, operation_history_id_type(0));
         BOOST_CHECK_EQUAL(histories.size(), 0u);

         // create a new account C = alice { 7 }
         auto alice = create_account("alice");

         generate_block();

         // f(C, 0, 4, 10) = { 7 }
         wait_for( ES_WAIT_TIME,  [&]() {
            histories = hist_api.get_account_history(
                  "alice", operation_history_id_type(0), 4, operation_history_id_type(10));
            return (histories.size() == 1u);
         });
         BOOST_REQUIRE_EQUAL(histories.size(), 1u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 7u);

         // f(C, 8, 4, 10) = { }
         histories = hist_api.get_account_history("alice", operation_history_id_type(8), 4, operation_history_id_type(10));
         BOOST_CHECK_EQUAL(histories.size(), 0u);

         // f(A, 0, 10, 0) = { 7, 5, 3, 1, 0 }
         histories = hist_api.get_account_history("1.2.0", operation_history_id_type(0), 10, operation_history_id_type(0));
         BOOST_REQUIRE_EQUAL(histories.size(), 5u);
         BOOST_CHECK_EQUAL(histories[0].id.instance(), 7u);
         BOOST_CHECK_EQUAL(histories[1].id.instance(), 5u);
         BOOST_CHECK_EQUAL(histories[2].id.instance(), 3u);
         BOOST_CHECK_EQUAL(histories[3].id.instance(), 1u);
         BOOST_CHECK_EQUAL(histories[4].id.instance(), 0u);
      }
   }
   catch (fc::exception &e) {
      edump((e.to_detail_string()));
      throw;
   }
}
BOOST_AUTO_TEST_SUITE_END()
