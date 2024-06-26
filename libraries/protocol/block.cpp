#include <boost/endian/conversion.hpp>
#include <graphene/protocol/block.hpp>
#include <graphene/protocol/fee_schedule.hpp>
#include <fc/io/raw.hpp>
#include <algorithm>

namespace graphene { namespace protocol {
   digest_type block_header::digest()const
   {
      return digest_type::hash(*this);
   }

   uint32_t block_header::num_from_id(const block_id_type& id)
   {
      return boost::endian::endian_reverse(id._hash[0].value());
   }

   const block_id_type& signed_block_header::id()const
   {
      if( 0 == _block_id._hash[0].value() )
      {
         auto tmp = fc::sha224::hash( *this );
         tmp._hash[0] = boost::endian::endian_reverse(block_num()); // store the block num in the ID, 160 bits is plenty for the hash
         static_assert( sizeof(tmp._hash[0]) == 4, "should be 4 bytes" );
         memcpy(_block_id._hash, tmp._hash, std::min(sizeof(_block_id), sizeof(tmp)));
      }
      return _block_id;
   }

   const fc::ecc::public_key& signed_block_header::signee()const
   {
      if( !_signee.valid() )
         _signee = fc::ecc::public_key( validator_signature, digest(), true/*enforce canonical*/ );
      return _signee;
   }

   void signed_block_header::sign( const fc::ecc::private_key& signer )
   {
      validator_signature = signer.sign_compact( digest() );
   }

   bool signed_block_header::validate_signee( const fc::ecc::public_key& expected_signee )const
   {
      return signee() == expected_signee;
   }

   const checksum_type& signed_block::calculate_merkle_root()const
   {
      static const checksum_type empty_checksum;
      if( transactions.size() == 0 ) 
         return empty_checksum;

      if( 0 == _calculated_merkle_root._hash[0].value() )
      {
         vector<digest_type> ids;
         ids.resize( transactions.size() );
         for( uint32_t i = 0; i < transactions.size(); ++i )
            ids[i] = transactions[i].merkle_digest();

         vector<digest_type>::size_type current_number_of_hashes = ids.size();
         while( current_number_of_hashes > 1 )
         {
            // hash ID's in pairs
            uint32_t i_max = current_number_of_hashes - (current_number_of_hashes&1);
            uint32_t k = 0;

            for( uint32_t i = 0; i < i_max; i += 2 )
               ids[k++] = digest_type::hash( std::make_pair( ids[i], ids[i+1] ) );

            if( current_number_of_hashes&1 )
               ids[k++] = ids[i_max];
            current_number_of_hashes = k;
         }
         _calculated_merkle_root = checksum_type::hash( ids[0] );
      }
      return _calculated_merkle_root;
   }
} }

GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::block_header)
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::signed_block_header)
GRAPHENE_IMPLEMENT_EXTERNAL_SERIALIZATION( graphene::protocol::signed_block)
