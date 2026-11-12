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
auto alice = ar::query<User>().where("email = ?", "alice@example.com").first();
if (alice) {
    std::cout << alice->get("name").as_string() << '\n';
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

    std::int64_t id{};
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
auto loaded = ar::query<User>().find(1);
std::cout << loaded.name << '\n';  // direct member access

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

    std::int64_t id{};
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

    std::int64_t id{};
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
| `ar::datetime` | INTEGER/BIGINT | Unix timestamp; see [Attributes](attributes.md#database-storage) |
| `enum` / `enum class` | INTEGER | Via underlying type |
| `std::optional<T>` | Any of the above, nullable | `std::nullopt` maps to NULL |

### Creating Tables

A typed model can create its own table from `columns()`. For the `User` model above:

```cpp
ar::create_table<User>({ .if_not_exists = true });   // <activerecord/schema.h>
```

Column types, modifiers (`limit`, `unique`, `references<U>()`, ...), the SQL each database gets, and hand-built schemas for the classic model are in [Creating Tables](tables.md).

### Relationships

`<activerecord/relations.h>` follows a foreign key between typed models. The examples use the `Author` and `Book` models of [Creating Tables](tables.md#quick-example), extended with accessors. A relationship is named by its foreign-key member, which must be mapped by exactly one column in `columns()` and hold a `std::int64_t` (optional or not). You write one short accessor per relationship:

```cpp
struct Book;
struct Profile;

struct Author : ar::typed_model<Author> {
    // table_name, primary_key, id, name and columns() as in Creating Tables (tables.md#quick-example)
    ar::query<Book> books() const;          // defined after Book: the body needs Book complete
    std::optional<Profile> profile() const; // likewise, after Profile
};

struct Book : ar::typed_model<Book> {
    // ... author_id with .references<Author>(), and an editor_id without references<>
    std::optional<Author> author() const { return ar::belongs_to<&Book::author_id>(*this); }
    std::optional<Author> editor() const { return ar::belongs_to<&Book::editor_id, Author>(*this); }
};

struct Profile : ar::typed_model<Profile> {
    // ... author_id with .references<Author>().unique()
};

inline ar::query<Book> Author::books() const { return ar::has_many<&Book::author_id>(*this); }
inline std::optional<Profile> Author::profile() const { return ar::has_one<&Profile::author_id>(*this); }

auto titles = author.books().order("title").limit(5).all(); // has_many is a query: chain on it
Book draft = ar::build<&Book::author_id>(author);           // a new book with author_id set
draft.title = "Draft";
draft.save();
```

| Function | Returns | Rails / PHP ActiveRecord |
|---|---|---|
| `belongs_to<&Child::fk[, Parent]>(child)` | `std::optional<Parent>` | `belongs_to` |
| `has_many<&Child::fk>(parent)` | `ar::query<Child>`, not yet run | `has_many` |
| `has_one<&Child::fk>(parent)` | `std::optional<Child>` | `has_one` |
| `build<&Child::fk>(parent)` | a new `Child` with only the foreign key set | `build` / `build_association` |
| `preload<&Child::fk[, Parent]>(children)` | `std::unordered_map<std::int64_t, Parent>` | `preload` / `include` |

- **The parent type** comes from `.references<Parent>()` on the column, or is given explicitly for a column without it. A mismatch, an unmapped member or a member that is not a `std::int64_t` does not compile. Relationships work between typed models only.
- **No cache:** unlike Rails and PHP, the model keeps nothing.
  - `belongs_to` with a non-NULL foreign key, and `has_one` on a saved parent, run a query on every call, so calling `author()` twice queries twice.
  - `has_many` only builds a query, which runs when you use it.
  - `build` sends no SQL.
  - Returned records are copies.
- **Unsaved records:**
  - an unsaved child's foreign key is read from its member;
  - an unsaved parent has no children: `has_many` returns a query that matches nothing (running it still sends that query), and `has_one` returns `std::nullopt` without a query. This is this library's rule for any unsaved parent; Rails' collections are likewise empty, and PHP ActiveRecord returns null without a query when the key is NULL;
  - `build` throws for an unsaved parent, because there is no id to set yet.
- **`has_one`** returns the child with the lowest primary key when several match. Add `.unique()` to the column so the database guarantees at most one.
- **Partly loaded records:**
  - a record loaded with `select()` must include the columns a relationship needs, otherwise it throws `ar::ex::activerecord` before any query: the child's foreign key for `belongs_to`, and the parent's primary key for the others;
  - the parent's id is the one loaded from the database, not its (possibly edited) member.
- **Queries and names:** relationship queries run on the default connection. The foreign-key and primary-key names are quoted, so they must be plain, unqualified names.
- **Not yet supported:** eager loading on a query (`includes`), preloading children (`has_many`), `has_many :through`, polymorphic relationships, and adding children through a relationship (`create`, `<<`). The database's `on_delete` handles dependent deletes.

#### Preloading

Calling `author()` for each of 100 books sends 100 queries. `preload` loads the parents of many children at once:

```cpp
std::vector<Book> books = ar::query<Book>().where("year > ?", 2000).all();
auto authors = ar::preload<&Book::author_id>(books); // one query per 1000 distinct authors
for (const Book& book : books) {
    if (auto found = authors.find(book.author_id); found != authors.end()) {
        std::cout << book.title << " by " << found->second.name << "\n";
    }
}
```

- **Input:** any range of the child model, read once: a vector, a `std::span`, a view.
- **The map** is keyed by the parent's primary key and holds copies of the parents.
- **Missing parents are absent**, so look them up with `find` (or `contains`), not `at`. NULL foreign keys are skipped.
- **Queries:** each distinct foreign key is sent once, in queries of at most 1000 ids. No foreign keys, or only NULLs, means no query at all, and not even a connection is needed.
- **Errors:** a persisted child without its foreign-key column throws before any query, as with `belongs_to`. A failing query throws, and nothing is returned.
- **No cache:** the children are not changed; `book.author()` still runs its own query.

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

This prevents accidental primary key corruption. On a new (unsaved) record the check does not apply, but the primary key is not written either: the insert leaves it out, and the record takes the key the database generates.

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

---

## Transactions

`ar::transaction` (`<activerecord/transaction.h>`) groups saves so that either all of them are kept or none are. It begins a transaction on the default connection (or the connection you pass), and rolls back when it goes out of scope unless you call `commit()`:

```cpp
{
    ar::transaction tx;
    from.balance -= 100;
    to.balance += 100;
    if (from.save() && to.save()) {
        tx.commit();
    }
} // without commit(), both updates are rolled back here
```

`save()` returns `false` without throwing when validation fails, so commit only when every save succeeded. The rollback happens when the transaction leaves its scope without a successful `commit()`, whether because a save returned `false` or because one threw.

- **Ending it early:** `rollback()` ends the transaction at once. Calling `commit()` or `rollback()` again does nothing.
- **Nesting:** there are no nested transactions or savepoints; do not start a second `ar::transaction` while one is open on the same connection.
- **Copying:** a transaction cannot be copied or moved.
- **Table creation:** inside a transaction, the database's rules for DDL apply (see [Creating Tables](tables.md#how-create_table-works)).
- **The connection API:** `begin_transaction()`, `commit()` and `rollback()` on a connection do the same without the automatic rollback (see [Adapters](adapters.md#transactions)).
