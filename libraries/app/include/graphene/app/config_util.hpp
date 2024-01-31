#pragma once

#include <fc/filesystem.hpp>
#include <boost/program_options.hpp>

namespace graphene { namespace app {

   void load_configuration_options(const fc::path &data_dir, const boost::program_options::options_description &cfg_options,
                           boost::program_options::variables_map &options);

} } // graphene::app