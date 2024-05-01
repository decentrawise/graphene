#pragma once

#define GRAPHENE_ADDRESS_PREFIX                                 "GPH"

#define GRAPHENE_CORE_ASSET_SYMBOL                              "CORE"
#define GRAPHENE_CORE_ASSET_PRECISION                           uint64_t(100000)
#define GRAPHENE_CORE_ASSET_PRECISION_DIGITS                    5
#define GRAPHENE_CORE_ASSET_MAX_SUPPLY                          int64_t(1000000000000000ll)   // 10 ^ 15

#define GRAPHENE_ACCOUNT_NAME_MIN_LENGTH                        1
#define GRAPHENE_ACCOUNT_NAME_MAX_LENGTH                        63

#define GRAPHENE_ASSET_SYMBOL_MIN_LENGTH                        3
#define GRAPHENE_ASSET_SYMBOL_MAX_LENGTH                        16

#define GRAPHENE_WORKER_NAME_MAX_LENGTH                         63

#define GRAPHENE_URL_MAX_LENGTH                                 127

#define GRAPHENE_IRREVERSIBLE_THRESHOLD                         (70 * GRAPHENE_1_PERCENT)

/**
 * every second, the fraction of burned core asset which cycles is
 * GRAPHENE_CORE_ASSET_CYCLE_RATE / (1 << GRAPHENE_CORE_ASSET_CYCLE_RATE_BITS)
 */
#define GRAPHENE_CORE_ASSET_CYCLE_RATE                          17
#define GRAPHENE_CORE_ASSET_CYCLE_RATE_BITS                     32

/**
 * Default configuration parameters
 */
#define GRAPHENE_DEFAULT_BLOCK_INTERVAL                         5   // seconds
#define GRAPHENE_DEFAULT_MAX_TRANSACTION_SIZE                   2048    // bytes
#define GRAPHENE_DEFAULT_MAX_BLOCK_SIZE                         (2*1000*1000) /* < 2 MiB (less than MAX_MESSAGE_SIZE in graphene/net/config.hpp) */
#define GRAPHENE_DEFAULT_MAX_TIME_UNTIL_EXPIRATION              (60*60*24)  // seconds, aka: 1 day
#define GRAPHENE_DEFAULT_MAINTENANCE_INTERVAL                   (60*60*24)  // seconds, aka: 1 day
#define GRAPHENE_DEFAULT_MAINTENANCE_SKIP_SLOTS                 3  // number of slots to skip for maintenance interval

#define GRAPHENE_DEFAULT_MAX_AUTHORITY_MEMBERSHIP               10
#define GRAPHENE_DEFAULT_MAX_ASSET_WHITELIST_AUTHORITIES        10
#define GRAPHENE_DEFAULT_MAX_ASSET_FEED_PUBLISHERS              10

#define GRAPHENE_DEFAULT_MAX_PRODUCER_COUNT                     (1001)  // SHOULD BE ODD
#define GRAPHENE_DEFAULT_MAX_COUNCIL_COUNT                      (1001)  // SHOULD BE ODD
#define GRAPHENE_DEFAULT_MAX_PROPOSAL_LIFETIME                  (60*60*24*7*4)  // Four weeks in seconds
#define GRAPHENE_DEFAULT_COUNCIL_PROPOSAL_REVIEW_PERIOD         (60*60*24*7*2)  // Two weeks in seconds
#define GRAPHENE_DEFAULT_NETWORK_PERCENT_OF_FEE                 (20*GRAPHENE_1_PERCENT)
#define GRAPHENE_DEFAULT_LIFETIME_REFERRER_PERCENT_OF_FEE       (30*GRAPHENE_1_PERCENT)
#define GRAPHENE_DEFAULT_CASHBACK_VESTING_PERIOD                (60*60*24*365)  ///< 1 year in seconds
#define GRAPHENE_DEFAULT_CASHBACK_VESTING_THRESHOLD             (GRAPHENE_CORE_ASSET_PRECISION*int64_t(100))
#define GRAPHENE_DEFAULT_MAX_ASSERT_OPCODE                      1
#define GRAPHENE_DEFAULT_ACCOUNTS_PER_FEE_SCALE                 1000
#define GRAPHENE_DEFAULT_ACCOUNT_FEE_SCALE_BITSHIFTS            4

#define GRAPHENE_DEFAULT_COUNT_NON_MEMBER_VOTES                 true
#define GRAPHENE_DEFAULT_ALLOW_NON_MEMBER_WHITELISTS            false

