#pragma once

#define P2P_IN_DEDICATED_THREAD

//#define ENABLE_DEBUG_ULOGS

#ifdef DEFAULT_LOGGER
# undef DEFAULT_LOGGER
#endif
#define DEFAULT_LOGGER "p2p"

//log these messages even at warn level when operating on the test network
#ifdef GRAPHENE_TEST_NETWORK
#define testnetlog wlog
#else
#define testnetlog(...) do {} while (0)
#endif

#include <memory>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <fc/thread/thread.hpp>
#include <fc/thread/mutex.hpp>
#include <fc/thread/scoped_lock.hpp>
#include <fc/log/logger.hpp>
#include <fc/network/tcp_socket.hpp>
#include <fc/network/rate_limiting.hpp>
#include <graphene/chain/config.hpp>
#include <graphene/protocol/types.hpp>
#include <graphene/net/node.hpp>
#include <graphene/net/core_messages.hpp>
#include <graphene/net/peer_connection.hpp>

namespace graphene { namespace net { namespace detail {

namespace bmi = boost::multi_index;

/*******
 * A class to wrap std::unordered_set for multithreading
 */
template <class Key, class Hash = std::hash<Key>, class Pred = std::equal_to<Key> >
class concurrent_unordered_set : private std::unordered_set<Key, Hash, Pred>
{
private:
   mutable fc::mutex mux;

public:
   /// Iterations require a lock. This exposes the mutex. Use with care (i.e. lock_guard)
   fc::mutex& get_mutex()const { return mux; }

   /// Insertion
   /// @{
   std::pair< typename std::unordered_set<Key, Hash, Pred>::iterator, bool> emplace( Key key)
   {
      fc::scoped_lock<fc::mutex> lock(mux);
      return std::unordered_set<Key, Hash, Pred>::emplace( key );
   }
   std::pair< typename std::unordered_set<Key, Hash, Pred>::iterator, bool> insert (const Key& val)
   {
      fc::scoped_lock<fc::mutex> lock(mux);
      return std::unordered_set<Key, Hash, Pred>::insert( val ); 
   }
   /// @}
   /// Size
   /// @{
   size_t size() const 
   { 
      fc::scoped_lock<fc::mutex> lock(mux);
      return std::unordered_set<Key, Hash, Pred>::size(); 
   }
   bool empty() const noexcept
   {
      fc::scoped_lock<fc::mutex> lock(mux);
      return std::unordered_set<Key, Hash, Pred>::empty();
   }
   /// @}
   /// Removal
   /// @{
   void clear() noexcept
   {
      fc::scoped_lock<fc::mutex> lock(mux);
      std::unordered_set<Key, Hash, Pred>::clear();
   }
   typename std::unordered_set<Key, Hash, Pred>::iterator erase( 
         typename std::unordered_set<Key, Hash, Pred>::const_iterator itr)
   { 
      fc::scoped_lock<fc::mutex> lock(mux);
      return std::unordered_set<Key, Hash, Pred>::erase( itr); 
   }
   size_t erase( const Key& key)
   {
      fc::scoped_lock<fc::mutex> lock(mux);
      return std::unordered_set<Key, Hash, Pred>::erase( key ); 
   }
   /// @}
   /// Swap
   /// @{
   void swap( typename std::unordered_set<Key, Hash, Pred>& other ) noexcept
   {
      fc::scoped_lock<fc::mutex> lock(mux);
      std::unordered_set<Key, Hash, Pred>::swap( other );
   }
   /// @}
   /// Iteration
   /// @{
   typename std::unordered_set<Key, Hash, Pred>::iterator begin() noexcept 
   { 
      fc::scoped_lock<fc::mutex> lock(mux);
      return std::unordered_set<Key, Hash, Pred>::begin(); 
   }
   typename std::unordered_set<Key, Hash, Pred>::const_iterator begin() const noexcept 
   { 
      fc::scoped_lock<fc::mutex> lock(mux);
      return std::unordered_set<Key, Hash, Pred>::begin(); 
   }
   typename std::unordered_set<Key, Hash, Pred>::local_iterator begin(size_t n) 
   { 
      fc::scoped_lock<fc::mutex> lock(mux);
      return std::unordered_set<Key, Hash, Pred>::begin(n); 
   }
   typename std::unordered_set<Key, Hash, Pred>::const_local_iterator begin(size_t n) const 
   { 
      fc::scoped_lock<fc::mutex> lock(mux);
      return std::unordered_set<Key, Hash, Pred>::begin(n); 
   }
   typename std::unordered_set<Key, Hash, Pred>::iterator end() noexcept 
   { 
      fc::scoped_lock<fc::mutex> lock(mux);
      return std::unordered_set<Key, Hash, Pred>::end(); 
   }
   typename std::unordered_set<Key, Hash, Pred>::const_iterator end() const noexcept 
   { 
      fc::scoped_lock<fc::mutex> lock(mux);
      return std::unordered_set<Key, Hash, Pred>::end(); 
   }
   typename std::unordered_set<Key, Hash, Pred>::local_iterator end(size_t n) 
   { 
      fc::scoped_lock<fc::mutex> lock(mux);
      return std::unordered_set<Key, Hash, Pred>::end(n); 
   }
   typename std::unordered_set<Key, Hash, Pred>::const_local_iterator end(size_t n) const 
   { 
      fc::scoped_lock<fc::mutex> lock(mux);
      return std::unordered_set<Key, Hash, Pred>::end(n); 
   }
   /// @}
   /// Search
   typename std::unordered_set<Key, Hash, Pred>::const_iterator find(Key key)
   {
      fc::scoped_lock<fc::mutex> lock(mux);
      return std::unordered_set<Key, Hash, Pred>::find(key); 
   }
};

class blockchain_tied_message_cache
{
private:
   static const uint32_t cache_duration_in_blocks = GRAPHENE_NET_MESSAGE_CACHE_DURATION_IN_BLOCKS;

