file(GLOB HEADERS "include/graphene/validator/*.hpp")

add_library( graphene_validator 
             validator.cpp
           )

target_link_libraries( graphene_validator graphene_app graphene_chain )
target_include_directories( graphene_validator
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if (MSVC)
    set_target_properties( graphene_validator PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

install( TARGETS
   graphene_validator

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
