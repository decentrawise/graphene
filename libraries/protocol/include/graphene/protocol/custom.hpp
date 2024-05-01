#pragma once

#include <graphene/protocol/base.hpp>
#include <graphene/protocol/asset.hpp>

namespace graphene { namespace protocol { 

   /**
    * @brief provides a generic way to add higher level protocols on top of validator consensus
    * @ingroup operations
    *
    * There is no validation for this operation other than that required auths are valid and a fee
    * is paid that is appropriate for the data contained.
    */
   struct custom_operation : public base_operation
   {
      struct fee_parameters_type { 
         uint64_t fee = GRAPHENE_CORE_ASSET_PRECISION; 
         uint32_t price_per_kbyte = 10;
      };

      asset                     fee;
      account_id_type           payer;
      flat_set<account_id_type> required_auths;
      uint16_t                  id = 0;
      vector<char>              data;

      account_id_type   fee_payer()const { return payer; }
      void              validate()const;
      amount_type        calculate_fee(const fee_parameters_type& k)const;
      void              get_required_active_authorities( flat_set<account_id_type>& auths )const {
         auths.insert(required_auths.begin(), required_auths.end());
      }
   };

} } // namespace graphene::protocol

FC_REFLECT( graphene::protocol::custom_operation::fee_parameters_type, (fee)(price_per_kbyte) )
FC_REFLECT( graphene::protocol::custom_operation, (fee)(payer)(required_auths)(id)(data) )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::custom_operation::fee_parameters_type )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::custom_operation )
