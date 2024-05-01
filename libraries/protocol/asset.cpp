#include <graphene/protocol/asset.hpp>
#include <boost/rational.hpp>
#include <boost/multiprecision/cpp_int.hpp>

#include <fc/io/raw.hpp>
#include <fc/uint128.hpp>

namespace graphene { namespace protocol {
   using fc::uint128_t;

   bool operator == ( const price& a, const price& b )
   {
      if( std::tie( a.base.asset_id, a.quote.asset_id ) != std::tie( b.base.asset_id, b.quote.asset_id ) )
         return false;

      const auto amult = uint128_t( b.quote.amount.value ) * a.base.amount.value;
      const auto bmult = uint128_t( a.quote.amount.value ) * b.base.amount.value;

      return amult == bmult;
   }

   bool operator < ( const price& a, const price& b )
   {
      if( a.base.asset_id < b.base.asset_id ) return true;
      if( a.base.asset_id > b.base.asset_id ) return false;
      if( a.quote.asset_id < b.quote.asset_id ) return true;
      if( a.quote.asset_id > b.quote.asset_id ) return false;

      const auto amult = uint128_t( b.quote.amount.value ) * a.base.amount.value;
      const auto bmult = uint128_t( a.quote.amount.value ) * b.base.amount.value;

      return amult < bmult;
   }

   asset operator * ( const asset& a, const price& b )
   {
      if( a.asset_id == b.base.asset_id )
      {
         FC_ASSERT( b.base.amount.value > 0 );
         uint128_t result = (uint128_t(a.amount.value) * b.quote.amount.value)/b.base.amount.value;
         FC_ASSERT( result <= GRAPHENE_CORE_ASSET_MAX_SUPPLY );
         return asset( static_cast<int64_t>(result), b.quote.asset_id );
      }
      else if( a.asset_id == b.quote.asset_id )
      {
         FC_ASSERT( b.quote.amount.value > 0 );
         uint128_t result = (uint128_t(a.amount.value) * b.base.amount.value)/b.quote.amount.value;
         FC_ASSERT( result <= GRAPHENE_CORE_ASSET_MAX_SUPPLY );
         return asset( static_cast<int64_t>(result), b.base.asset_id );
      }
      FC_THROW_EXCEPTION( fc::assert_exception, "invalid asset * price", ("asset",a)("price",b) );
   }

   asset asset::multiply_and_round_up( const price& b )const
   {
      const asset& a = *this;
      if( a.asset_id == b.base.asset_id )
      {
         FC_ASSERT( b.base.amount.value > 0 );
         uint128_t result = (uint128_t(a.amount.value) * b.quote.amount.value + b.base.amount.value - 1)/b.base.amount.value;
         FC_ASSERT( result <= GRAPHENE_CORE_ASSET_MAX_SUPPLY );
         return asset( static_cast<int64_t>(result), b.quote.asset_id );
      }
      else if( a.asset_id == b.quote.asset_id )
      {
         FC_ASSERT( b.quote.amount.value > 0 );
         uint128_t result = (uint128_t(a.amount.value) * b.base.amount.value + b.quote.amount.value - 1)/b.quote.amount.value;
         FC_ASSERT( result <= GRAPHENE_CORE_ASSET_MAX_SUPPLY );
         return asset( static_cast<int64_t>(result), b.base.asset_id );
      }
      FC_THROW_EXCEPTION( fc::assert_exception, "invalid asset::multiply_and_round_up(price)", ("asset",a)("price",b) );
   }

   price operator / ( const asset& base, const asset& quote )
   { try {
      FC_ASSERT( base.asset_id != quote.asset_id );
      return price{base,quote};
   } FC_CAPTURE_AND_RETHROW( (base)(quote) ) }

   price price::max( asset_id_type base, asset_id_type quote ) { return asset( amount_type(GRAPHENE_CORE_ASSET_MAX_SUPPLY), base ) / asset( amount_type(1), quote); }
   price price::min( asset_id_type base, asset_id_type quote ) { return asset( 1, base ) / asset( GRAPHENE_CORE_ASSET_MAX_SUPPLY, quote); }

