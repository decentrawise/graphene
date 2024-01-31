#include <graphene/app/plugin.hpp>
#include <graphene/protocol/fee_schedule.hpp>

namespace graphene { namespace app {

std::string plugin::plugin_name()const
{
   return "<unknown plugin>";
}

std::string plugin::plugin_description()const
{
   return "<no description>";
}

void plugin::plugin_initialize( const boost::program_options::variables_map& options )
{
   // nothing to do
}

void plugin::plugin_startup()
{
   // nothing to do
}

void plugin::plugin_shutdown()
{
   // nothing to do
}

void plugin::plugin_set_program_options(
   boost::program_options::options_description& command_line_options,
   boost::program_options::options_description& config_file_options
)
{
   // nothing to do
}

} } // graphene::app
