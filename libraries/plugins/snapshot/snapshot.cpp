#include <graphene/snapshot/snapshot.hpp>

#include <graphene/chain/database.hpp>

#include <fc/io/fstream.hpp>

using namespace graphene::snapshot_plugin;
using std::string;
using std::vector;

namespace bpo = boost::program_options;

static const char* OPT_BLOCK_NUM  = "snapshot-at-block";
static const char* OPT_BLOCK_TIME = "snapshot-at-time";
static const char* OPT_DEST       = "snapshot-to";

void snapshot_plugin::plugin_set_program_options(
   boost::program_options::options_description& command_line_options,
   boost::program_options::options_description& config_file_options)
{
   command_line_options.add_options()
         (OPT_BLOCK_NUM, bpo::value<uint32_t>(), "Block number after which to do a snapshot")
         (OPT_BLOCK_TIME, bpo::value<string>(), "Block time (ISO format) after which to do a snapshot")
         (OPT_DEST, bpo::value<string>(), "Pathname of JSON file where to store the snapshot")
         ;
   config_file_options.add(command_line_options);
}

std::string snapshot_plugin::plugin_name()const
{
   return "snapshot";
}

std::string snapshot_plugin::plugin_description()const
{
   return "Create snapshots at a specified time or block number.";
}

void snapshot_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{ try {
   ilog("snapshot plugin: plugin_initialize() begin");

   if( options.count(OPT_BLOCK_NUM) > 0 || options.count(OPT_BLOCK_TIME) > 0 )
   {
      FC_ASSERT( options.count(OPT_DEST) > 0,
                 "Must specify snapshot-to in addition to snapshot-at-block or snapshot-at-time!" );
      dest = options[OPT_DEST].as<std::string>();
      if( options.count(OPT_BLOCK_NUM) > 0 )
         snapshot_block = options[OPT_BLOCK_NUM].as<uint32_t>();
      if( options.count(OPT_BLOCK_TIME) > 0 )
         snapshot_time = fc::time_point_sec::from_iso_string( options[OPT_BLOCK_TIME].as<std::string>() );
      // connect with no group specified to process after the ones with a group specified
      database().applied_block.connect( [&]( const graphene::chain::signed_block& b ) {
         check_snapshot( b );
      });
   }
   else
      ilog("snapshot plugin is not enabled because neither snapshot-at-block nor snapshot-at-time is specified");

   ilog("snapshot plugin: plugin_initialize() end");
} FC_LOG_AND_RETHROW() }

static void create_snapshot( const graphene::chain::database& db, const fc::path& dest )
{
   ilog("snapshot plugin: creating snapshot");
   fc::ofstream out;
   try
   {
      out.open( dest );
   }
   catch ( fc::exception& e )
   {
      wlog( "Failed to open snapshot destination: ${ex}", ("ex",e) );
      return;
   }
   for( uint32_t space_id = 0; space_id < 256; space_id++ )
      for( uint32_t type_id = 0; type_id < 256; type_id++ )
      {
         try
         {
            db.get_index( (uint8_t)space_id, (uint8_t)type_id );
         }
         catch (fc::assert_exception& e)
         {
            continue;
         }
         auto& index = db.get_index( (uint8_t)space_id, (uint8_t)type_id );
         index.inspect_all_objects( [&out]( const graphene::db::object& o ) {
            out << fc::json::to_string( o.to_variant() ) << '\n';
         });
      }
   out.close();
   ilog("snapshot plugin: created snapshot");
}

void snapshot_plugin::check_snapshot( const graphene::chain::signed_block& b )
{ try {
    uint32_t current_block = b.block_num();
    if( (last_block < snapshot_block && snapshot_block <= current_block)
           || (last_time < snapshot_time && snapshot_time <= b.timestamp) )
       create_snapshot( database(), dest );
    last_block = current_block;
    last_time = b.timestamp;
} FC_LOG_AND_RETHROW() }
