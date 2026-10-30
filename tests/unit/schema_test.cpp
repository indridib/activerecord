// ============================================================================
// schema_test.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// Unit tests for column modifiers, table_schema_for<T>() and validate()
//

#include <activerecord/schema.h>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <optional>
#include <string>

using namespace ar;

namespace {

// NOLINTNEXTLINE(performance-enum-size) -- int base is deliberate: a signed underlying type
enum class priority : int { low = 1, high = 2 };
// NOLINTNEXTLINE(performance-enum-size) -- unsigned base is the point of this type
enum class flags : unsigned { none = 0 };

struct Author : typed_model<Author> {
	static constexpr auto table_name{ "authors" };
	static constexpr auto primary_key{ "id" };

	std::int64_t id{};
	std::string name{};

	static constexpr auto columns() {
		return ar::columns(col<&Author::id>("id"), col<&Author::name>("name").limit(100).unique());
	}
};

struct Book : typed_model<Book> {
	static constexpr auto table_name{ "books" };
	static constexpr auto primary_key{ "id" };

	std::int64_t id{};
	std::string title{};
	std::optional<std::string> summary{};
	double price{};
	bool in_print{};
	priority level{ priority::low };
	int pages{};
	datetime published{};
	std::int64_t author_id{};
	std::optional<std::int64_t> series_id{};

	static constexpr auto columns() {
		return ar::columns(col<&Book::id>("id"), col<&Book::title>("title").limit(200).index(),
						   col<&Book::summary>("summary").text(),
						   col<&Book::price>("price").decimal(10, 2).default_value(0),
						   col<&Book::in_print>("in_print").default_value(true),
						   col<&Book::level>("level").default_value(priority::high),
						   col<&Book::pages>("pages").integer(), col<&Book::published>("published"),
						   col<&Book::author_id>("author_id").references<Author>().on_delete(fk_action::cascade),
						   col<&Book::series_id>("series_id").references<Book>().on_delete(fk_action::set_null));
	}
};

struct MutualB;

struct MutualA : typed_model<MutualA, false> {
	static constexpr auto table_name{ "mutual_a" };
	static constexpr auto primary_key{ "id" };

	std::int64_t id{};
	std::optional<std::int64_t> b_id{};

	static constexpr auto columns() {
		return ar::columns(col<&MutualA::id>("id"), col<&MutualA::b_id>("b_id").references<MutualB>());
	}
};

struct MutualB : typed_model<MutualB> {
	static constexpr auto table_name{ "mutual_b" };
	static constexpr auto primary_key{ "id" };

	std::int64_t id{};
	std::int64_t a_id{};

