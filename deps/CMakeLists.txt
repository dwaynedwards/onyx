# Dependencies

# catch2
# https://github.com/catchorg/Catch2.git

# EXCLUDE_FROM_ALL skips Catch2's own internal self-tests
add_subdirectory(catch2 EXCLUDE_FROM_ALL)

# Expose Catch.cmake module path to root and sibling subdirectories (tests/)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/catch2/extras")
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" PARENT_SCOPE)