   struct message_hash_index{};
   struct message_contents_hash_index{};
   struct block_clock_index{};
   struct message_info
   {
      message_hash_type message_hash;
      message           message_body;
      uint32_t          block_clock_when_received;

      /// for network performance stats
      message_propagation_data propagation_data;
      /// hash of whatever the message contains
      /// (if it's a transaction, this is the transaction id, if it's a block, it's the block_id)
      message_hash_type message_contents_hash;

      message_info( const message_hash_type& message_hash,
                    const message&           message_body,
                    uint32_t                 block_clock_when_received,
                    const message_propagation_data& propagation_data,
                    message_hash_type        message_contents_hash ) :
            message_hash( message_hash ),
            message_body( message_body ),
            block_clock_when_received( block_clock_when_received ),
            propagation_data( propagation_data ),
            message_contents_hash( message_contents_hash )
      {}
   };

   using message_cache_container = boost::multi_index_container < message_info,
               bmi::indexed_by<
                  bmi::ordered_unique< bmi::tag<message_hash_index>,
                     bmi::member<message_info, message_hash_type, &message_info::message_hash> >,
                  bmi::ordered_non_unique< bmi::tag<message_contents_hash_index>,
                     bmi::member<message_info, message_hash_type, &message_info::message_contents_hash> >,
                  bmi::ordered_non_unique< bmi::tag<block_clock_index>,
                     bmi::member<message_info, uint32_t, &message_info::block_clock_when_received> > > >;

   message_cache_container _message_cache;

   uint32_t block_clock = 0;

public:
   void block_accepted();
   void cache_message( const message& message_to_cache,
                       const message_hash_type& hash_of_message_to_cache,
                       const message_propagation_data& propagation_data,
                       const message_hash_type& message_content_hash );
   message get_message( const message_hash_type& hash_of_message_to_lookup ) const;
   message_propagation_data get_message_propagation_data(
         const message_hash_type& hash_of_msg_contents_to_lookup ) const;
   size_t size() const { return _message_cache.size(); }
};

/// When requesting items from peers, we want to prioritize any blocks before
/// transactions, but otherwise request items in the order we heard about them
struct prioritized_item_id
{
  item_id  item;
  size_t sequence_number;
  fc::time_point timestamp; ///< the time we last heard about this item in an inventory message

  prioritized_item_id(const item_id& item, size_t sequence_number) :
    item(item),
    sequence_number(sequence_number),
    timestamp(fc::time_point::now())
  {}
  bool operator<(const prioritized_item_id& rhs) const
  {
    static_assert(graphene::net::block_message_type > graphene::net::trx_message_type,
                  "block_message_type must be greater than trx_message_type for prioritized_item_ids to sort correctly");
    if (item.item_type != rhs.item.item_type)
      return item.item_type > rhs.item.item_type;
    return rhs.sequence_number > sequence_number;
  }
};

class statistics_gathering_node_delegate_wrapper : public node_delegate
{
   private:
      std::shared_ptr<node_delegate> _node_delegate;
      fc::thread *_thread;

