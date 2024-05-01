#pragma once

#include <graphene/protocol/address.hpp>
#include <graphene/protocol/chain_parameters.hpp>
#include <graphene/chain/types.hpp>
#include <graphene/chain/immutable_chain_parameters.hpp>

#include <fc/crypto/sha256.hpp>

#include <string>
#include <vector>

namespace graphene { namespace chain {
using std::string;
using std::vector;

struct genesis_state_type {
   struct initial_account_type {
      initial_account_type(const string& name = string(),
                           const public_key_type& owner_key = public_key_type(),
                           const public_key_type& active_key = public_key_type(),
                           bool is_lifetime_member = false)
         : name(name),
           owner_key(owner_key),
           active_key(active_key == public_key_type()? owner_key : active_key),
           is_lifetime_member(is_lifetime_member)
      {}
      string name;
      public_key_type owner_key;
      public_key_type active_key;
      bool is_lifetime_member = false;
   };
   struct initial_asset_type {
      struct initial_collateral_position {
         address owner;
         amount_type collateral;
         amount_type debt;
      };

      string symbol;
      string issuer_name;

      string description;
      uint8_t precision = GRAPHENE_CORE_ASSET_PRECISION_DIGITS;

      amount_type max_supply;
      amount_type accumulated_fees;

      bool is_backed = false;
      vector<initial_collateral_position> collateral_records;
   };
   struct initial_balance_type {
      address owner;
      string asset_symbol;
      amount_type amount;
   };
   struct initial_vesting_balance_type {
      address owner;
      string asset_symbol;
      amount_type amount;
      time_point_sec begin_timestamp;
      uint32_t vesting_duration_seconds = 0;
      amount_type begin_balance;
   };
   struct initial_validator_type {
      /// Must correspond to one of the initial accounts
      string owner_name;
      public_key_type block_producer_key;
   };
   struct initial_delegate_type {
      /// Must correspond to one of the initial accounts
      string owner_name;
   };
   struct initial_worker_type {
      /// Must correspond to one of the initial accounts
      string owner_name;
      amount_type daily_pay;
   };

   time_point_sec                           initial_timestamp;
   amount_type                               max_core_supply = GRAPHENE_CORE_ASSET_MAX_SUPPLY;
   chain_parameters                         initial_parameters;
   immutable_chain_parameters               immutable_parameters;
   vector<initial_account_type>             initial_accounts;
   vector<initial_asset_type>               initial_assets;
   vector<initial_balance_type>             initial_balances;
   vector<initial_vesting_balance_type>     initial_vesting_balances;
   uint64_t                                 initial_block_producers = GRAPHENE_MIN_PRODUCER_COUNT;
   vector<initial_validator_type>           initial_validator_candidates;
   vector<initial_delegate_type>            initial_delegate_candidates;
   vector<initial_worker_type>              initial_worker_candidates;

   /**
    * Temporary, will be moved elsewhere.
    */
   chain_id_type                            initial_chain_id;

   /**
    * Get the chain_id corresponding to this genesis state.
    *
    * This is the SHA256 serialization of the genesis_state.
    */
   chain_id_type compute_chain_id() const;

   /// Method to override initial block producer keys for debug
   void override_validator_producer_keys( const std::string& new_key );

};

} } // namespace graphene::chain

FC_REFLECT_TYPENAME( graphene::chain::genesis_state_type::initial_account_type )
FC_REFLECT_TYPENAME( graphene::chain::genesis_state_type::initial_asset_type )
FC_REFLECT_TYPENAME( graphene::chain::genesis_state_type::initial_asset_type::initial_collateral_position )
FC_REFLECT_TYPENAME( graphene::chain::genesis_state_type::initial_balance_type )
FC_REFLECT_TYPENAME( graphene::chain::genesis_state_type::initial_vesting_balance_type )
FC_REFLECT_TYPENAME( graphene::chain::genesis_state_type::initial_validator_type )
FC_REFLECT_TYPENAME( graphene::chain::genesis_state_type::initial_delegate_type )
FC_REFLECT_TYPENAME( graphene::chain::genesis_state_type::initial_worker_type )
FC_REFLECT_TYPENAME( graphene::chain::genesis_state_type )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::genesis_state_type::initial_account_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::genesis_state_type::initial_asset_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::genesis_state_type::initial_asset_type::initial_collateral_position )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::genesis_state_type::initial_balance_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::genesis_state_type::initial_vesting_balance_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::genesis_state_type::initial_validator_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::genesis_state_type::initial_delegate_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::genesis_state_type::initial_worker_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::genesis_state_type )
