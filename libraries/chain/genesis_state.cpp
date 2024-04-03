#include <graphene/chain/genesis_state.hpp>
#include <graphene/protocol/fee_schedule.hpp>

#include <fc/io/raw.hpp>

namespace graphene { namespace chain {

chain_id_type genesis_state_type::compute_chain_id() const
{
   return initial_chain_id;
}

void genesis_state_type::override_validator_producer_keys(const std::string &new_key)
{
   public_key_type new_pubkey(new_key);
   for (auto &wit : initial_validator_candidates)
   {
      wit.block_producer_key = new_pubkey;
   }
}

} } // graphene::chain

FC_REFLECT_DERIVED_NO_TYPENAME(graphene::chain::genesis_state_type::initial_account_type, BOOST_PP_SEQ_NIL,
           (name)(owner_key)(active_key)(is_lifetime_member) )

FC_REFLECT_DERIVED_NO_TYPENAME(graphene::chain::genesis_state_type::initial_asset_type, BOOST_PP_SEQ_NIL,
           (symbol)(issuer_name)(description)(precision)(max_supply)(accumulated_fees)(is_backed)
           (collateral_records))

FC_REFLECT_DERIVED_NO_TYPENAME(graphene::chain::genesis_state_type::initial_asset_type::initial_collateral_position,
           BOOST_PP_SEQ_NIL, (owner)(collateral)(debt))

FC_REFLECT_DERIVED_NO_TYPENAME(graphene::chain::genesis_state_type::initial_balance_type, BOOST_PP_SEQ_NIL,
           (owner)(asset_symbol)(amount))

FC_REFLECT_DERIVED_NO_TYPENAME(graphene::chain::genesis_state_type::initial_vesting_balance_type, BOOST_PP_SEQ_NIL,
           (owner)(asset_symbol)(amount)(begin_timestamp)(vesting_duration_seconds)(begin_balance))

FC_REFLECT_DERIVED_NO_TYPENAME(graphene::chain::genesis_state_type::initial_validator_type, BOOST_PP_SEQ_NIL,
           (owner_name)(block_producer_key))

FC_REFLECT_DERIVED_NO_TYPENAME(graphene::chain::genesis_state_type::initial_delegate_type, BOOST_PP_SEQ_NIL,
           (owner_name))

FC_REFLECT_DERIVED_NO_TYPENAME(graphene::chain::genesis_state_type::initial_worker_type, BOOST_PP_SEQ_NIL,
           (owner_name)(daily_pay))

FC_REFLECT_DERIVED_NO_TYPENAME(graphene::chain::genesis_state_type, BOOST_PP_SEQ_NIL,
           (initial_timestamp)(max_core_supply)(initial_parameters)(initial_accounts)(initial_assets)
           (initial_balances)(initial_vesting_balances)(initial_block_producers)(initial_validator_candidates)
           (initial_delegate_candidates)(initial_worker_candidates)
           (immutable_parameters))

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::genesis_state_type::initial_account_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::genesis_state_type::initial_asset_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::genesis_state_type::initial_asset_type::initial_collateral_position )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::genesis_state_type::initial_balance_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::genesis_state_type::initial_vesting_balance_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::genesis_state_type::initial_validator_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::genesis_state_type::initial_delegate_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::genesis_state_type::initial_worker_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::genesis_state_type )
