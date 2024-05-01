#include "wallet_api_impl.hpp"
#include <graphene/utilities/key_conversion.hpp>

/*
 * Methods to handle voting / workers / council
 */

namespace graphene { namespace wallet { namespace detail {

   template<typename WorkerInit>
   static WorkerInit _create_worker_initializer( const variant& worker_settings )
   {
      WorkerInit result;
      from_variant( worker_settings, result, GRAPHENE_MAX_NESTED_OBJECTS );
      return result;
   }

   signed_transaction wallet_api_impl::update_worker_votes(
      string account,
      worker_vote_delta delta,
      bool broadcast
      )
   {
      account_object acct = get_account( account );

      // you could probably use a faster algorithm for this, but flat_set is fast enough :)
      flat_set< worker_id_type > merged;
      merged.reserve( delta.vote_approve.size() + delta.vote_abstain.size() );
      for( const worker_id_type& wid : delta.vote_approve )
      {
         bool inserted = merged.insert( wid ).second;
         FC_ASSERT( inserted, "worker ${wid} specified multiple times", ("wid", wid) );
      }
      for( const worker_id_type& wid : delta.vote_abstain )
      {
         bool inserted = merged.insert( wid ).second;
         FC_ASSERT( inserted, "worker ${wid} specified multiple times", ("wid", wid) );
      }

      // should be enforced by FC_ASSERT's above
      assert( merged.size() == delta.vote_approve.size() + delta.vote_abstain.size() );

      vector< object_id_type > query_ids;
      for( const worker_id_type& wid : merged )
         query_ids.push_back( object_id_type(wid) );

      flat_set<vote_id_type> new_votes( acct.options.votes );

      fc::variants objects = _remote_db->get_objects( query_ids, {} );
      for( const variant& obj : objects )
      {
         worker_object wo;
         worker_id_type wo_id { wo.id };
         from_variant( obj, wo, GRAPHENE_MAX_NESTED_OBJECTS );
         new_votes.erase( wo.vote_id );
         if( delta.vote_approve.find( wo_id ) != delta.vote_approve.end() )
            new_votes.insert( wo.vote_id );
         else
            assert( delta.vote_abstain.find( wo_id ) != delta.vote_abstain.end() );
      }

      account_update_operation update_op;
      update_op.account = acct.id;
      update_op.new_options = acct.options;
      update_op.new_options->votes = new_votes;

      signed_transaction tx;
      tx.operations.push_back( update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees() );
      tx.validate();

      return sign_transaction( tx, broadcast );
   }

