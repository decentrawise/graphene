#include <graphene/chain/database.hpp>
#include <graphene/chain/global_property_object.hpp>
#include <graphene/chain/validator_object.hpp>
#include <graphene/chain/producer_schedule_object.hpp>

#include <fc/popcount.hpp>

namespace graphene { namespace chain {

using boost::container::flat_set;

validator_id_type database::get_scheduled_producer( uint32_t slot_num )const
{
   const dynamic_global_property_object& dpo = get_dynamic_global_properties();
   const producer_schedule_object& wso = get_producer_schedule_object();
   uint64_t current_aslot = dpo.current_aslot + slot_num;
   return wso.current_shuffled_producers[ current_aslot % wso.current_shuffled_producers.size() ];
}

fc::time_point_sec database::get_slot_time(uint32_t slot_num)const
{
   if( slot_num == 0 )
      return fc::time_point_sec();

   auto interval = block_interval();
   const dynamic_global_property_object& dpo = get_dynamic_global_properties();

   if( head_block_num() == 0 )
   {
      // n.b. first block is at genesis_time plus one block interval
      fc::time_point_sec genesis_time = dpo.time;
      return genesis_time + slot_num * interval;
   }

   int64_t head_block_abs_slot = head_block_time().sec_since_epoch() / interval;
   fc::time_point_sec head_slot_time(head_block_abs_slot * interval);

   const global_property_object& gpo = get_global_properties();

   if( dpo.dynamic_flags & dynamic_global_property_object::maintenance_flag )
      slot_num += gpo.parameters.maintenance_skip_slots;

   // "slot 0" is head_slot_time
   // "slot 1" is head_slot_time,
   //   plus maint interval if head block is a maint block
   //   plus block interval if head block is not a maint block
   return head_slot_time + (slot_num * interval);
}

uint32_t database::get_slot_at_time(fc::time_point_sec when)const
{
   fc::time_point_sec first_slot_time = get_slot_time( 1 );
   if( when < first_slot_time )
      return 0;
   return (when - first_slot_time).to_seconds() / block_interval() + 1;
}

uint32_t database::update_producer_missed_blocks( const signed_block& b )
{
   uint32_t missed_blocks = get_slot_at_time( b.timestamp );
   FC_ASSERT( missed_blocks != 0, "Trying to push double-produced block onto current block?!" );
   missed_blocks--;
   const auto& validators = producer_schedule_id_type()(*this).current_shuffled_producers;
   if( missed_blocks < validators.size() )
      for( uint32_t i = 0; i < missed_blocks; ++i ) {
         const auto& producer_missed = get_scheduled_producer( i+1 )(*this);
         modify( producer_missed, []( validator_object& w ) {
            w.total_missed++;
         });
      }
   return missed_blocks;
}

uint32_t database::producer_participation_rate()const
{
   const dynamic_global_property_object& dpo = get_dynamic_global_properties();
   return uint64_t(GRAPHENE_100_PERCENT) * fc::popcount(dpo.recent_slots_filled) / 128;
}

void database::update_producer_schedule()
{
   const producer_schedule_object& wso = get_producer_schedule_object();
   const global_property_object& gpo = get_global_properties();

   if( head_block_num() % gpo.block_producers.size() == 0 )
   {
      modify( wso, [&]( producer_schedule_object& _wso )
      {
         _wso.current_shuffled_producers.clear();
         _wso.current_shuffled_producers.reserve( gpo.block_producers.size() );

         for( const validator_id_type& w : gpo.block_producers )
            _wso.current_shuffled_producers.push_back( w );

         auto now_hi = uint64_t(head_block_time().sec_since_epoch()) << 32;
         for( uint32_t i = 0; i < _wso.current_shuffled_producers.size(); ++i )
         {
            /// High performance random generator
            /// http://xorshift.di.unimi.it/
            uint64_t k = now_hi + uint64_t(i)*2685821657736338717ULL;
            k ^= (k >> 12);
            k ^= (k << 25);
            k ^= (k >> 27);
            k *= 2685821657736338717ULL;

            uint32_t jmax = _wso.current_shuffled_producers.size() - i;
            uint32_t j = i + k%jmax;
            std::swap( _wso.current_shuffled_producers[i],
                       _wso.current_shuffled_producers[j] );
         }
      });
   }
}

} }
