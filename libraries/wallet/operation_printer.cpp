#include "operation_printer.hpp"
#include <graphene/protocol/base.hpp>
#include "wallet_api_impl.hpp"
#include <graphene/utilities/key_conversion.hpp>

namespace graphene { namespace wallet { namespace detail {

class htlc_hash_to_string_visitor
{
public:
   typedef std::string result_type;

   result_type operator()( const fc::ripemd160& hash )const
   {
      return "RIPEMD160 " + hash.str();
   }

   result_type operator()( const fc::sha1& hash )const
   {
      return "SHA1 " + hash.str();
   }

   result_type operator()( const fc::sha256& hash )const
   {
      return "SHA256 " + hash.str();
   }
   result_type operator()( const fc::hash160& hash )const
   {
      return "HASH160 " + hash.str();
   }
};

std::string operation_printer::format_asset(const graphene::protocol::asset& a)const
{
   return wallet.get_asset(a.asset_id).amount_to_pretty_string(a);
}

void operation_printer::print_fee(const graphene::protocol::asset& a)const
{
   out << "   (Fee: " << format_asset(a) << ")";
}

void operation_printer::print_result()const
{
   operation_result_printer rprinter(wallet);
   std::string str_result = result.visit(rprinter);
   if( str_result != "" )
   {
      out << "   result: " << str_result;
   }
}

string operation_printer::print_memo( const fc::optional<graphene::protocol::memo_data>& memo )const
{
   std::string outstr;
   if( memo )
   {
      if( wallet.is_locked() )
      {
         out << " -- Unlock wallet to see memo.";
      } else {
         try {
            FC_ASSERT( wallet._keys.count(memo->to) > 0 || wallet._keys.count(memo->from) > 0,
                       "Memo is encrypted to a key ${to} or ${from} not in this wallet.",
                       ("to", memo->to)("from",memo->from) );
            if( wallet._keys.count(memo->to) > 0 ) {
               auto my_key = wif_to_key(wallet._keys.at(memo->to));
               FC_ASSERT(my_key, "Unable to recover private key to decrypt memo. Wallet may be corrupted.");
               outstr = memo->get_message(*my_key, memo->from);
               out << " -- Memo: " << outstr;
            } else {
               auto my_key = wif_to_key(wallet._keys.at(memo->from));
               FC_ASSERT(my_key, "Unable to recover private key to decrypt memo. Wallet may be corrupted.");
               outstr = memo->get_message(*my_key, memo->to);
               out << " -- Memo: " << outstr;
            }
         } catch (const fc::exception& e) {
            out << " -- could not decrypt memo";
         }
      }
   }
   return outstr;
}

void operation_printer::print_preimage(const std::vector<char>& preimage)const
{
   if (preimage.size() == 0)
      return;
   out << " with preimage \"";
   // cut it at 300 bytes max
   auto flags = out.flags();
   out << std::hex << setw(2) << setfill('0');
   for (size_t i = 0; i < std::min<size_t>(300, preimage.size()); i++)
      out << +preimage[i];
   out.flags(flags);
   if (preimage.size() > 300)
      out << "...(truncated due to size)";
   out << "\"";
}

void operation_printer::print_redeem(const graphene::protocol::htlc_id_type& id,
      const std::string& redeemer, const std::vector<char>& preimage,
      const graphene::protocol::asset& op_fee)const
{
   out << redeemer << " redeemed HTLC with id "
         << std::string( static_cast<object_id_type>(id));
   print_preimage( preimage );
   print_fee(op_fee);
}

std::string operation_printer::operator()(const transfer_from_blind_operation& op)const
{
   auto receiver = wallet.get_account( op.to );

   out <<  receiver.name
       << " received " << format_asset( op.amount ) << " from blinded balance";
   return "";
}
std::string operation_printer::operator()(const transfer_to_blind_operation& op)const
{
   auto sender = wallet.get_account( op.from );

   out <<  sender.name
       << " sent " << format_asset( op.amount ) << " to " << op.outputs.size()
       << " blinded balance" << (op.outputs.size()>1?"s":"");
   print_fee( op.fee );
   return "";
}

string operation_printer::operator()(const transfer_operation& op) const
{
   out << "Transfer " << format_asset(op.amount)
       << " from " << wallet.get_account(op.from).name << " to " << wallet.get_account(op.to).name;
   std::string memo = print_memo( op.memo );
   print_fee(op.fee);
   return memo;
}

std::string operation_printer::operator()(const account_create_operation& op) const
{
   out << "Create Account '" << op.name << "' with registrar "
       << wallet.get_account(op.registrar).name << " and referrer "
       << wallet.get_account(op.referrer).name;
   print_fee(op.fee);
   print_result();
   return "";
}

std::string operation_printer::operator()(const account_update_operation& op) const
{
   out << "Update Account '" << wallet.get_account(op.account).name << "'";
   print_fee(op.fee);
   return "";
}

std::string operation_printer::operator()(const asset_create_operation& op) const
{
   out << "Create ";
   if( op.bitasset_opts.valid() )
      out << "BitAsset ";
   else
      out << "User-Issue Asset ";
   out << "'" << op.symbol << "' with issuer " << wallet.get_account(op.issuer).name;
   print_fee(op.fee);
   print_result();
   return "";
}

std::string operation_printer::operator()(const htlc_redeem_operation& op) const
{
   print_redeem(op.htlc_id, wallet.get_account(op.redeemer).name, op.preimage, op.fee);
   return "";
}

std::string operation_printer::operator()(const htlc_redeemed_operation& op) const
{
   print_redeem(op.htlc_id, wallet.get_account(op.redeemer).name, op.preimage, op.fee);
   return "";
}

std::string operation_printer::operator()(const htlc_create_operation& op) const
{
   static htlc_hash_to_string_visitor vtor;

   auto fee_asset = wallet.get_asset( op.fee.asset_id );
   auto to = wallet.get_account( op.to );
   auto from = wallet.get_account( op.from );
   operation_result_printer rprinter(wallet);
   std::string database_id = result.visit(rprinter);

   out << "Create HTLC from " << from.name << " to " << to.name
         << " with id " << database_id
         << " preimage hash: [" << op.preimage_hash.visit( vtor ) << "] ";
   print_memo( op.extensions.value.memo );
   // determine if the block that the HTLC is in is before or after LIB
   int32_t pending_blocks = hist.block_num - wallet.get_dynamic_global_properties().last_irreversible_block_num;
   if (pending_blocks > 0)
      out << " (pending " << std::to_string(pending_blocks) << " blocks)";
   print_fee(op.fee);
   return "";
}

std::string operation_result_printer::operator()(const void_result& x) const
{
   return "";
}

std::string operation_result_printer::operator()(const graphene::protocol::object_id_type& oid) const
{
   return std::string(oid);
}

std::string operation_result_printer::operator()(const asset& a) const
{
   return _wallet.get_asset(a.asset_id).amount_to_pretty_string(a);
}

std::string operation_result_printer::operator()(const generic_operation_result& r) const
{
   return fc::json::to_string(r);
}

}}} // graphene::wallet::detail
