file(GLOB HEADERS "include/graphene/market_snapshot/*.hpp")

add_library( graphene_market_snapshot
             market_snapshot_plugin.cpp
           )

target_link_libraries( graphene_market_snapshot graphene_chain graphene_app )
target_include_directories( graphene_market_snapshot
                            PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

if(MSVC)
  set_source_files_properties( market_snapshot_plugin.cpp PROPERTIES COMPILE_FLAGS "/bigobj" )
endif(MSVC)

install( TARGETS
   graphene_market_snapshot

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
