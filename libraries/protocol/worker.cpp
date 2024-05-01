#include <graphene/protocol/worker.hpp>

#include <fc/io/raw.hpp>

namespace graphene { namespace protocol {

   void worker_create_operation::validate() const
   {
      FC_ASSERT(fee.amount >= 0);
      FC_ASSERT(work_end_date > work_begin_date);
      FC_ASSERT(daily_pay > 0);
      FC_ASSERT(daily_pay < GRAPHENE_CORE_ASSET_MAX_SUPPLY);
      FC_ASSERT(name.size() < GRAPHENE_WORKER_NAME_MAX_LENGTH );
      FC_ASSERT(url.size() < GRAPHENE_URL_MAX_LENGTH );
   }

} }

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::worker_create_operation::fee_parameters_type )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::worker_create_operation )
