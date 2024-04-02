#pragma once

#include <graphene/protocol/chain_parameters.hpp>
#include <graphene/chain/types.hpp>
#include <graphene/db/object.hpp>

namespace graphene { namespace chain {

   /**
    * @class global_property_object
    * @brief Maintains global state information (delegate list, current fees)
    * @ingroup object
    * @ingroup implementation
    *
    * This is an implementation detail. The values here are set by delegates to tune the blockchain parameters.
    */
   class global_property_object : public graphene::db::abstract_object<global_property_object,
                                                                       implementation_ids, impl_global_property_object_type>
   {
      public:
         chain_parameters           parameters;
         optional<chain_parameters> pending_parameters;

         uint32_t                           next_available_vote_id = 0;
         vector<delegate_id_type>   active_delegates; // updated once per maintenance interval
         flat_set<validator_id_type>          block_producers; // updated once per maintenance interval
         // n.b. validator scheduling is done by validator_schedule object
   };

   /**
    * @class dynamic_global_property_object
    * @brief Maintains global state information (delegate list, current fees)
    * @ingroup object
    * @ingroup implementation
    *
    * This is an implementation detail. The values here are calculated during normal chain operations and reflect the
    * current values of global blockchain properties.
    */
   class dynamic_global_property_object : public abstract_object<dynamic_global_property_object,
                                                                 implementation_ids, impl_dynamic_global_property_object_type>
   {
      public:
         uint32_t          head_block_number = 0;
         block_id_type     head_block_id;
         time_point_sec    time;
         validator_id_type   current_validator;
         time_point_sec    next_maintenance_time;
         time_point_sec    last_budget_time;
         share_type        validator_budget;
         uint32_t          accounts_registered_this_interval = 0;
         /**
          *  Every time a block is missed this increases by
          *  RECENTLY_MISSED_COUNT_INCREMENT,
          *  every time a block is found it decreases by
          *  RECENTLY_MISSED_COUNT_DECREMENT.  It is
          *  never less than 0.
          */
         uint32_t          recently_missed_count = 0;

         /**
          * The current absolute slot number.  Equal to the total
          * number of slots since genesis.  Also equal to the total
          * number of missed slots plus head_block_number.
          */
         uint64_t                current_aslot = 0;

         /**
          * used to compute validator participation.
          */
         fc::uint128_t recent_slots_filled;

         /**
          * dynamic_flags specifies chain state properties that can be
          * expressed in one bit.
          */
         uint32_t dynamic_flags = 0;

         uint32_t last_irreversible_block_num = 0;

         enum dynamic_flag_bits
         {
            /**
             * If maintenance_flag is set, then the head block is a
             * maintenance block.  This means
             * get_time_slot(1) - head_block_time() will have a gap
             * due to maintenance duration.
             *
             * This flag answers the question, "Was maintenance
             * performed in the last call to apply_block()?"
             */
            maintenance_flag = 0x01
         };
   };
}}

MAP_OBJECT_ID_TO_TYPE(graphene::chain::dynamic_global_property_object)
MAP_OBJECT_ID_TO_TYPE(graphene::chain::global_property_object)

FC_REFLECT_TYPENAME( graphene::chain::dynamic_global_property_object )
FC_REFLECT_TYPENAME( graphene::chain::global_property_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::dynamic_global_property_object )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::global_property_object )
