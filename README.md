# ActiveRecord for C++

A C++ implementation of the [Active Record pattern](https://en.wikipedia.org/wiki/Active_record_pattern), influenced by [Ruby on Rails](https://guides.rubyonrails.org/active_record_basics.html) and [PHP ActiveRecord](https://github.com/jpfuentes2/php-activerecord).

## Overview

ActiveRecord maps database rows to C++ objects. Each model knows its table and can query, insert, update, and delete itself — keeping domain data and persistence logic together instead of scattering raw SQL across your codebase.

The library offers two model styles: a classic dynamic model with string-based attribute access, and a `typed_model<T>` with direct struct member access. Both share the same query engine, adapter layer, and auto-managed timestamps and optimistic locking.

**Language standard:** C++23 (designed with C++26 reflection in mind)

## Features

- **Multi-database support** — SQLite, MySQL, PostgreSQL, and MS-SQL Server
- **Two model styles** — dynamic `model` (string-based) and `typed_model<T>` (direct member access via CRTP)
- **Lazy query iteration** — rows fetched one at a time, early termination supported
- **`std::optional` results** — `first()` returns optional, `first_or_throw()` for strict access
- **`std::ranges` compatible** — compose queries with views and pipelines
- **Auto-timestamps & optimistic locking** — convention-based `created_at`, `updated_at`, `version`
- **Full parameter binding** — prevents SQL injection across all adapters
- **Type-safe attribute system** — string, integer, float, bool, datetime, null
- **Dirty tracking** — only changed attributes are included in UPDATE statements
- **Cross-platform** — Windows (MSVC, static linking), Linux (GCC), macOS (Clang)
- **Comprehensive test suite** — unit and integration tests via GoogleTest

## Quick Start

vcpkg is included as a Git submodule — no separate installation needed.

**1. Clone with submodules:**
```bash
git clone --recurse-submodules https://github.com/indridib/activerecord.git
cd activerecord
```

**2. Setup (bootstraps vcpkg):**
```bash
# Windows
setup.bat

# Linux/macOS
chmod +x setup.sh && ./setup.sh
```

**3. Build and test:**
```bash
# Windows
build.bat

# Linux/macOS
chmod +x build.sh && ./build.sh

# Or manual CMake (all platforms)
cmake -B build -DAR_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

**4. Run examples:**
```bash
# Windows
.\build\examples\Release\sqlite_example.exe
.\build\examples\Release\typed_model_showcase.exe

# Linux/macOS
./build/examples/sqlite_example
./build/examples/typed_model_showcase
```

Dependencies (SQLite3, GoogleTest) install automatically via the `vcpkg.json` manifest.

## Database Adapters

| Adapter | Backend | Default |
|---------|---------|---------|
| **SQLite** | Embedded, zero configuration | Yes |
| **MySQL** | `libmysqlclient` | No |
| **PostgreSQL** | `libpq` | No |
| **MS-SQL Server** | ODBC | No |

Enable adapters with CMake flags: `-DAR_MYSQL=ON`, `-DAR_POSTGRES=ON`, `-DAR_MSSQL=ON`.

## Usage

### Classic Model (Dynamic Attributes)

The base `model` class uses string-keyed attributes — flexible and straightforward:

```cpp
#include <model.h>
#include <sql_builder.h>

class User : public ar::model {
  public:
    static constexpr std::string_view st_table_name() { return "users"; }
    static constexpr std::string_view st_primary_key() { return "id"; }

    User() : model(st_table_name(), st_primary_key()) {}
};

// Create
User alice;
alice.set("name", "Alice");
alice.set("email", "alice@example.com");
alice.save();

// Query
auto user = ar::query<User>().where("name = ?", "Alice").first_or_throw();
std::cout << user.get("name").as_string() << '\n';

// Update
user.set("email", "alice.new@example.com");
user.save();

// Delete
user.destroy();
```

### typed_model\<T\> (Direct Member Access)

`typed_model<T>` maps struct members directly to database columns. Same query engine, same adapters — but you work with real types instead of strings:

```cpp
#include <sql_builder.h>
#include <typed_model.h>

struct User : ar::typed_model<User> {
    static constexpr auto table_name{"users"};
    static constexpr auto primary_key{"id"};
    enum class status : int { pending = 0, active = 1, disabled = 2 };

    long id{};
    std::string name{};
    std::string email{};
    status account_status{status::pending};

    static constexpr auto columns() {
        return ar::columns(
            ar::col<&User::id>("id"),
            ar::col<&User::name>("name"),
            ar::col<&User::email>("email"),
            ar::col<&User::account_status>("account_status")
        );
    }
};

auto user = ar::query<User>().find(1);
std::cout << user.name << '\n';

user.email = "new@example.com";
user.account_status = User::status::active;
user.save();
```

Key behaviors:
- Query results automatically sync to struct members via `sync_from_attributes()`.
- Save uses snapshot-based dirty detection — only changed members generate SQL.
- Modifying a persisted primary key throws `ex::readonly`.
- After save, members are refreshed (auto-generated id, timestamps, version).
- Enum fields are stored as INTEGER using the underlying type.
- Metadata tracking (`created_at`, `updated_at`, `version`) is enabled by default.
  Opt out per model with `ar::typed_model<MyModel, false>`.

### Query API

**Lazy iteration** — rows are fetched one at a time, not loaded into memory:
```cpp
for (const User& user : ar::query<User>().where("active = ?", true)) {
    std::cout << user.get("name").as_string() << '\n';
}

// Early termination stops fetching
int count = 0;
for (const User& user : ar::query<User>().order("created_at DESC")) {
    process(user);
    if (++count >= 100) break;
}
```

**`std::optional` for safe lookups:**
```cpp
if (auto user = ar::query<User>().where("email = ?", "test@example.com").first()) {
    std::cout << "Found: " << user->get("name").as_string() << '\n';
}

// Throwing variant when absence is exceptional
User admin = ar::query<User>().where("role = ?", "admin").first_or_throw();
```

**`std::ranges` compatible:**
```cpp
auto active_emails = ar::query<User>()
    | std::views::filter([](const User& u) { return u.get("active").as_bool(); })
    | std::views::transform([](const User& u) { return u.get("email").as_string(); })
    | std::views::take(50);

for (const auto& email : active_emails) {
    std::cout << email << '\n';
}
```

### Auto-Timestamps & Optimistic Locking

**Convention-based:** if your table has `created_at`, `updated_at`, or `version` columns, the framework manages them automatically.

For the classic `model`, call `init_record_tracking(true)` on new records to register the columns. Records loaded via `query<T>` already have all columns from SELECT and are detected automatically.

```cpp
User alice;
alice.init_record_tracking(true);
alice.set("name", "Alice");
alice.save();
// created_at = now, updated_at = now, version = 1

auto loaded = ar::query<User>().where("name = ?", "Alice").first_or_throw();
loaded.set("email", "alice@example.com");
loaded.save();
// updated_at = now, version = 2 (created_at unchanged)
```

For `typed_model<T>`, include the fields in `columns()` and tracking works without any extra setup:

```cpp
struct User : ar::typed_model<User> {
    static constexpr auto table_name{"users"};
    static constexpr auto primary_key{"id"};

    long id{};
    std::string name;
    ar::datetime created_at;
    ar::datetime updated_at;
    int64_t version{};

    static constexpr auto columns() {
        return ar::columns(
            ar::col<&User::id>("id"),
            ar::col<&User::name>("name"),
            ar::col<&User::created_at>("created_at"),
            ar::col<&User::updated_at>("updated_at"),
            ar::col<&User::version>("version")
        );
    }
};

User user;
user.name = "Alice";
user.save(); // created_at, updated_at, version managed automatically
```

Disable tracking per model with `ar::typed_model<MyModel, false>`.

**Optimistic locking** prevents stale overwrites. The UPDATE includes `WHERE version = ?` with the old version, and throws `ex::stale_record` if no rows are affected:

```cpp
auto copy1 = ar::query<User>().find(1);  // version = 2
auto copy2 = ar::query<User>().find(1);  // version = 2

copy1.set("name", "First");
copy1.save();  // SET version=3 WHERE id=1 AND version=2 -> succeeds

copy2.set("name", "Second");
copy2.save();  // SET version=3 WHERE id=1 AND version=2 -> 0 rows, throws ex::stale_record
```

**Partial support:** any combination works — only `created_at`, only `version`, all three, or none. Models without these columns are unaffected.

## Building & Testing

### Prerequisites

- CMake 3.20+
- C++23 compiler:
  - **MSVC** 19.39+ (Visual Studio 2022 17.9+)
  - **GCC** 14+ (minimum GCC 13)
  - **Clang** 17+ (latest stable recommended)
- Git

### Running Tests

```bash
cmake -B build -DAR_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure

# Unit tests only
ctest --test-dir build -C Release -R AllUnitTests --output-on-failure
```

### Sanitizer Builds (GCC/Clang)

```bash
cmake -B build-sanitized -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=g++ \
  -DAR_BUILD_TESTS=ON \
  -DAR_SQLITE=ON \
  -DAR_SANITIZE_ADDRESS=ON \
  -DAR_SANITIZE_UNDEFINED=ON

cmake --build build-sanitized
ctest --test-dir build-sanitized --output-on-failure -R AllUnitTests
```

Use `-DCMAKE_CXX_COMPILER=clang++` for Clang.

## Conventions

Following Active Record conventions:

| Concept | Convention | Example |
|---------|-----------|---------|
| **Model class** | Singular, PascalCase | `BookClub` |
| **Database table** | Plural, snake_case | `book_clubs` |
| **Primary key** | `id` by default | `id INTEGER PRIMARY KEY` |
| **Foreign key** | `{singular_table}_id` | `item_id`, `order_id` |

## Roadmap

- **Reflection:** offer a `reflected_model<T>` alongside `typed_model<T>` that auto-discovers struct members, eliminating the manual `columns()` mapping. Pre-C++26: [reflect-cpp](https://github.com/getml/reflect-cpp) behind a build flag. Post-C++26: native reflection. `typed_model<T>` remains the dependency-free default.

- **Table creation / DDL:** generate `CREATE TABLE` from model definitions, with adapter-specific SQL.

- **Model validation:** declarative validation rules checked before save.

- **Indexed storage:** migrate internal storage from `std::map<string, attribute>` to `std::vector<attribute>` with a shared column index, reducing per-instance memory and enabling bitset-based dirty tracking.

## License

MIT — see [LICENSE.MIT](LICENSE.MIT) for details.
