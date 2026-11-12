# Creating Tables

A typed model can create its own table from the same `columns()` that maps its members. The modifiers on `col<>()` are optional and only describe the table; they never change how values are read or written. Everything here is in `<activerecord/schema.h>`.

The library creates tables, not databases: create the database and a user that can create tables in it first (see [Database Setup](database_setup.md)). SQLite creates its database file the first time it is opened.

---

## Quick Example

```cpp
#include <activerecord/schema.h>
#include <activerecord/typed_model.h>

struct Author : ar::typed_model<Author> {
    static constexpr auto table_name{ "authors" };
    static constexpr auto primary_key{ "id" };

    std::int64_t id{};
    std::string name{};

    static constexpr auto columns() {
        return ar::columns(ar::col<&Author::id>("id"), ar::col<&Author::name>("name").limit(100));
    }
};

struct Book : ar::typed_model<Book> {
    static constexpr auto table_name{ "books" };
    static constexpr auto primary_key{ "id" };

    std::int64_t id{};
    std::string title{};
    std::optional<std::string> summary{};
    double price{};
    std::int64_t author_id{};

    static constexpr auto columns() {
        return ar::columns(
            ar::col<&Book::id>("id"),
            ar::col<&Book::title>("title").limit(200).index(),
            ar::col<&Book::summary>("summary").text(),
            ar::col<&Book::price>("price").decimal(10, 2).default_value(0),
            ar::col<&Book::author_id>("author_id").references<Author>().on_delete(ar::fk_action::cascade));
    }
};

ar::create_tables<Author, Book>();                    // in this order
ar::create_table<Book>({ .if_not_exists = true });    // false if it already existed
ar::drop_tables<Author, Book>();                      // in reverse: Book, then Author
ar::drop_table<Book>({ .if_exists = true });
std::vector<std::string> sql = ar::create_table_sql<Book>();  // the statements, nothing is run
ar::table_schema schema = ar::table_schema_for<Book>();       // the table as plain data
```

The functions that use a database (all of them except `table_schema_for`) take an optional connection name as their last argument; empty means the default connection.

---

## How `create_table` Works

