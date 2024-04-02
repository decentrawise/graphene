#pragma once

#include <graphene/protocol/fee_schedule.hpp>

#include <graphene/chain/global_property_object.hpp>
#include <graphene/chain/node_property_object.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/fork_database.hpp>
#include <graphene/chain/block_database.hpp>
#include <graphene/chain/genesis_state.hpp>
#include <graphene/chain/evaluator.hpp>

#include <graphene/db/object_database.hpp>
#include <graphene/db/object.hpp>
#include <graphene/db/simple_index.hpp>
#include <fc/signals.hpp>

#include <fc/log/logger.hpp>

#include <map>

namespace graphene { namespace chain {
   using graphene::db::abstract_object;
   using graphene::db::object;
   class op_evaluator;
   class transaction_evaluation_state;
   class proposal_object;
   class operation_history_object;
   class chain_property_object;
   class validator_schedule_object;
   class validator_object;
   class force_settlement_object;
   class limit_order_object;
   class collateral_bid_object;
   class call_order_object;

   struct budget_record;
   enum class vesting_balance_type;

   /**
    *   @class database
    *   @brief tracks the blockchain state in an extensible manner
    */
   class database : public db::object_database
   {
         //////////////////// db_management.cpp ////////////////////
      public:

         database();
         ~database() override;

         enum validation_steps
         {
            skip_nothing                = 0,
            skip_validator_signature      = 1 << 0,  ///< used while reindexing
            skip_transaction_signatures = 1 << 1,  ///< used by non-validator nodes
            skip_transaction_dupe_check = 1 << 2,  ///< used while reindexing
            skip_block_size_check       = 1 << 4,  ///< used when applying locally generated transactions
            skip_tapos_check            = 1 << 5,  ///< used while reindexing -- note this skips expiration check too
            // skip_authority_check        = 1 << 6,  // removed, effectively identical to skip_transaction_signatures
            skip_merkle_check           = 1 << 7,  ///< used while reindexing
            skip_assert_evaluation      = 1 << 8,  ///< used while reindexing
            skip_undo_history_check     = 1 << 9,  ///< used while reindexing
            skip_validator_schedule_check = 1 << 10 ///< used while reindexing
         };

         /**
          * @brief Open a database, creating a new one if necessary
          *
          * Opens a database in the specified directory. If no initialized database is found, genesis_loader is called
          * and its return value is used as the genesis state when initializing the new database
          *
          * genesis_loader will not be called if an existing database is found.
          *
          * @param data_dir Path to open or create database in
          * @param genesis_loader A callable object which returns the genesis state to initialize new databases on
          * @param db_version a version string that changes when the internal database format and/or logic is modified
          */
          void open(
             const fc::path& data_dir,
             std::function<genesis_state_type()> genesis_loader,
             const std::string& db_version );

         /**
          * @brief Rebuild object graph from block history and open database
          * @param data_dir the path to store the database
          *
          * This method may be called after or instead of @ref database::open, and will rebuild the object graph by
          * replaying blockchain history. When this method exits successfully, the database will be open.
          */
         void reindex(fc::path data_dir);

         /**
          * @brief wipe Delete database from disk, and potentially the raw chain as well.
          * @param data_dir the path to store the database
          * @param include_blocks If true, delete the raw chain as well as the database.
          *
          * Will close the database before wiping. Database will be closed when this function returns.
          */
         void wipe(const fc::path& data_dir, bool include_blocks);
         void close(bool rewind = true);

         //////////////////// db_validator_schedule.cpp ////////////////////

         /**
          * @brief Get the validator scheduled for block production in a slot.
          *
          * slot_num always corresponds to a time in the future.
          *
          * If slot_num == 1, returns the next scheduled validator.
          * If slot_num == 2, returns the next scheduled validator after
          * 1 block gap.
          *
          * Use the get_slot_time() and get_slot_at_time() functions
          * to convert between slot_num and timestamp.
          *
          * Passing slot_num == 0 returns GRAPHENE_NULL_VALIDATOR
          */
         validator_id_type get_scheduled_validator(uint32_t slot_num)const;