#define GRAPHENE_DEFAULT_PRODUCER_PAY_PER_BLOCK                 (GRAPHENE_CORE_ASSET_PRECISION * int64_t( 10) )
#define GRAPHENE_DEFAULT_PRODUCER_PAY_VESTING_PERIOD            (60*60*24)  // seconds
#define GRAPHENE_DEFAULT_WORKER_BUDGET_PER_DAY                  (GRAPHENE_CORE_ASSET_PRECISION * int64_t(500) * 1000 )

#define GRAPHENE_DEFAULT_SIG_CHECK_MAX_DEPTH                    2

/**
 * Don't allow the delegates to publish some limits that would
 * make the network unable to operate.
 */
#define GRAPHENE_LIMIT_MIN_TRANSACTION_SIZE                     1024    // bytes
#define GRAPHENE_LIMIT_MIN_BLOCK_INTERVAL                       1       // seconds
#define GRAPHENE_LIMIT_MAX_BLOCK_INTERVAL                       30      // seconds
#define GRAPHENE_LIMIT_MIN_BLOCK_SIZE                           (GRAPHENE_LIMIT_MIN_TRANSACTION_SIZE * 5) // 5 transactions per block

/**
 * Immutable chain parameters
 */
#define GRAPHENE_MIN_PRODUCER_COUNT                             (11)    // SHOULD BE ODD
#define GRAPHENE_MIN_COUNCIL_COUNT                              (11)    // SHOULD BE ODD

/**
 * Backed Assets
 */
#define GRAPHENE_ASSET_FORCE_SETTLEMENT_DELAY                   (60*60*24) ///< 1 day
#define GRAPHENE_ASSET_FORCE_SETTLEMENT_OFFSET                  0 ///< 1%
#define GRAPHENE_ASSET_FORCE_SETTLEMENT_MAX_VOLUME              (20* GRAPHENE_1_PERCENT) ///< 20%
#define GRAPHENE_ASSET_PRICE_FEED_LIFETIME                      (60*60*24) ///< 1 day
#define GRAPHENE_ASSET_MIN_PRICE_FEEDS                          7
#define GRAPHENE_ASSET_MAX_BUYBACK_MARKETS                      4

/** percentage fields are fixed point with a denominator of 10,000 */
#define GRAPHENE_100_PERCENT                                    10000
#define GRAPHENE_1_PERCENT                                      (GRAPHENE_100_PERCENT/100)
/** NOTE: making this a power of 2 (say 2^15) would greatly accelerate fee calcs */

#define GRAPHENE_MAX_MARKET_FEE_PERCENT                         GRAPHENE_100_PERCENT

/**
 *  These ratios are fixed point numbers with a denominator of GRAPHENE_COLLATERAL_RATIO_DENOM, the
 *  minimum maintenance collateral is therefore 1.001x and the default
 *  maintenance ratio is 1.75x
 */
///@{
#define GRAPHENE_COLLATERAL_RATIO_DENOM                         1000
#define GRAPHENE_MIN_COLLATERAL_RATIO                           1001    ///< lower than this could result in divide by 0
#define GRAPHENE_MAX_COLLATERAL_RATIO                           32000   ///< higher than this is unnecessary and may exceed int16 storage
#define GRAPHENE_MAINTENANCE_COLLATERAL_RATIO                   1750    ///< Call when collateral only pays off 175% the debt
#define GRAPHENE_MAX_SHORT_SQUEEZE_RATIO                        1500    ///< Stop calling when collateral only pays off 150% of the debt
///@}

/**
 *  Reserved Account IDs with special meaning
 */
///@{
/// Represents the current delegates, two-week review period
#define GRAPHENE_COUNCIL_ACCOUNT                                (graphene::protocol::account_id_type(0))
/// Represents the current block producers
#define GRAPHENE_PRODUCERS_ACCOUNT                              (graphene::protocol::account_id_type(1))
/// Represents the current delegates
#define GRAPHENE_RELAXED_COUNCIL_ACCOUNT                        (graphene::protocol::account_id_type(2))
/// Represents the canonical account with NO authority (nobody can access funds in null account)
#define GRAPHENE_NULL_ACCOUNT                                   (graphene::protocol::account_id_type(3))
/// Represents the canonical account with WILDCARD authority (anybody can access funds in temp account)
#define GRAPHENE_TEMP_ACCOUNT                                   (graphene::protocol::account_id_type(4))
/// Represents the canonical account for specifying you will vote directly (as opposed to a proxy)
#define GRAPHENE_PROXY_TO_SELF_ACCOUNT                          (graphene::protocol::account_id_type(5))
/// Sentinel value used in the scheduler.
#define GRAPHENE_NULL_VALIDATOR                                 (graphene::protocol::validator_id_type(0))
///@}

#define GRAPHENE_FBA_STEALTH_DESIGNATED_ASSET (asset_id_type(743))  // TODO: to be removed
