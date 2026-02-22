# Timestamps and Optimistic Locking

ActiveRecord automatically manages `created_at`, `updated_at`, and `version` columns when they're present. This is convention-based — if the columns exist, the framework handles them. If they don't, nothing happens.

---

## Columns

| Column | Type | On INSERT | On UPDATE |
|--------|------|-----------|-----------|
| `created_at` | datetime | Set to current time | Not touched |
| `updated_at` | datetime | Set to current time | Updated to current time |
| `version` | integer | Set to 1 | Incremented by 1 |

For `typed_model<T>`, any combination works. You can have all three, just `version`, just timestamps, or none.
For classic `model` with `init_record_tracking(true)`, all three placeholders (`created_at`, `updated_at`, `version`) are registered together.

---

## Setup

### Classic Model

For records created in code, call `init_record_tracking()` before the first save:

```cpp
User user;
user.init_record_tracking(true);
user.set("name", "Alice");
user.save();
// created_at = now, updated_at = now, version = 1
```

Important: `init_record_tracking(true)` is all-or-nothing for classic `model`. Use it when the table includes the metadata columns you want managed.

Records loaded from a query don't need this — the columns are already present in `attributes_` from the SELECT result, and the framework detects them automatically:

```cpp
auto user = ar::query<User>().where("name = ?", "Alice").first_or_throw();
user.set("email", "alice@example.com");
user.save();
// updated_at = now, version = 2 (created_at unchanged)
```

### Typed Model

Include the tracking columns in your struct and `columns()` definition. That's all — `typed_model<T>` enables tracking by default:

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
user.save();
// user.created_at, user.updated_at, and user.version are all populated after save
```

### Opting Out

Disable tracking per model with the second template parameter:

```cpp
struct ImportRecord : ar::typed_model<ImportRecord, false> {
    static constexpr auto table_name{"import_records"};
    static constexpr auto primary_key{"id"};
    // ...
};
```

Even if the table has `created_at`/`updated_at`/`version` columns, the framework won't touch them.

---

## Optimistic Locking

When a `version` column is present and tracking is enabled, the library uses it to prevent stale overwrites.

### How It Works

**On UPDATE**, the generated SQL includes the version in both SET and WHERE:

```sql
UPDATE users SET name = ?, version = 3, updated_at = ?
WHERE id = 1 AND version = 2
```

If another process changed the record since you loaded it, the WHERE clause won't match (the version in the database is no longer 2). Zero rows are affected, and the library throws `ex::stale_record`.

**On DELETE**, the same guard applies:

```sql
DELETE FROM users WHERE id = 1 AND version = 2
```

### Example

```cpp
auto copy1 = ar::query<User>().find(1);  // version = 2
auto copy2 = ar::query<User>().find(1);  // version = 2

copy1.set("name", "First");
copy1.save();  // succeeds — version becomes 3

copy2.set("name", "Second");
copy2.save();  // throws ex::stale_record — version 2 no longer in database
```

### Handling Stale Records

`ex::stale_record` means the record was modified by someone else since you loaded it. The model's version is rolled back to the old value and it remains dirty. To recover, reload and retry:

```cpp
try {
    user.save();
} catch (const ar::ex::stale_record&) {
    // Reload from database and re-apply changes
    auto fresh = ar::query<User>().find(user.get("id").as_integer());
    fresh.set("name", user.get("name"));
    fresh.save();
}
```

---

## Timestamp Behavior Details

- `created_at` is set once during INSERT and never modified by the framework. Manual changes via `set()` are still possible.
- `updated_at` is set on INSERT and updated on every subsequent UPDATE, even if only non-timestamp fields changed.
- Timestamps use `ar::datetime::now()` at the moment `save()` is called.
- The framework only manages these columns if tracking is enabled **and** the column exists in `attributes_`. For `typed_model<T>`, listing the column in `columns()` is enough. For classic `model`, either load from a query or call `init_record_tracking(true)`.
