#include <graphene/chain/validator_evaluator.hpp>
#include <graphene/chain/validator_object.hpp>
#include <graphene/chain/delegate_object.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/protocol/vote.hpp>

namespace graphene { namespace chain {

void_result validator_create_evaluator::do_evaluate( const validator_create_operation& op )
{ try {
   FC_ASSERT(db().get(op.validator_account).is_lifetime_member());
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) } // GCOVR_EXCL_LINE

object_id_type validator_create_evaluator::do_apply( const validator_create_operation& op )
{ try {
   database& _db = db();
   vote_id_type vote_id;
   _db.modify( _db.get_global_properties(), [&vote_id](global_property_object& p) {
      vote_id = vote_id_type(vote_id_type::validator, p.next_available_vote_id++);
   });

   const auto& new_validator_object = _db.create<validator_object>( [&op,&vote_id]( validator_object& obj ){
         obj.validator_account  = op.validator_account;
         obj.signing_key      = op.block_signing_key;
         obj.vote_id          = vote_id;
         obj.url              = op.url;
   });
   return new_validator_object.id;
} FC_CAPTURE_AND_RETHROW( (op) ) } // GCOVR_EXCL_LINE

void_result validator_update_evaluator::do_evaluate( const validator_update_operation& op )
{ try {
   FC_ASSERT(db().get(op.validator).validator_account == op.validator_account);
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) } // GCOVR_EXCL_LINE

void_result validator_update_evaluator::do_apply( const validator_update_operation& op )
{ try {
   database& _db = db();
   _db.modify(
      _db.get(op.validator),
      [&op]( validator_object& wit )
      {
         if( op.new_url.valid() )
            wit.url = *op.new_url;
         if( op.new_signing_key.valid() )
            wit.signing_key = *op.new_signing_key;
      });
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) } // GCOVR_EXCL_LINE

} } // graphene::chain