   price operator *  ( const price& p, const ratio_type& r )
   { try {
      p.validate();

      FC_ASSERT( r.numerator() > 0 && r.denominator() > 0 );

      if( r.numerator() == r.denominator() ) return p;

      boost::rational<uint128_t> p128( p.base.amount.value, p.quote.amount.value );
      boost::rational<uint128_t> r128( r.numerator(), r.denominator() );
      auto cp = p128 * r128;
      auto ocp = cp;

      bool shrinked = false;
      bool using_max = false;
      static const uint128_t max( GRAPHENE_CORE_ASSET_MAX_SUPPLY );
      while( cp.numerator() > max || cp.denominator() > max )
      {
         if( cp.numerator() == 1 )
         {
            cp = boost::rational<uint128_t>( 1, max );
            using_max = true;
            break;
         }
         else if( cp.denominator() == 1 )
         {
            cp = boost::rational<uint128_t>( max, 1 );
            using_max = true;
            break;
         }
         else
         {
            cp = boost::rational<uint128_t>( cp.numerator() >> 1, cp.denominator() >> 1 );
            shrinked = true;
         }
      }
      if( shrinked ) // maybe not accurate enough due to rounding, do additional checks here
      {
         uint128_t num = ocp.numerator();
         uint128_t den = ocp.denominator();
         if( num > den )
         {
            num /= den;
            num = std::min( num , max );
            den = 1;
         }
         else
         {
            den /= num;
            den = std::min( den, max );
            num = 1;
         }
         boost::rational<uint128_t> ncp( num, den );
         if( num == max || den == max ) // it's on the edge, we know it's accurate enough
            cp = ncp;
         else
         {
            // from the accurate ocp, now we have ncp and cp. use the one which is closer to ocp.
            // TODO improve performance
            auto diff1 = (ncp >= ocp) ? (ncp - ocp) : (ocp - ncp);
            auto diff2 = (cp >= ocp) ? (cp - ocp) : (ocp - cp);
            if( diff1 < diff2 ) cp = ncp;
         }
      }

      price np = asset( static_cast<int64_t>(cp.numerator()), p.base.asset_id )
               / asset( static_cast<int64_t>(cp.denominator()), p.quote.asset_id );

      if( shrinked || using_max )
      {
         bool flipped = ( r.numerator() > r.denominator() ) ? ( np < p ) : ( np > p );
         if( flipped )     // even with an accurate result, if p is out of valid range, return it
            np = p;
      }

      np.validate();
      return np;
   } FC_CAPTURE_AND_RETHROW( (p)(r.numerator())(r.denominator()) ) }

   price operator /  ( const price& p, const ratio_type& r )
   { try {
      return p * ratio_type( r.denominator(), r.numerator() );
   } FC_CAPTURE_AND_RETHROW( (p)(r.numerator())(r.denominator()) ) }

   /**
    *  The black swan price is defined as debt/collateral, we want to perform a margin call
    *  before debt == collateral.   Given a debt/collateral ratio of 1 USD / CORE and
    *  a maintenance collateral requirement of 2x we can define the call price to be
    *  2 USD / CORE.
    *
    *  This method divides the collateral by the maintenance collateral ratio to derive
    *  a call price for the given black swan ratio.
    *
    *  There exists some cases where the debt and collateral values are so small that
    *  dividing by the collateral ratio will result in a 0 price or really poor
    *  rounding errors.   No matter what the collateral part of the price ratio can
    *  never go to 0 and the debt can never go more than GRAPHENE_CORE_ASSET_MAX_SUPPLY
    *
    *  CR * DEBT/COLLAT or DEBT/(COLLAT/CR)
    *
    *  Note: this function is candidate to be (re)moved, as call price is not cached anymore.
    */
   price price::call_price( const asset& debt, const asset& collateral, uint16_t collateral_ratio)
   { try {
      boost::rational<uint128_t> swan(debt.amount.value,collateral.amount.value);
      boost::rational<uint128_t> ratio( collateral_ratio, GRAPHENE_COLLATERAL_RATIO_DENOM );
      auto cp = swan * ratio;

      while( cp.numerator() > GRAPHENE_CORE_ASSET_MAX_SUPPLY || cp.denominator() > GRAPHENE_CORE_ASSET_MAX_SUPPLY )
         cp = boost::rational<uint128_t>( (cp.numerator() >> 1)+1, (cp.denominator() >> 1)+1 );

      return  (  asset( static_cast<int64_t>(cp.denominator()), collateral.asset_id )
               / asset( static_cast<int64_t>(cp.numerator()), debt.asset_id ) );
   } FC_CAPTURE_AND_RETHROW( (debt)(collateral)(collateral_ratio) ) }

