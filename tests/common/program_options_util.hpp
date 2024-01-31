#pragma once

#include <boost/program_options.hpp>

/**
 * Set value of a named variable in the program options map if the variable has not been set.
 */
template<typename T>
static void set_option( boost::program_options::variables_map& options, const std::string& name, const T& value )
{
   options.emplace( name, boost::program_options::variable_value( value, false ) );
}
