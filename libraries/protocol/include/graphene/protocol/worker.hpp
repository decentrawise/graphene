#pragma once

#include <graphene/protocol/base.hpp>
#include <graphene/protocol/asset.hpp>

namespace graphene { namespace protocol {

   /**
    * @defgroup workers The Blockchain Worker System
    * @ingroup operations
    *
    * Graphene blockchains allow the creation of special "workers" which are elected positions paid by the blockchain
    * for services they provide. There may be several types of workers, and the semantics of how and when they are paid
    * are defined by the @ref worker_type_enum enumeration. All workers are elected by core stakeholder approval, by
    * voting for or against them.
    *
    * Workers are paid from the blockchain's daily budget if their total approval (votes for - votes against) is
    * positive, ordered from most positive approval to least, until the budget is exhausted. Payments are processed at
    * the blockchain maintenance interval. If a worker does not have positive approval during payment processing, or if
    * the chain's budget is exhausted before the worker is paid, that worker is simply not paid at that interval.
    * Payment is not prorated based on percentage of the interval the worker was approved. If the chain attempts to pay
    * a worker, but the budget is insufficient to cover its entire pay, the worker is paid the remaining budget funds,
    * even though this does not fulfill his total pay. The worker will not receive extra pay to make up the difference
    * later. Worker pay is placed in a vesting balance and vests over the number of days specified at the worker's
    * creation.
    *
    * Once created, a worker is immutable and will be kept by the blockchain forever.
    *
    * @{
    */


   struct vesting_balance_worker_initializer
   {
      vesting_balance_worker_initializer(uint16_t days=0):pay_vesting_period_days(days){}
      uint16_t pay_vesting_period_days = 0;
   };

   struct burn_worker_initializer
   {};

   struct refund_worker_initializer
   {};


   typedef static_variant< 
      refund_worker_initializer,
      vesting_balance_worker_initializer,
      burn_worker_initializer > worker_initializer;


   /**
    * @brief Create a new worker object
    * @ingroup operations
    */
   struct worker_create_operation : public base_operation
   {
      struct fee_parameters_type { uint64_t fee = 5000*GRAPHENE_BLOCKCHAIN_PRECISION; };

      asset                fee;
      account_id_type      owner;
      time_point_sec       work_begin_date;
      time_point_sec       work_end_date;
      share_type           daily_pay;
      string               name;
      string               url;
      /// This should be set to the initializer appropriate for the type of worker to be created.
      worker_initializer   initializer;

      account_id_type   fee_payer()const { return owner; }
      void              validate()const;
   };
   ///@}

} }

FC_REFLECT( graphene::protocol::vesting_balance_worker_initializer, (pay_vesting_period_days) )
FC_REFLECT( graphene::protocol::burn_worker_initializer, )
FC_REFLECT( graphene::protocol::refund_worker_initializer, )
FC_REFLECT_TYPENAME( graphene::protocol::worker_initializer )

FC_REFLECT( graphene::protocol::worker_create_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::protocol::worker_create_operation,
            (fee)(owner)(work_begin_date)(work_end_date)(daily_pay)(name)(url)(initializer) )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::worker_create_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::worker_create_operation )
