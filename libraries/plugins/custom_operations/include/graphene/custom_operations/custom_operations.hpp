#pragma once

#include <graphene/chain/database.hpp>

#include "custom_objects.hpp"

namespace graphene { namespace custom_operations {

using namespace std;
using graphene::protocol::account_id_type;

struct account_storage_map : chain::base_operation
{
   bool remove;
   string catalog;
   flat_map<string, optional<string>> key_values;

   void validate()const;
};

} } //graphene::custom_operations

FC_REFLECT( graphene::custom_operations::account_storage_map, (remove)(catalog)(key_values) )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::custom_operations::account_storage_map )
