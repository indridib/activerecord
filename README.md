
### C++ implementation of Active Record pattern
Influenced by [Ruby on Rails active records](https://guides.rubyonrails.org/active_record_basics.html) and by [PHP Active records](https://github.com/jpfuentes2/php-activerecord).
Striving to use latest C++ conventions as possible. We shouldn't be stuck in the past :-)

**Language Standard:** C++23 (prepared for C++26 reflection)

### Prerequisites

- CMake 3.20+
- C++23 compiler (officially supported):
  - **Windows:** MSVC 19.37+ (Visual Studio 2022 17.7+) - **Recommended for static linking**
  - **Linux/macOS:** GCC 14+ (or GCC 13+)
- Git

**Note:** The project uses MSVC on Windows for optimal static linking support and GCC on Linux/macOS. Other compilers may work but are not officially tested.

### Quick Start (vcpkg included as submodule!)

**1. Clone with submodules:**
```bash
git clone --recurse-submodules https://github.com/yourusername/activerecord.git
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
- ✅ **121 tests** with **100% pass rate**
- ✅ **40+ attribute tests** - Type construction, conversions, JSON/XML serialization
- ✅ **29 model tests** - CRUD operations, dirty tracking, readonly mode
- ✅ **16 SQL builder tests** - SELECT, INSERT, UPDATE with various clauses
- ✅ **7 config tests** - Singleton pattern, connection management
- ✅ **29 integration tests** - Full database operations with SQLite adapter
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

### Documentation

- **[COMPILERS.md](COMPILERS.md)** - Compiler support strategy (MSVC + GCC focus)
- **[BUILD_WINDOWS.md](BUILD_WINDOWS.md)** - Windows build instructions (MSVC)
- **[DATABASE_ADAPTERS.md](DATABASE_ADAPTERS.md)** - Setting up MySQL, PostgreSQL, MS-SQL
- **[CPP23_MIGRATION.md](CPP23_MIGRATION.md)** - C++23 features and reflection preparation
- **[php_research.md](php_research.md)** - Feature comparison with PHP ActiveRecord
- **[claude_review.md](claude_review.md)** - Code quality review and recommendations

### TODO

- Create iterator for query, being able to read one record at a time, without having to read in all the records

- Create iterator for statements

- first() and last() returns null in PHP how can we do that? unique_ptr, empty T() or just throw

- Use "conditions" instead of "where" in sqlbuilder. Used in PHP activerecord.

- Move all variables not needed from model into subclass. Many of them could be virtual or static function or
static variables (connection_, db_, table_name_, primary_key_, alias_attrbutes_)
only real instance variables (not pr. class) are: attributes_, new_record_, dirty, read_only
Would be nice to be able to have the model as small in size as possible, reuducing memory size when working
with them in memory. Need to find a pretty way to be able to define table name etc in a pretty way, preferably
with out using MACROS()
Current the size of model is 224 bytes

- Possible keeping all the attributes in an array and a map from name to index, shared by all the instances.
Would save space, faster loading all the records or? We need to test.
You would then be able to get column map (name -> index) and then for each row a vector of attributes

- Need to solve that the query needs to be able to effectively set attributes for model, clear it etc. 

- Add/review datatypes, what is needed? uint64_t?
PHP Active Record has: STRING, INTEGER, DECIMAL, DATETIME, DATE, TIME
Should we go structural allowing array of variant as a variant? Might solve problems with bindings, i.e. for "IN (?)"

- NOTE: sizeof attribute is now 48 bytes, seems to be main reason sizeof(std::string) is 40. Should we
allocate for std::strings or create our own string class? Skipping std::string makes attribute 16 bytes of size
(guess 8 for other types and then type + alignment). In release version sizeof(std::string) is reduced to
32 bytes hence sizeof(attribute) is 40 bytes. See JSON implemenation JSON for Modern C++, he was  bragging about size. 
One possible solution would be to have a unique_ptr to a std::string. Unique_ptr only takes 8 bytes.

- Better support for date time, chrono or a special class? std::strftime ?

- test std::variant is it same or better -> replace. Some conserns that std::variant wasn't good enough 
hence started with boost.

- implement set_timestamps() called on update and insert. Will refresh updated_at and created_at.

- Possible allowing multiple calls to add_xxx(new) in sql_builder (simply assume: "(org) AND (new)").

- ✅ GoogleTest framework integrated with 70+ unit and integration tests

- implement insert, delete

- if attribute version is definied make sure it's verified and updated in update()
	set ..., version = version + 1 where id = ? and version = version
	if that fails check if id = ? exist and if so this was an update with old data.
	or maby we don't need to check ?

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
