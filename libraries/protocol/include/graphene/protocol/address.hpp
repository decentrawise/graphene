#pragma once

#include <graphene/protocol/types.hpp>

#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/ripemd160.hpp>

namespace graphene { namespace protocol {
   struct btc_address;

   /**
    *  @brief a 160 bit hash of a public key
    *
    *  An address can be converted to or from a base58 string with 32 bit checksum.
    *
    *  An address is calculated as ripemd160( sha512( compressed_ecc_public_key ) )
    *
    *  When converted to a string, checksum calculated as the first 4 bytes ripemd160( address ) is
    *  appended to the binary address before converting to base58.
    */
   class address
   {
      public:
       address(){} ///< constructs empty / null address
       explicit address( const std::string& base58str );          ///< converts to binary, validates checksum
       explicit address(const fc::ecc::public_key &pub);          ///< converts to binary
       explicit address( const fc::ecc::public_key_data& pub );   ///< converts to binary
       explicit address(const btc_address &pub);                  ///< converts to binary
       explicit address(const public_key_type &pubkey);

       static bool is_valid( const std::string& base58str, const std::string& prefix = GRAPHENE_ADDRESS_PREFIX );

       explicit operator std::string()const; ///< converts to base58 + checksum

       fc::ripemd160 addr;
   };
   inline bool operator == ( const address& a, const address& b ) { return a.addr == b.addr; }
   inline bool operator != ( const address& a, const address& b ) { return a.addr != b.addr; }
   inline bool operator <  ( const address& a, const address& b ) { return a.addr <  b.addr; }

   inline bool operator == ( const btc_address& a, const address& b ) { return address(a) == b; }
   inline bool operator == ( const address& a, const btc_address& b ) { return a == address(b); }

   inline bool operator == ( const public_key_type& a, const address& b ) { return address(a) == b; }
   inline bool operator == ( const address& a, const public_key_type& b ) { return a == address(b); }

} } // namespace graphene::protocol

namespace fc
{
   void to_variant( const graphene::protocol::address& var,  fc::variant& vo, uint32_t max_depth = 1 );
   void from_variant( const fc::variant& var,  graphene::protocol::address& vo, uint32_t max_depth = 1 );
}

FC_REFLECT( graphene::protocol::address, (addr) )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::address )
