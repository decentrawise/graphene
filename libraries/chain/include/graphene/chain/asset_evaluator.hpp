#pragma once
#include <graphene/protocol/operations.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/database.hpp>

#include <graphene/chain/hardfork.hpp>
#include <locale>

namespace graphene { namespace chain {

   class asset_create_evaluator : public evaluator<asset_create_evaluator>
   {
      public:
         typedef asset_create_operation operation_type;

         void_result do_evaluate( const asset_create_operation& o );
         object_id_type do_apply( const asset_create_operation& o );

         /** override the default behavior defined by generic_evalautor which is to
          * post the fee to fee_paying_account_stats.pending_fees
          */
         virtual void pay_fee() override;
      private:
         bool fee_is_odd;
   };

   class asset_issue_evaluator : public evaluator<asset_issue_evaluator>
   {
      public:
         typedef asset_issue_operation operation_type;
         void_result do_evaluate( const asset_issue_operation& o );
         void_result do_apply( const asset_issue_operation& o );

         const asset_dynamic_data_object* asset_dyn_data = nullptr;
         const account_object*            to_account = nullptr;
   };

   class asset_reserve_evaluator : public evaluator<asset_reserve_evaluator>
   {
      public:
         typedef asset_reserve_operation operation_type;
         void_result do_evaluate( const asset_reserve_operation& o );
         void_result do_apply( const asset_reserve_operation& o );

         const asset_dynamic_data_object* asset_dyn_data = nullptr;
         const account_object*            from_account = nullptr;
   };


   class asset_update_evaluator : public evaluator<asset_update_evaluator>
   {
      public:
         typedef asset_update_operation operation_type;

         void_result do_evaluate( const asset_update_operation& o );
         void_result do_apply( const asset_update_operation& o );

         const asset_object* asset_to_update = nullptr;
   };

   class asset_update_issuer_evaluator : public evaluator<asset_update_issuer_evaluator>
   {
      public:
         typedef asset_update_issuer_operation operation_type;

         void_result do_evaluate( const asset_update_issuer_operation& o );
         void_result do_apply( const asset_update_issuer_operation& o );

         const asset_object* asset_to_update = nullptr;
   };

   class asset_update_backed_asset_evaluator : public evaluator<asset_update_backed_asset_evaluator>
   {
      public:
         typedef asset_update_backed_asset_operation operation_type;

         void_result do_evaluate( const asset_update_backed_asset_operation& o );
         void_result do_apply( const asset_update_backed_asset_operation& o );

         const backed_asset_data_object* backed_asset_to_update = nullptr;
         const asset_object* asset_to_update = nullptr;
   };

   class asset_update_feed_producers_evaluator : public evaluator<asset_update_feed_producers_evaluator>
   {
      public:
         typedef asset_update_feed_producers_operation operation_type;

         void_result do_evaluate( const operation_type& o );
         void_result do_apply( const operation_type& o );

         const asset_object* asset_to_update = nullptr;
   };

   class asset_fund_fee_pool_evaluator : public evaluator<asset_fund_fee_pool_evaluator>
   {
      public:
         typedef asset_fund_fee_pool_operation operation_type;

         void_result do_evaluate(const asset_fund_fee_pool_operation& op);
         void_result do_apply(const asset_fund_fee_pool_operation& op);

         const asset_dynamic_data_object* asset_dyn_data = nullptr;
   };

   class asset_global_settle_evaluator : public evaluator<asset_global_settle_evaluator>
   {
      public:
         typedef asset_global_settle_operation operation_type;

         void_result do_evaluate(const operation_type& op);
         void_result do_apply(const operation_type& op);

         const asset_object* asset_to_settle = nullptr;
   };
   class asset_settle_evaluator : public evaluator<asset_settle_evaluator>
   {
      public:
         typedef asset_settle_operation operation_type;

         void_result do_evaluate(const operation_type& op);
         operation_result do_apply(const operation_type& op);

         const asset_object* asset_to_settle = nullptr;
   };

   class asset_publish_feeds_evaluator : public evaluator<asset_publish_feeds_evaluator>
   {
      public:
         typedef asset_publish_feed_operation operation_type;

         void_result do_evaluate( const asset_publish_feed_operation& o );
         void_result do_apply( const asset_publish_feed_operation& o );

         const asset_object* asset_ptr = nullptr;
         const backed_asset_data_object* backed_asset_ptr = nullptr;
   };

   class asset_claim_fees_evaluator : public evaluator<asset_claim_fees_evaluator>
   {
      public:
         typedef asset_claim_fees_operation operation_type;

         void_result do_evaluate( const asset_claim_fees_operation& o );
         void_result do_apply( const asset_claim_fees_operation& o );
   };

   class asset_claim_pool_evaluator : public evaluator<asset_claim_pool_evaluator>
   {
      public:
         typedef asset_claim_pool_operation operation_type;

         void_result do_evaluate( const asset_claim_pool_operation& o );
         void_result do_apply( const asset_claim_pool_operation& o );
   };

} } // graphene::chain
