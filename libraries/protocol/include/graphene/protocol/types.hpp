#pragma once

#include <memory>
#include <vector>
#include <deque>
#include <cstdint>

#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/transform.hpp>
#include <boost/preprocessor/seq/elem.hpp>
#include <boost/preprocessor/seq/enum.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/cat.hpp>

#include <boost/rational.hpp>

#include <fc/container/flat_fwd.hpp>
#include <fc/io/varint.hpp>
#include <fc/io/enum_type.hpp>
#include <fc/crypto/ripemd160.hpp>
#include <fc/crypto/sha1.hpp>
#include <fc/crypto/sha224.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/crypto/hash160.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/optional.hpp>
#include <fc/safe.hpp>
#include <fc/container/flat.hpp>
#include <fc/string.hpp>

#include <fc/io/datastream.hpp>
#include <fc/io/raw_fwd.hpp>
#include <fc/static_variant.hpp>

#include <graphene/protocol/object_id.hpp>
#include <graphene/protocol/config.hpp>

#define GRAPHENE_EXTERNAL_SERIALIZATION_VARIANT(ext, type)                             \
namespace fc {                                                                         \
   ext template void from_variant(const variant &v, type &vo, uint32_t max_depth);     \
   ext template void to_variant(const type &v, variant &vo, uint32_t max_depth);       \
}

#define GRAPHENE_EXTERNAL_SERIALIZATION_PACK(ext, type)                                                                        \
namespace fc { namespace raw {                                                                                                 \
   ext template void pack<datastream<size_t>, type>( datastream<size_t> & s, const type &tx, uint32_t _max_depth );            \
   ext template void pack<sha256::encoder, type>( sha256::encoder & s, const type &tx, uint32_t _max_depth );                  \
   ext template void pack<datastream<char *>, type>( datastream<char *> & s, const type &tx, uint32_t _max_depth );            \
   ext template void unpack<datastream<const char *>, type>( datastream<const char *> & s, type &tx, uint32_t _max_depth );    \
} }

#define GRAPHENE_EXTERNAL_SERIALIZATION(ext, type)         \
   GRAPHENE_EXTERNAL_SERIALIZATION_VARIANT(ext, type)      \
   GRAPHENE_EXTERNAL_SERIALIZATION_PACK(ext, type)

#define GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION(type) GRAPHENE_EXTERNAL_SERIALIZATION(extern, type)
#define GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION(type) GRAPHENE_EXTERNAL_SERIALIZATION(/*not extern*/, type)

#define GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION_VARIANT(type)     \
   GRAPHENE_EXTERNAL_SERIALIZATION_VARIANT(/*not extern*/, type)
#define GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION_PACK(type)        \
   GRAPHENE_EXTERNAL_SERIALIZATION_PACK(/*not extern*/, type)

#define GRAPHENE_NAME_TO_OBJECT_TYPE(x, prefix, name) BOOST_PP_CAT(prefix, BOOST_PP_CAT(name, _object_type))
#define GRAPHENE_NAME_TO_ID_TYPE(x, y, name) BOOST_PP_CAT(name, _id_type)
#define GRAPHENE_DECLARE_ID(x, space_prefix_seq, name)                                                                                  \
   using BOOST_PP_CAT(name, _id_type) = object_id<BOOST_PP_TUPLE_ELEM(2, 0, space_prefix_seq),                                          \
                                                  GRAPHENE_NAME_TO_OBJECT_TYPE(x, BOOST_PP_TUPLE_ELEM(2, 1, space_prefix_seq), name)>;
#define GRAPHENE_REFLECT_ID(x, id_namespace, name) FC_REFLECT_TYPENAME(graphene::id_namespace::name)

#define GRAPHENE_DEFINE_IDS(id_namespace, object_space, object_type_prefix, names_seq)                                      \
namespace graphene { namespace id_namespace {                                                                               \
   enum BOOST_PP_CAT(object_type_prefix, object_type) {                                                                     \
      BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_TRANSFORM(GRAPHENE_NAME_TO_OBJECT_TYPE, object_type_prefix, names_seq))                \
   };                                                                                                                       \
   BOOST_PP_SEQ_FOR_EACH(GRAPHENE_DECLARE_ID, (object_space, object_type_prefix), names_seq)                                \
} }                                                                                                                         \
FC_REFLECT_ENUM(graphene::id_namespace::BOOST_PP_CAT(object_type_prefix, object_type),                                      \
                BOOST_PP_SEQ_TRANSFORM(GRAPHENE_NAME_TO_OBJECT_TYPE, object_type_prefix, names_seq))                        \