   bool price::is_null() const
   {
      // Effectively same as "return *this == price();" but perhaps faster
      return ( base.asset_id == asset_id_type() && quote.asset_id == asset_id_type() );
   }

   void price::validate( bool check_upper_bound /* = false */ )const
   { try {
      FC_ASSERT( base.amount.value > 0, "Base amount should be positive" );
      FC_ASSERT( quote.amount.value > 0, "Quote amount should be positive" );
      FC_ASSERT( base.asset_id != quote.asset_id, "Base asset ID and quote asset ID should be different" );
      if( check_upper_bound )
      {
         FC_ASSERT( base.amount.value <= GRAPHENE_CORE_ASSET_MAX_SUPPLY,
                     "Base amount should not be greater than ${max}",
                     ("max", GRAPHENE_CORE_ASSET_MAX_SUPPLY) );
         FC_ASSERT( quote.amount.value <= GRAPHENE_CORE_ASSET_MAX_SUPPLY,
                     "Quote amount should not be greater than ${max}",
                     ("max", GRAPHENE_CORE_ASSET_MAX_SUPPLY) );
      }
   } FC_CAPTURE_AND_RETHROW( (base)(quote) ) }

   void price_feed::validate() const
   { try {
      if( !settlement_price.is_null() )
         settlement_price.validate();
      FC_ASSERT( maximum_short_squeeze_ratio >= GRAPHENE_MIN_COLLATERAL_RATIO );
      FC_ASSERT( maximum_short_squeeze_ratio <= GRAPHENE_MAX_COLLATERAL_RATIO );
      FC_ASSERT( maintenance_collateral_ratio >= GRAPHENE_MIN_COLLATERAL_RATIO );
      FC_ASSERT( maintenance_collateral_ratio <= GRAPHENE_MAX_COLLATERAL_RATIO );

      // Note: not checking `maintenance_collateral_ratio >= maximum_short_squeeze_ratio` since launch
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

   bool price_feed::is_for( asset_id_type asset_id ) const
   {
      try
      {
         if( !settlement_price.is_null() )
            return (settlement_price.base.asset_id == asset_id);
         if( !core_exchange_rate.is_null() )
            return (core_exchange_rate.base.asset_id == asset_id);
         // (null, null) is valid for any feed
         return true;
      }
      FC_CAPTURE_AND_RETHROW( (*this) )
   }

   price price_feed::max_short_squeeze_price()const
   {
      // settlement price is in debt/collateral
      return settlement_price * ratio_type( GRAPHENE_COLLATERAL_RATIO_DENOM, maximum_short_squeeze_ratio );
   }

   price price_feed::maintenance_collateralization()const
   {
      if( settlement_price.is_null() )
         return price();
      return ~settlement_price * ratio_type( maintenance_collateral_ratio, GRAPHENE_COLLATERAL_RATIO_DENOM );
   }

   // compile-time table of powers of 10 using template metaprogramming

   template< size_t N >
   struct p10
   {
      static constexpr int64_t v = 10 * p10<N-1>::v;
   };

   template<>
   struct p10<0>
   {
      static constexpr int64_t v = 1;
   };

   amount_type asset::scaled_precision( uint8_t precision )
   {
      FC_ASSERT( precision < 19 );
      static constexpr std::array<int64_t, 19> scaled_precision_lut =
      {
         p10<  0 >::v, p10<  1 >::v, p10<  2 >::v, p10<  3 >::v,
         p10<  4 >::v, p10<  5 >::v, p10<  6 >::v, p10<  7 >::v,
         p10<  8 >::v, p10<  9 >::v, p10< 10 >::v, p10< 11 >::v,
         p10< 12 >::v, p10< 13 >::v, p10< 14 >::v, p10< 15 >::v,
         p10< 16 >::v, p10< 17 >::v, p10< 18 >::v
      };

      return scaled_precision_lut[ precision ];
   }

} } // graphene::protocol

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::asset )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::price )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::price_feed )
