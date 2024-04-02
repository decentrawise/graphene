#pragma once
#include <graphene/chain/types.hpp>
#include <graphene/db/generic_index.hpp>
#include <graphene/protocol/vote.hpp>

namespace graphene { namespace chain {
   using namespace graphene::db;

   /**
    *  @brief tracks information about a delegate account.
    *  @ingroup object
    *
    *  A delegate is responsible for setting blockchain parameters and has
    *  dynamic multi-sig control over the council account.  The current set of
    *  active delegates has control.
    *
    *  delegates were separated into a separate object to make iterating over
    *  the set of delegate easy.
    */
   class delegate_object : public abstract_object<delegate_object,
                                                          protocol_ids, delegate_object_type>
   {
      public:
         account_id_type  delegate_account;
         vote_id_type     vote_id;
         uint64_t         total_votes = 0;
         string           url;
   };

   struct by_account;
   struct by_vote_id;
   using delegate_multi_index_type = multi_index_container<
      delegate_object,
      indexed_by<
         ordered_unique< tag<by_id>,
            member<object, object_id_type, &object::id>
         >,
         ordered_unique< tag<by_account>,
            member<delegate_object, account_id_type, &delegate_object::delegate_account>
         >,
         ordered_unique< tag<by_vote_id>,
            member<delegate_object, vote_id_type, &delegate_object::vote_id>
         >
      >
   >;
   using delegate_index = generic_index<delegate_object, delegate_multi_index_type>;
} } // graphene::chain

MAP_OBJECT_ID_TO_TYPE(graphene::chain::delegate_object)

FC_REFLECT_TYPENAME( graphene::chain::delegate_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::delegate_object )