         /**
          * Get the time at which the given slot occurs.
          *
          * If slot_num == 0, return time_point_sec().
          *
          * If slot_num == N for N > 0, return the Nth next
          * block-interval-aligned time greater than head_block_time().
          */
         fc::time_point_sec get_slot_time(uint32_t slot_num)const;

         /**
          * Get the last slot which occurs AT or BEFORE the given time.
          *
          * The return value is the greatest value N such that
          * get_slot_time( N ) <= when.
          *
          * If no such N exists, return 0.
          */
         uint32_t get_slot_at_time(fc::time_point_sec when)const;

         /**
          *  Calculate the percent of block production slots that were missed in the
          *  past 128 blocks, not including the current block.
          */
         uint32_t validator_participation_rate()const;

      private:
         uint32_t update_validator_missed_blocks( const signed_block& b );

         void update_validator_schedule();

         //////////////////// db_getter.cpp ////////////////////

      public:
         const chain_id_type&                   get_chain_id()const;
         const asset_object&                    get_core_asset()const;
         const asset_dynamic_data_object&       get_core_dynamic_data()const;
         const chain_property_object&           get_chain_properties()const;
         const global_property_object&          get_global_properties()const;
         const dynamic_global_property_object&  get_dynamic_global_properties()const;
         const node_property_object&            get_node_properties()const;
         const fee_schedule&                    current_fee_schedule()const;
         const account_statistics_object&       get_account_stats_by_owner( account_id_type owner )const;
         const validator_schedule_object&         get_validator_schedule_object()const;

         time_point_sec   head_block_time()const;
         uint32_t         head_block_num()const;
         block_id_type    head_block_id()const;
         validator_id_type  head_block_validator()const;

         decltype( chain_parameters::block_interval ) block_interval( )const;

         node_property_object& node_properties();

         uint32_t last_non_undoable_block_num() const;

         //////////////////// db_init.cpp ////////////////////
         ///@{

         /// Reset the object graph in-memory
         void initialize_indexes(); // Mark as public since it is used in tests
      private:
         void initialize_evaluators();
         void init_genesis(const genesis_state_type& genesis_state = genesis_state_type());

         template<typename EvaluatorType>
         void register_evaluator()
         {
            const auto op_type = operation::tag<typename EvaluatorType::operation_type>::value;
            FC_ASSERT( op_type >= 0, "Negative operation type" );
            FC_ASSERT( op_type < _operation_evaluators.size(),
                       "The operation type (${a}) must be smaller than the size of _operation_evaluators (${b})",
                       ("a", op_type)("b", _operation_evaluators.size()) );
            _operation_evaluators[op_type] = std::make_unique<op_evaluator_impl<EvaluatorType>>();
         }
         ///@}

         //////////////////// db_balance.cpp ////////////////////

      public:
         /**
          * @brief Retrieve a particular account's balance in a given asset
          * @param owner Account whose balance should be retrieved
          * @param asset_id ID of the asset to get balance in
          * @return owner's balance in asset
          */
         asset get_balance(account_id_type owner, asset_id_type asset_id)const;
         /// This is an overloaded method.
         asset get_balance(const account_object& owner, const asset_object& asset_obj)const;

         /**
          * @brief Adjust a particular account's balance in a given asset by a delta
          * @param account ID of account whose balance should be adjusted
          * @param delta Asset ID and amount to adjust balance by
          */
         void adjust_balance(account_id_type account, asset delta);

         void deposit_market_fee_vesting_balance(const account_id_type &account_id, const asset &delta);
         /**
          * @brief Retrieve a particular account's market fee vesting balance in a given asset
          * @param account_id Account whose balance should be retrieved
          * @param asset_id ID of the asset to get balance in
          * @return owner's balance in asset
          */
         asset get_market_fee_vesting_balance(const account_id_type &account_id, const asset_id_type &asset_id);

         /**
          * @brief Helper to make lazy deposit to CDD VBO.
          *
          * If the given optional VBID is not valid(),
          * or it does not have a CDD vesting policy,
          * or the owner / vesting_seconds of the policy
          * does not match the parameter, then credit amount
          * to newly created VBID and return it.
          *
          * Otherwise, credit amount to ovbid.
          * 
          * @return ID of newly created VBO, but only if VBO was created.
          */
         optional< vesting_balance_id_type > deposit_lazy_vesting(
            const optional< vesting_balance_id_type >& ovbid,
            share_type amount,
            uint32_t req_vesting_seconds,
            vesting_balance_type balance_type,
            account_id_type req_owner,
            bool require_vesting );

