# Agent Instructions & Project Preferences

**Copyright:** 2020-2026 Indridi Bjornsson
**Purpose:** Design criteria and coding standards for everyone working on this codebase, contributors and AI assistants alike: preferences, conventions, contracts, and behaviors.

---

## Scope of This File

This file holds durable working agreements: preferences, conventions, contracts, behaviors. Content must stay true regardless of which feature ships next or how the project is currently organized. Anything that describes **current state** — what's implemented, what's missing, what's in progress, what was recently done, what's planned next — does not belong here. When editing, ask: "could a normal feature shipping make this line wrong?" If yes, it's status — remove it.

---

## Project Philosophy

### Core Principle: Simplicity First
- Keep implementations simple and straightforward (KISS)
- Avoid over-engineering or premature optimization
- Only add complexity when clearly justified (YAGNI)
- "Three lines of code is better than a premature abstraction"
- We don't like warnings in our compiled code

### Forward-Looking
- Always use the latest stable C++ standards (currently C++23)
- Prepare for upcoming features (C++26 reflection)
- Don't be stuck in the past

## C++ coding guidelines
- In the library, name non-public data members with an underscore suffix, e.g. `config_` (examples and tests may differ).
- Keep non-public data members at the bottom of the class declaration.
- Give data members a default member initializer.
- Methods that do not update the state of the class should be `const`. Use `const` and `constexpr` everywhere possible.
- Don't pass `std::string` (or other heavy types) by value just to read it: use `const std::string&`, or `std::string_view` where a view is enough. Pass by value only when the function keeps its own copy, and `std::move` it into place.
- Limit the scope of variables — declare them in the smallest scope possible.
- Mark a method `noexcept` only when it is guaranteed not to throw.
- Return values instead of using out parameters.
- Don't use `new`/`delete`. If an object must be created on the heap, use `std::unique_ptr` or `std::shared_ptr`.
- Run `clang-format` (repo `.clang-format`, LLVM 19) on every C/C++ file you change. If the file wasn't clean before, format only your changed lines; whole-file reformatting goes in a separate formatting-only commit.

### Comments
- Spell-check comments before finishing a change.
- Keep function comments in headers short and focused. State what the function does and any relevant gotchas; document parameters or return values only when notable. Update the comment whenever behavior changes.
- Do not comment inside function bodies unless the code contradicts what a reader would reasonably assume; never restate what the next line already says. Keep all comments as brief as possible.
- We prefer self-explanatory code to comments that explain what it does.

---

## Compiler Strategy

### Supported Platforms and Compilers

Supported means tested: every compiler below passes the [hardening checks](docs/hardening.md) before a release.

1. **MSVC** (Windows) - Visual Studio 2022
   - Primary Windows compiler
   - Enables static linking
   - Best vcpkg integration

2. **GCC** (Linux) - GCC 14+
   - Primary Linux compiler
   - Standard toolchain
   - Excellent C++23 support

3. **Clang** (Linux) - Clang 18+ with libstdc++
   - Also used for clang-tidy and sanitizer builds

**Not supported:**
- ❌ **MinGW** - ABI incompatibility with MSVC-compiled vcpkg libraries
- ❌ Other platforms and compilers - not tested, so no promises

**Rationale:**
- Claiming only what is tested keeps maintenance small
- MSVC provides perfect static linking on Windows
- GCC is the standard on Linux

---

## Linking Strategy

### Strong Preference: Static Linking

We prefer static linking unless dynamic linking saves a lot of space: shared libraries (.dll, .so) always cause deployment problems.

**Windows (MSVC):**
- ✅ **ALWAYS use static linking**
- Eliminates DLL dependencies
- Single executable deployment
- No PATH issues

**Configuration:**
- CMake automatically sets `VCPKG_TARGET_TRIPLET=x64-windows-static` on Windows
- All dependencies (SQLite, etc.) statically linked
- Results in slightly larger executables but zero runtime dependencies

**Linux:**
- Standard dynamic linking is acceptable (system norm)

---

## C++ Standard & Features

### Current: C++23

**Why C++23:**
- Modern features available now
- `std::expected` for better error handling
- `std::print` for cleaner output
- `std::format` for safe string formatting
- Concepts, ranges, and more

**CMakeLists.txt:**
```cmake
set(CMAKE_CXX_STANDARD 23)
```

