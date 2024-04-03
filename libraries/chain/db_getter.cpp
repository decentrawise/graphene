#include <graphene/chain/database.hpp>

#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/chain_property_object.hpp>
#include <graphene/chain/global_property_object.hpp>

namespace graphene { namespace chain {

const asset_object& database::get_core_asset() const
{
   return *_p_core_asset_obj;
}

const asset_dynamic_data_object& database::get_core_dynamic_data() const
{
   return *_p_core_dynamic_data_obj;
}

const global_property_object& database::get_global_properties()const
{
   return *_p_global_prop_obj;
}

const chain_property_object& database::get_chain_properties()const
{
   return *_p_chain_property_obj;
}

const dynamic_global_property_object& database::get_dynamic_global_properties() const
{
   return *_p_dyn_global_prop_obj;
}

const fee_schedule&  database::current_fee_schedule()const
{
   return get_global_properties().parameters.get_current_fees();
}

time_point_sec database::head_block_time()const
{
   return get_dynamic_global_properties().time;
}

uint32_t database::head_block_num()const
{
   return get_dynamic_global_properties().head_block_number;
}

block_id_type database::head_block_id()const
{
   return get_dynamic_global_properties().head_block_id;
}

decltype( chain_parameters::block_interval ) database::block_interval( )const
{
   return get_global_properties().parameters.block_interval;
}

const chain_id_type& database::get_chain_id( )const
{
   return get_chain_properties().chain_id;
}

const node_property_object& database::get_node_properties()const
{
   return _node_property_object;
}

node_property_object& database::node_properties()
{
   return _node_property_object;
}

uint32_t database::last_non_undoable_block_num() const
{
   /*
   There is a case when a value of undo_db.size() is greater then head_block_num(),
   and as result we get a wrong value for last_non_undoable_block_num.
   To resolve it we should take into account a number of active_sessions in calculations of
   last_non_undoable_block_num (active sessions are related to a new block which is under generation).
   */
   return head_block_num() - ( _undo_db.size() - _undo_db.active_sessions() );
}

const account_statistics_object& database::get_account_stats_by_owner( account_id_type owner )const
{
   return account_statistics_id_type(owner.instance)(*this);
}

const producer_schedule_object& database::get_producer_schedule_object()const
{
   return *_p_producer_schedule_obj;
}

} }
