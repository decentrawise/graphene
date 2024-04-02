#pragma once

#include <graphene/protocol/base.hpp>
#include <graphene/protocol/asset.hpp>

namespace graphene { namespace protocol { 

  /**
    * @brief Create a validator object, as a bid to hold a validator position on the network.
    * @ingroup operations
    *
    * Accounts which wish to become validators may use this operation to create a validator object which stakeholders may
    * vote on to approve its position as a validator.
    */
   struct validator_create_operation : public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 5000 * GRAPHENE_BLOCKCHAIN_PRECISION; };

      asset             fee;
      /// The account which owns the validator. This account pays the fee for this operation.
      account_id_type   validator_account;
      string            url;
      public_key_type   block_signing_key;

      account_id_type fee_payer()const { return validator_account; }
      void            validate()const;
   };

  /**
    * @brief Update a validator object's URL and block signing key.
    * @ingroup operations
    */
   struct validator_update_operation : public base_operation
   {
      struct fee_parameters_type
      {
         share_type fee = 20 * GRAPHENE_BLOCKCHAIN_PRECISION;
      };

      asset             fee;
      /// The validator object to update.
      validator_id_type   validator;
      /// The account which owns the validator. This account pays the fee for this operation.
      account_id_type   validator_account;
      /// The new URL.
      optional< string > new_url;
      /// The new block signing key.
      optional< public_key_type > new_signing_key;

      account_id_type fee_payer()const { return validator_account; }
      void            validate()const;
   };

   /// TODO: validator_resign_operation : public base_operation

} } // graphene::protocol

FC_REFLECT( graphene::protocol::validator_create_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::validator_create_operation, (fee)(validator_account)(url)(block_signing_key) )

FC_REFLECT( graphene::protocol::validator_update_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::validator_update_operation, (fee)(validator)(validator_account)(new_url)(new_signing_key) )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::validator_create_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::validator_update_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::validator_create_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::validator_update_operation )
