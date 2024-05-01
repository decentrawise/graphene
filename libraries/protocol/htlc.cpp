#include <graphene/protocol/htlc.hpp>

#include <fc/io/raw.hpp>

#define SECONDS_PER_DAY (60 * 60 * 24)

namespace graphene { namespace protocol {

   void htlc_create_operation::validate()const {
      FC_ASSERT( fee.amount >= 0, "Fee amount should not be negative" );
      FC_ASSERT( amount.amount > 0, "HTLC amount should be greater than zero" );
   }

   amount_type htlc_create_operation::calculate_fee(const fee_parameters_type &fee_params) const
   {
      uint64_t days = (claim_period_seconds + SECONDS_PER_DAY - 1) / SECONDS_PER_DAY;
      amount_type total_fee = fee_params.fee;
      total_fee += amount_type(fee_params.fee_per_day) * days;
      if (extensions.value.memo.valid())
         total_fee += calculate_data_fee(fc::raw::pack_size(extensions.value.memo), fee_params.price_per_kbyte);
      return total_fee;
   }

   void htlc_redeem_operation::validate()const {
      FC_ASSERT( fee.amount >= 0, "Fee amount should not be negative" );
   }

   amount_type htlc_redeem_operation::calculate_fee( const fee_parameters_type& fee_params )const
   {
      uint64_t kb = ( preimage.size() + 1023 ) / 1024;
      uint64_t product = kb * fee_params.price_per_kbyte;
      FC_ASSERT( kb == 0 || product / kb == fee_params.price_per_kbyte, "Fee calculation overflow");
      return fee_params.fee + product;
   }

   void htlc_extend_operation::validate()const {
      FC_ASSERT( fee.amount >= 0 , "Fee amount should not be negative");
   }

   amount_type htlc_extend_operation::calculate_fee( const fee_parameters_type& fee_params )const
   {
      uint32_t days = ( seconds_to_add + SECONDS_PER_DAY - 1 ) / SECONDS_PER_DAY;
      uint64_t per_day_fee = fee_params.fee_per_day * days;
      FC_ASSERT( days == 0 || per_day_fee / days == fee_params.fee_per_day, "Fee calculation overflow" );
      return fee_params.fee + per_day_fee;
   }
} }

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::htlc_create_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::htlc_create_operation::additional_options_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::htlc_redeem_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::htlc_extend_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::htlc_create_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::htlc_redeem_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::htlc_redeemed_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::htlc_extend_operation )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::htlc_refund_operation )
