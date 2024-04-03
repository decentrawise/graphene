file(GLOB HEADERS "include/graphene/debug_validator/*.hpp")

add_library( graphene_debug_validator
             debug_api.cpp
             debug_validator.cpp
           )

target_link_libraries( graphene_debug_validator graphene_app graphene_chain )
target_include_directories( graphene_debug_validator
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

install( TARGETS
   graphene_debug_validator

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
INSTALL( FILES ${HEADERS} DESTINATION "include/graphene/debug_validator" )