      using call_stats_accumulator = boost::accumulators::accumulator_set< int64_t,
                                        boost::accumulators::stats< boost::accumulators::tag::min,
                                                                    boost::accumulators::tag::rolling_mean,
                                                                    boost::accumulators::tag::max,
                                                                    boost::accumulators::tag::sum,
                                                                    boost::accumulators::tag::count> >;
#define NODE_DELEGATE_METHOD_NAMES (has_item) \
                               (handle_message) \
                               (handle_block) \
                               (handle_transaction) \
                               (get_block_ids) \
                               (get_item) \
                               (get_chain_id) \
                               (get_blockchain_synopsis) \
                               (sync_status) \
                               (connection_count_changed) \
                               (get_block_number) \
                               (get_block_time) \
                               (get_head_block_id) \
                               (estimate_last_known_fork_from_git_revision_timestamp) \
                               (error_encountered) \
                               (get_current_block_interval_in_seconds)



#define DECLARE_ACCUMULATOR(r, data, method_name) \
      mutable call_stats_accumulator BOOST_PP_CAT(_, BOOST_PP_CAT(method_name, _execution_accumulator)); \
      mutable call_stats_accumulator BOOST_PP_CAT(_, BOOST_PP_CAT(method_name, _delay_before_accumulator)); \
      mutable call_stats_accumulator BOOST_PP_CAT(_, BOOST_PP_CAT(method_name, _delay_after_accumulator));
      BOOST_PP_SEQ_FOR_EACH(DECLARE_ACCUMULATOR, unused, NODE_DELEGATE_METHOD_NAMES)
#undef DECLARE_ACCUMULATOR

      class call_statistics_collector
      {
      private:
        fc::time_point _call_requested_time;
        fc::time_point _begin_execution_time;
        fc::time_point _execution_completed_time;
        const char* _method_name;
        call_stats_accumulator* _execution_accumulator;
        call_stats_accumulator* _delay_before_accumulator;
        call_stats_accumulator* _delay_after_accumulator;
      public:
        class actual_execution_measurement_helper
        {
          std::shared_ptr<call_statistics_collector> _collector;
        public:
          explicit actual_execution_measurement_helper(std::shared_ptr<call_statistics_collector> collector) :
            _collector(collector)
          {
            _collector->starting_execution();
          }
          ~actual_execution_measurement_helper()
          {
            _collector->execution_completed();
          }
        };
        call_statistics_collector(const char* method_name,
                                  call_stats_accumulator* execution_accumulator,
                                  call_stats_accumulator* delay_before_accumulator,
                                  call_stats_accumulator* delay_after_accumulator) :
          _call_requested_time(fc::time_point::now()),
          _method_name(method_name),
          _execution_accumulator(execution_accumulator),
          _delay_before_accumulator(delay_before_accumulator),
          _delay_after_accumulator(delay_after_accumulator)
        {}
        ~call_statistics_collector()
        {
          fc::time_point end_time(fc::time_point::now());
          fc::microseconds actual_execution_time(_execution_completed_time - _begin_execution_time);
          fc::microseconds delay_before(_begin_execution_time - _call_requested_time);
          fc::microseconds delay_after(end_time - _execution_completed_time);
          fc::microseconds total_duration(actual_execution_time + delay_before + delay_after);
          (*_execution_accumulator)(actual_execution_time.count());
          (*_delay_before_accumulator)(delay_before.count());
          (*_delay_after_accumulator)(delay_after.count());
          if (total_duration > fc::milliseconds(500))
          {
            dlog("Call to method node_delegate::${method} took ${total_duration}us, longer than our target maximum of 500ms",
                 ("method", _method_name)
                 ("total_duration", total_duration.count()));
            dlog("Actual execution took ${execution_duration}us, with a ${delegate_delay}us delay before the delegate thread started "
                 "executing the method, and a ${p2p_delay}us delay after it finished before the p2p thread started processing the response",
                 ("execution_duration", actual_execution_time)
                 ("delegate_delay", delay_before)
                 ("p2p_delay", delay_after));
          }
        }
        void starting_execution()
        {
          _begin_execution_time = fc::time_point::now();
        }
        void execution_completed()
        {
          _execution_completed_time = fc::time_point::now();
        }
      };
   public:
      statistics_gathering_node_delegate_wrapper(std::shared_ptr<node_delegate> delegate,
                                                 fc::thread* thread_for_delegate_calls);

      fc::variant_object get_call_statistics();

