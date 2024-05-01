#include <graphene/protocol/transfer.hpp>

#include <fc/io/raw.hpp>

namespace graphene { namespace protocol {

   amount_type transfer_operation::calculate_fee( const fee_parameters_type& schedule )const
   {
      amount_type core_fee_required = schedule.fee;
      if( memo )
         core_fee_required += calculate_data_fee( fc::raw::pack_size(memo), schedule.price_per_kbyte );
      return core_fee_required;
   }


   void transfer_operation::validate()const
   {
      FC_ASSERT( fee.amount >= 0 );
      FC_ASSERT( from != to );
      FC_ASSERT( amount.amount > 0 );
   }



   amount_type override_transfer_operation::calculate_fee( const fee_parameters_type& schedule )const
   {
      amount_type core_fee_required = schedule.fee;
      if( memo )
         core_fee_required += calculate_data_fee( fc::raw::pack_size(memo), schedule.price_per_kbyte );
      return core_fee_required;
   }


   void override_transfer_operation::validate()const
   {
      FC_ASSERT( fee.amount >= 0 );
      FC_ASSERT( from != to );
      FC_ASSERT( amount.amount > 0 );
      FC_ASSERT( issuer != from );
   }

} } // graphene::protocol

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::transfer_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::override_transfer_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::transfer_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::override_transfer_operation )