### Future: C++26 Reflection Ready

Keep the design simple and ready for C++26 reflection when it arrives.

**Guidelines:**
1. Keep models as simple structs with public fields
2. Minimize macro usage (reflection will replace)
3. Use strong typing
4. Avoid complex inheritance hierarchies
5. Keep metadata as constexpr

---

## Naming Conventions

### Build Flags: Short and Clear

**Format:** `AR_*` (not `ACTIVERECORD_*`)

**Examples:**
- ✅ `AR_BUILD_TESTS`
- ✅ `AR_SQLITE`
- ✅ `AR_MYSQL`
- ✅ `AR_POSTGRES`
- ✅ `AR_MSSQL`

---

## Database Adapters

The supported database backends and their build flags:

1. **SQLite** (default) - `AR_SQLITE=ON`
2. **MySQL** - `AR_MYSQL=ON`
3. **PostgreSQL** - `AR_POSTGRES=ON`
4. **MS-SQL** - `AR_MSSQL=ON`

---

## Build System

### Windows: `build.bat`
- Uses MSVC
- Static linking
- Visual Studio generator

### Linux: `build.sh`
- Uses the default compiler (GCC; `CC=clang CXX=clang++` for Clang)
- Standard linking
- Makefiles or Ninja

### vcpkg
- Included as Git submodule
- Automatic dependency installation via `vcpkg.json`
- Bootstrap with `setup.bat` / `setup.sh`

---

## Code Quality Notes

### Code Style Preferences

**String Formatting:**
- ✅ **Prefer `std::format`** where it improves readability and performance (C++23)
- Use for: SQL generation, error messages with multiple variables, complex formatting
- Simple concatenation (like `result += ", "`) is acceptable where it's clearer

**Example:**
```cpp
// ✅ Prefer std::format for complex formatting:
std::string sql = std::format("SELECT {} FROM {}", columns, table);
throw std::runtime_error(std::format("Error: {}", message));

// ✅ Simple concatenation is acceptable:
if (!result.empty()) result += ", ";
placeholders += "?";
```

**Initialization:**
- ✅ **Prefer brace initialization `{}`** where possible (uniform initialization)
- More consistent, prevents narrowing conversions, works with aggregates

**Example:**
```cpp
// ✅ Prefer brace initialization:
std::string name{"John"};
std::vector<int> ids{1, 2, 3};
User user{};
attribute attr{42};

// ⚠️ Also acceptable for compatibility:
std::string name = "John";
User user();  // Only when {} would be ambiguous
```

