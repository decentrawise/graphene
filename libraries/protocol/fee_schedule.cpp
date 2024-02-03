#include <algorithm>

#include <graphene/protocol/fee_schedule.hpp>

#include <fc/io/raw.hpp>
#include <fc/uint128.hpp>

#define MAX_FEE_STABILIZATION_ITERATION 4

namespace graphene { namespace protocol {

   fee_schedule fee_schedule::get_default_impl()
   {
      fee_schedule result;
      const auto count = fee_parameters::count();
      result.parameters.reserve(count);
      for( size_t i = 0; i < count; ++i )
      {
         fee_parameters x;
         x.set_which(i);
         result.parameters.insert(x);
      }
      return result;
   }

   const fee_schedule& fee_schedule::get_default()
   {
      static const auto result = get_default_impl();
      return result;
   }

   struct fee_schedule_validate_visitor
   {
      using result_type = void;

      template<typename T>
      void operator()( const T& p )const
      {
         //p.validate();
      }
   };

   void fee_schedule::validate()const
   {
      for( const auto& f : parameters )
         f.visit( fee_schedule_validate_visitor() );
   }

   struct calc_fee_visitor
   {
      using result_type = uint64_t;

      const fee_schedule& param;
      const operation::tag_type current_op;
      calc_fee_visitor( const fee_schedule& p, const operation& op ):param(p),current_op(op.which()){}

      template<typename OpType>
      result_type operator()( const OpType& op )const
      {
         try {
            return op.calculate_fee( param.get<OpType>() ).value;
         } catch (fc::assert_exception& e) {
             fee_parameters params;
             params.set_which(current_op);
             auto itr = param.parameters.find(params);
             if( itr != param.parameters.end() )
               params = *itr;
             return op.calculate_fee( params.get<typename OpType::fee_parameters_type>() ).value;
         }
      }
   };

   struct set_fee_visitor
   {
      using result_type = void;
      asset _fee;

      set_fee_visitor( asset f ):_fee(f){}

      template<typename OpType>
      void operator()( OpType& op )const
      {
         op.fee = _fee;
      }
   };

   struct zero_fee_visitor
   {
      using result_type = void;

      template<typename ParamType>
      result_type operator()(  ParamType& op )const
      {
         memset( (char*)&op, 0, sizeof(op) );
      }
   };

   void fee_schedule::zero_all_fees()
   {
      *this = get_default();
      for( fee_parameters& i : parameters )
         i.visit( zero_fee_visitor() );
      this->scale = 0;
   }

   asset fee_schedule::calculate_fee( const operation& op )const
   {
      uint64_t required_fee = op.visit( calc_fee_visitor( *this, op ) );
      if( scale != GRAPHENE_100_PERCENT )
      {
         auto scaled = fc::uint128_t(required_fee) * scale;
         scaled /= GRAPHENE_100_PERCENT;
         FC_ASSERT( scaled <= GRAPHENE_MAX_SHARE_SUPPLY,
                    "Required fee after scaling would exceed maximum possible supply" );
         required_fee = static_cast<uint64_t>(scaled);
      }
      return asset( required_fee );
   }

   asset fee_schedule::calculate_fee( const operation& op, const price& core_exchange_rate )const
   {
      return calculate_fee( op ).multiply_and_round_up( core_exchange_rate );
   }

   asset fee_schedule::set_fee( operation& op, const price& core_exchange_rate )const
   {
      auto f = calculate_fee( op, core_exchange_rate );
      for( int i = 0; i < MAX_FEE_STABILIZATION_ITERATION; ++i )
      {
         op.visit( set_fee_visitor( f ) );
         auto f2 = calculate_fee( op, core_exchange_rate );
         if( f >= f2 )
            break;
         f = f2;
         if( i == 0 )
         {
            // no need for warnings on later iterations
            wlog( "set_fee requires multiple iterations to stabilize with core_exchange_rate ${p} on operation ${op}",
               ("p", core_exchange_rate) ("op", op) );
         }
      }
      return f;
   }

} } // graphene::protocol

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::fee_schedule )
