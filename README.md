
### C++ implementation of Active Record pattern
Influenced by [Ruby on Rails active records](https://guides.rubyonrails.org/active_record_basics.html) and by [PHP Active records](https://github.com/jpfuentes2/php-activerecord).
Striving to use latest C++ conventions as possible. We shouldn't be stuck in the past :-)

**Language Standard:** C++23 (prepared for C++26 reflection)

### Prerequisites

- CMake 3.20+
- C++23 compiler (officially supported):
  - **Windows:** MSVC 19.37+ (Visual Studio 2022 17.7+) - **Recommended for static linking**
  - **Linux/macOS:** GCC 14+ (or GCC 13+)
  - **Clang:** Supported for development/testing and sanitizer runs
- Git

**Note:** The project uses MSVC on Windows for static release builds. Clang is supported and recommended for sanitizer-based checks in development.

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

**Clang + leak checks (unit tests):**
```bash
cmake -B build-clang -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DAR_BUILD_TESTS=ON \
  -DAR_SQLITE=ON \
  -DAR_SANITIZE_ADDRESS=ON \
  -DAR_SANITIZE_LEAK=ON

cmake --build build-clang
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-clang --output-on-failure -R AllUnitTests
```

**4. Run example:**
```bash
# Windows
.\build\examples\Release\sqlite_example.exe

# Linux/macOS
./build/examples/sqlite_example
```

**That's it!** Dependencies (SQLite3, GoogleTest) install automatically via `vcpkg.json` manifest.

### What Just Happened?

- ✅ vcpkg is included as a Git submodule (no separate installation needed)
- ✅ `setup.bat`/`setup.sh` bootstraps vcpkg automatically
- ✅ CMake auto-detects the project's vcpkg toolchain
- ✅ Dependencies install automatically via `vcpkg.json` manifest mode
- ✅ Completely self-contained and reproducible builds

### Features & Testing

**Database Adapters:**
- ✅ **SQLite** - Embedded database, zero configuration (default)
- ✅ **MySQL** - Popular open-source database
- ✅ **PostgreSQL** - Advanced open-source database
- ✅ **MS-SQL Server** - Microsoft SQL Server via ODBC

**Comprehensive Test Suite:**
- ✅ **237 tests** with **100% pass rate** (163 unit + 74 integration)
- ✅ **40+ attribute tests** - Type construction, conversions, JSON/XML serialization
- ✅ **36 model tests** - CRUD operations, dirty tracking, readonly mode, record tracking
- ✅ **6 model size tests** - Memory footprint tracking and regression detection
- ✅ **15 SQL builder tests** - SELECT, INSERT, UPDATE with various clauses
- ✅ **7 config tests** - Singleton pattern, connection management
- ✅ **66 integration tests** - Full database operations across all adapters
- GoogleTest integrated via FetchContent for easy setup

**Quality Assurance:**
- Modern C++23 codebase (prepared for C++26 reflection)
- Cross-platform (Windows, Linux, macOS)
- **Static linking on Windows** (MSVC) - No DLL dependencies
- Full parameter binding support (prevents SQL injection)
- Type-safe attribute system
- Comprehensive error handling
- See [CPP23_MIGRATION.md](CPP23_MIGRATION.md) for C++23 features and reflection preparation

**Build System:**
- **Windows:** MSVC with static linking (single executable, no DLLs)
- **Linux/macOS:** GCC with standard toolchain
- Automated dependency management via vcpkg
- Simple build scripts (`build.bat` / `build.sh`)

### C++23 Features & Roadmap

**Current (C++23):**
- 📦 `std::expected` for error handling without exceptions
- 🖨️ `std::print` for modern formatted output
- 🔍 Concepts for better template constraints
- 🎯 Ranges for expressive data processing
- 📝 `std::format` for safe string formatting

**Preparing for C++26 Reflection:**
- Simple struct-based models (reflection-ready)
- Automatic field-to-column mapping
- Type-safe direct field access
- No more string-based attribute access
- Compile-time schema validation

See [php_research.md](php_research.md) for feature comparison with PHP ActiveRecord and planned ORM enhancements (relationships, validations, callbacks).

### Query API - Lazy Loading & Modern C++

**Lazy Iteration** (90%+ memory savings):
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

**Memory Comparison** (100,000 records):
| Method | Memory | Behavior |
|--------|---------|----------|
| `all()` | 50 MB | Loads everything into vector |
| Iterator | 0.5 MB | One row at a time (99% savings!) |
| Iterator + break | 0.05 MB | Only fetches what you use |

### Auto-Timestamps & Optimistic Locking

**Convention-based:** If your table has `created_at`, `updated_at`, or `version` columns, the framework auto-manages them. On new records, call `init_record_tracking(created_at, updated_at, version)` with `true` only for columns that exist in that table.

```cpp
// New record: register only the tracking columns that exist on this table
User alice;
alice.init_record_tracking(true, true, true);  // Registers created_at, updated_at, version
alice.set("name", attribute("Alice"));
alice.save();
// created_at = now, updated_at = now, version = 1

// Loaded records already have all columns from SELECT — auto-detected
auto loaded = query<User>().where("name = ?", attribute("Alice")).first_or_throw();
loaded.set("email", attribute("alice@example.com"));
loaded.save();
// updated_at = now, version = 2 (created_at unchanged)
```

**Optimistic locking** prevents stale overwrites. The UPDATE includes `WHERE version = ?` with the old version:
```cpp
auto copy1 = query<User>().find(1);  // version = 2
auto copy2 = query<User>().find(1);  // version = 2

copy1.set("name", attribute("First"));
copy1.save();  // UPDATE ... SET version=3 WHERE id=1 AND version=2 → succeeds

copy2.set("name", attribute("Second"));
copy2.save();  // UPDATE ... SET version=3 WHERE id=1 AND version=2 → 0 rows affected (stale)
```

**Partial support:** Works with any combination — only `created_at`, only `version`, all three, or none. Models without these columns are completely unaffected.

### Documentation

- **[COMPILERS.md](COMPILERS.md)** - Compiler support strategy (MSVC + GCC focus)
- **[BUILD_WINDOWS.md](BUILD_WINDOWS.md)** - Windows build instructions (MSVC)
- **[DATABASE_ADAPTERS.md](DATABASE_ADAPTERS.md)** - Setting up MySQL, PostgreSQL, MS-SQL
- **[CPP23_MIGRATION.md](CPP23_MIGRATION.md)** - C++23 features and reflection preparation
- **[php_research.md](php_research.md)** - Feature comparison with PHP ActiveRecord
- **[claude_review.md](claude_review.md)** - Code quality review and recommendations

### Model Memory Optimization

**Phase 1 Complete!** ✅
- **Original size:** 224 bytes
- **Current size:** 96 bytes
- **Reduction:** 57% (128 bytes saved per instance!)

**What was optimized:**
- ✅ Removed unused `db_` member (32 bytes)
- ✅ Changed `table_name_` and `primary_key_` to `std::string_view` (32 bytes)
- ✅ Externalized `alias_attributes_` to static method (48+ bytes)

**Real-world impact:**
- 100k models: **22.4 MB → 9.6 MB** (57% savings)
- Combined with lazy iterators: **99.5% total memory reduction!**

See [todo/MODEL_OPTIMIZATION_codex.md](todo/MODEL_OPTIMIZATION_codex.md) and [todo/MODEL_OPTIMIZATION_claude.md](todo/MODEL_OPTIMIZATION_claude.md) for future optimization phases.

### TODO

- Use "conditions" instead of "where" in sqlbuilder. Used in PHP activerecord.

- ✅ **Model size optimization Phase 1** - Completed (96 bytes, 57% reduction)

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

- ✅ **Auto-timestamps** - `created_at` set on insert, `updated_at` refreshed on insert and update (convention-based detection)

- ✅ **Optimistic locking** - `version` attribute auto-incremented on update with WHERE clause check (prevents stale overwrites)

- Possible allowing multiple calls to add_xxx(new) in sql_builder (simply assume: "(org) AND (new)").

- implement delete

- Table creation, field definitions

- Structural binding?

- ✅ SQLite adapter implemented (complete with tests)
- ✅ PostgreSQL adapter implemented (complete with tests)
- ✅ MySQL adapter implemented (complete with tests)
- ✅ MS-SQL adapter implemented (complete with tests)

- escape for json strings and xml

- Add support for converting into json, [JSON for Modern C++](https://github.com/nlohmann/json)

- Use same exception names as Ruby and/or PHP?

- Model validation

- implement guard attributes?!?! Not sure it's helpful?

- callbacks as virtual functions? Is it helpful?

Conventions
===========
- Model Class - Singular with the first letter of each word capitalized (e.g., BookClub).
- Database Table - Plural with underscores separating words (e.g., book_clubs).

Every table should have a primary key by default named id
Foreign keys should be named following the pattern singularized_table_name_id (e.g., item_id, order_id)

