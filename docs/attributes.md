# Attributes

An `attribute` is the value type used throughout the library. It wraps a `std::variant` that can hold a string, integer, double, bool, datetime, or null.

---

## Attribute Types

| Type | Enum | Stored As |
|------|------|-----------|
| Null | `type_null` | `std::monostate` |
| String | `type_string` | `std::string` |
| Integer | `type_integer` | `int64_t` |
| Decimal | `type_decimal` | `double` |
| Boolean | `type_bool` | `bool` |
| Date/Time | `type_date` | `ar::datetime` |

Check the type at runtime:

```cpp
ar::attribute value = "hello";

value.type();     // ar::type_string
value.is_null();  // false
```

---

## Creating Attributes

`attribute` has implicit constructors from all common types. You rarely need to write `attribute(...)` explicitly:

```cpp
ar::attribute a1 = "hello";           // string
ar::attribute a2 = 42;               // integer (int → int64_t)
ar::attribute a3 = 3.14;             // decimal
ar::attribute a4 = true;             // bool
ar::attribute a5 = ar::datetime::now(); // datetime
ar::attribute a6;                     // null
```

This is why `model::set()` accepts values directly:

```cpp
user.set("name", "Alice");     // no wrapping needed
user.set("age", 30);
user.set("active", true);
user.set("joined", ar::datetime::now());
user.set("nickname", ar::attribute());  // explicit NULL
```

`const char*` with `nullptr` creates a null attribute:

```cpp
ar::attribute null_val(nullptr);  // type_null, not type_string
```

---

## Reading Attributes

### Type-Safe Getters

Each getter returns the value if the type matches, or throws `std::bad_variant_access` if it doesn't:

```cpp
ar::attribute name = "Alice";

name.as_string();   // "Alice"
name.as_integer();  // throws — it's a string, not an integer
```

| Method | Returns | Throws on mismatch |
|--------|---------|-------------------|
| `as_string()` | `std::string` | Yes |
| `as_integer()` | `int64_t` | Yes |
| `as_real()` | `double` | Yes |
| `as_bool()` | `bool` | Yes |
| `as_date()` | `ar::datetime` | No — see below |

`as_date()` is lenient. It handles datetime values stored as integers (Unix timestamps from SQLite) or strings (from MySQL/PostgreSQL) without throwing:

```cpp
ar::attribute ts = int64_t(1718451000);
ts.as_date();  // datetime from Unix timestamp

ar::attribute ds = "2024-06-15 14:30:00";
ds.as_date();  // datetime parsed from string
```

### Conversion Methods

These convert across types instead of requiring an exact match:

**`to_string()`** — converts any type to its string representation:

```cpp
ar::attribute age = 30;
age.to_string();  // "30"

ar::attribute flag = true;
flag.to_string();  // "1"

ar::attribute null_val;
null_val.to_string();  // ""
```

**`as_numeric()`** — converts to `int64_t`:

```cpp
ar::attribute pi = 3.14;
pi.as_numeric();  // 3 (truncated)

ar::attribute flag = true;
flag.as_numeric();  // 1

ar::attribute null_val;
null_val.as_numeric();  // 0

ar::attribute str = "42";
str.as_numeric();  // 42 (parsed)
```

`as_numeric()` throws `std::invalid_argument` on empty strings or unconvertible values.

---

## NULL Handling

Check for null before accessing the value:

```cpp
ar::attribute value = user.get("nickname");

if (value.is_null()) {
    std::cout << "no nickname set\n";
} else {
    std::cout << value.as_string() << '\n';
}
```

Create a NULL attribute:

```cpp
ar::attribute null_val;           // default constructor
ar::attribute also_null(nullptr); // const char* nullptr
```

---

## Attribute Collections

Two collection types are used throughout the library:

```cpp
ar::attribute_map   // std::map<std::string, attribute> — column name → value
ar::attribute_vector // std::vector<attribute> — ordered list (bind values)
```

`attribute_map` is what each model instance stores internally. `attribute_vector` is used for query bind values and parameter lists.

---

## datetime

`ar::datetime` is a lightweight date/time class backed by a Unix timestamp.

### Creating

```cpp
ar::datetime now = ar::datetime::now();
ar::datetime epoch;                        // 1970-01-01 00:00:00
ar::datetime ts(1718451000);               // from time_t
ar::datetime parsed("2024-06-15 14:30:00"); // from string
ar::datetime components(2024, 6, 15, 14, 30, 0);  // year, month, day, hour, min, sec
```

Month is 1-based (January = 1). Hour/minute/second default to 0.

### Formatting

```cpp
ar::datetime dt(2024, 6, 15, 14, 30, 0);

dt.to_string();       // "2024-06-15 14:30:00"
dt.to_date_string();  // "2024-06-15"
dt.to_time_string();  // "14:30:00"
```

### Components

```cpp
dt.year();    // 2024
dt.month();   // 6
dt.day();     // 15
dt.hour();    // 14
dt.minute();  // 30
dt.second();  // 0
```

### Comparison

All comparison operators are supported, based on the underlying timestamp:

```cpp
ar::datetime a(2024, 1, 1);
ar::datetime b(2024, 6, 15);

a < b;   // true
a == b;  // false
```

### Database Storage

How `datetime` is stored depends on the adapter:

- **SQLite** stores as INTEGER (Unix timestamp). `as_date()` converts from integer.
- **MySQL/PostgreSQL/MS-SQL** store as DATETIME or TIMESTAMP strings. `as_date()` parses from string.

This conversion is handled automatically — you don't need to think about it when using `datetime` fields.
