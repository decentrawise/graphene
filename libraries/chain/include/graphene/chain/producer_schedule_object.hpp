#pragma once
#include <graphene/protocol/types.hpp>
#include <graphene/db/generic_index.hpp>

namespace graphene { namespace chain {

      class producer_schedule_object : public graphene::db::abstract_object<producer_schedule_object,
                                                                           implementation_ids, impl_producer_schedule_object_type>
      {
      public:
         vector<validator_id_type> current_shuffled_producers;
};

} }

MAP_OBJECT_ID_TO_TYPE(graphene::chain::producer_schedule_object)

FC_REFLECT_TYPENAME( graphene::chain::producer_schedule_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::producer_schedule_object )
