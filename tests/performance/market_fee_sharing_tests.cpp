#include <boost/test/unit_test.hpp>

#include <chrono>
#include <graphene/chain/hardfork.hpp>
#include "../common/database_fixture.hpp"

using namespace graphene::chain;

BOOST_FIXTURE_TEST_CASE(mfs_performance_test, database_fixture)
{
   try
   {
      ACTORS((issuer));

      const unsigned int accounts = 3000;
      const unsigned int iterations = 20;

      std::vector<account_object> registrators;
      for (unsigned int i = 0; i < accounts; ++i)
      {
         auto account = create_account("registrar" + std::to_string(i));
         transfer(council_account, account.get_id(), asset(1000000));
         upgrade_to_lifetime_member(account);

         registrators.push_back(std::move(account));
      }

      generate_block();

      additional_asset_options_t options;
      options.value.reward_percent = 2 * GRAPHENE_1_PERCENT;

      const auto usd = create_user_asset(
                  "USD",
                  issuer,
                  charge_market_fee,
                  price(asset(1, asset_id_type(1)), asset(1)),
                  1,
                  20 * GRAPHENE_1_PERCENT,
                  options);

      issue_ua(issuer, usd.amount(iterations * accounts * 2000));

      std::vector<account_object> traders;
      for (unsigned int i = 0; i < accounts; ++i)
      {
         std::string name = "account" + std::to_string(i);
         auto account = create_account(name, registrators[i], registrators[i], GRAPHENE_1_PERCENT);
         transfer(council_account, account.get_id(), asset(1000000));
         transfer(issuer, account, usd.amount(iterations * 2000));

         traders.push_back(std::move(account));
      }

      using namespace std::chrono;

      const auto start = high_resolution_clock::now();

      for (unsigned int i = 0; i < iterations; ++i)
      {
         for (unsigned int j = 0; j < accounts; ++j)
         {
            create_sell_order(traders[j], usd.amount(2000), asset(1));
            create_sell_order(traders[accounts - j - 1], asset(1), usd.amount(1000));
         }
      }

      const auto end = high_resolution_clock::now();

      const auto elapsed = duration_cast<milliseconds>(end - start);
      wlog("Elapsed: ${c} ms", ("c", elapsed.count()));

      for (unsigned int i = 0; i < accounts; ++i)
      {
         const auto reward = get_market_fee_reward(registrators[i], usd);
         BOOST_CHECK_GT(reward, 0);
      }
   }
   FC_LOG_AND_RETHROW()
}
