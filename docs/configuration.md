# Configuration

All database connections are managed through a singleton `ar::config` instance. Set up connections before creating or querying models.

---

## Basic Setup

```cpp
#include <activerecord/config.h>
#include <activerecord/adapters/sqlite_adapter.h>

ar::config::instance().set_connections({
    {"default", std::make_shared<ar::sqlite_adapter>(":memory:")}
}, "default");
```

The second argument names the default connection. Models that don't specify a connection name use this one.

---

## Connection Methods

### set_connections

Set all connections at once:

```cpp
// Without default — you must specify connection name per model
ar::config::instance().set_connections({
    {"primary", primary_conn},
    {"analytics", analytics_conn}
});

// With default
ar::config::instance().set_connections({
    {"primary", primary_conn},
    {"analytics", analytics_conn}
}, "primary");
```

### set_connection_url

Create a connection from a URL string. The first connection set this way becomes the default automatically:

```cpp
ar::config::instance().set_connection_url("default", "sqlite://myapp.db");
ar::config::instance().set_connection_url("mysql", "mysql://root:pass@localhost:3306/mydb");
```

### set_connection_urls

Create multiple connections from URLs:

```cpp
ar::config::instance().set_connection_urls({
    {"primary", "mysql://root:pass@localhost/mydb"},
    {"dev", "sqlite://dev.db"}
}, "primary");
```

### get_connection

Retrieve a connection by name:

```cpp
auto conn = ar::config::instance().get_connection("primary");

// Or get the default
auto conn = ar::config::instance().get_connection();
```

Throws `ex::config` if the connection name isn't found or no default is set.

---

## Connection URLs

### Format

```
protocol://username:password@host[:port]/database
```

### Examples

```
sqlite://myapp.db
sqlite://../relative/path/to/file.db
sqlite://unix(/absolute/path/to/file.db)
sqlite://windows(c*/absolute/path/to/file.db)

mysql://root:password@localhost:3306/mydb
mysql://root:password@unix(/var/run/mysqld/mysqld.sock)/mydb

postgres://postgres:password@localhost:5432/mydb

mssql://sa:password@localhost:1433/mydb
```

SQLite URLs don't need username, password, or host — just the file path. Use `:memory:` or the path directly after `sqlite://`.

The `windows(...)` syntax handles Windows drive letters. Replace `:` with `*` in the path (e.g., `c*` becomes `c:`).

### Query Parameters

Append query parameters with `?`:

```
postgres://user:pass@localhost/mydb?decode=true
```

| Parameter | Effect |
|-----------|--------|
| `decode=true` | URL-decode the username and password |

The connection encoding is fixed: MySQL connections use `utf8mb4` and PostgreSQL connections `UTF8`. A `charset` parameter is accepted but has no effect.

URL-decoding is useful when credentials contain special characters:

```
mysql://user%40corp:p%40ss%21@localhost/db?decode=true
// Decodes to: user@corp / p@ss!
```

---

## Using Named Connections

A model can specify which connection to use:

```cpp
class User : public ar::model {
  public:
    static constexpr std::string_view st_table_name() { return "users"; }
    static constexpr std::string_view st_primary_key() { return "id"; }

    User() : model(st_table_name(), st_primary_key(), "analytics") {}
    //                                                  ^^^^^^^^^^^ connection name
};
```

If the third argument is empty or omitted, the default connection is used.

---

## Direct Connection Use

You can execute raw SQL through a connection without going through a model:

```cpp
auto conn = ar::config::instance().get_connection();

// DDL
conn->query("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)");

// Insert with bind values
conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "Alice", 30 });

// Transactions
conn->begin_transaction();
conn->query("UPDATE users SET age = ? WHERE name = ?", { 31, "Alice" });
conn->commit();
// or: conn->rollback();
```