         /// helper to handle cashback rewards
         void deposit_cashback(const account_object& acct, share_type amount, bool require_vesting = true);
         /// helper to handle validator pay
         void deposit_validator_pay(const validator_object& wit, share_type amount);

         string to_pretty_string( const asset& a )const;

         //////////////////// db_debug.cpp ////////////////////

         void debug_dump();
         void apply_debug_updates();
         void debug_update( const fc::variant_object& update );

         //////////////////// db_market.cpp ////////////////////

         /// @ingroup Market Helpers
         /// @{

         /// Globally settle @p bitasset at @p settle_price, let margin calls pay a premium and margin call fee if
         /// @p check_margin_calls is @c true (in this case others would be closed not at @p settle_price but at a
         /// price better for their owners).
         void globally_settle_asset(const asset_object &bitasset, const price &settle_price);
         void cancel_settle_order(const force_settlement_object& order, bool create_virtual_op = true);
         void cancel_limit_order(const limit_order_object& order, bool create_virtual_op = true);
         void revive_bitasset( const asset_object& bitasset );
         void cancel_bid(const collateral_bid_object& bid, bool create_virtual_op = true);
         void execute_bid( const collateral_bid_object& bid, share_type debt_covered, share_type collateral_from_fund, const price_feed& current_feed );

      private:
         void _cancel_bids_and_revive_mpa(const asset_object &bitasset, const asset_bitasset_data_object &bad);
         bool check_for_blackswan(const asset_object &mia, bool enable_black_swan = true,
                                  const asset_bitasset_data_object *bitasset_ptr = nullptr);
      public:
         /**
          * @brief Process a new limit order through the markets
          * @param new_order_object The new order to process
          * @return true if order was completely filled; false otherwise
          *
          * This function takes a new limit order, and runs the markets attempting to match it with existing orders
          * already on the books.
          */
         bool apply_order(const limit_order_object& new_order_object, bool allow_black_swan = true);

         bool check_call_orders(const asset_object &mia, bool enable_black_swan = true,
                                const asset_bitasset_data_object *bitasset_ptr = nullptr);

         // Note: Ideally this should be private.
         //       Now it is public because we use it in a non-member function in db_market.cpp .
         enum class match_result_type
         {
            none_filled = 0,
            only_taker_filled = 1,
            only_maker_filled = 2,
            both_filled = 3
         };

      private:
         /**
          * Matches the two orders, the first parameter is taker, the second is maker.
          *
          * @return a bit field indicating which orders were filled (and thus removed)
          */
         ///@{
         match_result_type match( const limit_order_object& taker, const limit_order_object& maker, const price& trade_price );
         match_result_type match( const limit_order_object& taker, const call_order_object& maker, const price& trade_price,
                                  const price& feed_price, const uint16_t maintenance_collateral_ratio,
                                  const optional<price>& maintenance_collateralization );
         ///@}

         /// Matches the two orders, the first parameter is taker, the second is maker.
         /// @return the amount of asset settled
         asset match(const call_order_object& call,
                   const force_settlement_object& settle,
                   const price& match_price,
                   asset max_settlement,
                   const price& fill_price);

         /**
          * @brief fills limit order
          * @param order the order
          * @param pays what the account is paying
          * @param receives what the account is receiving
          * @param cull_if_small take care of dust
          * @param fill_price the transaction price
          * @param is_maker TRUE if this order is maker, FALSE if taker
          * @return true if the order was completely filled and thus freed.
          */
         bool fill_limit_order(const limit_order_object &order, const asset &pays, const asset &receives, bool cull_if_small,
                               const price &fill_price, const bool is_maker);
         /***
          * @brief attempt to fill a call order
          * @param order the order
          * @param pays what the buyer pays for the collateral
          * @param receives the collateral received by the buyer
          * @param fill_price the price the transaction executed at
          * @param is_maker TRUE if the buyer is the maker, FALSE if the buyer is the taker
          * @param margin_fee Margin call fees paid in collateral asset
          * @param reduce_current_supply Whether to reduce current supply of the asset. Usually it is true.
          *                              When globally settleing or individually settling it is false.
          * @returns TRUE if the order was completely filled
          */
         bool fill_call_order(const call_order_object &order, const asset &pays, const asset &receives,
                              const price &fill_price, const bool is_maker);

