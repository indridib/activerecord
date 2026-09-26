# Apply the same warning and sanitizer policy used for the library and tests so
# example targets can't drift below the project's 0-warning baseline, and so they
# link against a sanitizer-instrumented activerecord when AR_SANITIZE_* is on.
function(ar_configure_example target)
  target_link_libraries(${target} PRIVATE activerecord)
  if(COMMAND ar_set_default_warnings)
    ar_set_default_warnings(${target})
  endif()
  if(COMMAND ar_enable_sanitizers)
    ar_enable_sanitizers(${target})
  endif()
endfunction()

# SQLite example (always available when SQLite is enabled)
if(AR_SQLITE)
  add_executable(sqlite_example sqlite_example.cpp)
  ar_configure_example(sqlite_example)

  add_executable(typed_model_showcase typed_model_showcase.cpp)
  ar_configure_example(typed_model_showcase)
endif()

# MySQL example
if(AR_MYSQL)
  add_executable(mysql_example mysql_example.cpp)
  ar_configure_example(mysql_example)
endif()

# PostgreSQL example
if(AR_POSTGRES)
  add_executable(postgres_example postgres_example.cpp)
  ar_configure_example(postgres_example)
endif()

# MS-SQL example
if(AR_MSSQL)
  add_executable(mssql_example mssql_example.cpp)
  ar_configure_example(mssql_example)
endif()
