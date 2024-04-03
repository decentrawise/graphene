#include <boost/test/unit_test.hpp>

#include <graphene/chain/asset_object.hpp>
#include <graphene/app/api.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;
using namespace graphene::app;

BOOST_FIXTURE_TEST_SUITE(grouped_orders_api_tests, database_fixture)
BOOST_AUTO_TEST_CASE(api_limit_get_grouped_limit_orders) {
   try
   {
   app.enable_plugin("grouped_orders");
   graphene::app::orders_api orders_api(app);
   optional< api_access_info > acc;
   optional<price> start;

	//account_id_type() do 3 ops
   create_backed_asset("USD", account_id_type());
   create_account("dan");
   create_account("bob");
   asset_id_type bit_jmj_id = create_backed_asset("JMJBIT").get_id();
   generate_block();
   fc::usleep(fc::milliseconds(100));
   GRAPHENE_CHECK_THROW(orders_api.get_grouped_limit_orders(std::string( static_cast<object_id_type>(asset_id_type())), std::string( static_cast<object_id_type>(asset_id_type())),10, start,260), fc::exception);
   auto orders =orders_api.get_grouped_limit_orders(std::string( static_cast<object_id_type>(asset_id_type())), std::string( static_cast<object_id_type>(bit_jmj_id)), 10,start,240);
   BOOST_REQUIRE_EQUAL( orders.size(), 0u);
   }catch (fc::exception &e)
   {
    edump((e.to_detail_string()));
    throw;
   }
}
BOOST_AUTO_TEST_SUITE_END()
