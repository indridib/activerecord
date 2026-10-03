# Models

ActiveRecord provides two model styles. Both share the same query engine, adapters, and persistence logic.

- **`model`** — dynamic, string-based attribute access. Flexible, no compile-time column mapping.
- **`typed_model<T>`** — struct members map directly to database columns. Type-safe, compile-time checked.

Pick `model` when schema is dynamic or unknown at compile time. Pick `typed_model<T>` when you want direct member access and snapshot-based dirty tracking.

---

## Classic Model

### Defining a Model

```cpp
#include <activerecord/model.h>
#include <activerecord/sql_builder.h>

class User : public ar::model {
  public:
    static constexpr std::string_view st_table_name() { return "users"; }
    static constexpr std::string_view st_primary_key() { return "id"; }

    User() : model(st_table_name(), st_primary_key()) {}
};
```

`st_table_name()` and `st_primary_key()` are required for `query<T>` to work. The constructor passes these to the base `model` class.

### Setting and Getting Attributes

```cpp
User user;
user.set("name", "Alice");
user.set("email", "alice@example.com");
user.set("age", 30);

std::string name = user.get("name").as_string();
int64_t age = user.get("age").as_integer();
```

`set()` accepts any type that `attribute` can be constructed from: `const char*`, `std::string`, `int`, `int64_t`, `long`, `double`, `bool`, `ar::datetime`. No explicit wrapping needed.

The `[]` operator is shorthand for `get()`:

```cpp
std::string name = user["name"].as_string();
```

### Checking Attributes

```cpp
if (user.isset("email")) {
    // attribute exists
}

// Access all attributes as a map
const ar::attribute_map& attrs = user.attributes();
```

### Create (INSERT)

```cpp
User user;
user.set("name", "Alice");
user.set("email", "alice@example.com");
user.save();
// user.is_new_record() is now false
// user.get("id") contains the auto-generated primary key
```

`save()` detects whether the record is new and issues INSERT or UPDATE accordingly.

### Read (SELECT)

```cpp
// Find by primary key — throws if not found
User user = ar::query<User>().find(42);

// Find with optional — returns nullopt if not found
auto user = ar::query<User>().where("email = ?", "alice@example.com").first();
if (user) {
    std::cout << user->get("name").as_string() << '\n';
}
```

See [Query API](query_api.md) for the full query interface.

### Update

```cpp
auto user = ar::query<User>().find(1);
user.set("email", "newemail@example.com");
user.save();
// Only the changed attribute is included in the UPDATE statement
```

Only dirty attributes are written. If nothing changed, `save()` returns `true` without issuing SQL.

### Delete

```cpp
auto user = ar::query<User>().find(1);
user.destroy();
// user.is_new_record() is now true
```

`destroy()` deletes the record by primary key. Returns `false` for new (unsaved) records.

### Mass Assignment

```cpp
// Update attributes without saving
user.set_attributes({
    {"name", "Bob"},
    {"email", "bob@example.com"}
});

// Update attributes and save in one call
user.update_attributes({
    {"name", "Bob"},
    {"email", "bob@example.com"}
});
```

### Attribute Safety

By default, `set()` creates new attributes if they don't exist. To restrict this:

```cpp
user.set("nonexistent", "value", false);  // throws ex::attribute_not_found
```

`update_attribute()` always restricts to existing attributes:

```cpp
user.update_attribute("name", "Charlie");  // updates and saves, no new attributes
```

---

## typed_model\<T\>

### Defining a Typed Model

```cpp
#include <activerecord/sql_builder.h>
#include <activerecord/typed_model.h>

struct User : ar::typed_model<User> {
    static constexpr auto table_name{"users"};
    static constexpr auto primary_key{"id"};

    long id{};
    std::string name;
    std::string email;

    static constexpr auto columns() {
        return ar::columns(
            ar::col<&User::id>("id"),
            ar::col<&User::name>("name"),
            ar::col<&User::email>("email")
        );
    }
};
```

Three things are required:
1. `table_name` — static constexpr string for the database table.
2. `primary_key` — static constexpr string for the primary key column.
3. `columns()` — returns a tuple mapping struct members to column names.

### Working with Typed Models

```cpp
// Create
User user;
user.name = "Alice";
user.email = "alice@example.com";
user.save();
// user.id is now set to the auto-generated primary key

// Query
auto user = ar::query<User>().find(1);
std::cout << user.name << '\n';  // direct member access

// Update
user.email = "new@example.com";
user.save();
// Only email is included in the UPDATE

// Delete
user.destroy();
```

