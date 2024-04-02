#pragma once

#include <graphene/protocol/base.hpp>
#include <graphene/protocol/account.hpp>
#include <graphene/protocol/assert.hpp>
#include <graphene/protocol/asset_ops.hpp>
#include <graphene/protocol/balance.hpp>
#include <graphene/protocol/custom.hpp>
#include <graphene/protocol/delegate.hpp>
#include <graphene/protocol/confidential.hpp>
#include <graphene/protocol/fba.hpp>
#include <graphene/protocol/market.hpp>
#include <graphene/protocol/proposal.hpp>
#include <graphene/protocol/transfer.hpp>
#include <graphene/protocol/vesting.hpp>
#include <graphene/protocol/withdraw_permission.hpp>
#include <graphene/protocol/witness.hpp>
#include <graphene/protocol/worker.hpp>
#include <graphene/protocol/htlc.hpp>

namespace graphene { namespace protocol {

   /**
    * @ingroup operations
    *
    * Defines the set of valid operations as a discriminated union type.
    */
   using operation = fc::static_variant<
      /*  0 */ transfer_operation,
      /*  1 */ limit_order_create_operation,
      /*  2 */ limit_order_cancel_operation,
      /*  3 */ call_order_update_operation,
      /*  4 */ fill_order_operation, // VIRTUAL
      /*  5 */ account_create_operation,
      /*  6 */ account_update_operation,
      /*  7 */ account_whitelist_operation,
      /*  8 */ account_upgrade_operation,
      /*  9 */ account_transfer_operation,
      /* 10 */ asset_create_operation,
      /* 11 */ asset_update_operation,
      /* 12 */ asset_update_bitasset_operation,
      /* 13 */ asset_update_feed_producers_operation,
      /* 14 */ asset_issue_operation,
      /* 15 */ asset_reserve_operation,
      /* 16 */ asset_fund_fee_pool_operation,
      /* 17 */ asset_settle_operation,
      /* 18 */ asset_global_settle_operation,
      /* 19 */ asset_publish_feed_operation,
      /* 20 */ witness_create_operation,
      /* 21 */ witness_update_operation,
      /* 22 */ proposal_create_operation,
      /* 23 */ proposal_update_operation,
      /* 24 */ proposal_delete_operation,
      /* 25 */ withdraw_permission_create_operation,
      /* 26 */ withdraw_permission_update_operation,
      /* 27 */ withdraw_permission_claim_operation,
      /* 28 */ withdraw_permission_delete_operation,
      /* 29 */ delegate_create_operation,
      /* 30 */ delegate_update_operation,
      /* 31 */ delegate_update_global_parameters_operation,
      /* 32 */ vesting_balance_create_operation,
      /* 33 */ vesting_balance_withdraw_operation,
      /* 34 */ worker_create_operation,
      /* 35 */ custom_operation,
      /* 36 */ assert_operation,
      /* 37 */ balance_claim_operation,
      /* 38 */ override_transfer_operation,
      /* 39 */ transfer_to_blind_operation,
      /* 40 */ blind_transfer_operation,
      /* 41 */ transfer_from_blind_operation,
      /* 42 */ asset_settle_cancel_operation, // VIRTUAL
      /* 43 */ asset_claim_fees_operation,
      /* 44 */ fba_distribute_operation, // VIRTUAL
      /* 45 */ bid_collateral_operation,
      /* 46 */ execute_bid_operation, // VIRTUAL
      /* 47 */ asset_claim_pool_operation,
      /* 48 */ asset_update_issuer_operation,
      /* 49 */ htlc_create_operation,
      /* 50 */ htlc_redeem_operation,
      /* 51 */ htlc_redeemed_operation, // VIRTUAL
      /* 52 */ htlc_extend_operation,
      /* 53 */ htlc_refund_operation // VIRTUAL
   >;

   /// @} // operations group

   /**
    *  Appends required authorities to the result vector.  The authorities appended are not the
    *  same as those returned by get_required_auth 
    *
    *  @return a set of required authorities for @ref op
    */
   void operation_get_required_authorities( const operation& op, 
                                            flat_set<account_id_type>& active,
                                            flat_set<account_id_type>& owner,
                                            vector<authority>&  other );

   void operation_validate( const operation& op );

   /**
    *  @brief necessary to support nested operations inside the proposal_create_operation
    */
   struct op_wrapper
   {
      public:
         op_wrapper(const operation& op = operation()):op(op){}
         operation op;
   };

} } // graphene::protocol

FC_REFLECT_TYPENAME( graphene::protocol::operation )
FC_REFLECT( graphene::protocol::op_wrapper, (op) )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::op_wrapper )
