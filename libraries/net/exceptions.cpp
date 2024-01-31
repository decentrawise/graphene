#include <graphene/net/exceptions.hpp>

namespace graphene { namespace net {

   FC_IMPLEMENT_EXCEPTION( net_exception, 90000, "P2P Networking Exception" )
   FC_IMPLEMENT_DERIVED_EXCEPTION( send_queue_overflow,                 net_exception, 90001,
                                   "send queue for this peer exceeded maximum size" )
   FC_IMPLEMENT_DERIVED_EXCEPTION( insufficient_relay_fee,              net_exception, 90002,
                                   "insufficient relay fee" )
   FC_IMPLEMENT_DERIVED_EXCEPTION( already_connected_to_requested_peer, net_exception, 90003,
                                   "already connected to requested peer" )
   FC_IMPLEMENT_DERIVED_EXCEPTION( block_older_than_undo_history,       net_exception, 90004,
                                   "block is older than our undo history allows us to process" )
   FC_IMPLEMENT_DERIVED_EXCEPTION( peer_is_on_an_unreachable_fork,      net_exception, 90005,
                                   "peer is on another fork" )
   FC_IMPLEMENT_DERIVED_EXCEPTION( unlinkable_block_exception,          net_exception, 90006, "unlinkable block" )
   FC_IMPLEMENT_DERIVED_EXCEPTION( block_timestamp_in_future_exception, net_exception, 90007,
                                   "block timestamp in the future" )

} }
