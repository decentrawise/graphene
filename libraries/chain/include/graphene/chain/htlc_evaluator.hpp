#pragma once
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/htlc_object.hpp>

namespace graphene { namespace chain {

   class htlc_create_evaluator : public evaluator<htlc_create_evaluator>
   {
      public:
         typedef htlc_create_operation operation_type;

         void_result do_evaluate( const htlc_create_operation& o);
         object_id_type do_apply( const htlc_create_operation& o);
   };

   class htlc_redeem_evaluator : public evaluator<htlc_redeem_evaluator>
   {
      public:
         typedef htlc_redeem_operation operation_type;

         void_result do_evaluate( const htlc_redeem_operation& o);
         void_result do_apply( const htlc_redeem_operation& o);
         const htlc_object* htlc_obj = nullptr;
   };

   class htlc_extend_evaluator : public evaluator<htlc_extend_evaluator>
   {
      public:
         typedef htlc_extend_operation operation_type;

         void_result do_evaluate( const htlc_extend_operation& o);
         void_result do_apply( const htlc_extend_operation& o);
         const htlc_object* htlc_obj = nullptr;
   };

} } // namespace graphene::chain
