# Changelog

## Unreleased

### Added

- **Relationships between typed models** (`<activerecord/relations.h>`): `belongs_to`, `has_many`, `has_one` and `build`, named by the foreign-key member and following Rails' and PHP ActiveRecord's behavior. `has_many` returns a chainable `query<Child>`. `preload` loads the parents of many children in one query per 1000 ids, as a map keyed by primary key; `preload_children` loads the children of many parents the same way, as a map from parent id to children. Nothing is cached on the model. See [Relationships](docs/models.md#relationships).

### Documentation

- New [Creating Tables](docs/tables.md) page: the SQL types each database gets, all column modifiers, hand-built schemas for the classic model, and `table_exists()`. The README examples now set up a connection and create their table.
- `ar::transaction` is documented in [Models](docs/models.md#transactions).

### Fixed

- **Transactions on MySQL and SQL Server:** `ar::transaction` threw on these adapters, because the transaction commands were sent as prepared statements. MySQL now sends them as plain queries, and SQL Server uses ODBC's own transaction control (autocommit off, then `SQLEndTran`). Commit, rollback and the automatic rollback are now tested on all four adapters.

## 0.10.0 (alpha), 2026-11-07

### Added

- **Table creation from typed models** (`<activerecord/schema.h>`):
  - column modifiers on `col<>()`: `limit`, `text`, `decimal`, `integer`, `not_null`, `default_value`, `unique`, `index`, `references<U>`, `on_delete`, `on_update`. A modifier that does not fit the member type does not compile;
  - `table_schema_for<T>()` turns a typed model into a plain, validated `ar::table_schema`;
  - `create_table`, `create_tables`, `drop_table`, `drop_tables` (with `if_not_exists` / `if_exists`) and `create_table_sql`;
  - each adapter writes its own SQL (types, auto-increment key, quoting; `ENGINE=InnoDB` on MySQL) and checks its limits before any SQL is sent;
  - `connection::table_exists()` and `connection::enforces_foreign_keys()`;
  - SQLite foreign keys can be turned on with `sqlite_adapter::foreign_keys(true)` or `?foreign_keys=on`.
- `query<T>::find_optional(id)`: returns `std::optional<T>`, empty when no record has that primary key. `find(id)` now also takes a 64-bit id (`std::int64_t`); `long` arguments still work.

### Changed

- Table and column names in a table schema must be plain lowercase identifiers: `a-z`, `0-9` and `_`, not starting with a digit, at most 63 characters.
- JSON and XML output, and `attribute::to_string()`, write `double` values in their shortest exact form (`3.14`, `1e-10`) instead of with six decimal places (`3.140000`), so serialized values no longer lose precision.

### Fixed

- PostgreSQL: `double` values were written with only six decimal places (`1.234567891` became `1.234568`, `1e-10` became `0`). They are now written with full precision.
- README: lazy iteration builds models one row at a time; MySQL and PostgreSQL still receive the whole result first (see `docs/query_api.md`).
- MySQL and PostgreSQL: reading a very small (subnormal) `double` such as `5e-324` threw `std::out_of_range`. It is now read correctly. MS-SQL uses the same locale-independent number parser (SQL Server `FLOAT` cannot store subnormals).

### Known gaps

- Table creation has no migrations or `ALTER TABLE`, and cannot create tables that reference each other in a cycle.
- MS-SQL tables use `VARCHAR` for strings; Unicode text (`NVARCHAR`) is not supported yet.
- The 0.9.1 gaps below remain, except `find_optional()`.

## 0.9.1 (alpha), 2026-10-17

The first public release. The API may change before 1.0.

### Included

- **Adapters:** SQLite (default), MySQL, PostgreSQL and MS-SQL Server, with parameter binding on every adapter.
- **Two model styles:** the dynamic `ar::model` (string-keyed attributes) and `ar::typed_model<T>` (struct members mapped to columns, including enums and `std::optional` for nullable columns).
- **Query API:** `ar::query<T>` with `where` / `where_in` / `where_not`, `order`, `group` / `having`, `joins`, `limit` / `offset`, `find`, `first` / `last` (returning `std::optional`) and their `_or_throw` variants, `count`, `exists`, `delete_all`, lazy iteration and `std::ranges` support.
- **Persistence:** dirty tracking (only changed columns are written), automatic `created_at` / `updated_at`, optimistic locking with a `version` column, primary-key immutability, read-only models.
- **Validations** for `typed_model<T>`: presence, length, format and numericality.
- **Serialization:** JSON and XML, with attribute filtering.
- **Transactions:** an RAII `ar::transaction` that rolls back unless committed.
- **Configuration:** named connections and connection URLs.
- **Packaging:** `find_package(activerecord)` works from an installed build; dependencies come from the bundled vcpkg.
- **Tested platforms:** Windows (MSVC, Visual Studio 2022) and Linux (GCC 14+, Clang 18+).

### Known gaps

- No relationships (`belongs_to`, `has_many`, `has_one`).
- No `find_optional()`; use `where(...).first()` instead.
- No lifecycle callbacks (`before_save`, `after_create`, ...).
- Validations do not yet support uniqueness or validation contexts.
- MS-SQL is tested on Windows only.
- `ar::datetime` is always written as a Unix timestamp; on MySQL, PostgreSQL and MS-SQL, native DATETIME/TIMESTAMP columns can be read but not written.
- `count()` and `exists()` do not support `group()`; they throw.
- `delete_all()` supports `limit()` on MySQL only and `offset()` on no adapter; both throw where unsupported.
- Thread safety of the `ar::config` singleton is not specified.
