#pragma once
#include <graphene/chain/types.hpp>
#include <graphene/db/generic_index.hpp>
#include <graphene/protocol/vote.hpp>

namespace graphene { namespace chain {
class database;

/**
  * @defgroup worker_types Implementations of the various worker types in the system
  *
  * The system has various worker types, which do different things with the money they are paid. These worker types
  * and their semantics are specified here.
  *
  * All worker types exist as a struct containing the data this worker needs to evaluate, as well as a method
  * pay_worker, which takes a pay amount and a non-const database reference, and applies the worker's specific pay
  * semantics to the worker_type struct and/or the database. Furthermore, all worker types have an initializer,
  * which is a struct containing the data needed to create that kind of worker.
  *
  * Each initializer type has a method, init, which takes a non-const database reference, a const reference to the
  * worker object being created, and a non-const reference to the specific *_worker_type object to initialize. The
  * init method creates any further objects, and initializes the worker_type object as necessary according to the
  * semantics of that particular worker type.
  *
  * To create a new worker type, define a my_new_worker_type struct with a pay_worker method which updates the
  * my_new_worker_type object and/or the database. Create a my_new_worker_type::initializer struct with an init
  * method and any data members necessary to create a new worker of this type. Reflect my_new_worker_type and
  * my_new_worker_type::initializer into FC's type system, and add them to @ref worker_type and @ref
  * worker_initializer respectively. Make sure the order of types in @ref worker_type and @ref worker_initializer
  * remains the same.
  * @{
  */
/**
 * @brief A worker who returns all of his pay to the reserve
 *
 * This worker type pays everything he receives back to the network's reserve funds pool.
 */
struct refund_worker_type
{
   /// Record of how much this worker has burned in his lifetime
   share_type total_burned;

   void pay_worker(share_type pay, database&);
};

/**
 * @brief A worker who sends his pay to a vesting balance
 *
 * This worker type takes all of his pay and places it into a vesting balance
 */
struct vesting_balance_worker_type
{
   /// The balance this worker pays into
   vesting_balance_id_type balance;

   void pay_worker(share_type pay, database& db);
};

/**
 * @brief A worker who permanently destroys all of his pay
 *
 * This worker sends all pay he receives to the null account.
 */
struct burn_worker_type
{
   /// Record of how much this worker has burned in his lifetime
   share_type total_burned;

   void pay_worker(share_type pay, database&);
};
///@}

// The ordering of types in these two static variants MUST be the same.
typedef static_variant<
   refund_worker_type,
   vesting_balance_worker_type,
   burn_worker_type
> worker_type;


/**
 * @brief Worker object contains the details of a blockchain worker. See @ref workers for details.
 */
class worker_object : public abstract_object<worker_object, protocol_ids, worker_object_type>
{
   public:
      /// ID of the account which owns this worker
      account_id_type worker_account;
      /// Time at which this worker begins receiving pay, if elected
      time_point_sec work_begin_date;
      /// Time at which this worker will cease to receive pay. Worker will be deleted at this time
      time_point_sec work_end_date;
      /// Amount in CORE this worker will be paid each day
      share_type daily_pay;
      /// ID of this worker's pay balance
      worker_type worker;
      /// Human-readable name for the worker
      string name;
      /// URL to a web page representing this worker
      string url;

      /// Voting ID of this worker
      vote_id_type vote_id;

      uint64_t total_votes = 0;

      bool is_active(fc::time_point_sec now)const {
         return now >= work_begin_date && now <= work_end_date;
      }
};

struct by_account;
struct by_vote_id;
struct by_end_date;
typedef multi_index_container<
   worker_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
      ordered_non_unique< tag<by_account>, member< worker_object, account_id_type, &worker_object::worker_account > >,
      ordered_unique< tag<by_vote_id>, member< worker_object, vote_id_type, &worker_object::vote_id > >,
      ordered_non_unique< tag<by_end_date>, member< worker_object, time_point_sec, &worker_object::work_end_date> >
   >
> worker_object_multi_index_type;

using worker_index = generic_index<worker_object, worker_object_multi_index_type>;

} } // graphene::chain

MAP_OBJECT_ID_TO_TYPE(graphene::chain::worker_object)

FC_REFLECT_TYPENAME( graphene::chain::refund_worker_type )
FC_REFLECT_TYPENAME( graphene::chain::vesting_balance_worker_type )
FC_REFLECT_TYPENAME( graphene::chain::burn_worker_type )
FC_REFLECT_TYPENAME( graphene::chain::worker_type )
FC_REFLECT_TYPENAME( graphene::chain::worker_object )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::chain::worker_object )
