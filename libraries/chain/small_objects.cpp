#include <graphene/protocol/fee_schedule.hpp>

#include <graphene/chain/balance_object.hpp>
#include <graphene/chain/block_summary_object.hpp>
#include <graphene/chain/budget_record_object.hpp>
#include <graphene/chain/buyback_object.hpp>
#include <graphene/chain/chain_property_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/confidential_object.hpp>
#include <graphene/chain/fba_object.hpp>
#include <graphene/chain/global_property_object.hpp>
#include <graphene/chain/htlc_object.hpp>
#include <graphene/chain/operation_history_object.hpp>
#include <graphene/chain/special_authority_object.hpp>
#include <graphene/chain/transaction_history_object.hpp>
#include <graphene/chain/withdraw_permission_object.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/witness_schedule_object.hpp>
#include <graphene/chain/worker_object.hpp>

#include <fc/io/raw.hpp>

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::balance_object, (graphene::db::object),
                    (owner)(balance)(vesting_policy)(last_claim_date) )

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::block_summary_object, (graphene::db::object), (block_id) )

FC_REFLECT_DERIVED_NO_TYPENAME(
   graphene::chain::budget_record, BOOST_PP_SEQ_NIL,
   (time_since_last_budget)
   (from_initial_reserve)
   (from_accumulated_fees)
   (from_unused_witness_budget)
   (requested_witness_budget)
   (total_budget)
   (witness_budget)
   (worker_budget)
   (leftover_worker_funds)
   (supply_delta)
)

FC_REFLECT_DERIVED_NO_TYPENAME(
   graphene::chain::budget_record_object,
   (graphene::db::object),
   (time)
   (record)
)

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::buyback_object, (graphene::db::object), (asset_to_buy) )


FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::immutable_chain_parameters, BOOST_PP_SEQ_NIL,
   (min_committee_member_count)
   (min_witness_count)
   (num_special_accounts)
   (num_special_assets)
)

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::chain_property_object, (graphene::db::object),
                    (chain_id)
                    (immutable_parameters)
                  )

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::committee_member_object, (graphene::db::object),
                    (committee_member_account)(vote_id)(total_votes)(url) )

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::blinded_balance_object, (graphene::db::object),
                                (commitment)(asset_id)(owner) )

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::fba_accumulator_object, (graphene::db::object),
                                (accumulated_fba_fees)(designated_asset) )

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::dynamic_global_property_object, (graphene::db::object),
                    (head_block_number)
                    (head_block_id)
                    (time)
                    (current_witness)
                    (next_maintenance_time)
                    (last_budget_time)
                    (witness_budget)
                    (accounts_registered_this_interval)
                    (recently_missed_count)
                    (current_aslot)
                    (recent_slots_filled)
                    (dynamic_flags)
                    (last_irreversible_block_num)
                  )

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::global_property_object, (graphene::db::object),
                    (parameters)
                    (pending_parameters)
                    (next_available_vote_id)
                    (active_committee_members)
                    (active_witnesses)
                  )

FC_REFLECT( graphene::chain::htlc_object::transfer_info,
   (from) (to) (amount) (asset_id) )
FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::htlc_object::condition_info::hash_lock_info, BOOST_PP_SEQ_NIL,
   (preimage_hash) (preimage_size) )
FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::htlc_object::condition_info::time_lock_info, BOOST_PP_SEQ_NIL,
   (expiration) )
FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::htlc_object::condition_info, BOOST_PP_SEQ_NIL,
   (hash_lock)(time_lock) )
FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::htlc_object, (graphene::db::object),
               (transfer) (conditions) )

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::operation_history_object, (graphene::chain::object),
                    (op)(result)(block_num)(trx_in_block)(op_in_trx)(virtual_op)(is_virtual)(block_time) )

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::account_history_object, (graphene::chain::object),
                    (account)(operation_id)(sequence)(next) )

FC_REFLECT_DERIVED_NO_TYPENAME(
   graphene::chain::special_authority_object,
   (graphene::db::object),
   (account)
)

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::transaction_history_object, (graphene::db::object), (trx)(trx_id) )

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::withdraw_permission_object, (graphene::db::object),
                    (withdraw_from_account)
                    (authorized_account)
                    (withdrawal_limit)
                    (withdrawal_period_sec)
                    (period_start_time)
                    (expiration)
                    (claimed_this_period)
                 )

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::witness_object, (graphene::db::object),
                    (witness_account)
                    (last_aslot)
                    (signing_key)
                    (pay_vb)
                    (vote_id)
                    (total_votes)
                    (url)
                    (total_missed)
                    (last_confirmed_block_num)
                  )

FC_REFLECT_DERIVED_NO_TYPENAME(
   graphene::chain::witness_schedule_object,
   (graphene::db::object),
   (current_shuffled_witnesses)
)


FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::refund_worker_type, BOOST_PP_SEQ_NIL, (total_burned) )
FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::vesting_balance_worker_type, BOOST_PP_SEQ_NIL, (balance) )
FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::burn_worker_type, BOOST_PP_SEQ_NIL, (total_burned) )
FC_REFLECT_DERIVED_NO_TYPENAME( graphene::chain::worker_object, (graphene::db::object),
                    (worker_account)
                    (work_begin_date)
                    (work_end_date)
                    (daily_pay)
                    (worker)
                    (vote_id)
                    (total_votes)
                    (name)
                    (url)
                  )

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::balance_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::block_summary_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::budget_record )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::budget_record_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::buyback_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::immutable_chain_parameters )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::chain_property_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::committee_member_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::blinded_balance_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::fba_accumulator_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::dynamic_global_property_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::global_property_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::htlc_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::operation_history_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::account_history_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::special_authority_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::transaction_history_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::withdraw_permission_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::witness_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::witness_schedule_object )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::chain::worker_object )
