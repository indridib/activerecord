# Documentation

Reference documentation for ActiveRecord for C++. Each page covers a single feature area with examples.

For installation, building, and a quick overview, see the [main README](../README.md).

---

## Getting Started

- **[Models](models.md)** — defining models with `model` and `typed_model<T>`, CRUD operations, dirty tracking, enum support
- **[Query API](query_api.md)** — building queries, WHERE helpers, retrieval methods, lazy iteration, `std::ranges` integration
- **[Configuration](configuration.md)** — setting up connections, connection URLs, multiple databases

## Core Features

- **[Attributes](attributes.md)** — attribute types, type-safe getters, conversions, NULL handling, `datetime`
- **[Timestamps & Locking](timestamps_and_locking.md)** — `created_at`, `updated_at`, `version`, optimistic locking
- **[Validations](validations.md)** — declarative rules (`presence`, `length`, `format`, `numericality`), error handling

## Infrastructure

- **[Adapters](adapters.md)** — SQLite, MySQL, PostgreSQL, MS-SQL Server setup and behavioral differences
- **[Serialization](serialization.md)** — JSON and XML output with attribute filtering

## Building & Contributing

- **[Compilers](compilers.md)** — supported platforms and compilers, installation
- **[Building on Windows](building_windows.md)** — what `setup.bat` / `build.bat` do, building with CMake directly, other adapters
- **[Database Setup](database_setup.md)** — installing the database servers and client libraries each adapter needs
- **[Hardening](hardening.md)** — pre-release checks: warnings-as-errors builds, clang-tidy, sanitizers, adapter test runs
