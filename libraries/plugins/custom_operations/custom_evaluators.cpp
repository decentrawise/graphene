#include <graphene/chain/database.hpp>

#include <graphene/custom_operations/custom_operations_plugin.hpp>
#include <graphene/custom_operations/custom_objects.hpp>
#include <graphene/custom_operations/custom_evaluators.hpp>

namespace graphene { namespace custom_operations {

custom_generic_evaluator::custom_generic_evaluator(database& db, const account_id_type account)
{
   _db = &db;
   _account = account;
}

vector<object_id_type> custom_generic_evaluator::do_apply(const account_storage_map& op)
{
   const auto &index = _db->get_index_type<account_storage_index>().indices().get<by_account_catalog_key>();
   vector<object_id_type> results;
   results.reserve( op.key_values.size() );

   if (op.remove)
   {
      for(auto const& row: op.key_values) {
         auto itr = index.find(make_tuple(_account, op.catalog, row.first));
         if(itr != index.end()) {
            results.push_back(itr->id);
            _db->remove(*itr);
         }
      }
   }
   else {
      for(auto const& row: op.key_values) {
         if(row.first.length() > CUSTOM_OPERATIONS_MAX_KEY_SIZE)
         {
            wlog("Key can't be bigger than ${max} characters", ("max", CUSTOM_OPERATIONS_MAX_KEY_SIZE));
            continue;
         }
         auto itr = index.find(make_tuple(_account, op.catalog, row.first));
         if(itr == index.end())
         {
            try {
               const auto& created = _db->create<account_storage_object>(
                                        [&op, this, &row]( account_storage_object& aso ) {
                  aso.account = _account;
                  aso.catalog = op.catalog;
                  aso.key = row.first;
                  if(row.second.valid())
                     aso.value = fc::json::from_string(*row.second);
               });
               results.push_back(created.id);
            }
            catch(const fc::parse_error_exception& e) { wlog(e.to_detail_string()); }
         }
         else
         {
            try {
               _db->modify(*itr, [&row](account_storage_object &aso) {
                  if(row.second.valid())
                     aso.value = fc::json::from_string(*row.second);
                  else
                     aso.value.reset();
               });
               results.push_back(itr->id);
            }
            catch(const fc::parse_error_exception& e) { wlog((e.to_detail_string())); }
         }
      }
   }
   return results;
}

} }
