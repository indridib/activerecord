set(ACTIVERECORD_BASE_SOURCES
  attribute.cpp
  connection_factory.cpp
  datetime.cpp
)

add_library(activerecord ${ACTIVERECORD_BASE_SOURCES})
add_library(activerecord::activerecord ALIAS activerecord)
# Debug and Release install to the same lib/ directory; without a postfix the second
# install overwrites the first (activerecord.lib), and MSVC consumers then link a
# library with the wrong runtime and iterator-debug level.
set_target_properties(activerecord PROPERTIES DEBUG_POSTFIX "d")

# Records a find_dependency() call for the installed package config
# (cmake/activerecord-config.cmake.in): one per dependency package this build links,
# so a consumer's find_package(activerecord) recreates every exported link target.
function(ar_package_dependency call)
  set_property(GLOBAL APPEND PROPERTY AR_PACKAGE_DEPENDENCIES "${call}")
endfunction()

include(${PROJECT_SOURCE_DIR}/cmake/Warnings.cmake)
ar_set_default_warnings(activerecord)
include(${PROJECT_SOURCE_DIR}/cmake/Sanitizers.cmake)
ar_enable_sanitizers(activerecord)

target_compile_features(activerecord PUBLIC cxx_std_23)

target_include_directories(activerecord
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/activerecord>
    $<INSTALL_INTERFACE:include/activerecord>
)

if(AR_SQLITE)
  target_sources(activerecord PRIVATE adapters/sqlite_adapter.cpp)

  # Find SQLite3 via vcpkg
  # Install: vcpkg install sqlite3
  find_package(unofficial-sqlite3 CONFIG)
  if(unofficial-sqlite3_FOUND)
    target_link_libraries(activerecord PRIVATE unofficial::sqlite3::sqlite3)
    ar_package_dependency("find_dependency(unofficial-sqlite3 CONFIG)")
  else()
    # Fallback to FindSQLite3
    find_package(SQLite3 REQUIRED)
    target_link_libraries(activerecord PRIVATE SQLite::SQLite3)
    ar_package_dependency("find_dependency(SQLite3)")
  endif()

  target_compile_definitions(activerecord PUBLIC AR_SQLITE=1)
endif()

if(AR_MYSQL)
  target_sources(activerecord PRIVATE adapters/mysql_adapter.cpp)

  # Find MySQL via vcpkg
  # Install: vcpkg install libmysql
  find_package(unofficial-libmysql CONFIG)
  if(unofficial-libmysql_FOUND)
    target_link_libraries(activerecord PRIVATE unofficial::libmysql::libmysql)
    ar_package_dependency("find_dependency(unofficial-libmysql CONFIG)")
  else()
    # Fallback: a caller-supplied FindMySQL.cmake or MySQLConfig.cmake
    find_package(MySQL REQUIRED)
    target_link_libraries(activerecord PRIVATE MySQL::MySQL)
    ar_package_dependency("find_dependency(MySQL)")
  endif()

  target_compile_definitions(activerecord PUBLIC AR_MYSQL=1)
endif()

if(AR_POSTGRES)
  target_sources(activerecord PRIVATE adapters/postgres_adapter.cpp)

  # Find PostgreSQL via vcpkg
  # Install: vcpkg install libpq
  find_package(PostgreSQL REQUIRED)
  target_link_libraries(activerecord PRIVATE PostgreSQL::PostgreSQL)
  ar_package_dependency("find_dependency(PostgreSQL)")

  target_compile_definitions(activerecord PUBLIC AR_POSTGRES=1)
endif()

if(AR_MSSQL)
  target_sources(activerecord PRIVATE adapters/mssql_adapter.cpp)

  # ODBC is typically system-provided on Windows, may need unixodbc on Linux
  if(WIN32)
    target_link_libraries(activerecord PRIVATE odbc32)
  else()
    find_package(ODBC REQUIRED)
    target_link_libraries(activerecord PRIVATE ODBC::ODBC)
    ar_package_dependency("find_dependency(ODBC)")
  endif()

  target_compile_definitions(activerecord PUBLIC AR_MSSQL=1)
endif()

install(TARGETS activerecord
  EXPORT activerecordTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
