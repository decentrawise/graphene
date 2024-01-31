#pragma once

#include <graphene/app/plugin.hpp>
#include <graphene/chain/database.hpp>

namespace graphene { namespace es_objects {

using namespace chain;

namespace detail
{
    class es_objects_plugin_impl;
}

class es_objects_plugin : public graphene::app::plugin
{
   public:
      explicit es_objects_plugin(graphene::app::application& app);
      ~es_objects_plugin() override;

      std::string plugin_name()const override;
      std::string plugin_description()const override;
      void plugin_set_program_options(
         boost::program_options::options_description& cli,
         boost::program_options::options_description& cfg) override;
      void plugin_initialize(const boost::program_options::variables_map& options) override;
      void plugin_startup() override;
      void plugin_shutdown() override;

   private:
      std::unique_ptr<detail::es_objects_plugin_impl> my;
};

} } //graphene::es_objects