	static constexpr auto columns() {
		return ar::columns(col<&MutualB::id>("id"), col<&MutualB::a_id>("a_id").references<MutualA>());
	}
};

// A model whose primary key is wrong in different ways, and models referencing it
struct MissingPk : typed_model<MissingPk> {
	static constexpr auto table_name{ "missing_pk" };
	static constexpr auto primary_key{ "id" };
	std::int64_t key{};
	static constexpr auto columns() { return ar::columns(col<&MissingPk::key>("key")); }
};

struct DuplicatePk : typed_model<DuplicatePk> {
	static constexpr auto table_name{ "duplicate_pk" };
	static constexpr auto primary_key{ "id" };
	std::int64_t id{};
	std::int64_t other{};
	static constexpr auto columns() { return ar::columns(col<&DuplicatePk::id>("id"), col<&DuplicatePk::other>("id")); }
};

struct OptionalPk : typed_model<OptionalPk> {
	static constexpr auto table_name{ "optional_pk" };
	static constexpr auto primary_key{ "id" };
	std::optional<std::int64_t> id{};
	static constexpr auto columns() { return ar::columns(col<&OptionalPk::id>("id")); }
};

struct NarrowPk : typed_model<NarrowPk> {
	static constexpr auto table_name{ "narrow_pk" };
	static constexpr auto primary_key{ "id" };
	int id{};
	static constexpr auto columns() { return ar::columns(col<&NarrowPk::id>("id")); }
};

template <class Target> struct RefersTo : typed_model<RefersTo<Target>> {
	static constexpr auto table_name{ "refers" };
	static constexpr auto primary_key{ "id" };
	std::int64_t id{};
	std::int64_t target_id{};
	static constexpr auto columns() {
		return ar::columns(col<&RefersTo::id>("id"),
						   col<&RefersTo::target_id>("target_id").template references<Target>());
	}
};

// Valid primary key, but another column is invalid: referencing it must not look further than the key
struct ValidPkBadColumn : typed_model<ValidPkBadColumn> {
	static constexpr auto table_name{ "valid_pk_bad_column" };
	static constexpr auto primary_key{ "id" };
	std::int64_t id{};
	std::string name{};
	static constexpr auto columns() {
		return ar::columns(col<&ValidPkBadColumn::id>("id"), col<&ValidPkBadColumn::name>("name").limit(0));
	}
};

// Members used to try modifiers on every kind of column
struct Kinds {
	std::int64_t id{};
	std::string text_value{};
	std::optional<std::string> optional_text{};
	double real_value{};
	bool flag{};
	int small{};
	priority level{ priority::low };
	flags unsigned_enum{ flags::none };
	unsigned count{};
	datetime when{};
	std::optional<std::int64_t> optional_id{};
};

template <class C>
concept has_limit = requires(C c) { c.limit(1); };
template <class C>
concept has_text = requires(C c) { c.text(); };
template <class C>
concept has_decimal = requires(C c) { c.decimal(10, 2); };
template <class C>
concept has_integer = requires(C c) { c.integer(); };
template <class C>
concept has_not_null = requires(C c) { c.not_null(); };
template <class C, class V>
concept has_default = requires(C c, V v) { c.default_value(v); };
template <class C, class U>
concept has_references = requires(C c) { c.template references<U>(); };

using id_col = decltype(col<&Kinds::id>("id"));
using text_col = decltype(col<&Kinds::text_value>("t"));
using optional_text_col = decltype(col<&Kinds::optional_text>("o"));
using real_col = decltype(col<&Kinds::real_value>("r"));
using flag_col = decltype(col<&Kinds::flag>("f"));
using small_col = decltype(col<&Kinds::small>("s"));
using level_col = decltype(col<&Kinds::level>("l"));
using when_col = decltype(col<&Kinds::when>("w"));
using optional_id_col = decltype(col<&Kinds::optional_id>("x"));

// Wrong-kind modifiers do not exist
static_assert(has_limit<text_col> && has_limit<optional_text_col> && !has_limit<id_col> && !has_limit<real_col>);
static_assert(has_text<text_col> && !has_text<id_col>);
static_assert(has_decimal<real_col> && !has_decimal<text_col> && !has_decimal<id_col>);
static_assert(has_integer<small_col> && has_integer<id_col> && !has_integer<flag_col> && !has_integer<real_col>);
static_assert(has_not_null<optional_text_col> && has_not_null<optional_id_col> && !has_not_null<text_col>);
static_assert(has_default<flag_col, bool> && !has_default<flag_col, int>);
static_assert(has_default<small_col, int> && has_default<small_col, long long> && !has_default<small_col, bool> &&
			  !has_default<small_col, double>);
static_assert(has_default<real_col, int> && has_default<real_col, double> && !has_default<real_col, bool>);
static_assert(has_default<level_col, priority> && !has_default<level_col, int>);
static_assert(!has_default<text_col, int> && !has_default<when_col, int>);
static_assert(has_references<id_col, Author> && has_references<optional_id_col, Author>);
static_assert(!has_references<small_col, Author> && !has_references<text_col, Author>);
static_assert(!has_references<id_col, void>);
using referencing_col = decltype(col<&Kinds::id>("id").references<Author>());
static_assert(!has_references<referencing_col, Book>, "a second references<>() does not compile");

// Member types a schema accepts; bool is accepted although std::is_unsigned_v<bool> is true
static_assert(detail::schema_value<bool> && detail::schema_value<priority> && detail::schema_value<int> &&
			  detail::schema_value<std::string> && detail::schema_value<datetime> && detail::schema_value<double>);
static_assert(!detail::schema_value<decltype(Kinds::count)> && !detail::schema_value<decltype(Kinds::unsigned_enum)> &&
			  !detail::schema_value<long double> && !detail::schema_value<std::uint64_t>);

// The descriptor stays a trivially copyable aggregate, and existing columns keep their interface
static_assert(std::is_aggregate_v<id_col> && std::is_trivially_copyable_v<id_col>);
static_assert(std::is_trivially_copyable_v<decltype(col<&Book::author_id>("author_id").references<Author>())>);
static_assert(std::same_as<id_col::member_type, std::int64_t> && std::same_as<id_col::owner_type, Kinds>);

// Defaults are evaluated at compile time
constexpr auto price_column{ col<&Book::price>("price").default_value(0) };
static_assert(price_column.options.default_value.kind == default_literal::kind_type::real);
static_assert(price_column.options.default_value.real == 0.0);
static_assert(col<&Kinds::level>("l").default_value(priority::high).options.default_value.integer == 2);
static_assert(
	col<&Kinds::id>("id").default_value(std::numeric_limits<std::uint64_t>::max()).options.default_value.invalid);
static_assert(
	!col<&Kinds::id>("id").default_value(std::numeric_limits<std::int64_t>::min()).options.default_value.invalid);
static_assert(
	col<&Kinds::real_value>("r").default_value(std::numeric_limits<double>::infinity()).options.default_value.invalid);
static_assert(
	col<&Kinds::real_value>("r").default_value(std::numeric_limits<double>::quiet_NaN()).options.default_value.invalid);

template <class Column> column_schema column_of(const Column& descriptor) {
	const table_schema schema{ "kinds", "id", {}, {} };
	return detail::make_column(descriptor, schema).column;
}

const column_schema& find_column(const table_schema& schema, std::string_view name) {
	for (const auto& column : schema.columns) {
		if (column.name == name) {
			return column;
		}
	}
	throw std::runtime_error("no such column");
}

table_schema minimal_schema() {
	table_schema schema{ "things", "id", {}, {} };
	schema.columns.push_back(column_schema{ "id" });
	return schema;
}

} // namespace

