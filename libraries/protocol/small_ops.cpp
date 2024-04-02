#include <graphene/protocol/balance.hpp>
#include <graphene/protocol/buyback.hpp>
#include <graphene/protocol/exceptions.hpp>
#include <graphene/protocol/fba.hpp>
#include <graphene/protocol/vesting.hpp>

#include <fc/io/raw.hpp>

namespace graphene { namespace protocol {

FC_IMPLEMENT_EXCEPTION( protocol_exception, 4000000, "protocol exception" )

FC_IMPLEMENT_DERIVED_EXCEPTION( transaction_exception,      protocol_exception, 4010000,
                                "transaction validation exception" )

FC_IMPLEMENT_DERIVED_EXCEPTION( tx_missing_active_auth,     transaction_exception, 4010001,
                                "missing required active authority" )
FC_IMPLEMENT_DERIVED_EXCEPTION( tx_missing_owner_auth,      transaction_exception, 4010002,
                                "missing required owner authority" )
FC_IMPLEMENT_DERIVED_EXCEPTION( tx_missing_other_auth,      transaction_exception, 4010003,
                                "missing required other authority" )
FC_IMPLEMENT_DERIVED_EXCEPTION( tx_irrelevant_sig,          transaction_exception, 4010004,
                                "irrelevant signature included" )
FC_IMPLEMENT_DERIVED_EXCEPTION( tx_duplicate_sig,           transaction_exception, 4010005,
                                "duplicate signature included" )
FC_IMPLEMENT_DERIVED_EXCEPTION( invalid_council_approval,   transaction_exception, 4010006,
                                "council account cannot directly approve transaction" )
FC_IMPLEMENT_DERIVED_EXCEPTION( insufficient_fee,           transaction_exception, 4010007, "insufficient fee" )

} } // graphene::protocol


GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::balance_claim_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::buyback_account_options )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::fba_distribute_operation )

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::vesting_balance_create_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::vesting_balance_withdraw_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::vesting_balance_create_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::vesting_balance_withdraw_operation )
