#pragma once

// This file contains various reflection methods that are used to
// support the wallet, e.g. allow specifying operations by name
// instead of ID.

#include <string>
#include <vector>
#include <boost/container/flat_map.hpp>
#include <fc/exception/exception.hpp>
#include <fc/variant.hpp>

namespace graphene { namespace wallet {

using std::string;
using std::vector;
using boost::container::flat_map;
using fc::variant;

struct static_variant_map
{
   flat_map< string, int > name_to_which;
   vector< string > which_to_name;
};

namespace impl {

std::string clean_name( const std::string& name );

struct static_variant_map_visitor
{
   static_variant_map_visitor() {}

   typedef void result_type;

   template< typename T >
   result_type operator()( const T& dummy )
   {
      FC_ASSERT( which == m.which_to_name.size(), "This should not happen" );
      std::string name = clean_name( fc::get_typename<T>::name() );
      m.name_to_which[ name ] = which;
      m.which_to_name.push_back( name );
   }

   static_variant_map m;
   uint16_t which; // 16 bit should be practically enough
};

template< typename StaticVariant >
struct from_which_visitor
{
   typedef StaticVariant result_type;

   template< typename Member >   // Member is member of static_variant
   result_type operator()( const Member& dummy )
   {
      Member result;
      from_variant( v, result, _max_depth );
      return result;    // converted from StaticVariant to Result automatically due to return type
   }

   const variant& v;
   const uint32_t _max_depth;

   from_which_visitor( const variant& _v, uint32_t max_depth ) : v(_v), _max_depth(max_depth) {}
};

} // namespace impl

template< typename T >
T from_which_variant( int which, const variant& v, uint32_t max_depth )
{
   // Parse a variant for a known which()
   T dummy;
   dummy.set_which( which );
   impl::from_which_visitor< T > vtor(v, max_depth);
   return dummy.visit( vtor );
}

template<typename T>
static_variant_map create_static_variant_map()
{
   T dummy;
   int n = dummy.count();
   FC_ASSERT( n <= std::numeric_limits<uint16_t>::max(), "Too many items in this static_variant" );
   impl::static_variant_map_visitor vtor;
   for( int i=0; i<n; i++ )
   {
      dummy.set_which(i);
      vtor.which = i;
      dummy.visit( vtor );
   }
   return vtor.m;
}

} } // namespace graphene::wallet
