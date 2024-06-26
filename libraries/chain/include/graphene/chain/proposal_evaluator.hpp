#pragma once

#include <graphene/protocol/operations.hpp>
#include <graphene/chain/evaluator.hpp>

namespace graphene { namespace chain {

   class proposal_eval_visitor
   {
   public:
      typedef void result_type;

      uint64_t max_update_instance = 0;
      uint64_t nested_update_count = 0;

      template<typename T>
      void operator()(const T &v) const {}

      void operator()(const proposal_update_operation &v);

      void operator()(const proposal_delete_operation &v);

      // loop and self visit in proposals
      void operator()(const graphene::chain::proposal_create_operation &v);
   };

   class proposal_create_evaluator : public evaluator<proposal_create_evaluator>
   {
      public:
         typedef proposal_create_operation operation_type;

         void_result do_evaluate( const proposal_create_operation& o );
         object_id_type do_apply( const proposal_create_operation& o );

         transaction _proposed_trx;
         flat_set<account_id_type> _required_active_auths;
         flat_set<account_id_type> _required_owner_auths;

         proposal_eval_visitor vtor_eval;
   };

   class proposal_update_evaluator : public evaluator<proposal_update_evaluator>
   {
      public:
         typedef proposal_update_operation operation_type;

         void_result do_evaluate( const proposal_update_operation& o );
         void_result do_apply( const proposal_update_operation& o );

         const proposal_object* _proposal = nullptr;
         processed_transaction _processed_transaction;
         bool _executed_proposal = false;
         bool _proposal_failed = false;
   };

   class proposal_delete_evaluator : public evaluator<proposal_delete_evaluator>
   {
      public:
         typedef proposal_delete_operation operation_type;

         void_result do_evaluate( const proposal_delete_operation& o );
         void_result do_apply(const proposal_delete_operation&);

         const proposal_object* _proposal = nullptr;
   };

} } // graphene::chain
