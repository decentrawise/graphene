#pragma once

#include <graphene/net/node.hpp>
#include <graphene/net/peer_database.hpp>
#include <graphene/net/message_oriented_connection.hpp>
#include <graphene/net/config.hpp>

#include <boost/tuple/tuple.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/hashed_index.hpp>

#include <queue>
#include <boost/container/deque.hpp>
#include <fc/thread/future.hpp>

namespace graphene { namespace net
  {
    class peer_connection;
    class peer_connection_delegate
    {
    public:
      virtual ~peer_connection_delegate() = default;
      virtual void on_message(peer_connection* originating_peer,
                              const message& received_message) = 0;
      virtual void on_connection_closed(peer_connection* originating_peer) = 0;
      virtual message get_message_for_item(const item_id& item) = 0;
    };

    using peer_connection_ptr = std::shared_ptr<peer_connection>;
    class peer_connection : public message_oriented_connection_delegate,
                            public std::enable_shared_from_this<peer_connection>
    {
    public:
      enum class our_connection_state
      {
        disconnected,
        just_connected, ///< If in this state, we have sent a hello_message
        connection_accepted, ///< Remote side has sent us a connection_accepted, we're operating normally with them
        /// Remote side has sent us a connection_rejected, we may be exchanging address with them or may just
        /// be waiting for them to close
        connection_rejected
      };
      enum class their_connection_state
      {
        disconnected,
        just_connected, ///< We have not yet received a hello_message
        connection_accepted, ///< We have sent them a connection_accepted
        connection_rejected ///< We have sent them a connection_rejected
      };
      enum class connection_negotiation_status
      {
        disconnected,
        connecting,
        connected,
        accepting,
        accepted,
        hello_sent,
        peer_connection_accepted,
        peer_connection_rejected,
        negotiation_complete,
        closing,
        closed
      };
    private:
      peer_connection_delegate*      _node;
      fc::optional<fc::ip::endpoint> _remote_endpoint;
      message_oriented_connection    _message_connection;

      /* a base class for messages on the queue, to hide the fact that some
       * messages are complete messages and some are only hashes of messages.
       */
      struct queued_message
      {
        fc::time_point enqueue_time;
        fc::time_point transmission_start_time;
        fc::time_point transmission_finish_time;

        explicit queued_message(fc::time_point enqueue_time = fc::time_point::now()) :
          enqueue_time(enqueue_time)
        {}

        virtual message get_message(peer_connection_delegate* node) = 0;
        /** returns roughly the number of bytes of memory the message is consuming while
         * it is sitting on the queue
         */
        virtual size_t get_size_in_queue() = 0;
        virtual ~queued_message() = default;
      };

      /* when you queue up a 'real_queued_message', a full copy of the message is
       * stored on the heap until it is sent
       */
      struct real_queued_message : queued_message
      {
        message        message_to_send;
        size_t         message_send_time_field_offset;

        real_queued_message(message message_to_send,
                            size_t message_send_time_field_offset = (size_t)-1) :
          message_to_send(std::move(message_to_send)),
          message_send_time_field_offset(message_send_time_field_offset)
        {}

        message get_message(peer_connection_delegate* node) override;
        size_t get_size_in_queue() override;
      };

      /* when you queue up a 'virtual_queued_message', we just queue up the hash of the
       * item we want to send.  When it reaches the top of the queue, we make a callback
       * to the node to generate the message.
       */
      struct virtual_queued_message : queued_message
      {
        item_id item_to_send;

        explicit virtual_queued_message(item_id the_item_to_send) :
          item_to_send(std::move(the_item_to_send))
        {}

        message get_message(peer_connection_delegate* node) override;
        size_t get_size_in_queue() override;
      };


      size_t _total_queued_messages_size = 0;
      std::queue<std::unique_ptr<queued_message>, std::list<std::unique_ptr<queued_message> > > _queued_messages;
      fc::future<void> _send_queued_messages_done;
    public:
      fc::time_point connection_initiation_time;
      fc::time_point connection_closed_time;
      fc::time_point connection_terminated_time;
      peer_connection_direction direction = peer_connection_direction::unknown;
      firewalled_state is_firewalled = firewalled_state::unknown;
      fc::microseconds clock_offset;
      fc::microseconds round_trip_delay;

      our_connection_state our_state = our_connection_state::disconnected;
      bool they_have_requested_close = false;
      their_connection_state their_state = their_connection_state::disconnected;
      bool we_have_requested_close = false;

      connection_negotiation_status negotiation_status = connection_negotiation_status::disconnected;
      fc::oexception connection_closed_error;

      fc::time_point get_connection_time()const { return _message_connection.get_connection_time(); }
      fc::time_point get_connection_terminated_time()const { return connection_terminated_time; }

      /// data about the peer node
      /// @{
      /** node_public_key from the hello message, zero-initialized before we get the hello */
      node_id_t        node_public_key;
      /** the unique identifier we'll use to refer to the node with.  zero-initialized before
       * we receive the hello message, at which time it will be filled with either the "node_id"
       * from the user_data field of the hello, or if none is present it will be filled with a
       * copy of node_public_key */
      node_id_t        node_id;
      uint32_t         core_protocol_version = 0;
      std::string      user_agent;
      fc::optional<std::string> graphene_git_revision_sha;
      fc::optional<fc::time_point_sec> graphene_git_revision_unix_timestamp;
      fc::optional<std::string> fc_git_revision_sha;
      fc::optional<fc::time_point_sec> fc_git_revision_unix_timestamp;
      fc::optional<std::string> platform;
      fc::optional<uint32_t> bitness;

      // Initially, these fields record info about our local socket,
      // they are useless (except the remote_inbound_endpoint field for outbound connections).
      // After we receive a hello message, they are replaced with the info in the hello message.
      fc::ip::address inbound_address;
      uint16_t inbound_port = 0;
      uint16_t outbound_port = 0;
      /// The inbound endpoint of the remote peer (our best guess)
      fc::optional<fc::ip::endpoint> remote_inbound_endpoint;
      /// Some nodes may be listening on multiple endpoints
      fc::flat_set<fc::ip::endpoint> additional_inbound_endpoints;
      /// Potential inbound endpoints of the peer
      fc::flat_map<fc::ip::endpoint, firewalled_state> potential_inbound_endpoints;
      /// @}

      using item_to_time_map_type = std::unordered_map<item_id, fc::time_point>;

      /// Blockchain synchronization state data
      /// @{
      /// ID of items in the blockchain that this peer has told us about
      boost::container::deque<item_hash_t> ids_of_items_to_get;
      /// List of all items this peer has offered use that we've already handed to the client but the client
      /// hasn't finished processing
      std::set<item_hash_t> ids_of_items_being_processed;
      /// Number of items in the blockchain that follow ids_of_items_to_get but the peer hasn't yet told us their IDs
      uint32_t number_of_unfetched_item_ids = 0;
      bool peer_needs_sync_items_from_us = false;
      bool we_need_sync_items_from_peer = false;
      /// We check this to detect a timed-out request and in busy()
      fc::optional<boost::tuple<std::vector<item_hash_t>, fc::time_point> > item_ids_requested_from_peer;
      /// The time we received the last sync item or the time we sent the last batch of sync item requests
      /// to this peer
      fc::time_point last_sync_item_received_time;
      /// IDs of blocks we've requested from this peer during sync. Fetch from another peer if this peer disconnects
      std::set<item_hash_t> sync_items_requested_from_peer;
      /// The hash of the last block  this peer has told us about that the peer knows
      item_hash_t last_block_delegate_has_seen;
      fc::time_point_sec last_block_time_delegate_has_seen;
      bool inhibit_fetching_sync_blocks = false;
      /// @}

      /// non-synchronization state data
      /// @{
      struct timestamped_item_id
      {
        item_id            item;
        fc::time_point_sec timestamp;
        timestamped_item_id(const item_id& item, const fc::time_point_sec timestamp) :
          item(item),
          timestamp(timestamp)
        {}
      };
      struct timestamp_index{};
      using timestamped_items_set_type = boost::multi_index_container< timestamped_item_id,
         boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<
               boost::multi_index::member<timestamped_item_id, item_id, &timestamped_item_id::item>,
               std::hash<item_id>
            >,
            boost::multi_index::ordered_non_unique<
               boost::multi_index::tag<timestamp_index>,
               boost::multi_index::member<timestamped_item_id, fc::time_point_sec, &timestamped_item_id::timestamp>
            >
         >
      >;
      timestamped_items_set_type inventory_peer_advertised_to_us;
      timestamped_items_set_type inventory_advertised_to_peer;

      /// Items we've requested from this peer during normal operation.
      /// Fetch from another peer if this peer disconnects
      item_to_time_map_type items_requested_from_peer;
      /// @}

      // if they're flooding us with transactions, we set this to avoid fetching for a few seconds to let the
      // blockchain catch up
      fc::time_point transaction_fetching_inhibited_until;

      uint32_t last_known_fork_block_number = 0;

      fc::future<void> accept_or_connect_task_done;

      /// Whether we're waiting for an address message
      bool expecting_address_message = false;

    private:
#ifndef NDEBUG
      fc::thread* _thread = nullptr;
      unsigned _send_message_queue_tasks_running = 0; // temporary debugging
#endif
      /// true while we're in the middle of handling a message from the remote system
      bool _currently_handling_message = false;
    protected:
      peer_connection(peer_connection_delegate* delegate);
    private:
      void destroy();
    public:
      /// Use this instead of the constructor
      static peer_connection_ptr make_shared(peer_connection_delegate* delegate);
      virtual ~peer_connection();

      fc::tcp_socket& get_socket();
      void accept_connection();
      void connect_to(const fc::ip::endpoint& remote_endpoint,
                      const fc::optional<fc::ip::endpoint>& local_endpoint = fc::optional<fc::ip::endpoint>());

      void on_message(message_oriented_connection* originating_connection, const message& received_message) override;
      void on_connection_closed(message_oriented_connection* originating_connection) override;

      void send_queueable_message(std::unique_ptr<queued_message>&& message_to_send);
      virtual void send_message( const message& message_to_send, size_t message_send_time_field_offset = (size_t)-1 );
      void send_item(const item_id& item_to_send);
      void close_connection();
      void destroy_connection();

      uint64_t get_total_bytes_sent() const;
      uint64_t get_total_bytes_received() const;

      fc::time_point get_last_message_sent_time() const;
      fc::time_point get_last_message_received_time() const;

      fc::optional<fc::ip::endpoint> get_remote_endpoint();
      fc::ip::endpoint get_local_endpoint();
      void set_remote_endpoint(fc::optional<fc::ip::endpoint> new_remote_endpoint);

      bool busy() const;
      bool idle() const;
      bool is_currently_handling_message() const;

      bool is_transaction_fetching_inhibited() const;
      fc::sha512 get_shared_secret() const;
      void clear_old_inventory();
      bool is_inventory_advertised_to_us_list_full_for_transactions() const;
      bool is_inventory_advertised_to_us_list_full() const;
      fc::optional<fc::ip::endpoint> get_endpoint_for_connecting() const;
    private:
      void send_queued_messages_task();
      void accept_connection_task();
      void connect_to_task(const fc::ip::endpoint& remote_endpoint);
    };
    typedef std::shared_ptr<peer_connection> peer_connection_ptr;

 } } // end namespace graphene::net

// not sent over the wire, just reflected for logging
FC_REFLECT_ENUM(graphene::net::peer_connection::our_connection_state, (disconnected)
                                                                 (just_connected)
                                                                 (connection_accepted)
                                                                 (connection_rejected))
FC_REFLECT_ENUM(graphene::net::peer_connection::their_connection_state, (disconnected)
                                                                   (just_connected)
                                                                   (connection_accepted)
                                                                   (connection_rejected))
FC_REFLECT_ENUM(graphene::net::peer_connection::connection_negotiation_status, (disconnected)
                                                                          (connecting)
                                                                          (connected)
                                                                          (accepting)
                                                                          (accepted)
                                                                          (hello_sent)
                                                                          (peer_connection_accepted)
                                                                          (peer_connection_rejected)
                                                                          (negotiation_complete)
                                                                          (closing)
                                                                          (closed) )

FC_REFLECT( graphene::net::peer_connection::timestamped_item_id, (item)(timestamp) )
