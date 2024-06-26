file(GLOB COMMON_SOURCES "common/*.cpp")
file(GLOB COMMON_HEADERS "common/*.hpp")

add_library( database_fixture
             ${COMMON_SOURCES}
             ${COMMON_HEADERS}
           )
target_link_libraries( database_fixture PUBLIC graphene_es_objects graphene_app graphene_egenesis_none )
target_include_directories( database_fixture
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/common" )

find_package( Gperftools QUIET )
if( GPERFTOOLS_FOUND )
    message( STATUS "Found gperftools; compiling tests with TCMalloc")
    list( APPEND PLATFORM_SPECIFIC_LIBS tcmalloc )
endif()

file(GLOB CHAIN_TESTS "chain/*.cpp")
add_executable( chain_test ${CHAIN_TESTS} )
target_link_libraries( chain_test database_fixture
                       graphene_validator graphene_wallet graphene_app ${PLATFORM_SPECIFIC_LIBS} )
if(MSVC)
  set_source_files_properties( tests/serialization_tests.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
  set_source_files_properties( tests/common/database_fixture.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

file(GLOB PERFORMANCE_TESTS "performance/*.cpp")
add_executable( performance_test ${PERFORMANCE_TESTS} )
target_link_libraries( performance_test database_fixture ${PLATFORM_SPECIFIC_LIBS} )

file(GLOB APP_SOURCES "app/*.cpp")
add_executable( app_test ${APP_SOURCES} )
target_link_libraries( app_test graphene_app graphene_egenesis_none
                       ${PLATFORM_SPECIFIC_LIBS} )

file(GLOB WALLET_SOURCES "wallet/*.cpp")
add_executable( wallet_test ${WALLET_SOURCES} )
if(WIN32)
   list(APPEND PLATFORM_SPECIFIC_LIBS ws2_32)
endif()
target_link_libraries( wallet_test graphene_wallet graphene_app graphene_egenesis_none
                       ${PLATFORM_SPECIFIC_LIBS} )
if(MSVC)
  set_source_files_properties( cli/main.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

file(GLOB ES_SOURCES "elasticsearch/*.cpp")
add_executable( es_test ${ES_SOURCES} )
target_link_libraries( es_test database_fixture ${PLATFORM_SPECIFIC_LIBS} )
                       
add_subdirectory( generate_empty_blocks )
