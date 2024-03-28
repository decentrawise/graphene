#pragma once

#include <graphene/protocol/operations.hpp>

namespace graphene { namespace protocol {

   /**
    * @defgroup transactions Transactions
    *
    * All transactions are sets of operations that must be applied atomically. Transactions must refer to a recent
    * block that defines the context of the operation so that they assert a known binding to the object id's referenced
    * in the transaction.
    *
    * Rather than specify a full block number, we only specify the lower 16 bits of the block number which means you
    * can reference any block within the last 65,536 blocks which is 3.5 days with a 5 second block interval or 18
    * hours with a 1 second interval.
    *
    * All transactions must expire so that the network does not have to maintain a permanent record of all transactions
    * ever published.  A transaction may not have an expiration date too far in the future because this would require
    * keeping too much transaction history in memory.
    *
    * The block prefix is the first 4 bytes of the block hash of the reference block number, which is the second 4
    * bytes of the @ref block_id_type (the first 4 bytes of the block ID are the block number)
    *
    * Note: A transaction which selects a reference block cannot be migrated between forks outside the period of
    * ref_block_num.time to (ref_block_num.time + rel_exp * interval). This fact can be used to protect market orders
    * which should specify a relatively short re-org window of perhaps less than 1 minute. Normal payments should
    * probably have a longer re-org window to ensure their transaction can still go through in the event of a momentary
    * disruption in service.
    *
    * @note It is not recommended to set the @ref ref_block_num, @ref ref_block_prefix, and @ref expiration
    * fields manually. Call the appropriate overload of @ref set_expiration instead.
    *
    * @{
    */

   /**
    *  @brief groups operations that should be applied atomically
    */
   class transaction
   {
   public:
      virtual ~transaction() = default;
      /**
       * Least significant 16 bits from the reference block number. If @ref relative_expiration is zero, this field
       * must be zero as well.
       */
      uint16_t           ref_block_num    = 0;
      /**
       * The first non-block-number 32-bits of the reference block ID. Recall that block IDs have 32 bits of block
       * number followed by the actual block hash, so this field should be set using the second 32 bits in the
       * @ref block_id_type
       */
      uint32_t           ref_block_prefix = 0;

      /**
       * This field specifies the absolute expiration for this transaction.
       */
      fc::time_point_sec expiration;

      vector<operation>  operations;
      
      extensions_type    extensions;

      /// Calculate the digest for a transaction
      digest_type                        digest()const;
      virtual const transaction_id_type& id()const;
      virtual void                       validate() const;

      void set_expiration( fc::time_point_sec expiration_time );
      void set_reference_block( const block_id_type& reference_block );

      /// visit all operations
      template<typename Visitor>
      vector<typename Visitor::result_type> visit( Visitor&& visitor )
      {
         vector<typename Visitor::result_type> results;
         for( auto& op : operations )
            results.push_back(op.visit( std::forward<Visitor>( visitor ) ));
         return results;
      }
      template<typename Visitor>
      vector<typename Visitor::result_type> visit( Visitor&& visitor )const
      {
         vector<typename Visitor::result_type> results;
         for( auto& op : operations )
            results.push_back(op.visit( std::forward<Visitor>( visitor ) ));
         return results;
      }

      void get_required_authorities( flat_set<account_id_type>& active,
                                     flat_set<account_id_type>& owner,
                                     vector<authority>& other )const;

      virtual uint64_t get_packed_size()const;

   protected:
      // Calculate the digest used for signature validation
      digest_type sig_digest( const chain_id_type& chain_id )const;
      mutable transaction_id_type _tx_id_buffer;
   };

   /**
    *  @brief adds a signature to a transaction
    */
   class signed_transaction : public transaction
   {
   public:
      signed_transaction( const transaction& trx = transaction() )
         : transaction(trx){}
      virtual ~signed_transaction() = default;

      /** signs and appends to signatures */
      const signature_type& sign( const private_key_type& key, const chain_id_type& chain_id );

      /** returns signature but does not append */
      signature_type sign( const private_key_type& key, const chain_id_type& chain_id )const;

      /**
       *  The purpose of this method is to identify some subset of
       *  @ref available_keys that will produce sufficient signatures
       *  for a transaction.  The result is not always a minimal set of
       *  signatures, but any non-minimal result will still pass
       *  validation.
       */
      set<public_key_type> get_required_signatures(
         const chain_id_type& chain_id,
         const flat_set<public_key_type>& available_keys,
         const std::function<const authority*(account_id_type)>& get_active,
         const std::function<const authority*(account_id_type)>& get_owner,
         uint32_t max_recursion = GRAPHENE_MAX_SIG_CHECK_DEPTH
         )const;

      /**
       * Checks whether signatures in this signed transaction are sufficient to authorize the transaction.
       *   Throws an exception when failed.
       *
       * @param chain_id the ID of a block chain
       * @param get_active callback function to retrieve active authorities of a given account
       * @param get_owner  callback function to retrieve owner authorities of a given account
       * @param max_recursion maximum level of recursion when verifying, since an account
       *            can have another account in active authorities and/or owner authorities
       */
      void verify_authority(
         const chain_id_type& chain_id,
         const std::function<const authority*(account_id_type)>& get_active,
         const std::function<const authority*(account_id_type)>& get_owner,
         uint32_t max_recursion = GRAPHENE_MAX_SIG_CHECK_DEPTH )const;

