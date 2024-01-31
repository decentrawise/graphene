#pragma once

#include <string>
#include <vector>

#include <curl/curl.h>

namespace graphene { namespace utilities {

   class ES {
      public:
         CURL *curl;
         std::vector <std::string> bulk_lines;
         std::string elasticsearch_url;
         std::string index_prefix;
         std::string auth;
         std::string endpoint;
         std::string query;
   };

   class CurlRequest {
      public:
         CURL *handler;
         std::string url;
         std::string type;
         std::string auth;
         std::string query;
   };

   bool checkES(ES& es);
   std::string getESVersion(ES& es);
   void checkESVersion7OrAbove(ES& es, bool& result) noexcept;
   std::string simpleQuery(ES& es);
   bool deleteAll(ES& es);
   std::string getEndPoint(ES& es);

   std::string doCurl(CurlRequest& curl);

} } // graphene::utilities
