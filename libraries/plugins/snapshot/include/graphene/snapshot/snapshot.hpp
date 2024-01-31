#pragma once

#include <graphene/app/plugin.hpp>
#include <graphene/chain/database.hpp>

#include <fc/time.hpp>

namespace graphene { namespace snapshot_plugin {

class snapshot_plugin : public graphene::app::plugin {
   public:
      using graphene::app::plugin::plugin;

      std::string plugin_name()const override;
      std::string plugin_description()const override;

      void plugin_set_program_options(
         boost::program_options::options_description &command_line_options,
         boost::program_options::options_description &config_file_options
      ) override;

      void plugin_initialize( const boost::program_options::variables_map& options ) override;

   private:
       void check_snapshot( const graphene::chain::signed_block& b);

       uint32_t           snapshot_block = -1, last_block = 0;
       fc::time_point_sec snapshot_time = fc::time_point_sec::maximum(), last_time = fc::time_point_sec(1);
       fc::path           dest;
};

} } //graphene::snapshot_plugin
