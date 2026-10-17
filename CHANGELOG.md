# Changelog

## 0.9.1 (alpha), unreleased

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
- Thread safety of the `ar::config` singleton is not specified.