      bool has_item( const graphene::net::item_id& id ) override;
      void handle_message( const message& ) override;
      bool handle_block( const graphene::net::block_message& block_message, bool sync_mode,
                         std::vector<message_hash_type>& contained_transaction_msg_ids ) override;
      void handle_transaction( const graphene::net::trx_message& transaction_message ) override;
      std::vector<item_hash_t> get_block_ids(const std::vector<item_hash_t>& blockchain_synopsis,
                                             uint32_t& remaining_item_count,
                                             uint32_t limit = 2000) override;
      message get_item( const item_id& id ) override;
      graphene::protocol::chain_id_type get_chain_id() const override;
      std::vector<item_hash_t> get_blockchain_synopsis(const item_hash_t& reference_point,
                                                       uint32_t number_of_blocks_after_reference_point) override;
      void     sync_status( uint32_t item_type, uint32_t item_count ) override;
      void     connection_count_changed( uint32_t c ) override;
      uint32_t get_block_number(const item_hash_t& block_id) override;
      fc::time_point_sec get_block_time(const item_hash_t& block_id) override;
      item_hash_t get_head_block_id() const override;
      uint32_t estimate_last_known_fork_from_git_revision_timestamp(uint32_t unix_timestamp) const override;
      void error_encountered(const std::string& message, const fc::oexception& error) override;
      uint8_t get_current_block_interval_in_seconds() const override;
};

/// This specifies configuration info for the local node.  It's stored as JSON
/// in the configuration directory (application data directory)
struct node_configuration
{
   fc::ip::endpoint listen_endpoint;
   fc::optional<fc::ip::endpoint> inbound_endpoint;
   bool accept_incoming_connections = true;
   bool connect_to_new_peers = true;
   bool wait_if_endpoint_is_busy = false;
   /**
    * Originally, our p2p code just had a 'node-id' that was a random number identifying this node
    * on the network.  This is now a private key/public key pair, where the public key is used
    * in place of the old random node-id.  The private part is unused, but might be used in
    * the future to support some notion of trusted peers.
    */
   fc::ecc::private_key private_key;
};

class node_impl : public peer_connection_delegate, public std::enable_shared_from_this<node_impl>
{
public:
   class address_builder
   {
   public:
      static std::shared_ptr<address_builder> create_default_address_builder();
      void build( node_impl* impl, address_message& ) const;
      virtual bool should_advertise(const  fc::ip::endpoint& in ) const = 0;
      virtual ~address_builder() = default;
   };

#ifdef P2P_IN_DEDICATED_THREAD
      std::shared_ptr<fc::thread> _thread = std::make_shared<fc::thread>("p2p");
      std::shared_ptr<fc::thread> get_thread() const { return _thread; }
#endif // P2P_IN_DEDICATED_THREAD
      std::unique_ptr<statistics_gathering_node_delegate_wrapper> _delegate;
      fc::sha256           _chain_id;

#define NODE_CONFIGURATION_FILENAME      "node_config.json"
#define POTENTIAL_PEER_DATABASE_FILENAME "peers.json"
      fc::path             _node_configuration_directory;
      node_configuration   _node_configuration;

      /// Stores the endpoint we're listening on.  This will be the same as
      /// _node_configuration.listen_endpoint, unless that endpoint was already
      /// in use.
      /// This will be 0.0.0.0:0 if the node is configured to not listen.
      // Note: updating the type to optional may break 3rd-party client applications.
      fc::ip::endpoint     _actual_listening_endpoint;

      /// Used by the task that manages connecting to peers
      /// @{
      /// List of peers we want to connect to as soon as possible
      std::list<potential_peer_record> _add_once_node_list;

      peer_database             _potential_peer_db;
      fc::promise<void>::ptr    _retrigger_connect_loop_promise;
      bool                      _potential_peer_db_updated = false;
      fc::future<void>          _p2p_network_connect_loop_done;
      /// @}

      /// Used by the task that fetches sync items during synchronization
      /// @{
      fc::promise<void>::ptr    _retrigger_fetch_sync_items_loop_promise;
      bool                      _sync_items_to_fetch_updated = false;
      fc::future<void>          _fetch_sync_items_loop_done;

      typedef std::unordered_map<graphene::net::block_id_type, fc::time_point> active_sync_requests_map;

      /// List of sync blocks we've asked for from peers but have not yet received
      active_sync_requests_map              _active_sync_requests;
      /// List of sync blocks we've just received but haven't yet tried to process
      std::list<graphene::net::block_message> _new_received_sync_items;
      /// List of sync blocks we've received, but can't yet process because we are still missing blocks
      /// that come earlier in the chain
      std::list<graphene::net::block_message> _received_sync_items;
      /// @}