1. It validates the schema and builds every statement. If anything is invalid, it throws `ar::ex::activerecord` and nothing is sent to the database.
2. With `if_not_exists`, it returns `false` if the table exists. The existing table is not compared or repaired. If another program creates the table after this check, the `CREATE TABLE` fails and the server's error propagates.
3. For a table with foreign keys, it throws `ar::ex::config` if the connection does not enforce them. On SQLite, turn enforcement on first with `foreign_keys(true)` or `?foreign_keys=on` (see [Adapters](adapters.md#sqlite)).
4. It runs the statements one by one: `CREATE TABLE`, then one `CREATE INDEX` per `index()` column. An index is named `ix_<length of table name>_<table>_<column>`.

If a statement fails, its error propagates. There is no automatic rollback or cleanup, so a table created by an earlier statement may remain; remove it with `drop_table`. Inside your own [transaction](models.md#transactions), the database's rules for DDL apply (MySQL, for example, commits DDL immediately).

`create_tables` checks each table separately and stops at the first failure without undoing the tables already created. There is no dependency sorting: list referenced tables first. Tables that reference each other in a cycle are not supported; a table may reference itself.

---

## Column Types

Without modifiers, the member type decides the column:

| Member type | Column | Notes |
|---|---|---|
| `std::int64_t`, other signed integers, signed enums | `bigint` | |
| `float`, `double` | `real` | |
| `bool` | `boolean` | |
| `std::string` | `string` (255) | |
| `ar::datetime` | `datetime` | stored as a Unix timestamp; see [Attributes](attributes.md#database-storage) |
| `std::optional<T>` | as for `T`, nullable | all other columns are NOT NULL |

- **The primary key** must be a `std::int64_t`, and becomes the auto-increment key.
- **Unsigned members** cannot be used in a schema.

Each adapter writes its own SQL:

| Column | SQLite | MySQL | PostgreSQL | SQL Server |
|---|---|---|---|---|
| primary key | `INTEGER PRIMARY KEY AUTOINCREMENT` | `BIGINT AUTO_INCREMENT PRIMARY KEY` | `BIGSERIAL PRIMARY KEY` | `BIGINT IDENTITY(1,1) PRIMARY KEY` |
| `bigint`, `datetime` | `INTEGER` | `BIGINT` | `BIGINT` | `BIGINT` |
| `integer` (`.integer()`) | `INTEGER` | `INT` | `INTEGER` | `INT` |
| `real` | `REAL` | `DOUBLE` | `DOUBLE PRECISION` | `FLOAT` |
| `decimal(p, s)` | `NUMERIC(p,s)` | `DECIMAL(p,s)` | `NUMERIC(p,s)` | `DECIMAL(p,s)` |
| `boolean` | `BOOLEAN` | `BOOLEAN` | `BOOLEAN` | `BIT` |
| `string` (`.limit(n)`) | `TEXT` | `VARCHAR(n)` | `VARCHAR(n)` | `VARCHAR(n)` |
| `text` (`.text()`) | `TEXT` | `TEXT` | `TEXT` | `VARCHAR(MAX)` |

- **MySQL:**
  - tables use `ENGINE=InnoDB`;
  - the library checks MySQL's limits for `VARCHAR`, `DECIMAL`, indexes and row size before any SQL is sent (see [Adapters](adapters.md#mysql)).
- **SQL Server:** strings are `VARCHAR`, not `NVARCHAR`, so non-ASCII text is stored in the server's code page.
- **Inspecting the SQL:** `create_table_sql<T>()` returns the exact statements for the connection, without running them.

---

## Column Modifiers

| Modifier | For | Effect |
|---|---|---|
| `.limit(n)` | strings | `VARCHAR(n)` instead of 255 (not enforced on SQLite) |
| `.text()` | strings | unbounded text |
| `.decimal(p, s)` | `float`, `double` | `DECIMAL(p, s)`: exact on MySQL, PostgreSQL and SQL Server, not on SQLite; the C++ value stays floating-point |
| `.integer()` | integers (not the primary key) | 32-bit `INT` |
| `.not_null()` | optional members | NOT NULL anyway; saving `nullopt` then fails |
| `.default_value(v)` | numbers, `bool`, enums | `DEFAULT v` in the table; a typed save still writes every member |
| `.unique()`, `.index()` | any column | unique constraint, index |
| `.references<U>()` | `std::int64_t`, optional or not | foreign key to `U`'s primary key |
| `.on_delete(a)`, `.on_update(a)` | referencing columns | `ar::fk_action::no_action`, `cascade` or `set_null` (nullable columns only) |

- **Wrong member type:** a modifier that does not fit the member type does not compile.
- **Invalid values:** `limit(0)`, a repeated modifier, or a default that does not fit makes `table_schema_for<T>()` throw `ar::ex::activerecord`.
- **Validation:** the modifiers only shape the table; they do not add validation rules to the model (see [Validations](validations.md)).
- **Relationships:** `references<U>()` is also what [relationships](models.md#relationships) use to find the parent model.

### Names

- **The format:** table and column names must be plain lowercase identifiers: 1 to 63 characters of `a-z`, `0-9` and `_`, not starting with a digit.
- **Why:** the ORM uses names unquoted, so on PostgreSQL a quoted mixed-case table would be a different table.
- **Keywords:** avoid SQL keywords such as `order` or `user` as names.

---

## Classic Models: a Hand-Built Schema

`ar::model` has no `columns()`, so it cannot describe its table. Build an `ar::table_schema` by hand and pass it to `create_table`. The same validation and SQL generation apply.

```cpp
ar::table_schema users{
    .name = "users",
    .primary_key = "id",
    .columns = {
        { .name = "id", .type = ar::column_type::bigint },
        { .name = "name", .type = ar::column_type::string, .length = 100 },
        { .name = "email", .type = ar::column_type::string, .length = 255, .nullable = true, .unique = true },
        { .name = "active", .type = ar::column_type::boolean, .default_value = true },
    },
};

ar::create_table(users, { .if_not_exists = true });
ar::drop_table("users", { .if_exists = true });
```

- **The primary key** must be one of the columns: a non-nullable `bigint` without a default.
- **String columns need a `length`:** the 255 default belongs to `col<>()` on typed models; a hand-built `string` column without one is rejected.
- **A foreign key** is an `ar::foreign_key` in `foreign_keys`: `{ .column = "user_id", .ref_table = "users", .ref_column = "id", .on_delete = ar::fk_action::cascade }`.
- **Any typed model** can give you its schema as data with `table_schema_for<T>()`.

---

## Checking the Database

The connection answers two questions directly:

```cpp
auto conn = ar::config::instance().get_connection();
bool exists = conn->table_exists("books");    // a base table with exactly this name, in the current database
bool fk_on = conn->enforces_foreign_keys();   // false on SQLite unless turned on, and on MySQL with foreign_key_checks off
```

---

## Not Supported Yet

- Migrations and `ALTER TABLE`: a changed model does not change an existing table.
- Tables that reference each other in a cycle.
- JSON and binary column types.
- `NVARCHAR` (Unicode) strings on SQL Server.
