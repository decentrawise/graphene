#include <fc/io/raw.hpp>

#include <graphene/net/message.hpp>

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::net::message_header, BOOST_PP_SEQ_NIL, (size)(msg_type) )
FC_REFLECT_DERIVED_NO_TYPENAME( graphene::net::message, (graphene::net::message_header), (data) )

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::net::message_header)
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::net::message)
