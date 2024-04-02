#pragma once
#include <graphene/protocol/types.hpp>
#include <graphene/db/generic_index.hpp>

namespace graphene { namespace chain {

      class validator_schedule_object : public graphene::db::abstract_object<validator_schedule_object,
                                                                           implementation_ids, impl_validator_schedule_object_type>
      {
      public:
         vector<validator_id_type> current_shuffled_validators;
};

} }

MAP_OBJECT_ID_TO_TYPE(graphene::chain::validator_schedule_object)

FC_REFLECT_TYPENAME( graphene::chain::validator_schedule_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::validator_schedule_object )
