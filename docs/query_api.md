# Query API

All queries start with `ar::query<T>()`, where `T` is a model class. The query builder returns a chainable object — build up the query, then execute it with a terminal method.

```cpp
#include <activerecord/sql_builder.h>

auto users = ar::query<User>()
    .where("active = ?", true)
    .order("name ASC")
    .limit(10)
    .all();
```

---

## Building Queries

### where

Adds a WHERE condition with parameterized values. Use `?` as placeholders.

```cpp
// Single value
ar::query<User>().where("age > ?", 21)

// Multiple values
ar::query<User>().where("age > ? AND role = ?", { 21, "admin" })
```

Multiple `where` calls are combined with AND:

```cpp
ar::query<User>()
    .where("active = ?", true)
    .where("age > ?", 18)
// WHERE (active = ?) AND (age > ?)
```

Each condition is wrapped in parentheses to prevent operator precedence issues. Bind values accumulate in call order.

### where_in

Generates `column IN (?, ?, ...)` with one placeholder per value:

```cpp
ar::query<User>().where_in("role_id", { 1, 2, 3 })
// WHERE role_id IN (?, ?, ?)
```

An empty value list produces `WHERE 1 = 0`, which matches no rows:

```cpp
ar::query<User>().where_in("id", {})
// WHERE 1 = 0  →  always empty result
```

### where_not

Wraps the condition in `NOT (...)`:

```cpp
ar::query<User>().where_not("active = ?", { false })
// WHERE NOT (active = ?)
```

### Combining WHERE Helpers

All `where*` methods accumulate with AND:

```cpp
ar::query<User>()
    .where("active = ?", true)
    .where_not("banned = ?", true)
    .where_in("role_id", { 1, 2, 3 })
// WHERE (active = ?) AND (NOT (banned = ?)) AND (role_id IN (?, ?, ?))
```

### select

Override the default `SELECT *`:

```cpp
ar::query<User>().select("id, name, email")
```

### distinct

Add `DISTINCT` to the SELECT:

```cpp
ar::query<User>().select("role").distinct()
// SELECT DISTINCT role FROM users
```

Calling `distinct()` twice has no effect — it's idempotent.

### order

Set the ORDER BY clause:

```cpp
ar::query<User>().order("created_at DESC")
ar::query<User>().order("name ASC, id DESC")
```

Calling `order()` again replaces the previous ordering (it does not append).

### group and having

```cpp
ar::query<User>()
    .select("role, COUNT(*) as count")
    .group("role")
    .having("COUNT(*) > ?", { 5 })
```

### joins

Add JOIN clauses:

```cpp
ar::query<User>()
    .joins("LEFT JOIN posts ON posts.user_id = users.id")
    .where("posts.published = ?", true)
```

### limit and offset

```cpp
ar::query<User>().order("id").limit(10).offset(20)
```

---

## Executing Queries

### all()

Returns all matching records as a vector:

```cpp
std::vector<User> users = ar::query<User>().where("active = ?", true).all();
```

### find(id)

Find a single record by primary key. Throws `ex::item_not_found` if not found:

```cpp
User user = ar::query<User>().find(42);
```

### find(ids)

Find multiple records by primary keys:

```cpp
std::vector<User> users = ar::query<User>().find({ 1, 2, 3 });
// Returns records found — no error for missing IDs
// Empty input returns empty vector
```

### first() and first_or_throw()

```cpp
// Returns std::optional<User> — nullopt if no match
auto user = ar::query<User>().where("email = ?", "alice@example.com").first();
if (user) {
    std::cout << user->get("name").as_string() << '\n';
}

// Throws ex::item_not_found if no match
User admin = ar::query<User>().where("role = ?", "admin").first_or_throw();
```

### last() and last_or_throw()

Returns the last matching record. If no ORDER BY is set, orders by primary key DESC. If an ORDER BY exists, it reverses the direction:

```cpp
auto newest = ar::query<User>().order("created_at ASC").last();
// Internally: ORDER BY created_at DESC LIMIT 1
```

### count()

Returns the number of matching records:

```cpp
long total = ar::query<User>().where("active = ?", true).count();
```

`count()` strips any ORDER BY, LIMIT, and OFFSET from the query — it always counts all matching rows.

### exists()

Returns `true` if at least one record matches:

```cpp
if (ar::query<User>().where("email = ?", "test@example.com").exists()) {
    // email is taken
}
```

### delete_all()

Deletes all records matching the current query scope. Returns the number of rows deleted:

```cpp
long deleted = ar::query<User>().where("active = ?", false).delete_all();
```

---

## Lazy Iteration

`query<T>` supports range-for loops. Rows are fetched one at a time from the database — not loaded into memory all at once:

```cpp
for (const User& user : ar::query<User>().where("active = ?", true)) {
    std::cout << user.get("name").as_string() << '\n';
}
```

Breaking out of the loop stops fetching:

```cpp
int count = 0;
for (const User& user : ar::query<User>().order("created_at DESC")) {
    process(user);
    if (++count >= 100) break;  // only 100 rows fetched
}
```

### std::ranges

`query<T>` provides an input iterator compatible with C++23 ranges:

```cpp
auto active_emails = ar::query<User>()
    | std::views::filter([](const User& u) { return u.get("active").as_bool(); })
    | std::views::transform([](const User& u) { return u.get("email").as_string(); })
    | std::views::take(50);

for (const auto& email : active_emails) {
    std::cout << email << '\n';
}
```

A note on performance: `std::views::filter` runs in C++, not in the database. For large tables, prefer database-side filtering with `where()`:

```cpp
// Fetches all users, filters in C++
auto slow = ar::query<User>()
    | std::views::filter([](auto& u) { return u.get("age").as_integer() > 30; });

// Filters in the database — only matching rows are fetched
auto fast = ar::query<User>().where("age > ?", 30);
```

Use ranges for transformations and post-processing. Use `where()` for filtering.

---

## Debugging

### to_sql()

Returns the SQL that would be executed, without running it:

```cpp
auto q = ar::query<User>()
    .where("active = ?", true)
    .order("name ASC")
    .limit(10);

std::cout << q.to_sql() << '\n';
// SELECT * FROM users WHERE active = ? ORDER BY name ASC LIMIT 10
```

### bind_values()

Returns the bind values for the current query:

```cpp
const auto& values = q.bind_values();
for (const auto& v : values) {
    std::cout << v.to_string() << '\n';
}
```

These are useful for logging, debugging, and test assertions.
