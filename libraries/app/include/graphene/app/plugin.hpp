#pragma once

#include <graphene/app/application.hpp>

#include <boost/program_options.hpp>
#include <fc/io/json.hpp>

namespace graphene { namespace app {

class abstract_plugin
{
   public:
      explicit abstract_plugin(application& a) : _app(a) {}
      virtual ~abstract_plugin() = default;

      /// Get the name of the plugin
      virtual std::string plugin_name()const = 0;

      /// Get the description of the plugin
      virtual std::string plugin_description()const = 0;

      /// Get a reference of the application bound to the plugin
      application& app()const { return _app; }

      /**
       * @brief Perform early startup routines and register plugin indexes, callbacks, etc.
       *
       * Plugins MUST supply a method initialize() which will be called early in the application startup. This method
       * should contain early setup code such as initializing variables, adding indexes to the database, registering
       * callback methods from the database, adding APIs, etc., as well as applying any options in the @p options map
       *
       * This method is called BEFORE the database is open, therefore any routines which require any chain state MUST
       * NOT be called by this method. These routines should be performed in startup() instead.
       *
       * @param options The options passed to the application, via configuration files or command line
       */
      virtual void plugin_initialize( const boost::program_options::variables_map& options ) = 0;

      /**
       * @brief Begin normal runtime operations
       *
       * Plugins MUST supply a method startup() which will be called at the end of application startup. This method
       * should contain code which schedules any tasks, or requires chain state.
       */
      virtual void plugin_startup() = 0;

      /**
       * @brief Cleanly shut down the plugin.
       *
       * This is called to request a clean shutdown (e.g. due to SIGINT or SIGTERM).
       */
      virtual void plugin_shutdown() = 0;

      /**
       * @brief Fill in command line parameters used by the plugin.
       *
       * @param command_line_options All options this plugin supports taking on the command-line
       * @param config_file_options All options this plugin supports storing in a configuration file
       *
       * This method populates its arguments with any
       * command-line and configuration file options the plugin supports.
       * If a plugin does not need these options, it
       * may simply provide an empty implementation of this method.
       */
      virtual void plugin_set_program_options(
         boost::program_options::options_description& command_line_options,
         boost::program_options::options_description& config_file_options
         ) = 0;
   protected:
      application& _app;
};

/**
 * Provides basic default implementations of abstract_plugin functions.
 */

class plugin : public abstract_plugin
{
   public:
      using abstract_plugin::abstract_plugin;

      std::string plugin_name()const override;
      std::string plugin_description()const override;
      void plugin_initialize( const boost::program_options::variables_map& options ) override;
      void plugin_startup() override;
      void plugin_shutdown() override;
      void plugin_set_program_options(
         boost::program_options::options_description& command_line_options,
         boost::program_options::options_description& config_file_options
         ) override;

      chain::database& database() { return *app().chain_database(); }
   protected:
      net::node_ptr p2p_node() const { return app().p2p_node(); }
};

/// @ingroup Some useful tools for boost::program_options arguments using vectors of JSON strings
/// @{
template<typename T>
T dejsonify(const string& s, uint32_t max_depth)
{
   return fc::json::from_string(s).as<T>(max_depth);
}

namespace impl {
   template<typename T>
   T dejsonify( const string& s )
   {
      return graphene::app::dejsonify<T>( s, GRAPHENE_MAX_NESTED_OBJECTS );
   }
}

#define DEFAULT_VALUE_VECTOR(value) default_value({fc::json::to_string(value)}, fc::json::to_string(value))
#define LOAD_VALUE_SET(options, name, container, type) \
do { \
   if( options.count(name) > 0 ) { \
      const std::vector<std::string>& ops = options[name].as<std::vector<std::string>>(); \
      std::transform(ops.begin(), ops.end(), std::inserter(container, container.end()), \
                     &graphene::app::impl::dejsonify<type>); \
   } \
} while (false)
/// @}

} } //graphene::app
