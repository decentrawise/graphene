#pragma once

#include <graphene/app/plugin.hpp>
#include <graphene/chain/database.hpp>

namespace graphene { namespace template_plugin {
using namespace chain;

//
// Plugins should #define their SPACE_ID's so plugins with
// conflicting SPACE_ID assignments can be compiled into the
// same binary (by simply re-assigning some of the conflicting #defined
// SPACE_ID's in a build script).
//
// Assignment of SPACE_ID's cannot be done at run-time because
// various template automagic depends on them being known at compile
// time.
//
#ifndef template_plugin_SPACE_ID
#define template_plugin_SPACE_ID @SPACE_ID@
#endif


namespace detail
{
    class template_plugin_impl;
}

class template_plugin : public graphene::app::plugin
{
   public:
      explicit template_plugin(graphene::app::application& app);
      ~template_plugin() override;

      std::string plugin_name()const override;
      std::string plugin_description()const override;
      void plugin_set_program_options(
         boost::program_options::options_description& cli,
         boost::program_options::options_description& cfg) override;
      void plugin_initialize(const boost::program_options::variables_map& options) override;
      void plugin_startup() override;
      void plugin_shutdown() override;

   private:
      void cleanup();
      std::unique_ptr<detail::template_plugin_impl> my;
};

} } //graphene::template
