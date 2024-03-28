#include <graphene/chain/database.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/transaction_evaluation_state.hpp>
#include <graphene/chain/proposal_object.hpp>

namespace graphene { namespace chain {

bool proposal_object::is_authorized_to_execute(database& db) const
{
   transaction_evaluation_state dry_run_eval(&db);

   try {
      verify_authority( proposed_transaction.operations, 
                        available_key_approvals,
                        [&]( account_id_type id ){ return &id(db).active; },
                        [&]( account_id_type id ){ return &id(db).owner;  },
                        db.get_global_properties().parameters.max_authority_depth,
                        true, /* allow committee */
                        available_active_approvals,
                        available_owner_approvals );
   } 
   catch ( const fc::exception& e )
   {
      return false;
   }
   return true;
}

void required_approval_index::object_inserted( const object& obj )
{
    assert( dynamic_cast<const proposal_object*>(&obj) );
    const proposal_object& p = static_cast<const proposal_object&>(obj);
    const proposal_id_type proposal_id = p.get_id();

    for( const auto& a : p.required_active_approvals )
       _account_to_proposals[a].insert( proposal_id );
    for( const auto& a : p.required_owner_approvals )
       _account_to_proposals[a].insert( proposal_id );
    for( const auto& a : p.available_active_approvals )
       _account_to_proposals[a].insert( proposal_id );
    for( const auto& a : p.available_owner_approvals )
       _account_to_proposals[a].insert( proposal_id );
}

void required_approval_index::remove( account_id_type a, proposal_id_type p )
{
    auto itr = _account_to_proposals.find(a);
    if( itr != _account_to_proposals.end() )
    {
        itr->second.erase( p );
        if( itr->second.empty() )
            _account_to_proposals.erase( itr->first );
    }
}

void required_approval_index::object_removed( const object& obj )
{
    assert( dynamic_cast<const proposal_object*>(&obj) );
    const proposal_object& p = static_cast<const proposal_object&>(obj);
    const proposal_id_type proposal_id = p.get_id();

    for( const auto& a : p.required_active_approvals )
       remove( a, proposal_id );
    for( const auto& a : p.required_owner_approvals )
       remove( a, proposal_id );
    for( const auto& a : p.available_active_approvals )
       remove( a, proposal_id );
    for( const auto& a : p.available_owner_approvals )
       remove( a, proposal_id );
}

void required_approval_index::insert_or_remove_delta( proposal_id_type p,
                                                      const flat_set<account_id_type>& before,
                                                      const flat_set<account_id_type>& after )
{
    auto b = before.begin();
    auto a = after.begin();
    while( b != before.end() || a != after.end() )
    {
       if( a == after.end() || (b != before.end() && *b < *a) )
       {
           remove( *b, p );
           ++b;
       }
       else if( b == before.end() || (a != after.end() && *a < *b) )
       {
           _account_to_proposals[*a].insert( p );
           ++a;
       }
       else // *a == *b
       {
           ++a;
           ++b;
       }
    }
}

void required_approval_index::about_to_modify( const object& before )
{
    const proposal_object& p = static_cast<const proposal_object&>(before);
    available_active_before_modify = p.available_active_approvals;
    available_owner_before_modify  = p.available_owner_approvals;
}

void required_approval_index::object_modified( const object& after )
{
    const proposal_object& p = static_cast<const proposal_object&>(after);
    const proposal_id_type proposal_id = p.get_id();
    insert_or_remove_delta( proposal_id, available_active_before_modify, p.available_active_approvals );
    insert_or_remove_delta( proposal_id, available_owner_before_modify,  p.available_owner_approvals );
}

} } // graphene::chain

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::proposal_object, (graphene::chain::object),
                    (expiration_time)(review_period_time)(proposed_transaction)(required_active_approvals)
                    (available_active_approvals)(required_owner_approvals)(available_owner_approvals)
                    (available_key_approvals)(proposer)(fail_reason) )

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::proposal_object )