// ==============================================================================
// table_schema_for<T>()
// ==============================================================================

TEST(SchemaTest, MapsMemberTypesAndModifiers) {
	const table_schema schema{ table_schema_for<Book>() };
	EXPECT_EQ("books", schema.name);
	EXPECT_EQ("id", schema.primary_key);
	ASSERT_EQ(10U, schema.columns.size());

	EXPECT_EQ(column_type::bigint, find_column(schema, "id").type);
	EXPECT_FALSE(find_column(schema, "id").nullable);

	const column_schema& title{ find_column(schema, "title") };
	EXPECT_EQ(column_type::string, title.type);
	EXPECT_EQ(200U, title.length);
	EXPECT_TRUE(title.index);
	EXPECT_FALSE(title.nullable);

	const column_schema& summary{ find_column(schema, "summary") };
	EXPECT_EQ(column_type::text, summary.type);
	EXPECT_TRUE(summary.nullable);

	const column_schema& price{ find_column(schema, "price") };
	EXPECT_EQ(column_type::decimal, price.type);
	EXPECT_EQ(10, price.precision);
	EXPECT_EQ(2, price.scale);
	EXPECT_EQ(column_default{ 0.0 }, price.default_value);

	EXPECT_EQ(column_type::boolean, find_column(schema, "in_print").type);
	EXPECT_EQ(column_default{ true }, find_column(schema, "in_print").default_value);
	EXPECT_EQ(column_type::bigint, find_column(schema, "level").type);
	EXPECT_EQ(column_default{ std::int64_t{ 2 } }, find_column(schema, "level").default_value);
	EXPECT_EQ(column_type::integer, find_column(schema, "pages").type);
	EXPECT_EQ(column_type::datetime, find_column(schema, "published").type);

	ASSERT_EQ(2U, schema.foreign_keys.size());
	EXPECT_EQ("author_id", schema.foreign_keys[0].column);
	EXPECT_EQ("authors", schema.foreign_keys[0].ref_table);
	EXPECT_EQ("id", schema.foreign_keys[0].ref_column);
	EXPECT_EQ(fk_action::cascade, schema.foreign_keys[0].on_delete);
	EXPECT_EQ("books", schema.foreign_keys[1].ref_table); // self-reference
	EXPECT_EQ(fk_action::set_null, schema.foreign_keys[1].on_delete);
}

TEST(SchemaTest, DefaultStringLengthIs255) {
	EXPECT_EQ(255U, column_of(col<&Kinds::text_value>("t")).length);
}

