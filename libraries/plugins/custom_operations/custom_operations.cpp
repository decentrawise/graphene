#include <graphene/custom_operations/custom_operations.hpp>

namespace graphene { namespace custom_operations {

void account_storage_map::validate()const
{
   FC_ASSERT(catalog.length() <= CUSTOM_OPERATIONS_MAX_KEY_SIZE && catalog.length() > 0);
}

} } //graphene::custom_operations

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::custom_operations::account_storage_map )
