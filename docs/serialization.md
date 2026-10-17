# Serialization

Models can be serialized to JSON or XML strings. Both formats support filtering which attributes to include.

---

## JSON

### Full Output

```cpp
auto user = ar::query<User>().find(1);
std::cout << user.to_json_string() << '\n';
```

Output:

```json
{"age":30,"email":"alice@example.com","id":1,"name":"Alice"}
```

Attributes are sorted alphabetically by key (they're stored in a `std::map`).

### Include Only Specific Attributes

```cpp
user.to_json_string({"name", "email"});
```

```json
{"email":"alice@example.com","name":"Alice"}
```

### Exclude Attributes

```cpp
user.to_json_string({}, {"id", "age"});
```

```json
{"email":"alice@example.com","name":"Alice"}
```

The first argument (`only`) takes priority. If it's non-empty, only those attributes are included and `exclude` is ignored.

---

## XML

### Full Output

```cpp
auto user = ar::query<User>().find(1);
std::cout << user.to_xml_string() << '\n';
```

Output:

```xml
<users><age>30</age><email>alice@example.com</email><id>1</id><name>Alice</name></users>
```

The root element defaults to the table name.

### Custom Root Element

```cpp
user.to_xml_string("user");
```

```xml
<user><age>30</age><email>alice@example.com</email><id>1</id><name>Alice</name></user>
```

### Filtering

Same `only` and `exclude` parameters as JSON:

```cpp
user.to_xml_string("user", {"name", "email"});
```

```xml
<user><email>alice@example.com</email><name>Alice</name></user>
```

---

## Type Representation

Values are serialized according to their attribute type:

| Type | JSON | XML |
|------|------|-----|
| String | `"value"` | `<key>value</key>` |
| Integer | `42` | `<key>42</key>` |
| Decimal | `3.140000` | `<key>3.140000</key>` |
| Boolean | `true` / `false` | `<key>1</key>` / `<key>0</key>` |
| Datetime | `"2024-06-15 14:30:00"` | `<key>2024-06-15 14:30:00</key>` |
| Null | `null` | `<key />` |

Decimals use six digits after the point. In XML, an empty string is written as `<key />` too.

---

## Free Functions

The serialization methods are also available as free functions operating on attribute maps directly:

```cpp
ar::attribute_map attrs = {
    {"name", "Alice"},
    {"age", 30}
};

std::string json = ar::to_json_string(attrs);
std::string xml = ar::to_xml_string(attrs, "user");
```