      fc::future<void> _process_backlog_of_sync_blocks_done;
      bool _suspend_fetching_sync_blocks = false;

      /// Used by the task that fetches items during normal operation
      /// @{
      fc::promise<void>::ptr _retrigger_fetch_item_loop_promise;
      bool                   _items_to_fetch_updated = false;
      fc::future<void>       _fetch_item_loop_done;

      struct item_id_index{};
      using items_to_fetch_set_type = boost::multi_index_container< prioritized_item_id,
               boost::multi_index::indexed_by<
                  boost::multi_index::ordered_unique< boost::multi_index::identity<prioritized_item_id> >,
                  boost::multi_index::hashed_unique<
                     boost::multi_index::tag<item_id_index>,
                     boost::multi_index::member<prioritized_item_id, item_id, &prioritized_item_id::item>,
                     std::hash<item_id>
                  >
               >
            >;
      /// Items to fetch sequence counter
      size_t  _items_to_fetch_seq_counter = 0;
      /// List of items we know another peer has and we want
      items_to_fetch_set_type _items_to_fetch;
      /// List of transactions we've recently pushed and had rejected by the delegate
      peer_connection::timestamped_items_set_type _recently_failed_items;
      /// @}

      /// Used by the task that advertises inventory during normal operation
      /// @{
      fc::promise<void>::ptr        _retrigger_advertise_inventory_loop_promise;
      fc::future<void>              _advertise_inventory_loop_done;
      /// List of items we have received but not yet advertised to our peers
      concurrent_unordered_set<item_id>   _new_inventory;
      /// @}

      fc::future<void>     _kill_inactive_conns_loop_done;
      /// A cached copy of the block interval, to avoid a thread hop to the blockchain to get the current value
      uint8_t _recent_block_interval_seconds = GRAPHENE_MAX_BLOCK_INTERVAL;

      std::string          _user_agent_string;
      /**
       * A key automatically generated when the client is first run, stored in
       * node_config.json.  It doesn't really have much of a purpose yet, there was just some thought
       * that we might someday have a use for nodes having a private key (sent in hello messages)
       */
      node_id_t            _node_public_key;
      /**
       * A random number generated each time the client is launched, used to prevent us
       * from connecting to the same client multiple times (sent in hello messages).
       * Since this was introduced after the hello_message was finalized, this is sent in the
       * user_data field.
       * While this shares the same underlying type as a public key, it is really just a random
       * number.
       */
      node_id_t            _node_id;

      /** If we have less than `_desired_number_of_connections`, we will try to connect with more nodes */
      uint32_t             _desired_number_of_connections = GRAPHENE_NET_DEFAULT_DESIRED_CONNECTIONS;
      /** If we have _maximum_number_of_connections or more, we will refuse any inbound connections */
      uint32_t             _maximum_number_of_connections = GRAPHENE_NET_DEFAULT_MAX_CONNECTIONS;
      /** Retry connections to peers that have failed or rejected us this often, in seconds */
      uint32_t             _peer_connection_retry_timeout = GRAPHENE_NET_DEFAULT_PEER_CONNECTION_RETRY_TIME;
      /** How many seconds of inactivity are permitted before disconnecting a peer */
      uint32_t             _peer_inactivity_timeout = GRAPHENE_NET_PEER_HANDSHAKE_INACTIVITY_TIMEOUT;

      fc::tcp_server       _tcp_server;
      fc::future<void>     _accept_loop_complete;

      /// Stores all connections which have not yet finished key exchange or are still sending
      /// initial handshaking messages back and forth (not yet ready to initiate syncing)
      concurrent_unordered_set<graphene::net::peer_connection_ptr>               _handshaking_connections;
      /** Stores fully established connections we're either syncing with or in normal operation with */
      concurrent_unordered_set<graphene::net::peer_connection_ptr>               _active_connections;
      /// Stores connections we've closed (sent closing message, not actually closed),
      /// but are still waiting for the remote end to close before we delete them
      concurrent_unordered_set<graphene::net::peer_connection_ptr>               _closing_connections;
      /// Stores connections we've closed, but are still waiting for the OS to notify us that the socket
      /// is really closed
      concurrent_unordered_set<graphene::net::peer_connection_ptr>               _terminating_connections;

