#pragma once

#include <graphene/app/plugin.hpp>
#include <graphene/chain/database.hpp>

#include <graphene/custom_operations/custom_objects.hpp>
#include <graphene/custom_operations/custom_operations.hpp>
#include <graphene/custom_operations/custom_evaluators.hpp>

namespace graphene { namespace custom_operations {
using namespace chain;

namespace detail
{
    class custom_operations_plugin_impl;
}

class custom_operations_plugin : public graphene::app::plugin
{
   public:
      explicit custom_operations_plugin(graphene::app::application& app);
      ~custom_operations_plugin() override;

      std::string plugin_name()const override;
      std::string plugin_description()const override;
      void plugin_set_program_options(
         boost::program_options::options_description& cli,
         boost::program_options::options_description& cfg) override;
      void plugin_initialize(const boost::program_options::variables_map& options) override;
      void plugin_startup() override;

   private:
      std::unique_ptr<detail::custom_operations_plugin_impl> my;
};

typedef fc::static_variant<account_storage_map> custom_plugin_operation;

} } //graphene::custom_operations

FC_REFLECT_TYPENAME( graphene::custom_operations::custom_plugin_operation )
