# Database Adapters Guide

ActiveRecord C++ supports multiple database backends through a unified adapter interface. This guide covers setup and usage for each adapter.

## 🎯 Quick Reference

| Database | CMake Option | vcpkg Package | Default Port |
|----------|-------------|---------------|--------------|
| SQLite | `AR_SQLITE` | `sqlite3` | N/A (file-based) |
| MySQL | `AR_MYSQL` | `libmysql` | 3306 |
| PostgreSQL | `AR_POSTGRES` | `libpq` | 5432 |
| MS-SQL Server | `AR_MSSQL` | System ODBC | 1433 |

## 📦 Installation

### SQLite (Default - No Server Required)

**1. Enable in build:**
```bash
cmake -B build -DAR_SQLITE=ON
cmake --build build --config Release
```

**2. Dependencies installed automatically via vcpkg**

### MySQL

**1. Install MySQL server** (if not already running):
```bash
# Windows (Chocolatey)
choco install mysql

# Linux
sudo apt-get install mysql-server

# macOS
brew install mysql
```

**2. Enable in build:**
```bash
cmake -B build -DAR_MYSQL=ON
cmake --build build --config Release
```

**3. vcpkg automatically installs libmysql**

### PostgreSQL

**1. Install PostgreSQL server** (if not already running):
```bash
# Windows (Chocolatey)
choco install postgresql

# Linux
sudo apt-get install postgresql

# macOS
brew install postgresql
```

**2. Enable in build:**
```bash
cmake -B build -DAR_POSTGRES=ON
cmake --build build --config Release
```

**3. vcpkg automatically installs libpq**

### MS-SQL Server

**1. Install MS-SQL Server** (if not already running):
```bash
# Windows (Chocolatey)
choco install sql-server-express

# Linux
# Follow Microsoft's official Linux installation guide
# https://docs.microsoft.com/en-us/sql/linux/sql-server-linux-setup

# macOS (via Docker)
docker pull mcr.microsoft.com/mssql/server
```

**2. Enable in build:**
```bash
cmake -B build -DAR_MSSQL=ON
cmake --build build --config Release
```

**3. ODBC driver required:** the adapter's server/database/user/password constructor connects with `DRIVER={ODBC Driver 17 for SQL Server}`, so that exact driver must be installed (Windows: `winget install Microsoft.msodbcsql.17`). The legacy `SQL Server` driver that ships with Windows is not enough. Driver 18 is not used by that constructor; it defaults to `Encrypt=yes` and needs `TrustServerCertificate=yes` for a self-signed server certificate. The connection-string constructor can name any driver.
- Linux: `sudo apt-get install unixodbc-dev` plus Microsoft's ODBC driver: [ODBC Driver for SQL Server](https://docs.microsoft.com/en-us/sql/connect/odbc/download-odbc-driver-for-sql-server)

**4. Server settings the adapter needs:** SQL Server authentication ("mixed mode") for user/password logins, and TCP/IP enabled on the port you pass (default 1433). SQL Server Developer and Express installs have both off by default. For the exact commands used for the project's test server, including a local `test` login (`CHECK_POLICY = OFF`, because Windows password policy rejects weak test passwords), see [HARDENING.md](HARDENING.md) "Windows SQL Server".

## 💻 Usage Examples

### SQLite Example

```cpp
#include <activerecord/config.h>
#include <activerecord/adapters/sqlite_adapter.h>
#include <activerecord/model.h>

int main() {
    // Setup SQLite (in-memory or file-based)
    ar::config::instance().set_connections({
        { "default", std::make_shared<ar::sqlite_adapter>(":memory:") }
        // or use a file: std::make_shared<ar::sqlite_adapter>("database.db")
    }, "default");

    // Use the database...
}
```

**Pros:**
- ✅ No server setup required
- ✅ Perfect for testing, embedded apps, small projects
- ✅ Zero configuration

**Cons:**
- ⚠️ Single writer (not ideal for high concurrency)
- ⚠️ File-based limits scalability

### MySQL Example

```cpp
#include <activerecord/config.h>
#include <activerecord/adapters/mysql_adapter.h>
#include <activerecord/model.h>

int main() {
    // Setup MySQL
    ar::config::instance().set_connections({
        { "default", std::make_shared<ar::mysql_adapter>(
            "localhost",  // host
            "myuser",     // username
            "mypass",     // password
            "mydb",       // database
            3306          // port (optional, defaults to 3306)
        )}
    }, "default");

    // Use the database...
}
```

