#include <boost/test/unit_test.hpp>

#include <graphene/chain/asset_object.hpp>
#include <graphene/app/api.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;
using namespace graphene::app;

BOOST_FIXTURE_TEST_SUITE(asset_api_tests, database_fixture)

BOOST_AUTO_TEST_CASE( asset_holders )
{
   graphene::app::asset_api asset_api(app);

   // create an asset and some accounts
   create_bitasset("USD", account_id_type());
   auto dan = create_account("dan");
   auto bob = create_account("bob");
   auto alice = create_account("alice");

   // send them some bts
   transfer(account_id_type()(db), dan, asset(100));
   transfer(account_id_type()(db), alice, asset(200));
   transfer(account_id_type()(db), bob, asset(300));

   // make call
   auto holders = asset_api.get_asset_holders( std::string( static_cast<object_id_type>(asset_id_type())), 0, 100);
   BOOST_CHECK_EQUAL(holders.size(), 4u);

   // by now we can guarantee the order
   BOOST_CHECK(holders[0].name == "council-account");
   BOOST_CHECK(holders[1].name == "bob");
   BOOST_CHECK(holders[2].name == "alice");
   BOOST_CHECK(holders[3].name == "dan");
}
BOOST_AUTO_TEST_CASE( api_limit_get_asset_holders )
{
   graphene::app::asset_api asset_api(app);

   // create an asset and some accounts
   create_bitasset("USD", account_id_type());
   auto dan = create_account("dan");
   auto bob = create_account("bob");
   auto alice = create_account("alice");

   // send them some bts
   transfer(account_id_type()(db), dan, asset(100));
   transfer(account_id_type()(db), alice, asset(200));
   transfer(account_id_type()(db), bob, asset(300));

   // make call
   GRAPHENE_CHECK_THROW(asset_api.get_asset_holders(std::string( static_cast<object_id_type>(asset_id_type())), 0, 260), fc::exception);
   auto holders = asset_api.get_asset_holders(std::string( static_cast<object_id_type>(asset_id_type())), 0, 210);
   BOOST_REQUIRE_EQUAL( holders.size(), 4u );
}

BOOST_AUTO_TEST_SUITE_END()