      /**
       * This is a slower replacement for get_required_signatures()
       * which returns a minimal set in all cases, including
       * some cases where get_required_signatures() returns a
       * non-minimal set.
       */
      set<public_key_type> minimize_required_signatures(
         const chain_id_type& chain_id,
         const flat_set<public_key_type>& available_keys,
         const std::function<const authority*(account_id_type)>& get_active,
         const std::function<const authority*(account_id_type)>& get_owner,
         uint32_t max_recursion = GRAPHENE_MAX_SIG_CHECK_DEPTH
         ) const;

      /**
       * @brief Extract public keys from signatures with given chain ID.
       * @param chain_id A chain ID
       * @return Public keys
       * @note If @ref signees is empty, E.G. when it's the first time calling
       *       this function for the signed transaction, public keys will be
       *       extracted with given chain ID, and be stored into the mutable
       *       @ref signees field, then @ref signees will be returned;
       *       otherwise, the @ref chain_id parameter will be ignored, and
       *       @ref signees will be returned directly.
       */
      virtual const flat_set<public_key_type>& get_signature_keys( const chain_id_type& chain_id )const;

      /** Signatures */
      vector<signature_type> signatures;

      /** Removes all operations and signatures */
      void clear() { operations.clear(); signatures.clear(); }

      /** Removes all signatures */
      void clear_signatures() { signatures.clear(); }
   protected:
      /** Public keys extracted from signatures */
      mutable flat_set<public_key_type> _signees;
   };

   /** This represents a signed transaction that will never have its operations,
    *  signatures etc. modified again, after initial creation. It is therefore
    *  safe to cache results from various calls.
    */
   class precomputable_transaction : public signed_transaction {
   public:
      precomputable_transaction() {}
      precomputable_transaction( const signed_transaction& tx ) : signed_transaction(tx) {};
      precomputable_transaction( signed_transaction&& tx ) : signed_transaction( std::move(tx) ) {};
      virtual ~precomputable_transaction() = default;

      virtual const transaction_id_type&       id()const override;
      virtual void                             validate()const override;
      virtual const flat_set<public_key_type>& get_signature_keys( const chain_id_type& chain_id )const override;
      virtual uint64_t                         get_packed_size()const override;
   protected:
      mutable bool _validated = false;
      mutable uint64_t _packed_size = 0;
   };

   /**
    * Checks whether given public keys and approvals are sufficient to authorize given operations.
    *   Throws an exception when failed.
    *
    * @param ops a vector of operations
    * @param sigs a set of public keys
    * @param get_active callback function to retrieve active authorities of a given account
    * @param get_owner  callback function to retrieve owner authorities of a given account
    * @param max_recursion maximum level of recursion when verifying, since an account
    *            can have another account in active authorities and/or owner authorities
    * @param allow_committee whether to allow the special "committee account" to authorize the operations
    * @param active_approvals accounts that approved the operations with their active authories
    * @param owner_approvals accounts that approved the operations with their owner authories
    */
   void verify_authority( const vector<operation>& ops, const flat_set<public_key_type>& sigs,
                          const std::function<const authority*(account_id_type)>& get_active,
                          const std::function<const authority*(account_id_type)>& get_owner,
                          uint32_t max_recursion = GRAPHENE_MAX_SIG_CHECK_DEPTH,
                          bool allow_committe = false,
                          const flat_set<account_id_type>& active_aprovals = flat_set<account_id_type>(),
                          const flat_set<account_id_type>& owner_approvals = flat_set<account_id_type>());

   /**
    *  @brief captures the result of evaluating the operations contained in the transaction
    *
    *  When processing a transaction some operations generate
    *  new object IDs and these IDs cannot be known until the
    *  transaction is actually included into a block.  When a
    *  block is produced these new ids are captured and included
    *  with every transaction.  The index in operation_results should
    *  correspond to the same index in operations.
    *
    *  If an operation did not create any new object IDs then 0
    *  should be returned.
    */
   struct processed_transaction : public precomputable_transaction
   {
      processed_transaction( const signed_transaction& trx = signed_transaction() )
         : precomputable_transaction(trx){}
      virtual ~processed_transaction() = default;

      vector<operation_result> operation_results;

      digest_type merkle_digest()const;
   };

   /// @} transactions group

} } // graphene::protocol

FC_REFLECT( graphene::protocol::transaction, (ref_block_num)(ref_block_prefix)(expiration)(operations)(extensions) )
// Note: not reflecting signees field for backward compatibility; in addition, it should not be in p2p messages
FC_REFLECT_DERIVED( graphene::protocol::signed_transaction, (graphene::protocol::transaction), (signatures) )
FC_REFLECT_DERIVED( graphene::protocol::precomputable_transaction, (graphene::protocol::signed_transaction), )
FC_REFLECT_DERIVED( graphene::protocol::processed_transaction, (graphene::protocol::precomputable_transaction), (operation_results) )

GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::transaction)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::signed_transaction)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::precomputable_transaction)
GRAPHENE_DECLARE_EXTERNAL_SERIALIZATION( graphene::protocol::processed_transaction)
