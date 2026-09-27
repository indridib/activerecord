cmake_minimum_required(VERSION 3.20)

# Use static libraries on Windows to avoid DLL path issues
if(WIN32 AND NOT DEFINED VCPKG_TARGET_TRIPLET)
  set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "")
endif()

# Auto-bootstrap vcpkg if it's in the project and not already configured
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
      CACHE STRING "Vcpkg toolchain file")
  message(STATUS "Using project vcpkg: ${CMAKE_TOOLCHAIN_FILE}")
endif()

# Adapter options must precede project(): the vcpkg toolchain installs manifest
# features during project(), so each enabled adapter adds its client library here.
# Appending keeps any features the caller passed via -DVCPKG_MANIFEST_FEATURES.
option(AR_MYSQL "Build MySQL adapter" OFF)
option(AR_POSTGRES "Build PostgreSQL adapter" OFF)
if(AR_MYSQL AND NOT "mysql" IN_LIST VCPKG_MANIFEST_FEATURES)
  list(APPEND VCPKG_MANIFEST_FEATURES "mysql")
endif()
if(AR_POSTGRES AND NOT "postgresql" IN_LIST VCPKG_MANIFEST_FEATURES)
  list(APPEND VCPKG_MANIFEST_FEATURES "postgresql")
endif()

project(activerecord VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  # Enforce static runtime (/MT, /MTd) for all targets on Windows.
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(AR_BUILD_TESTS "Build tests" OFF)
option(AR_BUILD_EXAMPLES "Build examples" ON)
option(AR_SQLITE "Build SQLite adapter" ON)
option(AR_MSSQL "Build MS-SQL adapter" OFF)
option(AR_CLANG_TIDY "Enable clang-tidy integration" OFF)
option(AR_SANITIZE_ADDRESS "Enable AddressSanitizer (Clang/GCC)" OFF)
option(AR_SANITIZE_LEAK "Enable LeakSanitizer (Clang/GCC)" OFF)
option(AR_SANITIZE_UNDEFINED "Enable UndefinedBehaviorSanitizer (Clang/GCC)" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(AR_CLANG_TIDY AND NOT CMAKE_CXX_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy)
  if(CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
  endif()
endif()

add_subdirectory(src)

if(AR_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(AR_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
  include(CTest)
  add_subdirectory(tests)
endif()

include(GNUInstallDirs)
install(
  DIRECTORY include/activerecord
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PATTERN "*.cpp" EXCLUDE
)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/activerecord-config-version.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/activerecord-config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/activerecord-config.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/activerecord
)

install(EXPORT activerecordTargets
  NAMESPACE activerecord::
  FILE activerecordTargets.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/activerecord
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/activerecord-config.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/activerecord-config-version.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/activerecord
)
