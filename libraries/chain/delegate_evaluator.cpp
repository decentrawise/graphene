#include <graphene/chain/delegate_evaluator.hpp>
#include <graphene/chain/delegate_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/protocol/fee_schedule.hpp>
#include <graphene/protocol/vote.hpp>
#include <graphene/chain/transaction_evaluation_state.hpp>

namespace graphene { namespace chain {

void_result delegate_create_evaluator::do_evaluate( const delegate_create_operation& op )
{ try {
   FC_ASSERT(db().get(op.delegate_account).is_lifetime_member());
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) } // GCOVR_EXCL_LINE

object_id_type delegate_create_evaluator::do_apply( const delegate_create_operation& op )
{ try {
   vote_id_type vote_id;
   db().modify(db().get_global_properties(), [&vote_id](global_property_object& p) {
      vote_id = vote_id_type(vote_id_type::committee, p.next_available_vote_id++);
   });

   const auto& new_del_object = db().create<delegate_object>( [&]( delegate_object& obj ){
         obj.delegate_account   = op.delegate_account;
         obj.vote_id            = vote_id;
         obj.url                = op.url;
   });
   return new_del_object.id;
} FC_CAPTURE_AND_RETHROW( (op) ) } // GCOVR_EXCL_LINE

void_result delegate_update_evaluator::do_evaluate( const delegate_update_operation& op )
{ try {
   FC_ASSERT(db().get(op.delegate).delegate_account == op.delegate_account);
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) } // GCOVR_EXCL_LINE

void_result delegate_update_evaluator::do_apply( const delegate_update_operation& op )
{ try {
   database& _db = db();
   _db.modify(
      _db.get(op.delegate),
      [&]( delegate_object& com )
      {
         if( op.new_url.valid() )
            com.url = *op.new_url;
      });
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) } // GCOVR_EXCL_LINE

void_result delegate_update_global_parameters_evaluator::do_evaluate(const delegate_update_global_parameters_operation& o)
{ try {
   FC_ASSERT(trx_state->_is_proposed_trx);

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) } // GCOVR_EXCL_LINE

void_result delegate_update_global_parameters_evaluator::do_apply(const delegate_update_global_parameters_operation& o)
{ try {
   db().modify(db().get_global_properties(), [&o](global_property_object& p) {
      p.pending_parameters = o.new_parameters;
   });

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) } // GCOVR_EXCL_LINE

} } // graphene::chain
