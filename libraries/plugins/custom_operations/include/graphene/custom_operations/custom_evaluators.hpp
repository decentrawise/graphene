#pragma once
#include <graphene/custom_operations/custom_objects.hpp>
#include <graphene/custom_operations/custom_operations.hpp>

namespace graphene { namespace custom_operations {

class custom_generic_evaluator
{
   public:
      database* _db;
      account_id_type _account;
      custom_generic_evaluator(database& db, const account_id_type account);

      vector<object_id_type> do_apply(const account_storage_map& o);
};

} }

FC_REFLECT_TYPENAME( graphene::custom_operations::custom_generic_evaluator )