No `set()`/`get()` calls needed. Struct members are the interface.

### How Sync Works

`typed_model<T>` bridges struct members and the underlying `attributes_` map:

- **On query result:** `sync_from_attributes()` copies database values into struct members.
- **On save:** `sync_to_attributes()` copies struct members back into `attributes_`, then `model::save()` does the SQL.
- **After save:** `sync_from_attributes()` runs again to pick up auto-generated values (insert id, timestamps, version).

This happens automatically. You don't call these methods yourself.

### Enum Fields

Enums are stored as integers using the underlying type:

```cpp
struct User : ar::typed_model<User> {
    static constexpr auto table_name{"users"};
    static constexpr auto primary_key{"id"};

    enum class Status : int { pending = 0, active = 1, disabled = 2 };

    long id{};
    Status account_status{Status::pending};

    static constexpr auto columns() {
        return ar::columns(
            ar::col<&User::id>("id"),
            ar::col<&User::account_status>("account_status")
        );
    }
};

user.account_status = User::Status::active;
user.save();  // stored as INTEGER 1
```

### Nullable Fields

Use `std::optional<T>` for columns that can be NULL:

```cpp
struct User : ar::typed_model<User> {
    static constexpr auto table_name{"users"};
    static constexpr auto primary_key{"id"};

    long id{};
    std::string name;
    std::optional<std::string> nickname;  // nullable

    static constexpr auto columns() {
        return ar::columns(
            ar::col<&User::id>("id"),
            ar::col<&User::name>("name"),
            ar::col<&User::nickname>("nickname")
        );
    }
};

user.nickname = "Ali";       // stored as "Ali"
user.nickname = std::nullopt; // stored as NULL
user.save();
```

### Supported Field Types

| C++ Type | Database Type | Notes |
|----------|--------------|-------|
| `std::string` | TEXT/VARCHAR | |
| `bool` | INTEGER/BOOLEAN | |
| `int`, `long`, `int64_t` | INTEGER | All stored as int64_t internally |
| `float`, `double` | REAL/DOUBLE | |
| `ar::datetime` | DATETIME/INTEGER | See [Attributes](attributes.md) |
| `enum` / `enum class` | INTEGER | Via underlying type |
| `std::optional<T>` | Any of the above, nullable | `std::nullopt` maps to NULL |

### Disabling Record Tracking

By default, `typed_model<T>` manages `created_at`, `updated_at`, and `version` columns if they exist. To disable:

```cpp
struct ImportRecord : ar::typed_model<ImportRecord, false> {
    // No automatic timestamp or version management
};
```

See [Timestamps and Locking](timestamps_and_locking.md) for details.

---

## Dirty Tracking

Both model styles track which attributes have been modified since the last load or save. Only dirty attributes are included in UPDATE statements.

### Classic Model

```cpp
User user;
user.set("name", "Alice");

user.is_dirty();                       // true
user.attribute_is_dirty("name");       // true
user.dirty_attributes();               // {"name"}

user.save();
user.is_dirty();                       // false
```

Every call to `set()` marks that attribute as dirty. `save()` clears the dirty set on success.

### Typed Model

`typed_model<T>` uses snapshot-based dirty detection. After a query result or save, a snapshot of all member values is taken. On the next `save()`, each member is compared against its snapshot — only members that actually changed are marked dirty.

```cpp
auto user = ar::query<User>().find(1);
// Snapshot taken: {id: 1, name: "Alice", email: "alice@example.com"}

user.name = "Alice";  // same value — NOT dirty
user.email = "new@example.com";  // different value — dirty

user.save();
// UPDATE users SET email = ? WHERE id = ?
// Only email is in the SET clause
```

---

## Primary Key Immutability

Once a record is persisted (loaded from the database or saved), its primary key cannot be changed. Attempting to do so throws `ex::readonly`:

```cpp
auto user = ar::query<User>().find(1);
user.set("id", 999);  // throws ex::readonly
```

For `typed_model<T>`, the check happens during `sync_to_attributes()` when `save()` is called:

```cpp
auto user = ar::query<User>().find(1);
user.id = 999;
user.save();  // throws ex::readonly
```

This prevents accidental primary key corruption. New (unsaved) records can have their primary key set freely.

---

## Read-Only Mode

Mark a model as read-only to prevent writes:

```cpp
auto user = ar::query<User>().find(1);
user.readonly(true);

user.save();     // throws ex::readonly
user.destroy();  // throws ex::readonly
```

Check the flag with `user.is_readonly()`.
