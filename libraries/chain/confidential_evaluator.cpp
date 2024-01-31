#include <graphene/chain/exceptions.hpp>
#include <graphene/protocol/confidential.hpp>
#include <graphene/chain/confidential_evaluator.hpp>
#include <graphene/chain/confidential_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/fba_accumulator_id.hpp>
#include <graphene/chain/hardfork.hpp>

namespace graphene { namespace chain {

void_result transfer_to_blind_evaluator::do_evaluate( const transfer_to_blind_operation& o )
{ try {
   const auto& d = db();

   const auto& atype = o.amount.asset_id(db()); 
   FC_ASSERT( atype.allow_confidential() );
   FC_ASSERT( !atype.is_transfer_restricted() );
   FC_ASSERT( !(atype.options.flags & white_list) );

   for( const auto& out : o.outputs )
   {
      for( const auto& a : out.owner.account_auths )
         a.first(d); // verify all accounts exist and are valid
   }
   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) } // GCOVR_EXCL_LINE


void_result transfer_to_blind_evaluator::do_apply( const transfer_to_blind_operation& o ) 
{ try {
   db().adjust_balance( o.from, -o.amount ); 

   const auto& add = o.amount.asset_id(db()).dynamic_asset_data_id(db());  // verify fee is a legit asset 
   db().modify( add, [&]( asset_dynamic_data_object& obj ){
      obj.confidential_supply += o.amount.amount;
      FC_ASSERT( obj.confidential_supply >= 0 );
   });
   for( const auto& out : o.outputs )
   {
      db().create<blinded_balance_object>( [&]( blinded_balance_object& obj ){
          obj.asset_id   = o.amount.asset_id;
          obj.owner      = out.owner;
          obj.commitment = out.commitment;
      });
   }
   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) } // GCOVR_EXCL_LINE

void transfer_to_blind_evaluator::pay_fee()
{
   if( db().head_block_time() >= HARDFORK_563_TIME )
      pay_fba_fee( fba_accumulator_id_transfer_to_blind );
   else
      generic_evaluator::pay_fee();
}

void_result transfer_from_blind_evaluator::do_evaluate( const transfer_from_blind_operation& o )
{ try {
   const auto& d = db();
   o.fee.asset_id(d);  // verify fee is a legit asset 
   const auto& bbi = d.get_index_type<blinded_balance_index>();
   const auto& cidx = bbi.indices().get<by_commitment>();
   for( const auto& in : o.inputs )
   {
      auto itr = cidx.find( in.commitment );
      FC_ASSERT( itr != cidx.end() );
      FC_ASSERT( itr->asset_id == o.fee.asset_id );
      FC_ASSERT( itr->owner == in.owner );
   }
   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) } // GCOVR_EXCL_LINE

void_result transfer_from_blind_evaluator::do_apply( const transfer_from_blind_operation& o ) 
{ try {
   db().adjust_balance( o.fee_payer(), o.fee ); 
   db().adjust_balance( o.to, o.amount ); 
   const auto& bbi = db().get_index_type<blinded_balance_index>();
   const auto& cidx = bbi.indices().get<by_commitment>();
   for( const auto& in : o.inputs )
   {
      auto itr = cidx.find( in.commitment );
      FC_ASSERT( itr != cidx.end() );
      db().remove( *itr );
   }
   const auto& add = o.amount.asset_id(db()).dynamic_asset_data_id(db());  // verify fee is a legit asset 
   db().modify( add, [&]( asset_dynamic_data_object& obj ){
      obj.confidential_supply -= o.amount.amount + o.fee.amount;
      FC_ASSERT( obj.confidential_supply >= 0 );
   });
   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) } // GCOVR_EXCL_LINE

void transfer_from_blind_evaluator::pay_fee()
{
   if( db().head_block_time() >= HARDFORK_563_TIME )
      pay_fba_fee( fba_accumulator_id_transfer_from_blind );
   else
      generic_evaluator::pay_fee();
}

void_result blind_transfer_evaluator::do_evaluate( const blind_transfer_operation& o )
{ try {
   const auto& d = db();
   o.fee.asset_id(db());  // verify fee is a legit asset 
   const auto& bbi = db().get_index_type<blinded_balance_index>();
   const auto& cidx = bbi.indices().get<by_commitment>();
   for( const auto& out : o.outputs )
   {
      for( const auto& a : out.owner.account_auths )
         a.first(d); // verify all accounts exist and are valid
   }
   for( const auto& in : o.inputs )
   {
      auto itr = cidx.find( in.commitment );
      GRAPHENE_ASSERT( itr != cidx.end(), blind_transfer_unknown_commitment, "", ("commitment",in.commitment) );
      FC_ASSERT( itr->asset_id == o.fee.asset_id );
      FC_ASSERT( itr->owner == in.owner );
   }
   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) } // GCOVR_EXCL_LINE

void_result blind_transfer_evaluator::do_apply( const blind_transfer_operation& o ) 
{ try {
   db().adjust_balance( o.fee_payer(), o.fee ); // deposit the fee to the temp account
   const auto& bbi = db().get_index_type<blinded_balance_index>();
   const auto& cidx = bbi.indices().get<by_commitment>();
   for( const auto& in : o.inputs )
   {
      auto itr = cidx.find( in.commitment );
      GRAPHENE_ASSERT( itr != cidx.end(), blind_transfer_unknown_commitment, "", ("commitment",in.commitment) );
      db().remove( *itr );
   }
   for( const auto& out : o.outputs )
   {
      db().create<blinded_balance_object>( [&]( blinded_balance_object& obj ){
          obj.asset_id   = o.fee.asset_id;
          obj.owner      = out.owner;
          obj.commitment = out.commitment;
      });
   }
   const auto& add = o.fee.asset_id(db()).dynamic_asset_data_id(db());  
   db().modify( add, [&]( asset_dynamic_data_object& obj ){
      obj.confidential_supply -= o.fee.amount;
      FC_ASSERT( obj.confidential_supply >= 0 );
   });

   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) } // GCOVR_EXCL_LINE

void blind_transfer_evaluator::pay_fee()
{
   if( db().head_block_time() >= HARDFORK_563_TIME )
      pay_fba_fee( fba_accumulator_id_blind_transfer );
   else
      generic_evaluator::pay_fee();
}

} } // graphene::chain