      /// The /n/ most recent blocks we've accepted (currently tuned to the max number of connections)
      boost::circular_buffer<item_hash_t> _most_recent_blocks_accepted { _maximum_number_of_connections };

      uint32_t _sync_item_type = 0;
      /// The number of items we still need to fetch while syncing
      uint32_t _total_num_of_unfetched_items = 0;
      /// List of all block numbers where there are hard forks
      std::vector<uint32_t> _hard_fork_block_numbers;

      /// Cache message we have received and might be required to provide to other peers via inventory requests
      blockchain_tied_message_cache _message_cache;

      fc::rate_limiting_group _rate_limiter { 0, 0 };

      /// Number of connections last reported to the client (to avoid sending duplicate messages)
      uint32_t _last_reported_number_of_conns = 0;

      std::shared_ptr<address_builder> _address_builder = address_builder::create_default_address_builder();

      fc::future<void> _fetch_updated_peer_lists_loop_done;

      /// Average network read speed in the past seconds
      boost::circular_buffer<uint32_t> _avg_net_read_speed_seconds { 60 };
      /// Average network write speed in the past seconds
      boost::circular_buffer<uint32_t> _avg_net_write_speed_seconds { 60 };
      /// Average network read speed in the past minutes
      boost::circular_buffer<uint32_t> _avg_net_read_speed_minutes { 60 };
      /// Average network write speed in the past minutes
      boost::circular_buffer<uint32_t> _avg_net_write_speed_minutes { 60 };
      /// Average network read speed in the past hours
      boost::circular_buffer<uint32_t> _avg_net_read_speed_hours { 72 };
      /// Average network write speed in the past hours
      boost::circular_buffer<uint32_t> _avg_net_write_speed_hours { 72 };
      /// Average network usage second counter
      size_t _avg_net_usage_second_counter = 0;
      /// Average network usage minute counter
      size_t _avg_net_usage_minute_counter = 0;

      fc::time_point_sec _bandwidth_monitor_last_update_time;
      fc::future<void> _bandwidth_monitor_loop_done;

      fc::future<void> _dump_node_status_task_done;

      /**
       * We have two alternate paths through the schedule_peer_for_deletion code -- one that
       * uses a mutex to prevent one fiber from adding items to the queue while another is deleting
       * items from it, and one that doesn't.  The one that doesn't is simpler and more efficient
       * code, but we're keeping around the version that uses the mutex because it crashes, and
       * this crash probably indicates a bug in our underlying threading code that needs
       * fixing.  To produce the bug, define USE_PEERS_TO_DELETE_MUTEX and then connect up
       * to the network and set your desired/max connection counts high
       */
      /// @{
//#define USE_PEERS_TO_DELETE_MUTEX 1
#ifdef USE_PEERS_TO_DELETE_MUTEX
      fc::mutex _peers_to_delete_mutex;
#endif
      std::list<peer_connection_ptr> _peers_to_delete;
      fc::future<void> _delayed_peer_deletion_task_done;
      /// @}

#ifdef ENABLE_P2P_DEBUGGING_API
      std::set<node_id_t> _allowed_peers;
#endif // ENABLE_P2P_DEBUGGING_API

      /// Set to true when we begin our destructor,
      /// used to prevent us from starting new tasks while we're shutting down
      bool _node_is_shutting_down = false;

      /// Maximum number of addresses to handle at one time
      size_t _max_addrs_to_handle_at_once = MAX_ADDRESSES_TO_HANDLE_AT_ONCE;
      /// Maximum number of blocks to handle at one time
      size_t _max_blocks_to_handle_at_once = MAX_BLOCKS_TO_HANDLE_AT_ONCE;
      /// Maximum number of sync blocks to prefetch
      size_t _max_sync_blocks_to_prefetch = MAX_SYNC_BLOCKS_TO_PREFETCH;
      /// Maximum number of blocks per peer during syncing
      size_t _max_sync_blocks_per_peer = GRAPHENE_NET_MAX_BLOCKS_PER_PEER_DURING_SYNCING;

      std::list<fc::future<void> > _handle_message_calls_in_progress;

      /// Used by the task that checks whether addresses of seed nodes have been updated
      /// @{
      boost::container::flat_set<std::string> _seed_nodes;
      fc::future<void> _update_seed_nodes_loop_done;
      void update_seed_nodes_task();
      void schedule_next_update_seed_nodes_task();
      /// @}

      explicit node_impl(const std::string& user_agent);
      ~node_impl() override;

      void save_node_configuration();

