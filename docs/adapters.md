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
auto memory = std::make_shared<ar::sqlite_adapter>(":memory:");

// File-based database
auto file = std::make_shared<ar::sqlite_adapter>("/path/to/myapp.db");

ar::config::instance().set_connections({{"default", file}}, "default");
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
- SQLite does not enforce declared column types: the `TEXT` column created for `.limit(n)` accepts longer strings, and `NUMERIC(p,s)` (from `.decimal(p, s)`) neither rounds to the scale nor stores exact decimals.
- SQLite does not always parse floating-point numbers in SQL text exactly: a column `DEFAULT` with an extreme exponent (e.g. `1e-300`) can be stored one unit in the last place off. Values written through the library are bound, not written as text, and keep full precision.
- Foreign keys are not enforced unless you turn them on: `adapter->foreign_keys(true)`, or `?foreign_keys=on` in the URL. It runs `PRAGMA foreign_keys = ON` and checks that it took effect. Inside a transaction SQLite ignores the PRAGMA, so trying to change the setting there throws. `enforces_foreign_keys()` reports the current setting.

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
mysql://root:password@localhost/mydb
```

### Notes

- Requires `libmysqlclient`.
- Empty inserts use `() VALUES()` (MySQL syntax).
- The connection uses `utf8mb4`; the `charset` URL parameter has no effect.
- Tables created by the library use `ENGINE=InnoDB`. Before generating SQL, the library checks MySQL's limits for the server defaults (`utf8mb4`, `ROW_FORMAT=DYNAMIC`, 16 KB pages): `VARCHAR` up to 16383 characters, at most 768 when indexed or unique, no index on `TEXT`, `DECIMAL` scale up to 30, and an estimated row size of at most 65535 bytes. InnoDB can still reject a table for its in-page limit, and on other server settings the limits differ; the server's error then comes through.

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
postgres://user:password@localhost/mydb
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
| ORDER BY and LIMIT in UPDATE/DELETE | Throws on LIMIT; ORDER BY ignored | Yes | Throws on LIMIT; ORDER BY ignored | Throws on LIMIT; ORDER BY ignored |
| OFFSET in UPDATE/DELETE | Throws | Throws | Throws | Throws |

A custom adapter reports UPDATE/DELETE support through `accepts_limit_and_order_for_update_and_delete()`, which is `false` by default. Then a `limit()` on `delete_all()` throws instead of being dropped, so a bounded delete can never remove every matching row.

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
