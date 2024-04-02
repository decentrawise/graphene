#pragma once

#include <graphene/protocol/asset.hpp>
#include <graphene/db/generic_index.hpp>

namespace graphene { namespace chain {
   using namespace graphene::db;

   class validator_object : public abstract_object<validator_object, protocol_ids, validator_object_type>
   {
      public:
         account_id_type  validator_account;
         uint64_t         last_aslot = 0;
         public_key_type  signing_key;
         optional< vesting_balance_id_type > pay_vb;
         vote_id_type     vote_id;
         uint64_t         total_votes = 0;
         string           url;
         int64_t          total_missed = 0;
         uint32_t         last_confirmed_block_num = 0;

         validator_object() : vote_id(vote_id_type::validator) {}
   };

   struct by_account;
   struct by_vote_id;
   struct by_last_block;
   using validator_multi_index_type = multi_index_container<
      validator_object,
      indexed_by<
         ordered_unique< tag<by_id>,
            member<object, object_id_type, &object::id>
         >,
         ordered_unique< tag<by_account>,
            member<validator_object, account_id_type, &validator_object::validator_account>
         >,
         ordered_unique< tag<by_vote_id>,
            member<validator_object, vote_id_type, &validator_object::vote_id>
         >
      >
   >;
   using validator_index = generic_index<validator_object, validator_multi_index_type>;
} } // graphene::chain

MAP_OBJECT_ID_TO_TYPE(graphene::chain::validator_object)

FC_REFLECT_TYPENAME( graphene::chain::validator_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::validator_object )
