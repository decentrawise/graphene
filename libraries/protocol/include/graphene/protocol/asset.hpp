#pragma once

#include <graphene/protocol/types.hpp>

namespace graphene { namespace protocol {

   struct price;

   struct asset
   {
      asset( share_type a = 0, asset_id_type id = asset_id_type() )
      :amount(a),asset_id(id){}

      share_type    amount;
      asset_id_type asset_id;

      asset& operator += ( const asset& o )
      {
         FC_ASSERT( asset_id == o.asset_id );
         amount += o.amount;
         return *this;
      }
      asset& operator -= ( const asset& o )
      {
         FC_ASSERT( asset_id == o.asset_id );
         amount -= o.amount;
         return *this;
      }
      asset operator -()const { return asset( -amount, asset_id ); }

      friend bool operator == ( const asset& a, const asset& b )
      {
         return std::tie( a.asset_id, a.amount ) == std::tie( b.asset_id, b.amount );
      }
      friend bool operator < ( const asset& a, const asset& b )
      {
         FC_ASSERT( a.asset_id == b.asset_id );
         return a.amount < b.amount;
      }
      friend inline bool operator <= ( const asset& a, const asset& b )
      {
         return !(b < a);
      }

      friend inline bool operator != ( const asset& a, const asset& b )
      {
         return !(a == b);
      }
      friend inline bool operator > ( const asset& a, const asset& b )
      {
         return (b < a);
      }
      friend inline bool operator >= ( const asset& a, const asset& b )
      {
         return !(a < b);
      }

      friend asset operator - ( const asset& a, const asset& b )
      {
         FC_ASSERT( a.asset_id == b.asset_id );
         return asset( a.amount - b.amount, a.asset_id );
      }
      friend asset operator + ( const asset& a, const asset& b )
      {
         FC_ASSERT( a.asset_id == b.asset_id );
         return asset( a.amount + b.amount, a.asset_id );
      }

      static share_type scaled_precision( uint8_t precision );

      asset multiply_and_round_up( const price& p )const; ///< Multiply and round up
   };

   /**
    * @brief The price struct stores asset prices in the Graphene system.
    *
    * A price is defined as a ratio between two assets, and represents a possible exchange rate between those two
    * assets. prices are generally not stored in any simplified form, i.e. a price of (1000 CORE)/(20 USD) is perfectly
    * normal.
    *
    * The assets within a price are labeled base and quote. Throughout the Graphene code base, the convention used is
    * that the base asset is the asset being sold, and the quote asset is the asset being purchased, where the price is
    * represented as base/quote, so in the example price above the seller is looking to sell CORE asset and get USD in
    * return.
    */
   struct price
   {
      explicit price(const asset& _base = asset(), const asset& _quote = asset())
         : base(_base),quote(_quote){}

      asset base;
      asset quote;

      static price max(asset_id_type base, asset_id_type quote );
      static price min(asset_id_type base, asset_id_type quote );

      static price call_price(const asset& debt, const asset& collateral, uint16_t collateral_ratio);

      /// The unit price for an asset type A is defined to be a price such that for any asset m, m*A=m
      static price unit_price(asset_id_type a = asset_id_type()) { return price(asset(1, a), asset(1, a)); }

      price max()const { return price::max( base.asset_id, quote.asset_id ); }
      price min()const { return price::min( base.asset_id, quote.asset_id ); }

      double to_real()const { return double(base.amount.value)/double(quote.amount.value); }

      bool is_null()const;

      /// @brief Check if the object is valid
      /// @param check_upper_bound Whether to check if the amounts in the price are too large
      void validate( bool check_upper_bound = false )const;
   };

   price operator / ( const asset& base, const asset& quote );
   inline price operator~( const price& p ) { return price{p.quote,p.base}; }

   bool  operator <  ( const price& a, const price& b );
   bool  operator == ( const price& a, const price& b );

