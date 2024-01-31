#pragma once

#include <fc/reflect/reflect.hpp>

#include <boost/container/flat_set.hpp>

#include <map>
#include <string>
#include <vector>

namespace graphene { namespace app {

struct api_access_info
{
   api_access_info() = default;
   api_access_info( const std::string& hash, const std::string& salt )
   : password_hash_b64(hash), password_salt_b64(salt)
   { /* Nothing else to do */ }

   std::string password_hash_b64;
   std::string password_salt_b64;
   boost::container::flat_set< std::string > allowed_apis;
};

struct api_access
{
   std::map< std::string, api_access_info > permission_map;
};

} } // graphene::app

FC_REFLECT( graphene::app::api_access_info,
    (password_hash_b64)
    (password_salt_b64)
    (allowed_apis)
   )

FC_REFLECT( graphene::app::api_access,
    (permission_map)
   )
