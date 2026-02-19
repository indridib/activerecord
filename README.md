
### C++ implementation of Active Record pattern
Influenced by [Ruby on Rails active records](https://guides.rubyonrails.org/active_record_basics.html) and by [PHP Active records](https://github.com/jpfuentes2/php-activerecord).
Active Record maps a database row to a C++ object, so you work with models instead of writing raw CRUD code everywhere. Each model knows its table and can load, query, insert, update, and delete itself while keeping domain data and persistence behavior close together.
Striving to use latest C++ conventions as possible. We shouldn't be stuck in the past.

**Language Standard:** C++23 (prepared for C++26 reflection)

### Prerequisites

- CMake 3.20+
- C++23 compiler (officially supported):
  - **Windows:** MSVC 19.39+ (Visual Studio 2022 17.9+) - **Recommended for static linking**
  - **Linux/macOS:** GCC 14+ (minimum GCC 13)
  - **Clang:** Clang 17+ (latest stable recommended)
- Git

**Note:** Official compiler support includes MSVC, GCC, and Clang.

### Quick Start (vcpkg included as submodule!)

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

**3. Build:**
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

**GCC/Clang + sanitizer checks (unit tests):**
```bash
cmake -B build-gcc-sanitized -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=g++ \
  -DAR_BUILD_TESTS=ON \
  -DAR_SQLITE=ON \
  -DAR_SANITIZE_ADDRESS=ON \
  -DAR_SANITIZE_UNDEFINED=ON

cmake --build build-gcc-sanitized
ctest --test-dir build-gcc-sanitized --output-on-failure -R AllUnitTests
```

Use `-DCMAKE_CXX_COMPILER=clang++` for a Clang sanitizer build.

**4. Run example:**
```bash
# Windows
.\build\examples\Release\sqlite_example.exe
.\build\examples\Release\typed_model_showcase.exe

# Linux/macOS
./build/examples/sqlite_example
./build/examples/typed_model_showcase
```

**That's it!** Dependencies (SQLite3, GoogleTest) install automatically via `vcpkg.json` manifest.

### What Just Happened?

- ✅ vcpkg is included as a Git submodule (no separate installation needed)
- ✅ `setup.bat`/`setup.sh` bootstraps vcpkg automatically
- ✅ CMake auto-detects the project's vcpkg toolchain
- ✅ Dependencies install automatically via `vcpkg.json` manifest mode
- ✅ Completely self-contained and reproducible builds

### Features & Testing

**General:**
- Modern C++23 codebase (prepared for C++26 reflection)
- Cross-platform (Windows, Linux, macOS)
- **Static linking on Windows** (MSVC) - No DLL dependencies
- Full parameter binding support (prevents SQL injection)
- Type-safe attribute system
- Comprehensive error handling

**Database Adapters:**
- ✅ **SQLite** - Embedded database, zero configuration (default)
- ✅ **MySQL** - Popular open-source database
- ✅ **PostgreSQL** - Advanced open-source database
- ✅ **MS-SQL Server** - Microsoft SQL Server via ODBC

**Tests Suite:**
- ✅ Extensive unit and integration coverage (GoogleTest)
- ✅ Unit coverage includes typed model mapping and sync behavior
- ✅ **40+ attribute tests** - Type construction, conversions, JSON/XML serialization
- ✅ **36+ model tests** - CRUD operations, dirty tracking, readonly mode, record tracking
- ✅ **6 model size tests** - Memory footprint tracking and regression detection
- ✅ **15 SQL builder tests** - SELECT, INSERT, UPDATE with various clauses
- ✅ **7 config tests** - Singleton pattern, connection management
- ✅ Integration tests across enabled adapters (SQLite by default)
- GoogleTest integrated via FetchContent for easy setup

**Build System:**
- **Windows:** MSVC with static linking (single executable, no DLLs)
- **Linux/macOS:** GCC with standard toolchain
- Automated dependency management via vcpkg
- Simple build scripts (`build.bat` / `build.sh`)

### Roadmap

**Preparing for C++26 Reflection:**
- Simple struct-based models (reflection-ready)
- Automatic field-to-column mapping
- Type-safe direct field access
- No more string-based attribute access
- Compile-time schema validation

### Query API - Lazy Loading & Modern C++

**Lazy Iteration**:
```cpp
// Process records one-at-a-time without loading everything into memory
for (const User& user : query<User>().where("active = ?", {true})) {
    std::cout << user.get("name").to_string() << '\n';
    // Only one row in memory at a time!
}

// Early termination - only fetches what you need
int count = 0;
for (const User& user : query<User>().order("created_at DESC")) {
    send_welcome_email(user);
    if (++count >= 100) break;  // Stops at 100, doesn't fetch remaining rows!
}
```

**std::optional for Single Results** (no exceptions for "not found"):
```cpp
// first() returns std::optional - safe handling of empty results
if (auto user = query<User>().where("email = ?", {"test@example.com"}).first()) {
    std::cout << "Found: " << user->get("name").to_string() << '\n';
} else {
    std::cout << "User not found\n";
}

// last() also returns std::optional
auto newest = query<User>().order("created_at DESC").first();
auto oldest = query<User>().order("created_at ASC").first();

// Throwing variants available when absence is exceptional
try {
    User admin = query<User>().where("role = ?", {"admin"}).first_or_throw();
    grant_admin_access(admin);
} catch (const ex::item_not_found&) {
    log_critical("No admin user exists!");
}
```

**Works with std::ranges**:
```cpp
// Compose queries with views for expressive data processing
auto active_emails = query<User>()
    | std::views::filter([](const User& u) {
        return u.get("active").as_bool();
    })
    | std::views::transform([](const User& u) {
        return u.get("email").to_string();
    })
    | std::views::take(50);  // Only fetches 50 matching users!

for (const auto& email : active_emails) {
    std::cout << email << '\n';
}
```

### typed_model<T> (Direct Member Access)

The new `typed_model<T>` API lets models expose plain typed members while still using
the existing `model`/`query<T>` infrastructure.

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

Behavior notes:
- Query hydration calls `sync_from_attributes()` automatically in `first()` and iterator paths.
- Save uses snapshot diff; only changed mapped members are marked dirty.
- Persisted primary key changes throw `ex::readonly`.
- After successful save, members are refreshed from attributes (insert id, timestamps, version).
- Tracking defaults to enabled for `created_at`, `updated_at`, and `version`.
- Per-model opt-out is available via template argument:
  - `ar::typed_model<MyModel, false>` (default is `true`)
- Enum fields are supported and stored as INTEGER using the enum's underlying type.

### Auto-Timestamps & Optimistic Locking

**Convention-based:** If your table has `created_at`, `updated_at`, or `version` columns, the framework auto-manages them.

- For classic `model` usage (`set/get`), tracking is strict all-or-nothing: call `init_record_tracking(true)` to enable metadata tracking as a group, or `init_record_tracking(false)` to disable all metadata tracking.
- For `typed_model<T>`, if those fields are declared and mapped in `columns()`, they are synchronized into attributes automatically, so tracking works without an explicit `init_record_tracking(...)` call.

```cpp
// Classic model: simple on/off metadata tracking.
User alice;
alice.init_record_tracking(true);  // Registers created_at, updated_at, version
alice.set("name", "Alice");
alice.save();
// created_at = now, updated_at = now, version = 1

// Loaded records already have all columns from SELECT — auto-detected
auto loaded = query<User>().where("name = ?", {"Alice"}).first_or_throw();
loaded.set("email", "alice@example.com");
loaded.save();
// updated_at = now, version = 2 (created_at unchanged)
```

```cpp
// typed_model<T>: metadata tracking is enabled by default.
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
user.save(); // created_at/updated_at/version managed automatically
```

```cpp
// typed_model<T, false> disables metadata tracking for this model.
struct ImportRow : ar::typed_model<ImportRow, false> {
    static constexpr auto table_name{"import_rows"};
    static constexpr auto primary_key{"id"};

    long id{};
    std::string payload;
    ar::datetime updated_at; // can still be mapped/read/written as normal field
    int64_t version{};

    static constexpr auto columns() {
        return ar::columns(
            ar::col<&ImportRow::id>("id"),
            ar::col<&ImportRow::payload>("payload"),
            ar::col<&ImportRow::updated_at>("updated_at"),
            ar::col<&ImportRow::version>("version")
        );
    }
};
```

**Optimistic locking** prevents stale overwrites by rejecting stale updates and throwing `ex::stale_record`. The UPDATE includes `WHERE version = ?` with the old version:
```cpp
auto copy1 = query<User>().find(1);  // version = 2
auto copy2 = query<User>().find(1);  // version = 2

copy1.set("name", "First");
copy1.save();  // UPDATE ... SET version=3 WHERE id=1 AND version=2 → succeeds

copy2.set("name", "Second");
copy2.save();  // UPDATE ... SET version=3 WHERE id=1 AND version=2 → 0 rows affected (stale)
```

**Partial support:** Works with any combination — only `created_at`, only `version`, all three, or none. Models without these columns are completely unaffected.

### Running Tests

```bash
# Configure (includes unit + integration tests when adapters are enabled)
cmake -B build -DAR_BUILD_TESTS=ON

# Build tests
cmake --build build --config Release

# Run all tests
ctest --test-dir build -C Release --output-on-failure

# Run unit tests only
ctest --test-dir build -C Release -R AllUnitTests --output-on-failure
```


### TODO

- Table creation, field definitions

- Model validation

- Reflection: offer a reflected_model<T> alongside typed_model<T> that auto-discovers struct members, eliminating the manual columns() mapping. Same typed member access, same save()/query<T> infrastructure — just zero registration. Pre-C++26: reflect-cpp (github.com/getml/reflect-cpp) behind a build flag. Post-C++26: native reflection replaces it. typed_model<T> remains the dependency-free default.

- implement guard attributes?!?! Not sure it's helpful?

- callbacks as virtual functions? Is it helpful?

- Structural binding?

- Use "conditions" instead of "where" in sqlbuilder. Used in PHP activerecord.

- Future optimization phases (see MODEL_OPTIMIZATION docs):
  - Phase 1.5: Remove `connection_` member (additional 16 bytes)
  - Phase 2: Indexed storage (`std::vector<attribute>` + bitset dirty tracking)
  - CRTP alternative: `model_base<T>` for class-level metadata

- Possible keeping all the attributes in an array and a map from name to index, shared by all the instances.
Would save space, faster loading all the records or? We need to test.
You would then be able to get column map (name -> index) and then for each row a vector of attributes

- Indexed storage roadmap:
  - Keep current map API surface for compatibility (`get/set` by name).
  - Internally migrate toward `std::vector<attribute>` + shared `name -> index` schema.
  - Replace string-set dirty tracking with index-based dirty flags/bitset.

- Need to solve that the query needs to be able to effectively set attributes for model, clear it etc. 

- Add/review datatypes, what is needed? uint64_t?
PHP Active Record has: STRING, INTEGER, DECIMAL, DATETIME, DATE, TIME
Should we go structural allowing array of variant as a variant? Might solve problems with bindings, i.e. for "IN (?)"

- Possible allowing multiple calls to add_xxx(new) in sql_builder (simply assume: "(org) AND (new)").


Conventions
===========
- Model Class - Singular with the first letter of each word capitalized (e.g., BookClub).
- Database Table - Plural with underscores separating words (e.g., book_clubs).

Every table should have a primary key by default named id
Foreign keys should be named following the pattern singularized_table_name_id (e.g., item_id, order_id)

