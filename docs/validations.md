# Validations

`typed_model<T>` supports declarative validation rules. Define a static `validations()` method on your model, and the framework runs all rules before saving. If any rule fails, `save()` returns `false` and the errors are available through `errors()`.

Models without a `validations()` method are unaffected — they save without any checks, same as before.

---

## Defining Validations

Add a `static validations()` method that returns `ar::validate(...)` with one or more rules:

```cpp
#include <activerecord/typed_model.h>
#include <activerecord/validations.h>

struct User : ar::typed_model<User> {
    static constexpr auto table_name{"users"};
    static constexpr auto primary_key{"id"};

    long id{};
    std::string name{};
    std::string email{};
    int age{};

    static constexpr auto columns() {
        return ar::columns(
            ar::col<&User::id>("id"),
            ar::col<&User::name>("name"),
            ar::col<&User::email>("email"),
            ar::col<&User::age>("age")
        );
    }

    static auto validations() {
        return ar::validate(
            ar::presence("name"),
            ar::presence("email"),
            ar::length("name", {.min = 2, .max = 100}),
            ar::format("email", std::regex{R"(^.+@.+$)"}),
            ar::numericality("age", {
                .greater_than = 0,
                .less_than_or_equal_to = 150,
                .only_integer = true
            })
        );
    }
};
```

The `validations()` method follows the same pattern as `columns()` — it returns a tuple of rule objects. Rules are evaluated in order, and all rules run even if earlier ones fail. This gives the caller the complete set of errors in one pass.

---

## Available Validators

### `ar::presence(attribute)`

Checks that the attribute exists, is not null, and (for strings) is not empty.

```cpp
ar::presence("name")
```

Error message: `"can't be blank"`

### `ar::length(attribute, options)`

Checks string length against minimum and/or maximum bounds. Skipped if the attribute is missing or null (use `presence` to require it).

```cpp
ar::length("name", {.min = 2, .max = 100})
ar::length("bio", {.max = 500})          // no minimum
ar::length("code", {.min = 6})           // no maximum
```

Error messages:
- `"is too short (minimum is 2)"`
- `"is too long (maximum is 500)"`

Both `.min` and `.max` are optional. If omitted, that bound is not checked.

### `ar::format(attribute, pattern [, message])`

Checks that the attribute's string value matches a `std::regex` pattern. Skipped if the attribute is missing or null.

```cpp
ar::format("email", std::regex{R"(^.+@.+$)"})
ar::format("zip", std::regex{R"(^\d{5}$)"}, "must be a 5-digit ZIP code")
```

Default error message: `"is invalid"`. Pass a third argument to customize it.

### `ar::numericality(attribute, options)`

Checks numeric constraints. Skipped if the attribute is missing or null.

```cpp
ar::numericality("age", {
    .greater_than = 0,
    .less_than_or_equal_to = 150,
    .only_integer = true
})
```

Available options (all optional):

| Option | Error message |
|--------|--------------|
| `.greater_than` | `"must be greater than N"` |
| `.greater_than_or_equal_to` | `"must be greater than or equal to N"` |
| `.less_than` | `"must be less than N"` |
| `.less_than_or_equal_to` | `"must be less than or equal to N"` |
| `.equal_to` | `"must be equal to N"` |
| `.only_integer` | `"must be an integer"` (rejects decimal values) |

Multiple constraints can be combined. If the value cannot be interpreted as a number, the error is `"is not a number"`.

---

## Checking Validity

### `is_valid()` / `is_invalid()`

Run validations without saving:

```cpp
User user;
user.name = "";
user.email = "bad";
user.age = -1;

if (user.is_invalid()) {
    // inspect errors
}
```

These methods sync struct members to the internal attribute map, clear any previous errors, then run all rules. They are non-const because they mutate the error state.

### `save()` and `save(false)`

`save()` runs validations before inserting or updating. If validation fails, `save()` returns `false` and no SQL is executed:

```cpp
User user;
user.name = "";
user.age = 0;

if (!user.save()) {
    // validation failed, nothing was written to the database
    for (const auto& msg : user.errors().full_messages()) {
        std::cerr << msg << '\n';
    }
}
```

Pass `false` to skip validation entirely:

```cpp
user.save(false);  // no validation, goes straight to INSERT/UPDATE
```

---

## Working with Errors

### `errors()`

Returns a const reference to the `ar::errors` object. Errors are keyed by attribute name.

```cpp
const ar::errors& errs = user.errors();
```

### `has_errors()`

Returns `true` if any validation errors exist:

```cpp
if (user.has_errors()) {
    // handle errors
}
```

### `errors().on(attribute)`

Returns a `string_vector` of error messages for a specific attribute. Returns an empty vector if there are no errors for that attribute:

```cpp
for (const auto& msg : user.errors().on("name")) {
    std::cerr << "name: " << msg << '\n';
}
// name: can't be blank
// name: is too short (minimum is 2)
```

### `errors().full_messages()`

Returns all errors as `"attribute message"` strings:

```cpp
for (const auto& msg : user.errors().full_messages()) {
    std::cerr << msg << '\n';
}
// name can't be blank
// name is too short (minimum is 2)
// email is invalid
// age must be greater than 0
```

### `errors().count()`

Returns the total number of error messages across all attributes:

```cpp
std::size_t n = user.errors().count();  // 4
```

### Errors are cleared automatically

Each call to `is_valid()`, `is_invalid()`, or `save()` clears previous errors before running validations. You don't need to clear them manually:

```cpp
user.name = "";
user.is_valid();          // false, errors populated
user.errors().count();    // 2

user.name = "Alice";
user.is_valid();          // true, previous errors gone
user.errors().count();    // 0
```

---

## Models Without Validations

If a `typed_model<T>` does not define a `validations()` method, `is_valid()` always returns `true` and `save()` proceeds directly to the database. No overhead is added — the check is resolved at compile time with `if constexpr`.

```cpp
struct SimpleUser : ar::typed_model<SimpleUser> {
    static constexpr auto table_name{"users"};
    static constexpr auto primary_key{"id"};

    long id{};
    std::string name{};

    static constexpr auto columns() {
        return ar::columns(
            ar::col<&SimpleUser::id>("id"),
            ar::col<&SimpleUser::name>("name")
        );
    }
    // no validations() — save() always attempts the INSERT/UPDATE
};
```

---

## How It Works

Internally, `typed_model<T>::save()` hides `model::save()`. When `should_validate` is true:

1. Struct members are synced to the internal attribute map (`sync_to_attributes()`).
2. Previous errors are cleared.
3. If `T::validations()` exists (detected via `requires`), all rules are evaluated against the attribute map.
4. If any rule added errors, `save()` returns `false` without executing SQL.
5. If no errors, `model::save(false)` is called (skipping the base class's own validation stub).

The `validations()` return type is a `std::tuple` of rule structs, dispatched with `std::apply` and a fold expression. This means all rules are resolved at compile time — no virtual calls, no heap allocation for the rules themselves.

Errors are stored in a lazily-allocated `std::unique_ptr<ar::errors>` on the base `model`. Models that never use validation pay no memory cost for the error storage.