         bool fill_settle_order( const force_settlement_object& settle, const asset& pays, const asset& receives,
                                 const price& fill_price, const bool is_maker );

         /// helpers to fill_order
         /// @{
         void pay_order( const account_object& receiver, const asset& receives, const asset& pays );

      public:
         /**
          * @brief Calculate the market fee that is to be taken
          * @param trade_asset the asset (passed in to avoid a lookup)
          * @param trade_amount the quantity that the fee calculation is based upon
          * @param is_maker TRUE if this is the fee for a maker, FALSE if taker
          */
         asset calculate_market_fee( const asset_object &recv_asset, const asset &trade_amount );
         /// @brief Pay market fees to asset owner
         /// @param recv_asset   the asset (passed in to avoid lookup)
         /// @param receives     the trade size
         /// @return             the fees paid
         asset pay_market_fees( const asset_object& recv_asset, const asset& receives );
         /// @brief Pay market fees to asset owner and rewards to referrer program
         /// @param seller       the account to check for referral program
         /// @param recv_asset   the asset (passed in to avoid lookup)
         /// @param receives     the trade size
         /// @return             the fees paid
         asset pay_market_fees( const account_object& seller, const asset_object& recv_asset, const asset& receives );
         /// @}

         //////////////////// db_block.cpp ////////////////////

         /**
          *  @return true if the block is in our fork DB or saved to disk as
          *  part of the official chain, otherwise return false
          */
         bool                       is_known_block( const block_id_type& id )const;
         bool                       is_known_transaction( const transaction_id_type& id )const;
         block_id_type              get_block_id_for_num( uint32_t block_num )const;
         optional<signed_block>     fetch_block_by_id( const block_id_type& id )const;
         optional<signed_block>     fetch_block_by_number( uint32_t num )const;
         const signed_transaction&  get_recent_transaction( const transaction_id_type& trx_id )const;
         std::vector<block_id_type> get_block_ids_on_fork(block_id_type head_of_fork) const;

         void                       add_checkpoints( const flat_map<uint32_t,block_id_type>& checkpts );
         const flat_map<uint32_t,block_id_type> get_checkpoints()const { return _checkpoints; }
         bool before_last_checkpoint()const;

         bool push_block( const signed_block& b, uint32_t skip = skip_nothing );
         processed_transaction push_transaction( const precomputable_transaction& trx, uint32_t skip = skip_nothing );
      private:
         bool _push_block( const signed_block& b );
      public:
         // It is public because it is used in pending_transactions_restorer in db_with.hpp
         processed_transaction _push_transaction( const precomputable_transaction& trx );
         ///@throws fc::exception if the proposed transaction fails to apply.
         processed_transaction push_proposal( const proposal_object& proposal );

         signed_block generate_block(
            const fc::time_point_sec when,
            validator_id_type validator_id,
            const fc::ecc::private_key& block_signing_private_key,
            uint32_t skip
            );
      private:
         signed_block _generate_block(
            const fc::time_point_sec when,
            validator_id_type validator_id,
            const fc::ecc::private_key& block_signing_private_key
            );

      public:
         void pop_block();
         void clear_pending();

         /**
          *  This method is used to track applied operations during the evaluation of a block, these
          *  operations should include any operation actually included in a transaction as well
          *  as any implied/virtual operations that resulted, such as filling an order.  The
          *  applied operations is cleared after applying each block and calling the block
          *  observers which may want to index these operations.
          *  @param op The operation to push
          *  @param is_virtual Whether the operation is a virtual operation
          *
          *  @return the op_id which can be used to set the result after it has finished being applied.
          */
         uint32_t  push_applied_operation( const operation& op, bool is_virtual = true );
         void      set_applied_operation_result( uint32_t op_id, const operation_result& r );
         const vector<optional< operation_history_object > >& get_applied_operations()const;

