#pragma once
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/validator_object.hpp>

namespace graphene { namespace chain {

   class validator_create_evaluator : public evaluator<validator_create_evaluator>
   {
      public:
         typedef validator_create_operation operation_type;

         void_result do_evaluate( const validator_create_operation& o );
         object_id_type do_apply( const validator_create_operation& o );
   };

   class validator_update_evaluator : public evaluator<validator_update_evaluator>
   {
      public:
         typedef validator_update_operation operation_type;

         void_result do_evaluate( const validator_update_operation& o );
         void_result do_apply( const validator_update_operation& o );
   };

} } // graphene::chain
