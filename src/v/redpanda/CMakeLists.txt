set(swags
  config
  cluster_config
  raft
  security
  status
  features
  broker
  partition
  hbadger
  transaction
  debug)

set(swag_files)
foreach(swag ${swags})
  seastar_generate_swagger(
    TARGET ${swag}_swagger
    VAR ${swag}_swagger_files
    IN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/admin/api-doc/${swag}.json
    OUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/admin/api-doc/${swag}.json.h
  )
  list(APPEND swag_files ${${swag}_swagger_files})
endforeach()

v_cc_library(
  NAME application
  SRCS 
    ${swag_files}
    admin_server.cc
    request_auth.cc
    application.cc
  DEPS
    Seastar::seastar
    v::cluster
    v::syschecks
    v::kafka
    v::coproc
    v::pandaproxy_rest
    v::pandaproxy_schema_registry
    v::archival
  )

add_executable(redpanda
    main.cc
  )
target_link_libraries(redpanda PUBLIC v::application v::raft v::kafka)
set_property(TARGET redpanda PROPERTY POSITION_INDEPENDENT_CODE ON)

if(CMAKE_BUILD_TYPE MATCHES Release)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ltosupported OUTPUT error)
  if(ltosupported)
    set_property(TARGET redpanda PROPERTY INTERPROCEDURAL_OPTIMIZATION ON)
  endif()
endif()
install(TARGETS redpanda DESTINATION bin)
