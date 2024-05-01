#pragma once

#include <graphene/protocol/base.hpp>
#include <graphene/protocol/asset.hpp>
#include <graphene/protocol/chain_parameters.hpp>

namespace graphene { namespace protocol { 

   /**
    * @brief Create a delegate object, as a bid to hold a delegate seat on the network.
    * @ingroup operations
    *
    * Accounts which wish to become delegates may use this operation to create a delegate object which stakeholders may
    * vote on to approve its position as a delegate.
    */
   struct delegate_create_operation : public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 5000 * GRAPHENE_CORE_ASSET_PRECISION; };

      asset                                 fee;
      /// The account which owns the delegate. This account pays the fee for this operation.
      account_id_type                       delegate_account;
      string                                url;

      account_id_type fee_payer()const { return delegate_account; }
      void            validate()const;
   };

   /**
    * @brief Update a delegate object.
    * @ingroup operations
    *
    * Currently the only field which can be updated is the `url`
    * field.
    */
   struct delegate_update_operation : public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 20 * GRAPHENE_CORE_ASSET_PRECISION; };

      asset                                 fee;
      /// The delegate to update.
      delegate_id_type                      delegate;
      /// The account which owns the delegate. This account pays the fee for this operation.
      account_id_type                       delegate_account;
      optional< string >                    new_url;

      account_id_type fee_payer()const { return delegate_account; }
      void            validate()const;
   };

   /**
    * @brief Used by delegates to update the global parameters of the blockchain.
    * @ingroup operations
    *
    * This operation allows the delegates to update the global parameters on the blockchain. These control various
    * tunable aspects of the chain, including block and maintenance intervals, maximum data sizes, the fees charged by
    * the network, etc.
    *
    * This operation may only be used in a proposed transaction, and a proposed transaction which contains this
    * operation must have a review period specified in the current global parameters before it may be accepted.
    */
   struct delegate_update_global_parameters_operation : public base_operation
   {
      struct fee_parameters_type { uint64_t fee = GRAPHENE_CORE_ASSET_PRECISION; };

      asset             fee;
      chain_parameters  new_parameters;

      account_id_type fee_payer()const { return account_id_type(); }
      void            validate()const;
   };

   /// TODO: delegate_resign_operation : public base_operation

} } // graphene::protocol

FC_REFLECT( graphene::protocol::delegate_create_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::delegate_update_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::delegate_update_global_parameters_operation::fee_parameters_type, (fee) )

FC_REFLECT( graphene::protocol::delegate_create_operation,
            (fee)(delegate_account)(url) )
FC_REFLECT( graphene::protocol::delegate_update_operation,
            (fee)(delegate)(delegate_account)(new_url) )
FC_REFLECT( graphene::protocol::delegate_update_global_parameters_operation, (fee)(new_parameters) );

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::delegate_create_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::delegate_update_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::delegate_update_global_parameters_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::delegate_create_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::delegate_update_operation )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::delegate_update_global_parameters_operation )