   signed_transaction wallet_api_impl::create_delegate(string owner_account, string url,
         bool broadcast )
   { try {

      delegate_create_operation delegate_create_op;
      delegate_create_op.delegate_account = get_account_id(owner_account);
      delegate_create_op.url = url;
      if (_remote_db->get_delegate_by_account(owner_account))
         FC_THROW("Account ${owner_account} is already a delegate", ("owner_account", owner_account));

      signed_transaction tx;
      tx.operations.push_back( delegate_create_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (owner_account)(broadcast) ) }

   validator_object wallet_api_impl::get_validator(string owner_account)
   {
      try
      {
         fc::optional<validator_id_type> validator_id = maybe_id<validator_id_type>(owner_account);
         if (validator_id)
         {
            std::vector<validator_id_type> ids_to_get;
            ids_to_get.push_back(*validator_id);
            std::vector<fc::optional<validator_object>> validator_objects = _remote_db->get_validators(ids_to_get);
            if (validator_objects.front())
               return *validator_objects.front();
            FC_THROW("No validator is registered for id ${id}", ("id", owner_account));
         }
         else
         {
            // then maybe it's the owner account
            try
            {
               auto owner_account_id = std::string(get_account_id(owner_account));
               fc::optional<validator_object> validator = _remote_db->get_validator_by_account(owner_account_id);
               if (validator)
                  return *validator;
               else
                  FC_THROW("No validator is registered for account ${account}", ("account", owner_account));
            }
            catch (const fc::exception&)
            {
               FC_THROW("No account or validator named ${account}", ("account", owner_account));
            }
         }
      }
      FC_CAPTURE_AND_RETHROW( (owner_account) )
   }

   delegate_object wallet_api_impl::get_delegate(string owner_account)
   {
      try
      {
         fc::optional<delegate_id_type> delegate_id =
               maybe_id<delegate_id_type>(owner_account);
         if (delegate_id)
         {
            std::vector<delegate_id_type> ids_to_get;
            ids_to_get.push_back(*delegate_id);
            std::vector<fc::optional<delegate_object>> delegate_objects =
                  _remote_db->get_delegates(ids_to_get);
            if (delegate_objects.front())
               return *delegate_objects.front();
            FC_THROW("No delegate is registered for id ${id}", ("id", owner_account));
         }
         else
         {
            // then maybe it's the owner account
            try
            {
               fc::optional<delegate_object> delegate =
                     _remote_db->get_delegate_by_account(owner_account);
               if (delegate)
                  return *delegate;
               else
                  FC_THROW("No delegate is registered for account ${account}", ("account", owner_account));
            }
            catch (const fc::exception&)
            {
               FC_THROW("No account or delegate named ${account}", ("account", owner_account));
            }
         }
      }
      FC_CAPTURE_AND_RETHROW( (owner_account) )
   }

   signed_transaction wallet_api_impl::create_validator(string owner_account,
         string url, bool broadcast /* = false */)
   { try {
      account_object validator_account = get_account(owner_account);
      fc::ecc::private_key active_private_key = get_private_key_for_account(validator_account);
      int validator_key_index = find_first_unused_derived_key_index(active_private_key);
      fc::ecc::private_key validator_private_key =
            derive_private_key(key_to_wif(active_private_key), validator_key_index);
      graphene::chain::public_key_type validator_public_key = validator_private_key.get_public_key();

      validator_create_operation validator_create_op;
      validator_create_op.validator_account = validator_account.id;
      validator_create_op.block_producer_key = validator_public_key;
      validator_create_op.url = url;

      if (_remote_db->get_validator_by_account(std::string(validator_create_op.validator_account)))
         FC_THROW("Account ${owner_account} is already a validator", ("owner_account", owner_account));

      signed_transaction tx;
      tx.operations.push_back( validator_create_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      _wallet.pending_validator_registrations[owner_account] = key_to_wif(validator_private_key);

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (owner_account)(broadcast) ) }

   signed_transaction wallet_api_impl::update_validator(string validator_name, string url,
         string block_producer_key, bool broadcast )
   { try {
      validator_object validator = get_validator(validator_name);
      account_object validator_account = get_account( validator.validator_account );

      validator_update_operation validator_update_op;
      validator_update_op.validator = validator.id;
      validator_update_op.validator_account = validator_account.id;
      if( url != "" )
         validator_update_op.new_url = url;
      if( block_producer_key != "" )
         validator_update_op.new_block_producer_key = public_key_type( block_producer_key );

      signed_transaction tx;
      tx.operations.push_back( validator_update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees() );
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (validator_name)(url)(block_producer_key)(broadcast) ) }

   signed_transaction wallet_api_impl::create_worker( string owner_account, time_point_sec work_begin_date,
         time_point_sec work_end_date, amount_type daily_pay, string name, string url,
         variant worker_settings, bool broadcast)
   {
      worker_initializer init;
      std::string wtype = worker_settings["type"].get_string();

      // TODO:  Use introspection to do this dispatch
      if( wtype == "burn" )
         init = _create_worker_initializer< burn_worker_initializer >( worker_settings );
      else if( wtype == "refund" )
         init = _create_worker_initializer< refund_worker_initializer >( worker_settings );
      else if( wtype == "vesting" )
         init = _create_worker_initializer< vesting_balance_worker_initializer >( worker_settings );
      else
      {
         FC_ASSERT( false, "unknown worker[\"type\"] value" );
      }

      worker_create_operation op;
      op.owner = get_account( owner_account ).id;
      op.work_begin_date = work_begin_date;
      op.work_end_date = work_end_date;
      op.daily_pay = daily_pay;
      op.name = name;
      op.url = url;
      op.initializer = init;

      signed_transaction tx;
      tx.operations.push_back( op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees() );
      tx.validate();

      return sign_transaction( tx, broadcast );
   }

   signed_transaction wallet_api_impl::vote_for_delegate(string voting_account,
         string delegate, bool approve, bool broadcast )
   { try {
      account_object voting_account_object = get_account(voting_account);
      fc::optional<delegate_object> delegate_obj =
            _remote_db->get_delegate_by_account(delegate);
      if (!delegate_obj)
         FC_THROW("Account ${delegate} is not registered as a delegate",
                  ("delegate", delegate));
      if (approve)
      {
         auto insert_result = voting_account_object.options.votes.insert(delegate_obj->vote_id);
         if (!insert_result.second)
            FC_THROW("Account ${account} was already voting for delegate ${delegate}",
                     ("account", voting_account)("delegate", delegate));
      }
      else
      {
         auto votes_removed = voting_account_object.options.votes.erase(delegate_obj->vote_id);
         if( 0 == votes_removed )
            FC_THROW("Account ${account} is already not voting for delegate ${delegate}",
                     ("account", voting_account)("delegate", delegate));
      }
      account_update_operation account_update_op;
      account_update_op.account = voting_account_object.id;
      account_update_op.new_options = voting_account_object.options;

      signed_transaction tx;
      tx.operations.push_back( account_update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (voting_account)(delegate)(approve)(broadcast) ) }

   signed_transaction wallet_api_impl::vote_for_validator(string voting_account, string validator,
         bool approve, bool broadcast )
   { try {
      account_object voting_account_object = get_account(voting_account);

      fc::optional<validator_object> validator_obj = _remote_db->get_validator_by_account(validator);
      if (!validator_obj)
         FC_THROW("Account ${validator} is not registered as a validator", ("validator", validator));
      if (approve)
      {
         auto insert_result = voting_account_object.options.votes.insert(validator_obj->vote_id);
         if (!insert_result.second)
            FC_THROW("Account ${account} was already voting for validator ${validator}",
                     ("account", voting_account)("validator", validator));
      }
      else
      {
         auto votes_removed = voting_account_object.options.votes.erase(validator_obj->vote_id);
         if( 0 == votes_removed )
            FC_THROW("Account ${account} is already not voting for validator ${validator}",
                     ("account", voting_account)("validator", validator));
      }
      account_update_operation account_update_op;
      account_update_op.account = voting_account_object.id;
      account_update_op.new_options = voting_account_object.options;

      signed_transaction tx;
      tx.operations.push_back( account_update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (voting_account)(validator)(approve)(broadcast) ) }

   signed_transaction wallet_api_impl::set_voting_proxy(string account_to_modify,
         optional<string> voting_account, bool broadcast )
   { try {
      account_object account_object_to_modify = get_account(account_to_modify);
      if (voting_account)
      {
         account_id_type new_voting_account_id = get_account_id(*voting_account);
         if (account_object_to_modify.options.voting_account == new_voting_account_id)
            FC_THROW("Voting proxy for ${account} is already set to ${voter}",
                     ("account", account_to_modify)("voter", *voting_account));
         account_object_to_modify.options.voting_account = new_voting_account_id;
      }
      else
      {
         if (account_object_to_modify.options.voting_account == GRAPHENE_PROXY_TO_SELF_ACCOUNT)
            FC_THROW("Account ${account} is already voting for itself", ("account", account_to_modify));
         account_object_to_modify.options.voting_account = GRAPHENE_PROXY_TO_SELF_ACCOUNT;
      }

      account_update_operation account_update_op;
      account_update_op.account = account_object_to_modify.id;
      account_update_op.new_options = account_object_to_modify.options;

      signed_transaction tx;
      tx.operations.push_back( account_update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (account_to_modify)(voting_account)(broadcast) ) }

   signed_transaction wallet_api_impl::set_desired_validator_and_delegate_count(
         string account_to_modify, uint16_t desired_number_of_validators, uint16_t desired_number_of_delegates,
         bool broadcast )
   { try {
      account_object account_object_to_modify = get_account(account_to_modify);

      if (account_object_to_modify.options.num_producers == desired_number_of_validators &&
          account_object_to_modify.options.num_delegates == desired_number_of_delegates)
         FC_THROW("Account ${account} is already voting for ${validators} validators"
                  " and ${delegates} delegates",
                  ("account", account_to_modify)("validators", desired_number_of_validators)
                  ("delegates",desired_number_of_validators));
      account_object_to_modify.options.num_producers = desired_number_of_validators;
      account_object_to_modify.options.num_delegates = desired_number_of_delegates;

      account_update_operation account_update_op;
      account_update_op.account = account_object_to_modify.id;
      account_update_op.new_options = account_object_to_modify.options;

      signed_transaction tx;
      tx.operations.push_back( account_update_op );
      set_operation_fees( tx, _remote_db->get_global_properties().parameters.get_current_fees());
      tx.validate();

      return sign_transaction( tx, broadcast );
   } FC_CAPTURE_AND_RETHROW( (account_to_modify)(desired_number_of_validators)
                             (desired_number_of_delegates)(broadcast) ) }

   signed_transaction wallet_api_impl::propose_parameter_change( const string& proposing_account,
         fc::time_point_sec expiration_time, const variant_object& changed_values, bool broadcast )
   {
      FC_ASSERT( !changed_values.contains("current_fees") );

      const chain_parameters& current_params = get_global_properties().parameters;
      chain_parameters new_params = current_params;
      fc::reflector<chain_parameters>::visit(
         fc::from_variant_visitor<chain_parameters>( changed_values, new_params, GRAPHENE_MAX_NESTED_OBJECTS )
         );

      delegate_update_global_parameters_operation update_op;
      update_op.new_parameters = new_params;

      proposal_create_operation prop_op;

      prop_op.expiration_time = expiration_time;
      prop_op.review_period_seconds = current_params.council_proposal_review_period;
      prop_op.fee_paying_account = get_account(proposing_account).id;

      prop_op.proposed_ops.emplace_back( update_op );
      current_params.get_current_fees().set_fee( prop_op.proposed_ops.back().op );

      signed_transaction tx;
      tx.operations.push_back(prop_op);
      set_operation_fees(tx, current_params.get_current_fees());
      tx.validate();

      return sign_transaction(tx, broadcast);
   }

   signed_transaction wallet_api_impl::propose_fee_change( const string& proposing_account,
         fc::time_point_sec expiration_time, const variant_object& changed_fees, bool broadcast )
   {
      const chain_parameters& current_params = get_global_properties().parameters;
      const fee_schedule_type& current_fees = current_params.get_current_fees();

      flat_map< int, fee_parameters > fee_map;
      fee_map.reserve( current_fees.parameters.size() );
      for( const fee_parameters& op_fee : current_fees.parameters )
         fee_map[ op_fee.which() ] = op_fee;
      uint32_t scale = current_fees.scale;

      for( const auto& item : changed_fees )
      {
         const string& key = item.key();
         if( key == "scale" )
         {
            int64_t _scale = item.value().as_int64();
            FC_ASSERT( _scale >= 0 );
            FC_ASSERT( _scale <= std::numeric_limits<uint32_t>::max() );
            scale = uint32_t( _scale );
            continue;
         }
         // is key a number?
         auto is_numeric = [&key]() -> bool
         {
            size_t n = key.size();
            for( size_t i=0; i<n; i++ )
            {
               if( 0 == isdigit( key[i] ) )
                  return false;
            }
            return true;
         };

         int which;
         if( is_numeric() )
            which = std::stoi( key );
         else
         {
            const auto& n2w = _operation_which_map.name_to_which;
            auto it = n2w.find( key );
            FC_ASSERT( it != n2w.end(), "unknown operation" );
            which = it->second;
         }

         fee_parameters fp = from_which_variant< fee_parameters >( which, item.value(), GRAPHENE_MAX_NESTED_OBJECTS );
         fee_map[ which ] = fp;
      }

      fee_schedule_type new_fees;

      for( const std::pair< int, fee_parameters >& item : fee_map )
         new_fees.parameters.insert( item.second );
      new_fees.scale = scale;

      chain_parameters new_params = current_params;
      new_params.get_mutable_fees() = new_fees;

      delegate_update_global_parameters_operation update_op;
      update_op.new_parameters = new_params;

      proposal_create_operation prop_op;

      prop_op.expiration_time = expiration_time;
      prop_op.review_period_seconds = current_params.council_proposal_review_period;
      prop_op.fee_paying_account = get_account(proposing_account).id;

      prop_op.proposed_ops.emplace_back( update_op );
      current_params.get_current_fees().set_fee( prop_op.proposed_ops.back().op );

      signed_transaction tx;
      tx.operations.push_back(prop_op);
      set_operation_fees(tx, current_params.get_current_fees());
      tx.validate();

      return sign_transaction(tx, broadcast);
   }

}}} // namespace graphene::wallet::detail