TEST(SchemaTest, MutualReferencesAndBothMetadataPolicies) {
	const table_schema a{ table_schema_for<MutualA>() };
	const table_schema b{ table_schema_for<MutualB>() };
	EXPECT_EQ("mutual_b", a.foreign_keys.at(0).ref_table);
	EXPECT_EQ("mutual_a", b.foreign_keys.at(0).ref_table);
}

TEST(SchemaTest, SchemaOutlivesDescriptors) {
	const table_schema schema{ [] { return table_schema_for<Author>(); }() };
	EXPECT_EQ("authors", schema.name);
	EXPECT_TRUE(find_column(schema, "name").unique);
	EXPECT_EQ(100U, find_column(schema, "name").length);
}

TEST(SchemaTest, PrimaryKeyErrors) {
	EXPECT_THROW(table_schema_for<MissingPk>(), ex::activerecord);
	EXPECT_THROW(table_schema_for<DuplicatePk>(), ex::activerecord);
	EXPECT_THROW(table_schema_for<OptionalPk>(), ex::activerecord);
	EXPECT_THROW(table_schema_for<NarrowPk>(), ex::activerecord);
}

TEST(SchemaTest, ReferencedPrimaryKeyErrors) {
	EXPECT_THROW(table_schema_for<RefersTo<MissingPk>>(), ex::activerecord);
	EXPECT_THROW(table_schema_for<RefersTo<DuplicatePk>>(), ex::activerecord);
	EXPECT_THROW(table_schema_for<RefersTo<OptionalPk>>(), ex::activerecord);
	EXPECT_THROW(table_schema_for<RefersTo<NarrowPk>>(), ex::activerecord);
}

TEST(SchemaTest, ReferencedModelIsNotExpanded) {
	EXPECT_THROW(table_schema_for<ValidPkBadColumn>(), ex::activerecord);
	EXPECT_NO_THROW(table_schema_for<RefersTo<ValidPkBadColumn>>());
}

// ==============================================================================
// Descriptor checks
// ==============================================================================

TEST(SchemaTest, RepeatedModifiersThrow) {
	EXPECT_THROW(column_of(col<&Kinds::text_value>("t").limit(5).limit(6)), ex::activerecord);
	EXPECT_THROW(column_of(col<&Kinds::text_value>("t").text().text()), ex::activerecord);
	EXPECT_THROW(column_of(col<&Kinds::real_value>("r").decimal(5, 1).decimal(6, 1)), ex::activerecord);
	EXPECT_THROW(column_of(col<&Kinds::small>("s").integer().integer()), ex::activerecord);
	EXPECT_THROW(column_of(col<&Kinds::optional_text>("o").not_null().not_null()), ex::activerecord);
	EXPECT_THROW(column_of(col<&Kinds::small>("s").default_value(1).default_value(2)), ex::activerecord);
	EXPECT_THROW(column_of(col<&Kinds::small>("s").unique().unique()), ex::activerecord);
	EXPECT_THROW(column_of(col<&Kinds::small>("s").index().index()), ex::activerecord);
	EXPECT_THROW(column_of(col<&Kinds::optional_id>("x")
							   .references<Author>()
							   .on_delete(fk_action::cascade)
							   .on_delete(fk_action::no_action)),
				 ex::activerecord);
	EXPECT_THROW(column_of(col<&Kinds::optional_id>("x")
							   .references<Author>()
							   .on_update(fk_action::cascade)
							   .on_update(fk_action::cascade)),
				 ex::activerecord);
}

TEST(SchemaTest, InvalidModifierValuesThrow) {
	EXPECT_THROW(column_of(col<&Kinds::text_value>("t").limit(0)), ex::activerecord);
	EXPECT_THROW(column_of(col<&Kinds::text_value>("t").limit(0).limit(10)), ex::activerecord); // repeat is not hidden
	EXPECT_THROW(column_of(col<&Kinds::real_value>("r").decimal(0, 0)), ex::activerecord);
	EXPECT_THROW(column_of(col<&Kinds::real_value>("r").decimal(10, 11)), ex::activerecord);
	EXPECT_THROW(column_of(col<&Kinds::real_value>("r").decimal(39, 2)), ex::activerecord);
	EXPECT_THROW(column_of(col<&Kinds::real_value>("r").decimal(10, -1)), ex::activerecord);
	EXPECT_NO_THROW(column_of(col<&Kinds::real_value>("r").decimal(38, 38)));
	EXPECT_THROW(column_of(col<&Kinds::text_value>("t").text().limit(10)), ex::activerecord);
}