      void p2p_network_connect_loop();
      void trigger_p2p_network_connect_loop();

      bool have_already_received_sync_item( const item_hash_t& item_hash );
      void request_sync_item_from_peer( const peer_connection_ptr& peer, const item_hash_t& item_to_request );
      void request_sync_items_from_peer( const peer_connection_ptr& peer, const std::vector<item_hash_t>& items_to_request );
      void fetch_sync_items_loop();
      void trigger_fetch_sync_items_loop();

      bool is_item_in_any_peers_inventory(const item_id& item) const;
      void fetch_items_loop();
      void trigger_fetch_items_loop();

      void advertise_inventory_loop();
      void trigger_advertise_inventory_loop();

      void kill_inactive_conns_loop(node_impl_ptr self);

      void fetch_updated_peer_lists_loop();
      void update_bandwidth_data(uint32_t bytes_read_this_second, uint32_t bytes_written_this_second);
      void bandwidth_monitor_loop();
      void dump_node_status_task();

      bool is_accepting_new_connections();
      bool is_wanting_new_connections();
      uint32_t get_number_of_connections();
      peer_connection_ptr get_peer_by_node_id(const node_id_t& id) const;

      bool merge_address_info_with_potential_peer_database( const std::vector<address_info> addresses );
      void display_current_connections();
      uint32_t calculate_unsynced_block_count_from_all_peers();
      std::vector<item_hash_t> create_blockchain_synopsis_for_peer( const peer_connection* peer );
      void fetch_next_batch_of_item_ids_from_peer( peer_connection* peer, bool reset_fork_tracking_data_for_peer = false );

      fc::variant_object generate_hello_user_data();
      void parse_hello_user_data_for_peer( peer_connection* originating_peer, const fc::variant_object& user_data );

      void on_message( peer_connection* originating_peer,
                       const message& received_message ) override;

      void on_hello_message( peer_connection* originating_peer,
                             const hello_message& hello_message_received );

      void on_connection_accepted_message( peer_connection* originating_peer,
                                           const connection_accepted_message& ) const;

      void on_connection_rejected_message( peer_connection* originating_peer,
                                           const connection_rejected_message& connection_rejected_message_received );

      void on_address_request_message( peer_connection* originating_peer, const address_request_message&);

      void on_address_message( peer_connection* originating_peer,
                               const address_message& address_message_received );

      void on_fetch_blockchain_item_ids_message( peer_connection* originating_peer,
                                                 const fetch_blockchain_item_ids_message& fetch_blockchain_item_ids_message_received );

      void on_blockchain_item_ids_inventory_message( peer_connection* originating_peer,
                                                     const blockchain_item_ids_inventory_message& blockchain_item_ids_inventory_message_received );

      void on_fetch_items_message( peer_connection* originating_peer,
                                   const fetch_items_message& fetch_items_message_received );

      void on_item_not_available_message( peer_connection* originating_peer,
                                          const item_not_available_message& item_not_available_message_received );

      void on_item_ids_inventory_message( peer_connection* originating_peer,
                                          const item_ids_inventory_message& item_ids_inventory_message_received );

      void on_closing_connection_message( peer_connection* originating_peer,
                                          const closing_connection_message& closing_connection_message_received );

      void on_current_time_request_message( peer_connection* originating_peer,
                                            const current_time_request_message& current_time_request_message_received );

      void on_current_time_reply_message( peer_connection* originating_peer,
                                          const current_time_reply_message& current_time_reply_message_received );

      void on_connection_closed(peer_connection* originating_peer) override;

      void send_sync_block_to_node_delegate(const graphene::net::block_message& block_message_to_send);
      void process_backlog_of_sync_blocks();
      void trigger_process_backlog_of_sync_blocks();
      void process_block_during_syncing(
                  peer_connection* originating_peer,
                  const graphene::net::block_message& block_message,
                  const message_hash_type& message_hash);
      void process_block_when_in_sync(
                  peer_connection* originating_peer,
                  const graphene::net::block_message& block_message,
                  const message_hash_type& message_hash);
      void process_block_message(
                  peer_connection* originating_peer,
                  const message& message_to_process,
                  const message_hash_type& message_hash);

      void process_ordinary_message(
                  peer_connection* originating_peer,
                  const message& message_to_process,
                  const message_hash_type& message_hash);

      void start_synchronizing();
      void start_synchronizing_with_peer(const peer_connection_ptr& peer);

      /// Called after a peer finishes handshaking, kicks off syncing
      void new_peer_just_added(const peer_connection_ptr& peer);

