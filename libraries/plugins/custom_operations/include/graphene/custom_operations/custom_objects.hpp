#pragma once

#include <boost/multi_index/composite_key.hpp>
#include <graphene/chain/database.hpp>

namespace graphene { namespace custom_operations {

using namespace chain;

#ifndef CUSTOM_OPERATIONS_SPACE_ID
#define CUSTOM_OPERATIONS_SPACE_ID 7
#endif

#define CUSTOM_OPERATIONS_MAX_KEY_SIZE (200)

enum class custom_operations_object_types {
   account_map = 0
};

struct account_storage_object : public abstract_object<account_storage_object, CUSTOM_OPERATIONS_SPACE_ID,
                                                       static_cast<uint8_t>( custom_operations_object_types::account_map )>
{
   account_id_type account;
   string catalog;
   string key;
   optional<variant> value;
};

struct by_account_catalog_key;
struct by_account_catalog;
struct by_account;
struct by_catalog_key;
struct by_catalog;

using account_storage_multi_index_type = multi_index_container<
   account_storage_object,
   indexed_by<
      ordered_unique<tag<by_id>, member<object, object_id_type, &object::id>>,
      ordered_unique<tag<by_account_catalog_key>,
         composite_key<account_storage_object,
            member<account_storage_object, account_id_type, &account_storage_object::account>,
            member<account_storage_object, string, &account_storage_object::catalog>,
            member<account_storage_object, string, &account_storage_object::key>
         >
      >,
      ordered_unique< tag<by_account_catalog>,
         composite_key< account_storage_object,
            member< account_storage_object, account_id_type, &account_storage_object::account >,
            member< account_storage_object, string, &account_storage_object::catalog >,
            member< object, object_id_type, &object::id >
         >
      >,
      ordered_unique< tag<by_account>,
         composite_key< account_storage_object,
            member< account_storage_object, account_id_type, &account_storage_object::account >,
            member< object, object_id_type, &object::id >
         >
      >,
      ordered_unique< tag<by_catalog_key>,
         composite_key< account_storage_object,
            member< account_storage_object, string, &account_storage_object::catalog >,
            member< account_storage_object, string, &account_storage_object::key >,
            member< object, object_id_type, &object::id >
         >
      >,
      ordered_unique< tag<by_catalog>,
         composite_key< account_storage_object,
            member< account_storage_object, string, &account_storage_object::catalog >,
            member< object, object_id_type, &object::id >
         >
      >
   >
>;

using account_storage_index = generic_index<account_storage_object, account_storage_multi_index_type>;

using account_storage_id_type = object_id<account_storage_object::space_id, account_storage_object::type_id>;

} } // graphene::custom_operations

FC_REFLECT_DERIVED(graphene::custom_operations::account_storage_object, (graphene::db::object),
                   (account)(catalog)(key)(value))
FC_REFLECT_ENUM(graphene::custom_operations::custom_operations_object_types, (account_map))