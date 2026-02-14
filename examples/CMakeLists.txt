# SQLite example (always available when SQLite is enabled)
if(AR_SQLITE)
  add_executable(sqlite_example sqlite_example.cpp)
  target_link_libraries(sqlite_example PRIVATE activerecord)
endif()

# MySQL example
if(AR_MYSQL)
  add_executable(mysql_example mysql_example.cpp)
  target_link_libraries(mysql_example PRIVATE activerecord)
endif()

# PostgreSQL example
if(AR_POSTGRES)
  add_executable(postgres_example postgres_example.cpp)
  target_link_libraries(postgres_example PRIVATE activerecord)
endif()

# MS-SQL example
if(AR_MSSQL)
  add_executable(mssql_example mssql_example.cpp)
  target_link_libraries(mssql_example PRIVATE activerecord)
endif()