      void close();

      void accept_connection_task(peer_connection_ptr new_peer);
      void accept_loop();
      void send_hello_message(const peer_connection_ptr& peer);
      void connect_to_task(peer_connection_ptr new_peer, const fc::ip::endpoint& remote_endpoint);
      bool is_connected_to_endpoint(const fc::ip::endpoint& remote_endpoint) const;

      void move_peer_to_active_list(const peer_connection_ptr& peer);
      void move_peer_to_closing_list(const peer_connection_ptr& peer);
      void move_peer_to_terminating_list(const peer_connection_ptr& peer);

      /***
       * Look for an active connection at the given address
       * @param remote_endpoint the address we are interested in
       * @returns the connection, or peer_connection_ptr() if not found
       */
      peer_connection_ptr get_active_conn_for_endpoint( const fc::ip::endpoint& remote_endpoint ) const;
      /***
       * Look for a connection that is either active or currently in the handshaking process
       * @param remote_endpoint the address we are interested in
       * @returns the connection, or peer_connection_ptr() if not found
       */
      peer_connection_ptr get_connection_for_endpoint( const fc::ip::endpoint& remote_endpoint ) const;

      void dump_node_status();

      void delayed_peer_deletion_task();
      void schedule_peer_for_deletion(const peer_connection_ptr& peer_to_delete);

      void disconnect_from_peer( peer_connection* originating_peer,
                               const std::string& reason_for_disconnect,
                                bool caused_by_error = false,
                               const fc::oexception& additional_data = fc::oexception() );

      // methods implementing node's public interface
      void set_node_delegate(std::shared_ptr<node_delegate> del, fc::thread* thread_for_delegate_calls);
      void load_configuration( const fc::path& configuration_directory );
      void listen_to_p2p_network();
      void connect_to_p2p_network(node_impl_ptr self);
      void add_node( const fc::ip::endpoint& ep );
      void set_advertise_algorithm( const std::string& algo,
            const std::vector<std::string>& advertise_or_exclude_list );
      void add_seed_node( const std::string& seed_string );
      void resolve_seed_node_and_add( const std::string& seed_string );
      void initiate_connect_to(const peer_connection_ptr& peer);
      void connect_to_endpoint(const fc::ip::endpoint& ep);
      void set_listen_endpoint(const fc::ip::endpoint& ep , bool wait_if_not_available);
      void set_inbound_endpoint( const fc::ip::endpoint& ep );
      void set_accept_incoming_connections(bool accept);
      void set_connect_to_new_peers( bool connect );

      fc::ip::endpoint         get_actual_listening_endpoint() const;
      std::vector<peer_status> get_connected_peers() const;
      uint32_t                 get_connection_count() const;

      void broadcast(const message& item_to_broadcast, const message_propagation_data& propagation_data);
      void broadcast(const message& item_to_broadcast);
      void sync_from(const item_id& current_head_block, const std::vector<uint32_t>& hard_fork_block_numbers);
      bool is_connected() const;
      std::vector<potential_peer_record> get_potential_peers() const;
      void set_advanced_node_parameters( const fc::variant_object& params );

      fc::variant_object         get_advanced_node_parameters();
      message_propagation_data   get_tx_propagation_data(
                                       const graphene::net::transaction_id_type& transaction_id ) const;
      message_propagation_data   get_block_propagation_data( const graphene::net::block_id_type& block_id ) const;

      node_id_t                  get_node_id() const;
      void                       set_allowed_peers( const std::vector<node_id_t>& allowed_peers );
      void                       clear_peer_database();
      void                       set_total_bandwidth_limit( uint32_t upload_bytes_per_second,
                                                            uint32_t download_bytes_per_second );
      fc::variant_object         get_call_statistics() const;
      graphene::net::message     get_message_for_item(const item_id& item) override;

      fc::variant_object         network_get_info() const;
      fc::variant_object         network_get_usage_stats() const;

      bool is_hard_fork_block(uint32_t block_number) const;
      uint32_t get_next_known_hard_fork_block_number(uint32_t block_number) const;
    }; // end class node_impl

    struct node_impl_deleter
    {
      void operator()(node_impl*);
    };

}}} // end of namespace graphene::net::detail

FC_REFLECT( graphene::net::detail::node_configuration,
            (listen_endpoint)
            (inbound_endpoint)
            (accept_incoming_connections)
            (connect_to_new_peers)
            (wait_if_endpoint_is_busy)
            (private_key) )
