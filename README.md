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
- **Typed model validations** — declarative rules with per-attribute error collection
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

A quick taste of both model styles. For full details, see the [documentation](docs/).

### Classic Model

```cpp
#include <model.h>
#include <sql_builder.h>

class User : public ar::model {
  public:
    static constexpr std::string_view st_table_name() { return "users"; }
    static constexpr std::string_view st_primary_key() { return "id"; }
    User() : model(st_table_name(), st_primary_key()) {}
};

User alice;
alice.set("name", "Alice");
alice.set("email", "alice@example.com");
alice.save();

auto user = ar::query<User>().where("name = ?", "Alice").first_or_throw();
std::cout << user.get("name").as_string() << '\n';
```

### typed_model\<T\>

```cpp
#include <typed_model.h>
#include <sql_builder.h>

struct User : ar::typed_model<User> {
    static constexpr auto table_name{"users"};
    static constexpr auto primary_key{"id"};

    long id{};
    std::string name{};
    std::string email{};

    static constexpr auto columns() {
        return ar::columns(
            ar::col<&User::id>("id"),
            ar::col<&User::name>("name"),
            ar::col<&User::email>("email")
        );
    }
};

auto user = ar::query<User>().find(1);
user.email = "new@example.com";
user.save();
```

See the [documentation](docs/) for the full query API, validations, timestamps, optimistic locking, serialization, and more.

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

- **Validation:** uniqueness checks, validation contexts (`on_create`/`on_update`), and richer custom validators.

- **Indexed storage:** migrate internal storage from `std::map<string, attribute>` to `std::vector<attribute>` with a shared column index, reducing per-instance memory and enabling bitset-based dirty tracking.

## License

MIT — see [LICENSE.MIT](LICENSE.MIT) for details.
