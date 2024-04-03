#pragma once
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/types.hpp>

namespace graphene { namespace chain {

   class account_object;
   class asset_object;
   class backed_asset_data_object;
   class call_order_object;
   class limit_order_object;
   class collateral_bid_object;

   class limit_order_create_evaluator : public evaluator<limit_order_create_evaluator>
   {
      public:
         typedef limit_order_create_operation operation_type;

         void_result do_evaluate( const limit_order_create_operation& o );
         object_id_type do_apply( const limit_order_create_operation& o );

         asset calculate_market_fee( const asset_object* aobj, const asset& trade_amount );

         /** override the default behavior defined by generic_evalautor
          */
         virtual void convert_fee() override;

         /** override the default behavior defined by generic_evalautor which is to
          * post the fee to fee_paying_account_stats.pending_fees
          */
         virtual void pay_fee() override;

         share_type                          _deferred_fee  = 0;
         asset                               _deferred_paid_fee;
         const limit_order_create_operation* _op            = nullptr;
         const account_object*               _seller        = nullptr;
         const asset_object*                 _sell_asset    = nullptr;
         const asset_object*                 _receive_asset = nullptr;
   };

   class limit_order_cancel_evaluator : public evaluator<limit_order_cancel_evaluator>
   {
      public:
         typedef limit_order_cancel_operation operation_type;

         void_result do_evaluate( const limit_order_cancel_operation& o );
         asset do_apply( const limit_order_cancel_operation& o );

         const limit_order_object* _order;
   };

   class call_order_update_evaluator : public evaluator<call_order_update_evaluator>
   {
      public:
         typedef call_order_update_operation operation_type;

         void_result do_evaluate( const call_order_update_operation& o );
         object_id_type do_apply( const call_order_update_operation& o );

         bool _closing_order = false;
         const asset_object* _debt_asset = nullptr;
         const account_object* _paying_account = nullptr;
         const call_order_object* _order = nullptr;
         const backed_asset_data_object* _backed_asset_data = nullptr;
         const asset_dynamic_data_object*  _dynamic_data_obj = nullptr;
   };

   class bid_collateral_evaluator : public evaluator<bid_collateral_evaluator>
   {
      public:
         typedef bid_collateral_operation operation_type;

         void_result do_evaluate( const bid_collateral_operation& o );
         void_result do_apply( const bid_collateral_operation& o );

         const asset_object* _debt_asset = nullptr;
         const backed_asset_data_object* _backed_asset_data = nullptr;
         const account_object* _paying_account = nullptr;
         const collateral_bid_object* _bid = nullptr;
   };

} } // graphene::chain
