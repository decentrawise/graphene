#pragma once
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/delegate_object.hpp>

namespace graphene { namespace chain {

   class delegate_create_evaluator : public evaluator<delegate_create_evaluator>
   {
      public:
         typedef delegate_create_operation operation_type;

         void_result do_evaluate( const delegate_create_operation& o );
         object_id_type do_apply( const delegate_create_operation& o );
   };

   class delegate_update_evaluator : public evaluator<delegate_update_evaluator>
   {
      public:
         typedef delegate_update_operation operation_type;

         void_result do_evaluate( const delegate_update_operation& o );
         void_result do_apply( const delegate_update_operation& o );
   };

   class delegate_update_global_parameters_evaluator : public evaluator<delegate_update_global_parameters_evaluator>
   {
      public:
         typedef delegate_update_global_parameters_operation operation_type;

         void_result do_evaluate( const delegate_update_global_parameters_operation& o );
         void_result do_apply( const delegate_update_global_parameters_operation& o );
   };

} } // graphene::chain