TEST(SchemaTest, DanglingActionsThrow) {
	EXPECT_THROW(column_of(col<&Kinds::small>("s").on_delete(fk_action::cascade)), ex::activerecord);
	EXPECT_THROW(column_of(col<&Kinds::small>("s").on_update(fk_action::cascade)), ex::activerecord);
}

TEST(SchemaTest, PrimaryKeyModifiersThrow) {
	EXPECT_THROW(column_of(col<&Kinds::id>("id").integer()), ex::activerecord);
	EXPECT_THROW(column_of(col<&Kinds::id>("id").default_value(1)), ex::activerecord);
}

TEST(SchemaTest, DefaultsAreCheckedBeforeNarrowing) {
	EXPECT_THROW(column_of(col<&Kinds::optional_id>("x").default_value(std::numeric_limits<std::uint64_t>::max())),
				 ex::activerecord);
	EXPECT_NO_THROW(column_of(col<&Kinds::optional_id>("x").default_value(std::numeric_limits<std::int64_t>::max())));
	EXPECT_NO_THROW(column_of(col<&Kinds::optional_id>("x").default_value(std::numeric_limits<std::int64_t>::min())));
	EXPECT_THROW(column_of(col<&Kinds::real_value>("r").default_value(-std::numeric_limits<double>::infinity())),
				 ex::activerecord);
	// Where long double is wider than double (not on MSVC), its maximum does not fit a double column
	const bool wider{ std::numeric_limits<long double>::max() > std::numeric_limits<double>::max() };
	const auto widest{ col<&Kinds::real_value>("r").default_value(std::numeric_limits<long double>::max()) };
	EXPECT_EQ(wider, widest.options.default_value.invalid);
}

TEST(SchemaTest, IntegerColumnDefaultMustFitInt) {
	EXPECT_NO_THROW(column_of(col<&Kinds::small>("s").integer().default_value(2147483647)));
	EXPECT_NO_THROW(column_of(col<&Kinds::small>("s").integer().default_value(-2147483647 - 1)));
	EXPECT_THROW(column_of(col<&Kinds::optional_id>("x").integer().default_value(2147483648LL)), ex::activerecord);
}

TEST(SchemaTest, SetNullNeedsEffectivelyNullableColumn) {
	table_schema schema{ minimal_schema() };
	schema.columns.push_back(column_of(col<&Kinds::optional_id>("parent_id").not_null()));
	schema.foreign_keys.push_back(
		foreign_key{ "parent_id", "parents", "id", fk_action::set_null, fk_action::no_action });
	EXPECT_THROW(validate(schema), ex::activerecord);
	schema.columns.back().nullable = true;
	EXPECT_NO_THROW(validate(schema));
}

// ==============================================================================
// validate() on hand-built schemas
// ==============================================================================

TEST(SchemaValidateTest, MinimalSchemaIsValid) {
	EXPECT_NO_THROW(validate(minimal_schema()));
}

TEST(SchemaValidateTest, NameLimitsAreInBytes) {
	table_schema schema{ minimal_schema() };
	schema.columns.push_back(column_schema{ std::string(63, 'c') });
	EXPECT_NO_THROW(validate(schema));
	schema.columns.back().name = std::string(64, 'c');
	EXPECT_THROW(validate(schema), ex::activerecord);

	std::string multibyte;
	for (int i = 0; i < 31; ++i) {
		multibyte += "\xC3\xA9"; // é: 2 bytes
	}
	schema.columns.back().name = multibyte + "x"; // 63 bytes, 32 characters
	EXPECT_NO_THROW(validate(schema));
	schema.columns.back().name = multibyte + "xy"; // 64 bytes
	EXPECT_THROW(validate(schema), ex::activerecord);
}

TEST(SchemaValidateTest, BadNamesThrow) {
	table_schema schema{ minimal_schema() };
	schema.columns.push_back(column_schema{ std::string{ "a\0b", 3 } });
	EXPECT_THROW(validate(schema), ex::activerecord);
	schema.columns.back().name = "a.b";
	EXPECT_THROW(validate(schema), ex::activerecord);
	schema.columns.back().name = "";
	EXPECT_THROW(validate(schema), ex::activerecord);
	schema.columns.back().name = "id";
	EXPECT_THROW(validate(schema), ex::activerecord); // duplicate
	table_schema qualified{ minimal_schema() };
	qualified.name = "main.things";
	EXPECT_THROW(validate(qualified), ex::activerecord);
}

