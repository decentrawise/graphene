#pragma once

#include <graphene/protocol/types.hpp>

namespace graphene { namespace chain { using namespace protocol; } }

GRAPHENE_DEFINE_IDS(chain, implementation_ids, impl_,
                    /* 2.0.x  */ (global_property)
                    /* 2.1.x  */ (dynamic_global_property)
                    /* 2.2.x  */ (reserved0) // unused, but can not be simply deleted due to API compatibility
                    /* 2.3.x  */ (asset_dynamic_data)
                    /* 2.4.x  */ (asset_bitasset_data)
                    /* 2.5.x  */ (account_balance)
                    /* 2.6.x  */ (account_statistics)
                    /* 2.7.x  */ (transaction_history)
                    /* 2.8.x  */ (block_summary)
                    /* 2.9.x  */ (account_history)
                    /* 2.10.x */ (blinded_balance)
                    /* 2.11.x */ (chain_property)
                    /* 2.12.x */ (validator_schedule)
                    /* 2.13.x */ (budget_record)
                    /* 2.14.x */ (special_authority)
                    /* 2.15.x */ (buyback)
                    /* 2.16.x */ (fba_accumulator)
                    /* 2.17.x */ (collateral_bid)
                   )