BOOST_PP_SEQ_FOR_EACH(GRAPHENE_REFLECT_ID, id_namespace, BOOST_PP_SEQ_TRANSFORM(GRAPHENE_NAME_TO_ID_TYPE, , names_seq))

namespace graphene { namespace protocol {

   using namespace graphene::db;

   using std::map;
   using std::vector;
   using std::unordered_map;
   using std::string;
   using std::deque;
   using std::shared_ptr;
   using std::weak_ptr;
   using std::unique_ptr;
   using std::set;
   using std::pair;
   using std::enable_shared_from_this;
   using std::tie;
   using std::make_pair;

   using fc::variant_object;
   using fc::variant;
   using fc::enum_type;
   using fc::optional;
   using fc::unsigned_int;
   using fc::time_point_sec;
   using fc::time_point;
   using fc::safe;
   using fc::flat_map;
   using fc::flat_set;
   using fc::static_variant;
   using fc::ecc::range_proof_type;
   using fc::ecc::range_proof_info;
   using fc::ecc::commitment_type;
   struct void_t{};

   using private_key_type = fc::ecc::private_key;
   using chain_id_type = fc::sha256;
   using ratio_type = boost::rational<int32_t>;

   /**
    * @note
    * If one of the following bits is set in asset issuer permissions,
    * it means the asset issuer (or owner for bitassets) has the permission to update
    * the corresponding flag, parameters or perform certain actions.
    * - @ref charge_market_fee
    * - @ref white_list
    * - @ref override_authority
    * - @ref transfer_restricted
    * - @ref disable_force_settle
    * - @ref global_settle
    * - @ref disable_confidential
    * - @ref validator_fed_asset
    * - @ref delegate_fed_asset
    */
   enum asset_issuer_permission_flags {
      // If one of the following bits is set in asset issuer permissions,
      // it means the asset issuer (or owner for bitassets) has the permission to update
      // the corresponding flag, parameters or perform certain actions.
      // Note: This comment is copied and reformatted above for better Doxygen documentation formatting.
      charge_market_fee    = 0x01, /**< an issuer-specified percentage of all market trades in this asset is paid to the issuer */
      white_list           = 0x02, /**< accounts must be whitelisted in order to hold this asset */
      override_authority   = 0x04, /**< issuer may transfer asset back to himself */
      transfer_restricted  = 0x08, /**< require the issuer to be one party to every transfer */
      disable_force_settle = 0x10, /**< disable force settling */
      global_settle        = 0x20, /**< allow the bitasset issuer to force a global settling -- this may be set in permissions, but not flags */
      disable_confidential = 0x40, /**< allow the asset to be used with confidential transactions */
      validator_fed_asset  = 0x80, /**< allow the asset to be fed by validators */
      delegate_fed_asset   = 0x100 /**< allow the asset to be fed by the delegates */
   };

   /// The bits that can be used in asset issuer permissions for non-User Assets
   const static uint16_t ASSET_ISSUER_PERMISSION_MASK =
      charge_market_fee
      | white_list
      | override_authority
      | transfer_restricted
      | disable_force_settle
      | global_settle
      | disable_confidential
      | validator_fed_asset
      | delegate_fed_asset;

   /// The bits that can be used in asset issuer permissions for User Assets
   const static uint16_t USER_ASSET_ISSUER_PERMISSION_MASK =
      charge_market_fee
      | white_list
      | override_authority
      | transfer_restricted
      | disable_confidential;

   enum reserved_spaces {
      relative_protocol_ids = 0,
      protocol_ids          = 1,
      implementation_ids    = 2
   };

   inline bool is_relative(object_id_type o) { return o.space() == 0; }