**Pros:**
- ✅ Excellent performance for web applications
- ✅ Mature, widely deployed
- ✅ Great tooling and documentation

**Cons:**
- ⚠️ Requires server setup
- ⚠️ More complex than SQLite

### PostgreSQL Example

```cpp
#include <activerecord/config.h>
#include <activerecord/adapters/postgres_adapter.h>
#include <activerecord/model.h>

int main() {
    // Setup PostgreSQL
    ar::config::instance().set_connections({
        { "default", std::make_shared<ar::postgres_adapter>(
            "localhost",  // host
            "myuser",     // username
            "mypass",     // password
            "mydb",       // database
            5432          // port (optional, defaults to 5432)
        )}
    }, "default");

    // Use the database...
}
```

**Pros:**
- ✅ Advanced features (JSON, arrays, full-text search)
- ✅ Strong SQL standard compliance
- ✅ Excellent for complex queries

**Cons:**
- ⚠️ Requires server setup
- ⚠️ Slightly more memory usage than MySQL

### MS-SQL Server Example

```cpp
#include <activerecord/config.h>
#include <activerecord/adapters/mssql_adapter.h>
#include <activerecord/model.h>

int main() {
    // Setup MS-SQL Server (multiple connection options)

    // Option 1: Server, database, user, password
    ar::config::instance().set_connections({
        { "default", std::make_shared<ar::mssql_adapter>(
            "localhost",  // server
            "mydb",       // database
            "myuser",     // username
            "mypass",     // password
            1433          // port (optional, defaults to 1433)
        )}
    }, "default");

    // Option 2: Connection string
    // auto adapter = std::make_shared<ar::mssql_adapter>(
    //     "DRIVER={ODBC Driver 17 for SQL Server};SERVER=localhost;DATABASE=mydb;UID=myuser;PWD=mypass;"
    // );

    // Option 3: DSN (Data Source Name)
    // auto adapter = std::make_shared<ar::mssql_adapter>("MyDSN", "myuser", "mypass");

    // Use the database...
}
```

**Pros:**
- ✅ Enterprise-grade features and support
- ✅ Excellent Windows integration
- ✅ T-SQL support for advanced queries
- ✅ Great Visual Studio tooling

**Cons:**
- ⚠️ Requires server setup
- ⚠️ Licensing costs for production (Express edition is free)
- ⚠️ ODBC driver needed on Linux/macOS

## 🧪 Running Tests

All adapters share the **same test suite** via parameterized tests:

```bash
# Build with multiple adapters
cmake -B build \
  -DAR_BUILD_TESTS=ON \
  -DAR_SQLITE=ON \
  -DAR_MYSQL=ON \
  -DAR_POSTGRES=ON \
  -DAR_MSSQL=ON

cmake --build build --config Release

# Run tests - automatically tests all enabled adapters
ctest --test-dir build -C Release
```

Every enabled adapter must be reachable with the test credentials (`test`/`test`, database `test_db`): an adapter compiled into the build that cannot connect **fails** its tests instead of skipping them. [HARDENING.md](HARDENING.md) has the server setup (MySQL and PostgreSQL in WSL, MS-SQL on Windows) and the check that confirms the adapter tests actually ran.

**Test output example:**
```
[ RUN      ] AllAdapters/DatabaseAdapterTest.InsertAndRetrieve/SQLite
[       OK ] AllAdapters/DatabaseAdapterTest.InsertAndRetrieve/SQLite (0 ms)
[ RUN      ] AllAdapters/DatabaseAdapterTest.InsertAndRetrieve/MySQL
[       OK ] AllAdapters/DatabaseAdapterTest.InsertAndRetrieve/MySQL (1 ms)
[ RUN      ] AllAdapters/DatabaseAdapterTest.InsertAndRetrieve/PostgreSQL
[       OK ] AllAdapters/DatabaseAdapterTest.InsertAndRetrieve/PostgreSQL (2 ms)
[ RUN      ] AllAdapters/DatabaseAdapterTest.InsertAndRetrieve/MSSQL
[       OK ] AllAdapters/DatabaseAdapterTest.InsertAndRetrieve/MSSQL (3 ms)
```

## 🔄 Switching Adapters

You can easily switch adapters by changing the connection configuration:

```cpp
class User : public ar::model {
public:
    static const char* st_table_name() { return "users"; }
    static const char* st_primary_key() { return "id"; }
    User() : model(st_table_name(), st_primary_key()) {}
};

// Your code stays the same - just swap the adapter!
#ifdef USE_SQLITE
    auto adapter = std::make_shared<ar::sqlite_adapter>(":memory:");
#elif USE_MYSQL
    auto adapter = std::make_shared<ar::mysql_adapter>("localhost", "user", "pass", "db");
#elif USE_POSTGRESQL
    auto adapter = std::make_shared<ar::postgres_adapter>("localhost", "user", "pass", "db");
#elif USE_MSSQL
    auto adapter = std::make_shared<ar::mssql_adapter>("localhost", "db", "user", "pass");
#endif

ar::config::instance().set_connections({{"default", adapter}}, "default");

// This code works with ANY adapter!
User user;
user.set("name", ar::attribute("Alice"));
user.set("email", ar::attribute("alice@example.com"));
user.save();
```

## ⚙️ Connection Pooling

For production applications, consider implementing connection pooling:

```cpp
// Multiple named connections
ar::config::connection_map connections;
connections["primary"] = std::make_shared<ar::postgres_adapter>(...);
connections["replica"] = std::make_shared<ar::postgres_adapter>(...);
connections["cache"] = std::make_shared<ar::sqlite_adapter>(":memory:");

ar::config::instance().set_connections(connections, "primary");

// Use specific connections
ar::query<User> q;
q.where("active = ?", ar::attribute(true)).all();  // Uses primary
```

## 🔐 Security Best Practices

**Always use parameter binding:**
```cpp
// ✅ GOOD - Uses prepared statements
ar::query<User> q;
q.where("email = ?", ar::attribute(user_input)).all();

// ❌ BAD - SQL injection risk!
// q.where("email = '" + user_input + "'").all();  // DON'T DO THIS!
```

All adapters support prepared statements with automatic parameter binding, protecting against SQL injection.

## 📊 Feature Comparison

| Feature | SQLite | MySQL | PostgreSQL | MS-SQL |
|---------|--------|-------|------------|--------|
| Prepared Statements | ✅ | ✅ | ✅ | ✅ |
| Transactions | ✅ | ✅ | ✅ | ✅ |
| Parameter Binding | ✅ | ✅ | ✅ | ✅ |
| LIMIT/OFFSET | ✅ | ✅ | ✅ | ✅ |
| Server Required | ❌ | ✅ | ✅ | ✅ |
| Multi-client | Limited | ✅ | ✅ | ✅ |
| JSON Support | ✅ | ✅ | ✅✅ | ✅ |
| Full-Text Search | ✅ | ✅ | ✅✅ | ✅ |

## 🆘 Troubleshooting

### SQLite: "database is locked"
- Use WAL mode: `PRAGMA journal_mode=WAL;`
- Reduce concurrent writes
- Consider MySQL/PostgreSQL for high concurrency

### MySQL: "Can't connect to server"
```bash
# Check MySQL is running
sudo systemctl status mysql  # Linux
net start MySQL              # Windows

# Test connection
mysql -u root -p
```

### PostgreSQL: "connection refused"
```bash
# Check PostgreSQL is running
sudo systemctl status postgresql  # Linux
pg_ctl status                     # Windows

# Test connection
psql -U postgres
```

### MS-SQL: "Login failed" or "Cannot open database"
```bash
# Windows - Check SQL Server is running
net start MSSQLSERVER

# Test connection with sqlcmd
sqlcmd -S localhost -U sa -P YourPassword

# Check ODBC drivers installed
odbcinst -q -d  # Lists installed ODBC drivers
```

**Common issues:**
- Ensure SQL Server authentication is enabled (mixed mode)
- Firewall may be blocking port 1433
- ODBC Driver 17 or later is recommended
- On Linux: Install `unixodbc` and `msodbcsql17`

### CMake: "Could not find package"
- Ensure vcpkg toolchain is configured
- Run: `vcpkg install sqlite3 libmysql libpq`
- Check: `vcpkg list`

## 📚 Additional Resources

- [SQLite Documentation](https://www.sqlite.org/docs.html)
- [MySQL Documentation](https://dev.mysql.com/doc/)
- [PostgreSQL Documentation](https://www.postgresql.org/docs/)
- [SQL Server Documentation](https://docs.microsoft.com/en-us/sql/sql-server/)
- [ODBC Driver for SQL Server](https://docs.microsoft.com/en-us/sql/connect/odbc/download-odbc-driver-for-sql-server)
- [vcpkg Package Manager](https://vcpkg.io/)