         /**
          *  This signal is emitted after all operations and virtual operation for a
          *  block have been applied but before the get_applied_operations() are cleared.
          *
          *  You may not yield from this callback because the blockchain is holding
          *  the write lock and may be in an "inconstant state" until after it is
          *  released.
          */
         fc::signal<void(const signed_block&)>           applied_block;

         /**
          * This signal is emitted any time a new transaction is added to the pending
          * block state.
          */
         fc::signal<void(const signed_transaction&)>     on_pending_transaction;

         /**
          *  Emitted After a block has been applied and committed.  The callback
          *  should not yield and should execute quickly.
          */
         fc::signal<void(const vector<object_id_type>&, const flat_set<account_id_type>&)> new_objects;

         /**
          *  Emitted After a block has been applied and committed.  The callback
          *  should not yield and should execute quickly.
          */
         fc::signal<void(const vector<object_id_type>&, const flat_set<account_id_type>&)> changed_objects;

         /** this signal is emitted any time an object is removed and contains a
          * pointer to the last value of every object that was removed.
          */
         fc::signal<void(const vector<object_id_type>&, const vector<const object*>&, const flat_set<account_id_type>&)>  removed_objects;


         ///@{
         /**
          *  This method validates transactions without adding it to the pending state.
          *  @return true if the transaction would validate
          */
         processed_transaction validate_transaction( const signed_transaction& trx );


         /** when popping a block, the transactions that were removed get cached here so they
          * can be reapplied at the proper time */
         std::deque< precomputable_transaction > _popped_tx;

         /**
          * @}
          */

         /** Precomputes digests, signatures and operation validations depending
          *  on skip flags. "Expensive" computations may be done in a parallel
          *  thread.
          *
          * @param block the block to preprocess
          * @param skip indicates which computations can be skipped
          * @return a future that will resolve to the input block with
          *         precomputations applied
          */
         fc::future<void> precompute_parallel( const signed_block& block, const uint32_t skip = skip_nothing )const;

         /** Precomputes digests, signatures and operation validations.
          *  "Expensive" computations may be done in a parallel thread.
          *
          * @param trx the transaction to preprocess
          * @return a future that will resolve to the input transaction with
          *         precomputations applied
          */
         fc::future<void> precompute_parallel( const precomputable_transaction& trx )const;
      private:
         template<typename Trx>
         void _precompute_parallel( const Trx* trx, const size_t count, const uint32_t skip )const;

      protected:
         // Mark pop_undo() as protected -- we do not want outside calling pop_undo(),
         // it should call pop_block() instead
         void pop_undo() { object_database::pop_undo(); }

      private:
         optional<undo_database::session>       _pending_tx_session;
         vector< unique_ptr<op_evaluator> >     _operation_evaluators;

         template<class Index>
         vector<std::reference_wrapper<const typename Index::object_type>> sort_votable_objects(size_t count)const;

      public:
         // these were formerly private, but they have a fairly well-defined API, so let's make them public
         void                  apply_block( const signed_block& next_block, uint32_t skip = skip_nothing );
         processed_transaction apply_transaction( const signed_transaction& trx, uint32_t skip = skip_nothing );
         operation_result      apply_operation( transaction_evaluation_state& eval_state, const operation& op,
                                                bool is_virtual = true );

      private:
         void                  _apply_block( const signed_block& next_block );
         processed_transaction _apply_transaction( const signed_transaction& trx );

         ///Steps involved in applying a new block
         ///@{

         const validator_object& validate_block_header( uint32_t skip, const signed_block& next_block )const;
         const validator_object& _validate_block_header( const signed_block& next_block )const;
         void verify_signing_validator( const signed_block& new_block, const fork_item& fork_entry )const;
         void update_validators( fork_item& fork_entry )const;
         void create_block_summary(const signed_block& next_block);

         //////////////////// db_notify.cpp ////////////////////

      protected:
         void notify_applied_block(const signed_block &block);
         void notify_on_pending_transaction(const signed_transaction &tx);
         void notify_changed_objects();

