#pragma once

#include <graphene/app/plugin.hpp>

namespace graphene { namespace delayed_node {
namespace detail { struct delayed_node_plugin_impl; }

class delayed_node_plugin : public graphene::app::plugin
{
   std::unique_ptr<detail::delayed_node_plugin_impl> my;
public:
   explicit delayed_node_plugin(graphene::app::application& app);
   ~delayed_node_plugin() override;

   std::string plugin_name()const override { return "delayed_node"; }
   void plugin_set_program_options(boost::program_options::options_description&,
                                   boost::program_options::options_description& cfg) override;
   void plugin_initialize(const boost::program_options::variables_map& options) override;
   void plugin_startup() override;
   void mainloop();

protected:
   void connection_failed();
   void connect();
   void sync_with_trusted_node();
};

} } //graphene::account_history