TEST(SchemaValidateTest, IndexNames) {
	EXPECT_EQ("ix_5_books_title", index_name("books", "title"));
	EXPECT_NE(index_name("a_b", "c"), index_name("a", "b_c"));

	table_schema schema{ minimal_schema() }; // "things": ix_6_things_ is 12 bytes
	schema.columns.push_back(column_schema{ std::string(51, 'c') });
	schema.columns.back().index = true;
	EXPECT_NO_THROW(validate(schema)); // 63 bytes
	schema.columns.back().name = std::string(52, 'c');
	EXPECT_THROW(validate(schema), ex::activerecord); // 64 bytes
}

TEST(SchemaValidateTest, PrimaryKeyRules) {
	table_schema missing{ minimal_schema() };
	missing.columns.clear();
	EXPECT_THROW(validate(missing), ex::activerecord);

	table_schema nullable{ minimal_schema() };
	nullable.columns[0].nullable = true;
	EXPECT_THROW(validate(nullable), ex::activerecord);

	table_schema with_default{ minimal_schema() };
	with_default.columns[0].default_value = std::int64_t{ 1 };
	EXPECT_THROW(validate(with_default), ex::activerecord);

	table_schema narrow{ minimal_schema() };
	narrow.columns[0].type = column_type::integer;
	EXPECT_THROW(validate(narrow), ex::activerecord);
}

TEST(SchemaValidateTest, TypesAndDefaults) {
	const auto check = [](column_schema column) {
		table_schema schema{ minimal_schema() };
		schema.columns.push_back(std::move(column));
		validate(schema);
	};
	EXPECT_THROW(check(column_schema{ "x", static_cast<column_type>(99) }), ex::activerecord);
	EXPECT_THROW(check(column_schema{ "x", column_type::string, 0 }), ex::activerecord);
	EXPECT_NO_THROW(check(column_schema{ "x", column_type::string, 10 }));
	EXPECT_THROW(check(column_schema{ "x", column_type::decimal, 0, 0, 0 }), ex::activerecord);
	EXPECT_THROW(check(column_schema{ "x", column_type::text, 0, 0, 0, false, false, false, std::int64_t{ 1 } }),
				 ex::activerecord);
	EXPECT_THROW(check(column_schema{ "x", column_type::boolean, 0, 0, 0, false, false, false, 1.0 }),
				 ex::activerecord);
	EXPECT_THROW(
		check(column_schema{ "x", column_type::integer, 0, 0, 0, false, false, false, std::int64_t{ 1LL << 40 } }),
		ex::activerecord);
	EXPECT_THROW(check(column_schema{ "x", column_type::real, 0, 0, 0, false, false, false,
									  std::numeric_limits<double>::quiet_NaN() }),
				 ex::activerecord);
	EXPECT_NO_THROW(check(column_schema{ "x", column_type::real, 0, 0, 0, false, false, false, 1.5 }));
}

TEST(SchemaValidateTest, ForeignKeyRules) {
	table_schema schema{ minimal_schema() };
	schema.columns.push_back(column_schema{ "parent_id", column_type::bigint, 0, 0, 0, true });
	schema.columns.push_back(column_schema{ "label", column_type::string, 10 });

	schema.foreign_keys = { foreign_key{ "parent_id", "parents", "id" } };
	EXPECT_NO_THROW(validate(schema));
	schema.foreign_keys = { foreign_key{ "missing", "parents", "id" } };
	EXPECT_THROW(validate(schema), ex::activerecord);
	schema.foreign_keys = { foreign_key{ "label", "parents", "id" } };
	EXPECT_THROW(validate(schema), ex::activerecord);
	schema.foreign_keys = { foreign_key{ "parent_id", "", "id" } };
	EXPECT_THROW(validate(schema), ex::activerecord);
	schema.foreign_keys = { foreign_key{ "parent_id", "parents", "id", static_cast<fk_action>(7) } };
	EXPECT_THROW(validate(schema), ex::activerecord);
}