         //////////////////// db_update.cpp ////////////////////
      private:
         void update_global_dynamic_data( const signed_block& b, const uint32_t missed_blocks );
         void update_signing_validator(const validator_object& signing_validator, const signed_block& new_block);
         void update_last_irreversible_block();
         void clear_expired_transactions();
         void clear_expired_proposals();
         void clear_expired_orders();
         void clear_expired_force_settlements();
         void update_expired_feeds();
         void update_core_exchange_rates();
         void update_maintenance_flag( bool new_maintenance_flag );
         void update_withdraw_permissions();
         void clear_expired_htlcs();

         ///Steps performed only at maintenance intervals
         ///@{

         //////////////////// db_maint.cpp ////////////////////

         void initialize_budget_record( fc::time_point_sec now, budget_record& rec )const;
         void process_budget();
         void pay_workers( share_type& budget );
         void perform_chain_maintenance(const signed_block& next_block);
         void update_block_producers();
         void update_active_delegates();
         void update_worker_votes();
         void process_bids( const asset_bitasset_data_object& bad );
         void process_bitassets();

         template<class Type>
         void perform_account_maintenance( Type tally_helper );
         ///@}
         ///@}

         vector< processed_transaction >        _pending_tx;
         fork_database                          _fork_db;

         /**
          *  Note: we can probably store blocks by block num rather than
          *  block id because after the undo window is past the block ID
          *  is no longer relevant and its number is irreversible.
          *
          *  During the "fork window" we can cache blocks in memory
          *  until the fork is resolved.  This should make maintaining
          *  the fork tree relatively simple.
          */
         block_database   _block_id_to_block;

         /**
          * Contains the set of ops that are in the process of being applied from
          * the current block.  It contains real and virtual operations in the
          * order they occur and is cleared after the applied_block signal is
          * emitted.
          */
         vector<optional<operation_history_object> >  _applied_ops;

      public:
         fc::time_point_sec                _current_block_time;
         uint32_t                          _current_block_num    = 0;
      private:
         uint16_t                          _current_trx_in_block = 0;
         uint16_t                          _current_op_in_trx    = 0;
         uint32_t                          _current_virtual_op   = 0;

         vector<uint64_t>                  _vote_tally_buffer;
         vector<uint64_t>                  _validator_count_histogram_buffer;
         vector<uint64_t>                  _council_count_histogram_buffer;
         uint64_t                          _total_voting_stake;

         flat_map<uint32_t,block_id_type>  _checkpoints;

         node_property_object              _node_property_object;

         /// Whether to update votes of standby validators and delegates when performing chain maintenance.
         /// Set it to true to provide accurate data to API clients, set to false to have better performance.
         bool                              _track_standby_votes = true;

         /**
          * Whether database is successfully opened or not.
          *
          * The database is considered open when there's no exception
          * or assertion fail during database::open() method, and
          * database::close() has not been called, or failed during execution.
          */
         bool                              _opened = false;

         // Counts nested proposal updates
         uint32_t                           _undo_session_nesting_depth = 0;

         /// Pointers to core asset object and global objects who will have immutable addresses after created
         ///@{
         const asset_object*                    _p_core_asset_obj          = nullptr;
         const asset_dynamic_data_object*       _p_core_dynamic_data_obj   = nullptr;
         const global_property_object*          _p_global_prop_obj         = nullptr;
         const dynamic_global_property_object*  _p_dyn_global_prop_obj     = nullptr;
         const chain_property_object*           _p_chain_property_obj      = nullptr;
         const validator_schedule_object*         _p_validator_schedule_obj    = nullptr;
         ///@}

      public:
         /// Enable or disable tracking of votes of standby validators and delegates
         inline void enable_standby_votes_tracking(bool enable)  { _track_standby_votes = enable; }
   };

   namespace detail
   {
       template<int... Is>
       struct seq { };

       template<int N, int... Is>
       struct gen_seq : gen_seq<N - 1, N - 1, Is...> { };

       template<int... Is>
       struct gen_seq<0, Is...> : seq<Is...> { };

       template<typename T, int... Is>
       void for_each(T&& t, const account_object& a, seq<Is...>)
       {
           auto l = { (std::get<Is>(t)(a), 0)... };
           (void)l;
       }
   }

} }
