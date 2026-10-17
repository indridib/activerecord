# Database Adapters

ActiveRecord supports four databases. SQLite is built by default; the others require CMake flags and their respective client libraries.

---

## Enabling Adapters

| Adapter | CMake Flag | Default |
|---------|-----------|---------|
| SQLite | `-DAR_SQLITE=ON` | On |
| MySQL | `-DAR_MYSQL=ON` | Off |
| PostgreSQL | `-DAR_POSTGRES=ON` | Off |
| MS-SQL Server | `-DAR_MSSQL=ON` | Off |

```bash
cmake -B build -DAR_BUILD_TESTS=ON -DAR_MYSQL=ON -DAR_POSTGRES=ON
```

---

## SQLite

Embedded database, no server needed. Good for testing, prototyping, and single-user applications.

### Setup

```cpp
#include <activerecord/adapters/sqlite_adapter.h>

// In-memory database (lost when connection closes)
auto conn = std::make_shared<ar::sqlite_adapter>(":memory:");

// File-based database
auto conn = std::make_shared<ar::sqlite_adapter>("/path/to/myapp.db");

ar::config::instance().set_connections({{"default", conn}}, "default");
```

### Via URL

```
sqlite://:memory:
sqlite://myapp.db
sqlite://unix(/absolute/path/to/file.db)
sqlite://windows(c*/path/to/file.db)
```

### Notes

- Stores datetime values as INTEGER (Unix timestamps).
- Empty inserts use `DEFAULT VALUES`.
- No username/password — authentication parameters are ignored.

---

## MySQL

### Setup

```cpp
#include <activerecord/adapters/mysql_adapter.h>

auto conn = std::make_shared<ar::mysql_adapter>(
    "localhost",  // host
    "root",       // user
    "password",   // password
    "mydb",       // database
    3306          // port (default)
);

ar::config::instance().set_connections({{"default", conn}}, "default");
```

### Via URL

```
mysql://root:password@localhost:3306/mydb
mysql://root:password@localhost/mydb?charset=utf8mb4
mysql://root:password@unix(/var/run/mysqld/mysqld.sock)/mydb
```

### Notes

- Requires `libmysqlclient`.
- Empty inserts use `() VALUES()` (MySQL syntax).
- Use the `charset` query parameter to set character encoding.

---

## PostgreSQL

### Setup

```cpp
#include <activerecord/adapters/postgres_adapter.h>

auto conn = std::make_shared<ar::postgres_adapter>(
    "localhost",  // host
    "postgres",   // user
    "password",   // password
    "mydb",       // database
    5432          // port (default)
);

ar::config::instance().set_connections({{"default", conn}}, "default");
```

### Via URL

```
postgres://postgres:password@localhost:5432/mydb
postgres://user:password@localhost/mydb?charset=utf8
```

### Notes

- Requires `libpq`.

---

## MS-SQL Server

Three connection styles are available.

### Setup via Server/Database

```cpp
#include <activerecord/adapters/mssql_adapter.h>

auto conn = std::make_shared<ar::mssql_adapter>(
    "localhost",  // server
    "mydb",       // database
    "sa",         // user
    "password",   // password
    1433          // port (default)
);
```

### Setup via Connection String

```cpp
auto conn = std::make_shared<ar::mssql_adapter>(
    "DRIVER={ODBC Driver 17 for SQL Server};"
    "SERVER=localhost;"
    "DATABASE=mydb;"
    "UID=sa;"
    "PWD=password;"
);
```

### Setup via DSN

```cpp
auto conn = std::make_shared<ar::mssql_adapter>("MyDSN", "sa", "password");
```

### Via URL

```
mssql://sa:password@localhost:1433/mydb
```

### Notes

- Uses ODBC.
- Pagination uses `OFFSET ... ROWS FETCH NEXT ... ROWS ONLY` syntax (requires ORDER BY).

---

## Adapter Differences

Most differences are handled internally by the library. For reference:

| Behavior | SQLite | MySQL | PostgreSQL | MS-SQL |
|----------|--------|-------|-----------|--------|
| Empty INSERT clause | `DEFAULT VALUES` | `() VALUES()` | `DEFAULT VALUES` | `DEFAULT VALUES` |
| Datetime storage | INTEGER (Unix timestamp) | BIGINT (Unix timestamp) | BIGINT (Unix timestamp) | BIGINT (Unix timestamp) |
| Native date/time columns | None (a DATETIME column stores the integer) | Read only | Read only | Read only |
| Auto-increment | AUTOINCREMENT | AUTO_INCREMENT | SERIAL | IDENTITY |
| LIMIT/OFFSET in UPDATE/DELETE | Adapter-dependent (only when adapter explicitly enables it) | Adapter-dependent (only when adapter explicitly enables it) | Adapter-dependent (only when adapter explicitly enables it) | Adapter-dependent (only when adapter explicitly enables it) |

The query builder generates adapter-appropriate SQL for pagination. You don't need to handle these differences in your code.

---

## Raw SQL

All adapters support executing raw SQL through the connection:

```cpp
auto conn = ar::config::instance().get_connection();

// Execute DDL
conn->query("CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, name TEXT)");

// Execute with bind values
conn->query("INSERT INTO users (name) VALUES (?)", { "Alice" });

// Read results
auto stmt = conn->query("SELECT * FROM users WHERE name = ?", { "Alice" });
while (stmt->fetch()) {
    ar::attribute_map attrs;
    stmt->get_attributes(attrs);
    // attrs["name"] = "Alice", attrs["id"] = 1, etc.
}
```

### Transactions

```cpp
auto conn = ar::config::instance().get_connection();

conn->begin_transaction();
try {
    conn->query("UPDATE accounts SET balance = balance - ? WHERE id = ?", { 100, 1 });
    conn->query("UPDATE accounts SET balance = balance + ? WHERE id = ?", { 100, 2 });
    conn->commit();
} catch (...) {
    conn->rollback();
    throw;
}
```
