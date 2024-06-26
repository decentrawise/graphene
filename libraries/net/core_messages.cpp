#include <graphene/net/core_messages.hpp>

#include <fc/io/raw.hpp>

namespace graphene { namespace net {

  const core_message_type_enum trx_message::type                             = core_message_type_enum::trx_message_type;
  const core_message_type_enum block_message::type                           = core_message_type_enum::block_message_type;
  const core_message_type_enum item_ids_inventory_message::type              = core_message_type_enum::item_ids_inventory_message_type;
  const core_message_type_enum blockchain_item_ids_inventory_message::type   = core_message_type_enum::blockchain_item_ids_inventory_message_type;
  const core_message_type_enum fetch_blockchain_item_ids_message::type       = core_message_type_enum::fetch_blockchain_item_ids_message_type;
  const core_message_type_enum fetch_items_message::type                     = core_message_type_enum::fetch_items_message_type;
  const core_message_type_enum item_not_available_message::type              = core_message_type_enum::item_not_available_message_type;
  const core_message_type_enum hello_message::type                           = core_message_type_enum::hello_message_type;
  const core_message_type_enum connection_accepted_message::type             = core_message_type_enum::connection_accepted_message_type;
  const core_message_type_enum connection_rejected_message::type             = core_message_type_enum::connection_rejected_message_type;
  const core_message_type_enum address_request_message::type                 = core_message_type_enum::address_request_message_type;
  const core_message_type_enum address_message::type                         = core_message_type_enum::address_message_type;
  const core_message_type_enum closing_connection_message::type              = core_message_type_enum::closing_connection_message_type;
  const core_message_type_enum current_time_request_message::type            = core_message_type_enum::current_time_request_message_type;
  const core_message_type_enum current_time_reply_message::type              = core_message_type_enum::current_time_reply_message_type;
  const core_message_type_enum check_firewall_message::type                  = core_message_type_enum::check_firewall_message_type;
  const core_message_type_enum check_firewall_reply_message::type            = core_message_type_enum::check_firewall_reply_message_type;
  const core_message_type_enum get_current_connections_request_message::type = core_message_type_enum::get_current_connections_request_message_type;
  const core_message_type_enum get_current_connections_reply_message::type   = core_message_type_enum::get_current_connections_reply_message_type;

} } // graphene::net

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::net::trx_message, BOOST_PP_SEQ_NIL, (trx) )
FC_REFLECT_DERIVED_NO_TYPENAME( graphene::net::block_message, BOOST_PP_SEQ_NIL, (block)(block_id) )

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::net::item_id, BOOST_PP_SEQ_NIL,
                               (item_type)
                               (item_hash) )
FC_REFLECT_DERIVED_NO_TYPENAME( graphene::net::item_ids_inventory_message, BOOST_PP_SEQ_NIL,
                                                  (item_type)
                                                  (item_hashes_available) )
FC_REFLECT_DERIVED_NO_TYPENAME( graphene::net::blockchain_item_ids_inventory_message, BOOST_PP_SEQ_NIL,
                                                             (total_remaining_item_count)
                                                             (item_type)
                                                             (item_hashes_available) )
FC_REFLECT_DERIVED_NO_TYPENAME( graphene::net::fetch_blockchain_item_ids_message, BOOST_PP_SEQ_NIL,
                                                         (item_type)
                                                         (blockchain_synopsis) )
FC_REFLECT_DERIVED_NO_TYPENAME( graphene::net::fetch_items_message, BOOST_PP_SEQ_NIL,
                                           (item_type)
                                           (items_to_fetch) )
FC_REFLECT_DERIVED_NO_TYPENAME( graphene::net::item_not_available_message, BOOST_PP_SEQ_NIL, (requested_item) )
FC_REFLECT_DERIVED_NO_TYPENAME( graphene::net::hello_message, BOOST_PP_SEQ_NIL,
                                     (user_agent)
                                     (core_protocol_version)
                                     (inbound_address)
                                     (inbound_port)
                                     (outbound_port)
                                     (node_public_key)
                                     (signed_shared_secret)
                                     (chain_id)
                                     (user_data) )