   using block_id_type = fc::ripemd160;
   using checksum_type = fc::ripemd160;
   using transaction_id_type = fc::ripemd160;
   using digest_type = fc::sha256;
   using signature_type = fc::ecc::compact_signature;
   using share_type = safe<int64_t>;
   using weight_type = uint16_t;

   struct public_key_type {
      struct binary_key {
         binary_key() = default;
         uint32_t check = 0;
         fc::ecc::public_key_data data;
      };
      fc::ecc::public_key_data key_data;
      public_key_type();
      public_key_type(const fc::ecc::public_key_data& data);
      public_key_type(const fc::ecc::public_key& pubkey);
      explicit public_key_type(const std::string& base58str);
      operator fc::ecc::public_key_data() const;
      operator fc::ecc::public_key() const;
      explicit operator std::string() const;
      friend bool operator == (const public_key_type& p1, const fc::ecc::public_key& p2);
      friend bool operator == (const public_key_type& p1, const public_key_type& p2);
      friend bool operator != (const public_key_type& p1, const public_key_type& p2);
      friend bool operator < (const public_key_type& p1, const public_key_type& p2);
   };

   class pubkey_comparator {
   public:
      inline bool operator()(const public_key_type& a, const public_key_type& b) const {
         return a.key_data < b.key_data;
      }
   };

   struct fee_schedule;

} }  // graphene::protocol

namespace fc {

   void to_variant(const graphene::protocol::public_key_type& var,  fc::variant& vo, uint32_t max_depth = 2);
   void from_variant(const fc::variant& var,  graphene::protocol::public_key_type& vo, uint32_t max_depth = 2);


   template<>
   struct get_typename<std::shared_ptr<const graphene::protocol::fee_schedule>> { static const char* name() {
      return "shared_ptr<const fee_schedule>";
   } };
   template<>
   struct get_typename<std::shared_ptr<graphene::protocol::fee_schedule>> { static const char* name() {
      return "shared_ptr<fee_schedule>";
   } };
   void from_variant( const fc::variant& var, std::shared_ptr<const graphene::protocol::fee_schedule>& vo,
                     uint32_t max_depth = 2 );

} // fc

/// Object types in the Protocol Space (enum object_type (1.x.x))
GRAPHENE_DEFINE_IDS(protocol, protocol_ids, /*protocol objects are not prefixed*/,
                    /* 1.0.x  */ (null) // no data
                    /* 1.1.x  */ (base) // no data
                    /* 1.2.x  */ (account)
                    /* 1.3.x  */ (asset)
                    /* 1.4.x  */ (force_settlement)
                    /* 1.5.x  */ (delegate)
                    /* 1.6.x  */ (validator)
                    /* 1.7.x  */ (limit_order)
                    /* 1.8.x  */ (call_order)
                    /* 1.9.x  */ (custom) // unused
                    /* 1.10.x */ (proposal)
                    /* 1.11.x */ (operation_history) // strictly speaking it is not in protocol
                    /* 1.12.x */ (withdraw_permission)
                    /* 1.13.x */ (vesting_balance)
                    /* 1.14.x */ (worker)
                    /* 1.15.x */ (balance)
                    /* 1.16.x */ (htlc)
)

FC_REFLECT(graphene::protocol::public_key_type, (key_data))
FC_REFLECT(graphene::protocol::public_key_type::binary_key, (data)(check))

FC_REFLECT_TYPENAME(graphene::protocol::share_type)
FC_REFLECT(graphene::protocol::void_t,)

FC_REFLECT_ENUM(graphene::protocol::asset_issuer_permission_flags,
                (charge_market_fee)
                (white_list)
                (transfer_restricted)
                (override_authority)
                (disable_force_settle)
                (global_settle)
                (disable_confidential)
                (validator_fed_asset)
                (delegate_fed_asset)
)

namespace fc { namespace raw {
   extern template void pack( datastream<size_t>& s, const graphene::protocol::public_key_type& tx,
                              uint32_t _max_depth );
   extern template void pack( datastream<char*>& s, const graphene::protocol::public_key_type& tx,
                              uint32_t _max_depth );
   extern template void unpack( datastream<const char*>& s, graphene::protocol::public_key_type& tx,
                                uint32_t _max_depth );
} } // fc::raw
