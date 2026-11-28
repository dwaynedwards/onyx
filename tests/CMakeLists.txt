# Tests

include(Catch)

# Helper to register an isolated Catch2 test executable + CTest discovery
function(onyx_add_test target_name)
  file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

  add_executable(${target_name}
      ${TEST_SOURCES}
  )

  target_link_libraries(${target_name} PRIVATE
      ${ONYX}
      Catch2::Catch2WithMain
  )

  onyx_configure_target(${target_name})

  # Discover all standard Catch2 tests (excluding [.asan] death tests)
  catch_discover_tests(${target_name}
      TEST_PREFIX "${target_name}::"
      TEST_SPEC "~[.asan]"
  )
endfunction()

add_subdirectory(arena)
