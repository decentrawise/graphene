#pragma once

#include <array>
#include <cstring>
#include <string>

#include <fc/io/datastream.hpp>
#include <fc/io/raw_fwd.hpp>
#include <fc/variant.hpp>

namespace fc { namespace ecc { class public_key; } }

namespace graphene { namespace protocol {

   /**
    *  Implements BTC address stringification and validation
    */
   struct btc_address
   {
       btc_address(); ///< constructs empty / null address
       btc_address( const std::string& base58str );   ///< converts to binary, validates checksum
       btc_address( const fc::ecc::public_key& pub, bool compressed = true, uint8_t version=56 ); ///< converts to binary

       uint8_t version()const { return addr.at(0); }
       bool is_valid()const;

       operator std::string()const; ///< converts to base58 + checksum

       std::array<char,25> addr{}; ///< binary representation of address, 0-initialized
   };

   inline bool operator == ( const btc_address& a, const btc_address& b ) { return a.addr == b.addr; }
   inline bool operator != ( const btc_address& a, const btc_address& b ) { return a.addr != b.addr; }
   inline bool operator <  ( const btc_address& a, const btc_address& b ) { return a.addr <  b.addr; }

} } // namespace graphene::protocol

namespace std
{
   template<>
   struct hash<graphene::protocol::btc_address> 
   {
       public:
         size_t operator()(const graphene::protocol::btc_address &a) const 
         {
            size_t s;
            std::memcpy( (char*)&s, a.addr.data() + a.addr.size() - sizeof(s), sizeof(s) );
            return s;
         }
   };
}

#include <fc/reflect/reflect.hpp>
FC_REFLECT( graphene::protocol::btc_address, (addr) )

namespace fc 
{ 
   void to_variant( const graphene::protocol::btc_address& var,  fc::variant& vo, uint32_t max_depth = 1 );
   void from_variant( const fc::variant& var,  graphene::protocol::btc_address& vo, uint32_t max_depth = 1 );

namespace raw {
   extern template void pack( datastream<size_t>& s, const graphene::protocol::btc_address& tx,
                              uint32_t _max_depth );
   extern template void pack( datastream<char*>& s, const graphene::protocol::btc_address& tx,
                              uint32_t _max_depth );
   extern template void unpack( datastream<const char*>& s, graphene::protocol::btc_address& tx,
                                uint32_t _max_depth );
} } // fc::raw