**Function Qualifiers:**
- `const` and `noexcept`: see [C++ coding guidelines](#c-coding-guidelines)
- ❌ **DO NOT use `[[nodiscard]]`** - project convention: this attribute is not used

**Example:**
```cpp
// ✅ Correct - const and noexcept where appropriate:
class model {
public:
    std::string table_name() const noexcept { return table_name_; }
    bool is_dirty() const noexcept { return dirty_; }
    bool is_new_record() const noexcept { return new_record_; }

    // Non-const because it modifies state
    void set(const std::string& name, const attribute& value);

    // Throws exceptions, so no noexcept
    attribute get(const std::string& name) const;
};

// ❌ AVOID - [[nodiscard]] not used in this project:
[[nodiscard]] bool save();  // Don't use this
```

---

## Runtime Contracts (High Priority)

These are correctness contracts and should be treated as non-negotiable:

1. **Date/Time as `ar::datetime`**
   - When reading SQL date/time/timestamp columns from adapters, convert to `ar::datetime`.
   - Avoid returning raw strings for date/time fields.
   - Normalize adapter-specific timestamp text as needed before parsing.

2. **Primary key immutability for persisted records**
   - Persisted model primary keys must not be modified via normal attribute setters.
   - PK value is loaded from DB or synchronized after insert.

3. **Config lookup behavior**
   - `config::get_connection("")` must throw if there is no explicit default connection.
   - Never silently fallback to the first map entry.

4. **Adapter-specific SQL behavior**
   - MSSQL: `OFFSET/FETCH` requires `ORDER BY`.
   - Empty INSERT payload must use adapter-appropriate SQL (`DEFAULT VALUES` or equivalent).

---

## Testing Best Practices

### GoogleTest Integration
- Tests use GoogleTest with FetchContent (automatic download)
- Unit tests: `tests/unit/` - Test individual classes in isolation
- Integration tests: `tests/integration/` - Test with real database adapters
- Run tests: `ctest --test-dir build -C Release --output-on-failure`

### Test Isolation with Singletons

**IMPORTANT:** The `config` singleton persists state across tests. Always reset state explicitly.

**Problem Example:**
```cpp
// Test 1 sets default
TEST(ConfigTest, Test1) {
    config::instance().set_connections(connections, "default");
    // ...
}

// Test 2 assumes clean state - WRONG!
TEST(ConfigTest, Test2) {
    config::instance().set_connections(connections);
    // default_connection_ still "default" from Test 1!
}
```

**Solution:**
```cpp
TEST(ConfigTest, Test2) {
    config::instance().set_connections(connections, "");  // ✅ Explicitly clear
    auto conn = config::instance().get_connection("");   // ✅ Explicit empty name
}
```

**Best Practice:** Use test fixtures with `SetUp()` and `TearDown()`:
```cpp
class MyTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        auto adapter = std::make_shared<sqlite_adapter>(":memory:");
        config::connection_map connections;
        connections["default"] = adapter;
        config::instance().set_connections(connections, "default");
    }

    void TearDown() override {
        // Clean up if needed
    }
};

TEST_F(MyTestFixture, TestName) {
    // Test uses clean state from SetUp()
}
```

### Model Test Patterns

**Creating Test Models:**
```cpp
class User : public model {
public:
    User() : model(st_table_name(), st_primary_key()) {}  // ✅ Explicit constructor

    static const char* st_table_name() { return "users"; }
    static const char* st_primary_key() { return "id"; }

    // Accessors
    int id() const { return static_cast<int>(get("id").as_integer()); }
    std::string name() const { return get("name").as_string(); }

    // Setters
    void set_name(const std::string& value) { set("name", value); }
};
```

**Note:** There is no `AR_CONSTRUCTORS` macro - always write explicit constructors.

### DateTime Testing

**Creating datetime attributes:**
```cpp
// ✅ From components
datetime dt{2024, 6, 15, 14, 30, 0};

// ✅ From string
datetime dt = datetime::parse("2024-06-15 14:30:00");

// ✅ In queries - use explicit attribute_vector to avoid ambiguity
query<Event> q;
q.where("starts_at >= ?", attribute_vector{attribute{datetime{2024, 6, 12, 0, 0, 0}}});
```

---

## Project Structure

- **Headers:** `include/activerecord/`
- **Sources:** `src/`
- **Adapters:** `src/adapters/` (sqlite, mysql, postgres, mssql)
- **Tests:** `tests/unit/` and `tests/integration/`
- **Examples:** `examples/`

### Conventions
- Model classes: Singular CamelCase (e.g., `User`, `Post`)
- Tables: Plural snake_case (e.g., `users`, `posts`)
- Primary key: `id` (default)

---

## Summary for Contributors and AI Assistants

**Key Preferences:**
- ✅ Support only what is tested: MSVC on Windows, GCC and Clang on Linux
- ✅ Static linking on Windows (no DLLs)
- ✅ Keep code simple
- ✅ Prepare for C++26 reflection
- ✅ Use short build flag names (`AR_*`)
- ✅ Build scripts are `.bat` (Windows) and `.sh` (Linux); runbook snippets may use PowerShell

**Companion Docs:**
- [docs/compilers.md](docs/compilers.md) - Compiler strategy details
- [docs/hardening.md](docs/hardening.md) - Pre-release checks and waived findings

### Review Expectations

When asked to review:

1. Findings first, ordered by severity (high to low)
2. Always include file + line references
3. Prioritize:
   - Runtime correctness and data integrity
   - SQL compatibility by adapter
   - Silent fallback behavior
   - Unimplemented/stubbed public API paths
4. Keep summaries short; focus on actionable defects
5. If no issues are found, say that explicitly and call out remaining test gaps

### TODO/FIXME Convention

Use a stable issue-tagged format for deferred work:

- `TODO(issue-<id>): pending implementation.`

This makes deferred work searchable and avoids accidental "helpful" completion by agents.

---

**Remember:** We value simplicity, modern C++, and avoiding DLL/SO problems. Keep the codebase clean, maintainable, and reflection-ready.
