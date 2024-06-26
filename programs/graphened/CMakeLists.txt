add_executable( graphened main.cpp )
if( UNIX AND NOT APPLE )
  set(rt_library rt )
endif()

find_package( Gperftools QUIET )
if( GPERFTOOLS_FOUND )
    message( STATUS "Found gperftools; compiling graphened with TCMalloc")
    list( APPEND PLATFORM_SPECIFIC_LIBS tcmalloc )
endif()

# We have to link against graphene_debug_validator because deficiency in our API infrastructure doesn't allow plugins to be fully abstracted #246
target_link_libraries( graphened

PRIVATE graphene_app graphene_delayed_node graphene_account_history graphene_elasticsearch graphene_market_history graphene_grouped_orders graphene_validator graphene_chain graphene_debug_validator graphene_egenesis_full graphene_snapshot graphene_es_objects
        graphene_api_helper_indexes graphene_custom_operations
        fc ${CMAKE_DL_LIBS} ${PLATFORM_SPECIFIC_LIBS} )

if (MSVC)
    set_target_properties( graphened PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

install( TARGETS
   graphened

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
