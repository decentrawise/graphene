#include <boost/test/unit_test.hpp>

#include <graphene/app/api.hpp>
#include <graphene/chain/hardfork.hpp>

#include <fc/crypto/digest.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE(network_broadcast_api_tests, database_fixture)

BOOST_AUTO_TEST_CASE( broadcast_transaction_with_callback_test ) {
   try {

      uint32_t called = 0;
      auto callback = [&]( const variant& v )
      {
         ++called;
      };

      fc::ecc::private_key cid_key = fc::ecc::private_key::regenerate( fc::digest("key") );
      const account_id_type cid_id = create_account( "cid", cid_key.get_public_key() ).get_id();
      fund( cid_id(db) );

      auto nb_api = std::make_shared< graphene::app::network_broadcast_api >( app );

      set_expiration( db, trx );
      transfer_operation trans;
      trans.from = cid_id;
      trans.to   = account_id_type();
      trans.amount = asset(1);
      trx.operations.push_back( trans );
      sign( trx, cid_key );

      nb_api->broadcast_transaction_with_callback( callback, trx );

      trx.clear();

      generate_block();

      fc::usleep(fc::milliseconds(200)); // sleep a while to execute callback in another thread

      BOOST_CHECK_EQUAL( called, 1u );

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( broadcast_transaction_too_large ) {
   try {

      fc::ecc::private_key cid_key = fc::ecc::private_key::regenerate( fc::digest("key") );
      const account_id_type cid_id = create_account( "cid", cid_key.get_public_key() ).get_id();
      fund( cid_id(db) );

      auto nb_api = std::make_shared< graphene::app::network_broadcast_api >( app );

      generate_blocks( HARDFORK_CORE_1573_TIME + 10 );

      set_expiration( db, trx );
      transfer_operation trans;
      trans.from = cid_id;
      trans.to   = account_id_type();
      trans.amount = asset(1);
      for(int i = 0; i < 250; ++i )
         trx.operations.push_back( trans );
      sign( trx, cid_key );

      BOOST_CHECK_THROW( nb_api->broadcast_transaction( trx ), fc::exception );

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