   inline bool  operator >  ( const price& a, const price& b ) { return (b < a); }
   inline bool  operator <= ( const price& a, const price& b ) { return !(b < a); }
   inline bool  operator >= ( const price& a, const price& b ) { return !(a < b); }
   inline bool  operator != ( const price& a, const price& b ) { return !(a == b); }

   asset operator *  ( const asset& a, const price& b ); ///< Multiply and round down

   price operator *  ( const price& p, const ratio_type& r );
   price operator /  ( const price& p, const ratio_type& r );

   inline price& operator *=  ( price& p, const ratio_type& r )
   { return p = p * r; }
   inline price& operator /=  ( price& p, const ratio_type& r )
   { return p = p / r; }

   /**
    *  @class price_feed
    *  @brief defines market parameters for margin positions
    */
   struct price_feed
   {
      /**
       *  Required maintenance collateral is defined
       *  as a fixed point number with a maximum value of 10.000
       *  and a minimum value of 1.000.  (denominated in GRAPHENE_COLLATERAL_RATIO_DENOM)
       *
       *  A black swan event occurs when value_of_collateral equals
       *  value_of_debt, to avoid a black swan a margin call is
       *  executed when value_of_debt * required_maintenance_collateral
       *  equals value_of_collateral using rate.
       *
       *  Default requirement is $1.75 of collateral per $1 of debt
       *
       *  BlackSwan ---> SQR ---> MCR ----> SP
       */
      ///@{
      /**
       * Forced settlements will evaluate using this price, defined as ASSET / COLLATERAL
       */
      price settlement_price;

      /// Price at which automatically exchanging this asset for CORE from fee pool occurs (used for paying fees)
      price core_exchange_rate;

      /** Fixed point between 1.000 and 10.000, implied fixed point denominator is GRAPHENE_COLLATERAL_RATIO_DENOM */
      uint16_t maintenance_collateral_ratio = GRAPHENE_DEFAULT_MAINTENANCE_COLLATERAL_RATIO;

      /** Fixed point between 1.000 and 10.000, implied fixed point denominator is GRAPHENE_COLLATERAL_RATIO_DENOM */
      uint16_t maximum_short_squeeze_ratio = GRAPHENE_DEFAULT_MAX_SHORT_SQUEEZE_RATIO;

      /** When selling collateral to pay off debt, the least amount of debt to receive should be
       *  min_usd = max_short_squeeze_price() * collateral
       *
       *  This is provided to ensure that a black swan cannot be trigged due to poor liquidity alone, it
       *  must be confirmed by having the max_short_squeeze_price() move below the black swan price.
       */
      price max_short_squeeze_price()const;

      /// Call orders with collateralization (aka collateral/debt) not greater than this value are in margin call territory.
      /// Calculation: ~settlement_price * maintenance_collateral_ratio / GRAPHENE_COLLATERAL_RATIO_DENOM
      price maintenance_collateralization()const;

      ///@}

      friend bool operator == ( const price_feed& a, const price_feed& b )
      {
         if (&a == &b)
            return true;
         return std::tie(a.settlement_price, a.maintenance_collateral_ratio, a.maximum_short_squeeze_ratio) ==
                std::tie(b.settlement_price, b.maintenance_collateral_ratio, b.maximum_short_squeeze_ratio);
      }

      void validate() const;
      bool is_for( asset_id_type asset_id ) const;
   };

} }

FC_REFLECT( graphene::protocol::asset, (amount)(asset_id) )
FC_REFLECT( graphene::protocol::price, (base)(quote) )

// used in asset_object to calculate the median feed
#define GRAPHENE_PRICE_FEED_FIELDS (settlement_price)(maintenance_collateral_ratio)(maximum_short_squeeze_ratio)(core_exchange_rate)

FC_REFLECT(graphene::protocol::price_feed, GRAPHENE_PRICE_FEED_FIELDS)

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::asset )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::price )
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::price_feed )