FC_REFLECT_DERIVED_NO_TYPENAME( graphene::net::connection_accepted_message, BOOST_PP_SEQ_NIL, BOOST_PP_SEQ_NIL )
FC_REFLECT_DERIVED_NO_TYPENAME( graphene::net::connection_rejected_message, BOOST_PP_SEQ_NIL,
                                                   (user_agent)
                                                   (core_protocol_version)
                                                   (remote_endpoint)
                                                   (reason_code)
                                                   (reason_string))
FC_REFLECT_DERIVED_NO_TYPENAME( graphene::net::address_request_message, BOOST_PP_SEQ_NIL, BOOST_PP_SEQ_NIL )
FC_REFLECT_DERIVED_NO_TYPENAME( graphene::net::address_info, BOOST_PP_SEQ_NIL,
                                    (remote_endpoint)
                                    (last_seen_time)
                                    (latency)
                                    (node_id)
                                    (direction)
                                    (firewalled) )
FC_REFLECT_DERIVED_NO_TYPENAME( graphene::net::address_message, BOOST_PP_SEQ_NIL, (addresses) )
FC_REFLECT_DERIVED_NO_TYPENAME( graphene::net::closing_connection_message, BOOST_PP_SEQ_NIL,
                                                  (reason_for_closing)
                                                  (closing_due_to_error)
                                                  (error) )

FC_REFLECT_DERIVED_NO_TYPENAME(graphene::net::current_time_request_message, BOOST_PP_SEQ_NIL, (request_sent_time))
FC_REFLECT_DERIVED_NO_TYPENAME(graphene::net::current_time_reply_message, BOOST_PP_SEQ_NIL,
                                                 (request_sent_time)
                                                 (request_received_time)
                                                 (reply_transmitted_time))
FC_REFLECT_DERIVED_NO_TYPENAME(graphene::net::check_firewall_message, BOOST_PP_SEQ_NIL, (node_id)(endpoint_to_check))
FC_REFLECT_DERIVED_NO_TYPENAME(graphene::net::check_firewall_reply_message, BOOST_PP_SEQ_NIL,
                                (node_id)(endpoint_checked)(result))
FC_REFLECT_DERIVED_NO_TYPENAME(graphene::net::get_current_connections_request_message,
                                BOOST_PP_SEQ_NIL, BOOST_PP_SEQ_NIL)
FC_REFLECT_DERIVED_NO_TYPENAME(graphene::net::current_connection_data, BOOST_PP_SEQ_NIL,
                                              (connection_duration)
                                              (remote_endpoint)
                                              (node_id)
                                              (clock_offset)
                                              (round_trip_delay)
                                              (connection_direction)
                                              (firewalled)
                                              (user_data))
FC_REFLECT_DERIVED_NO_TYPENAME(graphene::net::get_current_connections_reply_message, BOOST_PP_SEQ_NIL,
                                                            (upload_rate_one_minute)
                                                            (download_rate_one_minute)
                                                            (upload_rate_fifteen_minutes)
                                                            (download_rate_fifteen_minutes)
                                                            (upload_rate_one_hour)
                                                            (download_rate_one_hour)
                                                            (current_connections))

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::net::trx_message )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::net::block_message )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::net::item_id )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::net::item_ids_inventory_message )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::net::blockchain_item_ids_inventory_message )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::net::fetch_blockchain_item_ids_message )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::net::fetch_items_message )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::net::item_not_available_message )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::net::hello_message )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::net::connection_accepted_message )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::net::connection_rejected_message )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::net::address_request_message )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::net::address_info )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::net::address_message )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::net::closing_connection_message )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::net::current_time_request_message )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::net::current_time_reply_message )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::net::check_firewall_message )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::net::check_firewall_reply_message )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::net::get_current_connections_request_message )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::net::current_connection_data )
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::net::get_current_connections_reply_message )
