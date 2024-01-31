#pragma once
#include <boost/program_options.hpp>

namespace graphene { namespace utilities {

template<typename T>
void get_program_option( const boost::program_options::variables_map& from, const std::string& key, T& to )
{
   if( from.count( key ) > 0 )
   {
      to = from[key].as<T>();
   }
}

} } // end namespace graphene::utilities
