
### C++ implementation of Active Record pattern
Influenced by [Ruby on Rails active records](https://guides.rubyonrails.org/active_record_basics.html) and by [PHP Active records](https://github.com/jpfuentes2/php-activerecord).
Striving to use latest C++ conventions as possible. We shouldn't be stuck in the past :-)

### Prerequisites

- CMake
- A C++17 compiler
- Boost 
- SQLApi, MySQL, PostgreSQL and ODBC client libraries (optional)


### TODO

- Create iterator for query, being able to read one record at a time, without having to read in all the records

- Create iterator for statements

- first() and last() returns null in PHP how can we do that? unique_ptr, empty T() or just throw

- Use "conditions" instead of "where" in sqlbuilder. Used in PHP activerecord.

- Move all variables not needed from model into subclass. Many of them could be virtual or static function or
static variables (connection_, db_, table_name_, primary_key_, alias_attrbutes_)
only real instance variables (not pr. class) are: attributes_, new_record_, dirty, read_only
Would be nice to be able to have the model as small in size as possible, reuducing memory size when working
with them in memory. Need to find a pretty way to be able to define table name etc in a pretty way, preferably
with out using MACROS()
Current the size of model is 224 bytes

- Possible keeping all the attributes in an array and a map from name to index, shared by all the instances.
Would save space, faster loading all the records or? We need to test.
You would then be able to get column map (name -> index) and then for each row a vector of attributes

- Need to solve that the query needs to be able to effectively set attributes for model, clear it etc. 

- Add/review datatypes, what is needed? uint64_t?
PHP Active Record has: STRING, INTEGER, DECIMAL, DATETIME, DATE, TIME
Should we go structural allowing array of variant as a variant? Might solve problems with bindings, i.e. for "IN (?)"

- NOTE: sizeof attribute is now 48 bytes, seems to be main reason sizeof(std::string) is 40. Should we
allocate for std::strings or create our own string class? Skipping std::string makes attribute 16 bytes of size
(guess 8 for other types and then type + alignment). In release version sizeof(std::string) is reduced to
32 bytes hence sizeof(attribute) is 40 bytes. See JSON implemenation JSON for Modern C++, he was  bragging about size. 
One possible solution would be to have a unique_ptr to a std::string. Unique_ptr only takes 8 bytes.

- Better support for date time, chrono or a special class? std::strftime ?

- test std::variant is it same or better -> replace. Some conserns that std::variant wasn't good enough 
hence started with boost.

- implement set_timestamps() called on update and insert. Will refresh updated_at and created_at.

- Possible allowing multiple calls to add_xxx(new) in sql_builder (simply assume: "(org) AND (new)").

- Add unittests [gtest](https://www.bogotobogo.com/cplusplus/google_unit_test_gtest.php)?

- implement insert, delete

- if attribute version is definied make sure it's verified and updated in update()
	set ..., version = version + 1 where id = ? and version = version
	if that fails check if id = ? exist and if so this was an update with old data.
	or maby we don't need to check ?

- Table creation, field definitions

- Structural binding?

- Implement adaptors for SQLite, PostgreSQL, MS-SQL and MySQL without using SQLApi

- escape for json strings and xml

- Add support for converting into json, [JSON for Modern C++](https://github.com/nlohmann/json)

- Use same exception names as Ruby and/or PHP?

- Model validation

- implement guard attributes?!?! Not sure it's helpful?

- callbacks as virtual functions? Is it helpful?

Conventions
===========
- Model Class - Singular with the first letter of each word capitalized (e.g., BookClub).
- Database Table - Plural with underscores separating words (e.g., book_clubs).

Every table should have a primary key by default named id
Foreign keys should be named following the pattern singularized_table_name_id (e.g., item_id, order_id)
