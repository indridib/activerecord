# ==============================================================================
# tests/CMakeLists.txt
#
# SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
# SPDX-License-Identifier: MIT
# See file LICENSE.MIT for detail
#
# GoogleTest-based testing for ActiveRecord library
# ==============================================================================

include(FetchContent)
include(${PROJECT_SOURCE_DIR}/cmake/Sanitizers.cmake)

# Download GoogleTest automatically
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
  GIT_SHALLOW    TRUE
)

# For Windows static runtime (/MT): ensure GoogleTest uses the same CRT.
set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# Enable GoogleTest's CMake integration
include(GoogleTest)

# ==============================================================================
# Unit Tests
# ==============================================================================

add_executable(unit_tests
  unit/attribute_test.cpp
  unit/connection_factory_test.cpp
  unit/connection_url_test.cpp
  unit/datetime_test.cpp
  unit/model_test.cpp
  unit/model_size_test.cpp
  unit/sql_builder_test.cpp
  unit/config_test.cpp
  unit/sql_injection_test.cpp
  unit/typed_model_test.cpp
  unit/transaction_test.cpp
)

target_link_libraries(unit_tests PRIVATE
  activerecord::activerecord
  GTest::gtest_main
)

# Apply same warning settings as main library
if(COMMAND ar_set_default_warnings)
  ar_set_default_warnings(unit_tests)
endif()
if(COMMAND ar_enable_sanitizers)
  ar_enable_sanitizers(unit_tests)
endif()

# Add a single test that runs all unit tests
add_test(NAME AllUnitTests COMMAND unit_tests)

# ==============================================================================
# Integration Tests (require at least one database adapter)
# ==============================================================================

if(AR_SQLITE OR AR_MYSQL OR AR_POSTGRES OR AR_MSSQL)
  set(INTEGRATION_TEST_SOURCES
    integration/adapter_tests.cpp  # Parameterized tests for all adapters
  )

  # Add adapter-specific tests
  if(AR_SQLITE)
    list(APPEND INTEGRATION_TEST_SOURCES
      integration/sqlite_adapter_test.cpp
      integration/database_operations_test.cpp
      integration/datetime_integration_test.cpp
    )
  endif()

  add_executable(integration_tests ${INTEGRATION_TEST_SOURCES})

  target_link_libraries(integration_tests PRIVATE
    activerecord::activerecord
    GTest::gtest_main
  )

  if(COMMAND ar_set_default_warnings)
    ar_set_default_warnings(integration_tests)
  endif()

  # Add a single test that runs all integration tests
  add_test(NAME AllIntegrationTests COMMAND integration_tests)
else()
  message(STATUS "No database adapters enabled; skipping integration tests")
endif()
