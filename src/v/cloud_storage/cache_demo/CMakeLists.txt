add_executable(cache_demo cache_demo.cc)
target_link_libraries(cache_demo PUBLIC v::cloud_storage)
set_property(TARGET cache_demo PROPERTY POSITION_INDEPENDENT_CODE ON)

add_executable(eviction_demo eviction_demo.cc)
target_link_libraries(eviction_demo PUBLIC v::cloud_storage)
set_property(TARGET eviction_demo PROPERTY POSITION_INDEPENDENT_CODE ON)
