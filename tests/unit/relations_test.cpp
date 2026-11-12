// ============================================================================
// relations_test.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// Unit tests for belongs_to, has_many, has_one, build, preload and preload_children (SQLite in memory)
//

#include <activerecord/adapters/sqlite_adapter.h>
#include <activerecord/config.h>
#include <activerecord/relations.h>
#include <activerecord/schema.h>
#include <activerecord/transaction.h>
#include <cstddef>
#include <cstdint>
#include <format>
#include <gtest/gtest.h>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace ar;

namespace {

struct RelAuthor : typed_model<RelAuthor> {
	static constexpr auto table_name{ "rel_authors" };
	static constexpr auto primary_key{ "id" };
	std::int64_t id{};
	std::string name{};
	static constexpr auto columns() { return ar::columns(col<&RelAuthor::id>("id"), col<&RelAuthor::name>("name")); }
};

struct RelBook : typed_model<RelBook> {
	static constexpr auto table_name{ "rel_books" };
	static constexpr auto primary_key{ "id" };
	std::int64_t id{};
	std::string title{};
	std::int64_t author_id{};
	std::optional<std::int64_t> editor_id{}; // no references<>: needs an explicit parent
	static constexpr auto columns() {
		return ar::columns(col<&RelBook::id>("id"), col<&RelBook::title>("title"),
						   col<&RelBook::author_id>("author_id").references<RelAuthor>(),
						   col<&RelBook::editor_id>("editor_id"));
	}
};

struct RelProfile : typed_model<RelProfile> {
	static constexpr auto table_name{ "rel_profiles" };
	static constexpr auto primary_key{ "id" };
	std::int64_t id{};
	std::string bio{};
	std::int64_t author_id{};
	static constexpr auto columns() {
		return ar::columns(col<&RelProfile::id>("id"), col<&RelProfile::bio>("bio"),
						   col<&RelProfile::author_id>("author_id").references<RelAuthor>());
	}
};

// Record tracking off, a custom primary key name and an FK column named after a keyword
struct RelLightAuthor : typed_model<RelLightAuthor, false> {
	static constexpr auto table_name{ "rel_light_authors" };
	static constexpr auto primary_key{ "code" };
	std::int64_t code{};
	std::string name{};
	static constexpr auto columns() {
		return ar::columns(col<&RelLightAuthor::code>("code"), col<&RelLightAuthor::name>("name"));
	}
};

struct RelNote : typed_model<RelNote, false> {
	static constexpr auto table_name{ "rel_notes" };
	static constexpr auto primary_key{ "id" };
	std::int64_t id{};
	std::int64_t owner{};
	static constexpr auto columns() {
		return ar::columns(col<&RelNote::id>("id"), col<&RelNote::owner>("order").references<RelLightAuthor>());
	}
};

// A custom child primary key, a keyword FK column, tracking off
struct RelPage : typed_model<RelPage, false> {
	static constexpr auto table_name{ "rel_pages" };
	static constexpr auto primary_key{ "page_no" };
	std::int64_t page_no{};
	std::int64_t book{};
	static constexpr auto columns() {
		return ar::columns(col<&RelPage::page_no>("page_no"),
						   col<&RelPage::book>("order").references<RelLightAuthor>());
	}
};

// Misuse, for the negative checks
struct RelTwice : typed_model<RelTwice> {
	static constexpr auto table_name{ "rel_twice" };
	static constexpr auto primary_key{ "id" };
	std::int64_t id{};
	std::int64_t author_id{};
	int small_id{};
	std::int64_t unmapped_id{};
	static constexpr auto columns() {
		return ar::columns(col<&RelTwice::id>("id"), col<&RelTwice::author_id>("author_id"),
						   col<&RelTwice::author_id>("author_id_again"), col<&RelTwice::small_id>("small_id"));
	}
};

static_assert(RelTwice::st_table_name() == "rel_twice" && RelTwice::st_primary_key() == "id");

class ClassicAuthor : public model {
  public:
	ClassicAuthor() : model(st_table_name(), st_primary_key()) {}
	static const char* st_table_name() { return "classic_authors"; }
	static const char* st_primary_key() { return "id"; }
};

struct NotAModel {
	std::int64_t id{};
};

constexpr std::int64_t free_id{ 0 };

template <auto F, class C>
concept can_belongs_to = requires(const C& c) { ar::belongs_to<F>(c); };
template <auto F, class P, class C>
concept can_belongs_to_as = requires(const C& c) { ar::belongs_to<F, P>(c); };
template <auto F, class P>
concept can_has_many = requires(const P& p) { ar::has_many<F>(p); };
template <auto F, class P>
concept can_has_one = requires(const P& p) { ar::has_one<F>(p); };
template <auto F, class P>
concept can_build = requires(const P& p) { ar::build<F>(p); };

// Positive
static_assert(can_belongs_to<&RelBook::author_id, RelBook>);
static_assert(can_belongs_to_as<&RelBook::editor_id, RelAuthor, RelBook>);		  // untagged, explicit parent
static_assert(can_belongs_to_as<&RelBook::author_id, RelAuthor, RelBook>);		  // explicit matches the tag
static_assert(can_belongs_to_as<&RelBook::author_id, const RelAuthor&, RelBook>); // normalized
static_assert(can_has_many<&RelBook::author_id, RelAuthor>);
static_assert(can_has_many<&RelBook::editor_id, RelAuthor>);
static_assert(can_has_one<&RelProfile::author_id, RelAuthor>);
static_assert(can_build<&RelBook::author_id, RelAuthor>);
static_assert(can_has_many<&RelNote::owner, RelLightAuthor>);
static_assert(std::is_same_v<decltype(ar::belongs_to<&RelBook::author_id>(std::declval<const RelBook&>())),
							 std::optional<RelAuthor>>);
static_assert(
	std::is_same_v<decltype(ar::belongs_to<&RelBook::author_id, const RelAuthor&>(std::declval<const RelBook&>())),
				   std::optional<RelAuthor>>);
static_assert(
	std::is_same_v<decltype(ar::has_many<&RelBook::author_id>(std::declval<const RelAuthor&>())), query<RelBook>>);
static_assert(std::is_same_v<decltype(ar::has_one<&RelProfile::author_id>(std::declval<const RelAuthor&>())),
							 std::optional<RelProfile>>);
static_assert(std::is_same_v<decltype(ar::build<&RelBook::author_id>(std::declval<const RelAuthor&>())), RelBook>);

// Negative
static_assert(!can_belongs_to<&free_id, RelBook>);				 // not a member pointer
static_assert(!can_has_many<&free_id, RelAuthor>);				 // not a member pointer
static_assert(!can_belongs_to<&NotAModel::id, NotAModel>);		 // owner is not a typed model
static_assert(!can_has_many<&RelTwice::unmapped_id, RelAuthor>); // not in columns()
static_assert(!can_belongs_to_as<&RelTwice::unmapped_id, RelAuthor, RelTwice>);
static_assert(!can_has_many<&RelTwice::author_id, RelAuthor>); // mapped twice
static_assert(!can_belongs_to_as<&RelTwice::author_id, RelAuthor, RelTwice>);
static_assert(!can_has_many<&RelTwice::small_id, RelAuthor>); // int, not std::int64_t
static_assert(!can_belongs_to_as<&RelTwice::small_id, RelAuthor, RelTwice>);
static_assert(!can_belongs_to_as<&RelBook::author_id, RelProfile, RelBook>); // explicit differs from the tag
static_assert(!can_has_many<&RelBook::author_id, RelProfile>);				 // parent differs from the tag
static_assert(!can_has_one<&RelProfile::author_id, RelBook>);
static_assert(!can_build<&RelBook::author_id, RelProfile>);
static_assert(!can_belongs_to<&RelBook::editor_id, RelBook>);					// untagged, no parent given
static_assert(!can_belongs_to_as<&RelBook::editor_id, ClassicAuthor, RelBook>); // classic model parent
static_assert(!can_belongs_to_as<&RelBook::editor_id, NotAModel, RelBook>);		// non-model parent
static_assert(!can_has_many<&RelBook::editor_id, ClassicAuthor>);
static_assert(!can_has_many<&RelBook::editor_id, NotAModel>);
static_assert(!can_build<&RelBook::editor_id, ClassicAuthor>);
static_assert(!can_belongs_to<&RelBook::author_id, RelProfile>); // wrong child

// An input-only range that refuses a second begin() and counts its traversal
template <class T> class single_pass {
  public:
	explicit single_pass(std::vector<T> items) : items_(std::move(items)) {}

	class iterator {
	  public:
		using value_type = T;
		using difference_type = std::ptrdiff_t;
		iterator() = default;
		iterator(single_pass* owner, std::size_t pos) : owner_(owner), pos_(pos) {}
		const T& operator*() const {
			++owner_->derefs;
			return owner_->items_[pos_];
		}
		iterator& operator++() {
			++owner_->increments;
			++pos_;
			return *this;
		}
		void operator++(int) { ++*this; }
		bool operator==(std::default_sentinel_t /*end*/) const { return pos_ == owner_->items_.size(); }

	  private:
		single_pass* owner_{ nullptr };
		std::size_t pos_{ 0 };
	};

	iterator begin() {
		if (begun_) {
			throw std::logic_error("single_pass: second begin()");
		}
		begun_ = true;
		return { this, 0 };
	}
	std::default_sentinel_t end() const { return {}; }

	int derefs{ 0 };
	int increments{ 0 };

  private:
	std::vector<T> items_{};
	bool begun_{ false };
};

static_assert(std::ranges::input_range<single_pass<RelBook>> && !std::ranges::forward_range<single_pass<RelBook>>);

constexpr auto copy_book{ [](const RelBook& book) { return book; } };
constexpr auto any_book{ [](const RelBook& /*book*/) { return true; } };
using books = std::vector<RelBook>;
using book_copies = decltype(std::declval<const books&>() | std::views::transform(copy_book));
using filtered_books = decltype(std::declval<books&>() | std::views::filter(any_book));
static_assert(std::is_same_v<std::ranges::range_reference_t<book_copies>, RelBook>); // a prvalue reference

template <auto F, class R>
concept can_preload = requires(R& r) { ar::preload<F>(r); };
template <auto F, class P, class R>
concept can_preload_as = requires(R& r) { ar::preload<F, P>(r); };

// preload: positive
static_assert(can_preload<&RelBook::author_id, books>);
static_assert(can_preload<&RelBook::author_id, const books>);
static_assert(can_preload<&RelBook::author_id, std::span<const RelBook>>);
static_assert(can_preload<&RelBook::author_id, filtered_books>);
static_assert(can_preload<&RelBook::author_id, book_copies>);
static_assert(can_preload<&RelBook::author_id, single_pass<RelBook>>);
static_assert(can_preload_as<&RelBook::editor_id, RelAuthor, books>);
static_assert(can_preload_as<&RelBook::editor_id, const RelAuthor&, books>);
static_assert(can_preload<&RelNote::owner, std::vector<RelNote>>);
static_assert(std::is_same_v<decltype(ar::preload<&RelBook::author_id>(std::declval<books&>())),
							 std::unordered_map<std::int64_t, RelAuthor>>);
static_assert(std::is_same_v<decltype(ar::preload<&RelBook::editor_id, const RelAuthor&>(std::declval<books&>())),
							 std::unordered_map<std::int64_t, RelAuthor>>);

// preload: negative, in constraint order
static_assert(!can_preload<&RelBook::author_id, int>);									  // not an input range
static_assert(!can_preload<&free_id, books>);											  // not a member pointer
static_assert(!can_preload<&NotAModel::id, std::vector<NotAModel>>);					  // owner is not a typed model
static_assert(!can_preload_as<&RelTwice::unmapped_id, RelAuthor, std::vector<RelTwice>>); // not in columns()
static_assert(!can_preload_as<&RelTwice::author_id, RelAuthor, std::vector<RelTwice>>);	  // mapped twice
static_assert(!can_preload_as<&RelTwice::small_id, RelAuthor, std::vector<RelTwice>>);	  // int, not std::int64_t
static_assert(!can_preload<&RelBook::author_id, std::vector<RelAuthor>>);				  // range of the wrong model
static_assert(!can_preload_as<&RelBook::author_id, RelAuthor, std::vector<RelAuthor>>);
static_assert(!can_preload<&RelBook::editor_id, books>);				   // untagged, no parent given
static_assert(!can_preload_as<&RelBook::author_id, RelProfile, books>);	   // explicit differs from the tag
static_assert(!can_preload_as<&RelBook::editor_id, ClassicAuthor, books>); // classic model parent
static_assert(!can_preload_as<&RelBook::editor_id, NotAModel, books>);	   // non-model parent

constexpr auto copy_author{ [](const RelAuthor& author) { return author; } };
constexpr auto any_author{ [](const RelAuthor& /*author*/) { return true; } };
using authors = std::vector<RelAuthor>;
using author_copies = decltype(std::declval<const authors&>() | std::views::transform(copy_author));
using filtered_authors = decltype(std::declval<authors&>() | std::views::filter(any_author));

template <auto F, class R>
concept can_preload_children = requires(R& r) { ar::preload_children<F>(r); };

// preload_children: positive
static_assert(can_preload_children<&RelBook::author_id, authors>);
static_assert(can_preload_children<&RelBook::author_id, const authors>);
static_assert(can_preload_children<&RelBook::author_id, std::span<const RelAuthor>>);
static_assert(can_preload_children<&RelBook::author_id, filtered_authors>);
static_assert(can_preload_children<&RelBook::author_id, author_copies>);
static_assert(can_preload_children<&RelBook::author_id, single_pass<RelAuthor>>);
static_assert(can_preload_children<&RelBook::editor_id, authors>); // untagged: the parent is the range's model
static_assert(can_preload_children<&RelNote::owner, std::vector<RelLightAuthor>>);
static_assert(can_preload_children<&RelPage::book, std::vector<RelLightAuthor>>);
static_assert(std::is_same_v<decltype(ar::preload_children<&RelBook::author_id>(std::declval<authors&>())),
							 std::unordered_map<std::int64_t, std::vector<RelBook>>>);

// preload_children: negative, in constraint order
static_assert(!can_preload_children<&RelBook::author_id, int>);						   // not an input range
static_assert(!can_preload_children<&free_id, authors>);							   // not a member pointer
static_assert(!can_preload_children<&NotAModel::id, authors>);						   // owner is not a typed model
static_assert(!can_preload_children<&RelTwice::unmapped_id, authors>);				   // not in columns()
static_assert(!can_preload_children<&RelTwice::author_id, authors>);				   // mapped twice
static_assert(!can_preload_children<&RelTwice::small_id, authors>);					   // int, not std::int64_t
static_assert(!can_preload_children<&RelBook::author_id, std::vector<RelProfile>>);	   // parent differs from the tag
static_assert(!can_preload_children<&RelBook::editor_id, std::vector<ClassicAuthor>>); // classic model parent
static_assert(!can_preload_children<&RelBook::editor_id, std::vector<NotAModel>>);	   // non-model parent

// Counts every query
class counting_sqlite : public sqlite_adapter {
  public:
	counting_sqlite() : sqlite_adapter(":memory:") {}
	std::unique_ptr<statement> query(const std::string& sql, const attribute_vector& values = {}) override {
		++calls;
		binds.push_back(values.size());
		last_sql = sql;
		if (calls == fail_at) {
			throw ex::activerecord("injected failure");
		}
		return sqlite_adapter::query(sql, values);
	}
	int calls{ 0 };
	int fail_at{ 0 };
	std::vector<std::size_t> binds{};
	std::string last_sql{};
};

class RelationsTest : public ::testing::Test {
  protected:
	void SetUp() override {
		conn_ = std::make_shared<counting_sqlite>();
		conn_->foreign_keys(true);
		config::instance().set_connections({ { "default", conn_ } }, "default");
		create_tables<RelAuthor, RelBook, RelProfile, RelLightAuthor, RelNote, RelPage>();
	}

	void TearDown() override { config::instance().set_connections({}, ""); }

	static RelAuthor saved_author(const std::string& name) {
		RelAuthor author;
		author.name = name;
		EXPECT_TRUE(author.save());
		return author;
	}

	static RelBook saved_book(const std::string& title, std::int64_t author_id) {
		RelBook book;
		book.title = title;
		book.author_id = author_id;
		EXPECT_TRUE(book.save());
		return book;
	}

	int calls() const { return conn_->calls; }

	std::shared_ptr<counting_sqlite> conn_{};
};

// ==============================================================================
// belongs_to
// ==============================================================================

TEST_F(RelationsTest, BelongsToLoadsTheParent) {
	const RelAuthor ann{ saved_author("Ann") };
	const RelBook book{ saved_book("Dune", ann.id) };

	const auto author{ ar::belongs_to<&RelBook::author_id>(book) };
	ASSERT_TRUE(author.has_value());
	EXPECT_EQ("Ann", author->name);
	EXPECT_FALSE(author->is_new_record());

	const RelBook reloaded{ query<RelBook>().find(book.id) };
	EXPECT_EQ(ann.id, ar::belongs_to<&RelBook::author_id>(reloaded)->id);
}

TEST_F(RelationsTest, BelongsToNullFkSendsNoQuery) {
	const RelBook book{ saved_book("Dune", saved_author("Ann").id) };
	const int before{ calls() };
	EXPECT_FALSE((ar::belongs_to<&RelBook::editor_id, RelAuthor>(book).has_value()));
	EXPECT_EQ(before, calls());
}

TEST_F(RelationsTest, BelongsToMissingRowIsNullopt) {
	RelBook book{ saved_book("Dune", saved_author("Ann").id) };
	book.editor_id = 999999; // untagged: no FK constraint
	const int before{ calls() };
	EXPECT_FALSE((ar::belongs_to<&RelBook::editor_id, RelAuthor>(book).has_value()));
	EXPECT_EQ(before + 1, calls());
}

TEST_F(RelationsTest, BelongsToOnAnUnsavedChildUsesTheMember) {
	const RelAuthor ann{ saved_author("Ann") };
	RelBook book;
	book.author_id = ann.id;
	EXPECT_EQ("Ann", ar::belongs_to<&RelBook::author_id>(book)->name);
}

TEST_F(RelationsTest, BelongsToIdZeroIsNotNull) {
	conn_->query("INSERT INTO rel_authors (id, name) VALUES (0, 'Zero')", {});
	RelBook book;
	book.author_id = 0;
	const int before{ calls() };
	EXPECT_EQ("Zero", ar::belongs_to<&RelBook::author_id>(book)->name);
	EXPECT_EQ(before + 1, calls());
}

TEST_F(RelationsTest, BelongsToFollowsAnEditedLoadedFk) {
	const RelAuthor ann{ saved_author("Ann") };
	const RelAuthor bob{ saved_author("Bob") };
	RelBook book{ query<RelBook>().find(saved_book("Dune", ann.id).id) };
	book.author_id = bob.id;
	EXPECT_EQ("Bob", ar::belongs_to<&RelBook::author_id>(book)->name);
}

TEST_F(RelationsTest, BelongsToRejectsAnUnloadedFk) {
	const RelBook saved{ saved_book("Dune", saved_author("Ann").id) };
	RelBook partial{ *query<RelBook>().select("id, title").where("id = ?", saved.id).first() };
	const int before{ calls() };
	EXPECT_THROW(ar::belongs_to<&RelBook::author_id>(partial), ex::activerecord);
	partial.author_id = saved.author_id; // an edit does not load the column
	EXPECT_THROW(ar::belongs_to<&RelBook::author_id>(partial), ex::activerecord);
	EXPECT_EQ(before, calls());
}

// ==============================================================================
// has_many
// ==============================================================================

TEST_F(RelationsTest, HasManyIsALazyChainableQuery) {
	const RelAuthor ann{ saved_author("Ann") };
	const RelAuthor bob{ saved_author("Bob") };
	saved_book("C", ann.id);
	saved_book("A", ann.id);
	saved_book("B", ann.id);
	saved_book("Other", bob.id);

	const int before{ calls() };
	auto books{ ar::has_many<&RelBook::author_id>(ann) };
	EXPECT_EQ(before, calls());

	const auto ordered{ books.order("title").all() };
	ASSERT_EQ(3U, ordered.size());
	EXPECT_EQ("A", ordered[0].title);
	EXPECT_EQ("C", ordered[2].title);
	EXPECT_EQ(3, ar::has_many<&RelBook::author_id>(ann).count());
	EXPECT_EQ(1, ar::has_many<&RelBook::author_id>(ann).where("title > ?", attribute{ "B" }).count());
	EXPECT_EQ(2U, ar::has_many<&RelBook::author_id>(ann).limit(2).all().size());
}

TEST_F(RelationsTest, HasManyOfAnUnsavedParentMatchesNothing) {
	saved_book("Dune", saved_author("Ann").id);
	const RelAuthor unsaved;
	const int before{ calls() };
	auto books{ ar::has_many<&RelBook::author_id>(unsaved) };
	EXPECT_EQ(before, calls());
	EXPECT_TRUE(books.all().empty());
	EXPECT_EQ(before + 1, calls());
}

TEST_F(RelationsTest, HasManyUsesTheStoredIdNotTheMember) {
	RelAuthor ann{ saved_author("Ann") };
	saved_book("Dune", ann.id);
	ann.id = 999999;
	EXPECT_EQ(1, ar::has_many<&RelBook::author_id>(ann).count());
}

TEST_F(RelationsTest, HasManyThroughAnUntaggedFk) {
	const RelAuthor ann{ saved_author("Ann") };
	RelBook book;
	book.title = "Edited";
	book.author_id = ann.id;
	book.editor_id = ann.id;
	ASSERT_TRUE(book.save());
	saved_book("Not edited", ann.id);
	EXPECT_EQ(1, ar::has_many<&RelBook::editor_id>(ann).count());
	EXPECT_EQ(2, ar::has_many<&RelBook::author_id>(ann).count());
}

// ==============================================================================
// Parent primary key checks (has_many, has_one, build)
// ==============================================================================

TEST_F(RelationsTest, InverseHelpersRejectAnUnusablePrimaryKey) {
	saved_author("Ann");
	const RelAuthor unloaded{ *query<RelAuthor>().select("name").first() };
	const RelAuthor null_id{ *query<RelAuthor>().select("NULL AS id, name").first() };
	const RelAuthor text_id{ *query<RelAuthor>().select("'7' AS id, name").first() };

	const int before{ calls() };
	for (const RelAuthor* parent : { &unloaded, &null_id, &text_id }) {
		EXPECT_THROW(ar::has_many<&RelBook::author_id>(*parent), ex::activerecord);
		EXPECT_THROW(ar::has_one<&RelProfile::author_id>(*parent), ex::activerecord);
		EXPECT_THROW(ar::build<&RelBook::author_id>(*parent), ex::activerecord);
	}
	EXPECT_EQ(before, calls());
}

TEST_F(RelationsTest, InverseHelpersAcceptAStoredIdZero) {
	conn_->query("INSERT INTO rel_authors (id, name) VALUES (0, 'Zero')", {});
	const RelAuthor zero{ query<RelAuthor>().find(0) };
	saved_book("Zero's book", 0);
	RelProfile profile;
	profile.author_id = 0;
	ASSERT_TRUE(profile.save());

	EXPECT_EQ(1, ar::has_many<&RelBook::author_id>(zero).count());
	EXPECT_EQ(profile.id, ar::has_one<&RelProfile::author_id>(zero)->id);
	EXPECT_EQ(0, ar::build<&RelBook::author_id>(zero).author_id);
}

// ==============================================================================
// has_one
// ==============================================================================

TEST_F(RelationsTest, HasOneZeroOneOrSeveralRows) {
	const RelAuthor ann{ saved_author("Ann") };
	const int before{ calls() };
	EXPECT_FALSE(ar::has_one<&RelProfile::author_id>(ann).has_value());
	EXPECT_EQ(before + 1, calls());

	RelProfile first;
	first.bio = "first";
	first.author_id = ann.id;
	ASSERT_TRUE(first.save());
	EXPECT_EQ("first", ar::has_one<&RelProfile::author_id>(ann)->bio);

	RelProfile second;
	second.bio = "second";
	second.author_id = ann.id;
	ASSERT_TRUE(second.save());
	EXPECT_EQ(first.id, ar::has_one<&RelProfile::author_id>(ann)->id); // the lower primary key
}

TEST_F(RelationsTest, HasOneOfAnUnsavedParentSendsNoQuery) {
	const RelAuthor unsaved;
	const int before{ calls() };
	EXPECT_FALSE(ar::has_one<&RelProfile::author_id>(unsaved).has_value());
	EXPECT_EQ(before, calls());
}

// ==============================================================================
// build
// ==============================================================================

TEST_F(RelationsTest, BuildSetsOnlyTheFk) {
	const RelAuthor ann{ saved_author("Ann") };
	const int before{ calls() };
	RelBook book{ ar::build<&RelBook::author_id>(ann) };
	EXPECT_EQ(before, calls());
	EXPECT_TRUE(book.is_new_record());
	EXPECT_EQ(ann.id, book.author_id);
	EXPECT_EQ(0, book.id);
	EXPECT_TRUE(book.title.empty());
	EXPECT_FALSE(book.editor_id.has_value());

	const RelBook edited{ ar::build<&RelBook::editor_id>(ann) };
	EXPECT_EQ(ann.id, edited.editor_id);
	EXPECT_EQ(0, edited.author_id);

	book.title = "Built";
	ASSERT_TRUE(book.save());
	EXPECT_EQ(ann.id, query<RelBook>().find(book.id).author_id);
}

TEST_F(RelationsTest, BuildOnAnUnsavedParentThrows) {
	const RelAuthor unsaved;
	const int before{ calls() };
	EXPECT_THROW(ar::build<&RelBook::author_id>(unsaved), ex::activerecord);
	EXPECT_EQ(before, calls());
}

// ==============================================================================
// Record tracking off, custom primary key, keyword FK column
// ==============================================================================

TEST_F(RelationsTest, UntrackedModelsWithCustomKeyAndKeywordColumn) {
	RelLightAuthor light;
	light.name = "Light";
	ASSERT_TRUE(light.save());
	ASSERT_NE(0, light.code);

	RelNote note{ ar::build<&RelNote::owner>(light) };
	ASSERT_TRUE(note.save());

	EXPECT_EQ(1, ar::has_many<&RelNote::owner>(light).count());
	EXPECT_EQ(note.id, ar::has_one<&RelNote::owner>(light)->id);
	EXPECT_EQ("Light", ar::belongs_to<&RelNote::owner>(note)->name);
	EXPECT_EQ("Light", ar::belongs_to<&RelNote::owner>(query<RelNote>().find(note.id))->name);
}

// ==============================================================================
// preload
// ==============================================================================

TEST_F(RelationsTest, PreloadWithoutIdsNeedsNoConnection) {
	RelBook null_editor;
	null_editor.author_id = 1;
	const books nulls{ null_editor, null_editor };
	const int before{ calls() };
	config::instance().set_connections({}, "");

	EXPECT_TRUE(ar::preload<&RelBook::author_id>(books{}).empty());
	EXPECT_TRUE((ar::preload<&RelBook::editor_id, RelAuthor>(nulls).empty()));
	EXPECT_EQ(before, calls());
}

TEST_F(RelationsTest, PreloadRejectsAnUnloadedFkBeforeAnySql) {
	const RelBook saved{ saved_book("Dune", saved_author("Ann").id) };
	RelBook unsaved;
	unsaved.author_id = saved.author_id;
	const books mixed{ unsaved, *query<RelBook>().select("id, title").where("id = ?", saved.id).first() };
	const int before{ calls() };
	EXPECT_THROW(ar::preload<&RelBook::author_id>(mixed), ex::activerecord);
	EXPECT_EQ(before, calls());
}

TEST_F(RelationsTest, PreloadBindsEachIdOnceAndSkipsMissingAndNull) {
	const RelAuthor ann{ saved_author("Ann") };
	const RelAuthor bob{ saved_author("Bob") };
	RelBook missing;
	missing.author_id = 999999;
	books children{ saved_book("A", ann.id), saved_book("B", ann.id), saved_book("C", bob.id), missing };
	children[1].editor_id = bob.id;

	const int before{ calls() };
	const auto authors{ ar::preload<&RelBook::author_id>(children) };
	EXPECT_EQ(before + 1, calls());
	EXPECT_EQ(3U, conn_->binds.back());
	ASSERT_EQ(2U, authors.size());
	EXPECT_EQ("Ann", authors.at(ann.id).name);
	EXPECT_EQ("Bob", authors.at(bob.id).name);
	EXPECT_FALSE(authors.contains(999999));
	EXPECT_FALSE(authors.at(ann.id).is_new_record());

	const auto editors{ ar::preload<&RelBook::editor_id, RelAuthor>(children) }; // three NULL editors
	EXPECT_EQ(1U, conn_->binds.back());
	ASSERT_EQ(1U, editors.size());
	EXPECT_EQ("Bob", editors.at(bob.id).name);
}

class RelationsChunkTest : public RelationsTest {
  protected:
	// 1001 authors and one missing id above them: 1002 distinct ids
	void SetUp() override {
		RelationsTest::SetUp();
		transaction tx;
		for (int i = 0; i < 1001; ++i) {
			RelBook book;
			book.author_id = saved_author(std::format("A{}", i)).id;
			children_.push_back(book);
		}
		tx.commit();
		missing_ = children_.back().author_id + 1;
		RelBook book;
		book.author_id = missing_;
		children_.push_back(book);
		children_.push_back(children_.front()); // a duplicate
	}

	std::vector<std::size_t> last_binds(std::size_t n) const {
		return { conn_->binds.end() - static_cast<std::ptrdiff_t>(n), conn_->binds.end() };
	}

	books children_{};
	std::int64_t missing_{ 0 };
};

TEST_F(RelationsChunkTest, PreloadSplitsAt1000Ids) {
	const int before{ calls() };
	const auto authors{ ar::preload<&RelBook::author_id>(children_) };
	EXPECT_EQ(before + 2, calls());
	EXPECT_EQ((std::vector<std::size_t>{ 1000, 2 }), last_binds(2));
	EXPECT_EQ(1001U, authors.size());
	EXPECT_FALSE(authors.contains(missing_));
}

TEST_F(RelationsChunkTest, PreloadPropagatesAnErrorInTheSecondChunk) {
	const int before{ calls() };
	conn_->fail_at = before + 2;
	EXPECT_THROW(ar::preload<&RelBook::author_id>(children_), ex::activerecord);
	EXPECT_EQ(before + 2, calls());
	EXPECT_EQ((std::vector<std::size_t>{ 1000, 2 }), last_binds(2));
}

TEST_F(RelationsTest, PreloadReadsASinglePassRangeOnce) {
	const RelAuthor ann{ saved_author("Ann") };
	const RelAuthor bob{ saved_author("Bob") };
	RelBook first;
	first.author_id = ann.id;
	RelBook second;
	second.author_id = bob.id;
	single_pass input{ books{ first, second, first } };

	const auto authors{ ar::preload<&RelBook::author_id>(input) };
	EXPECT_EQ(2U, authors.size());
	EXPECT_EQ(3, input.derefs);
	EXPECT_EQ(3, input.increments);
	EXPECT_THROW(input.begin(), std::logic_error);
}

TEST_F(RelationsTest, PreloadAcceptsViews) {
	const RelAuthor ann{ saved_author("Ann") };
	const books children{ saved_book("A", ann.id) };
	EXPECT_EQ("Ann", ar::preload<&RelBook::author_id>(children | std::views::transform(copy_book)).at(ann.id).name);
	EXPECT_EQ(1U, ar::preload<&RelBook::author_id>(std::span{ children }).size());
	EXPECT_EQ(1U, ar::preload<&RelBook::author_id>(children | std::views::filter(any_book)).size());
}

TEST_F(RelationsTest, PreloadUntrackedModelsWithCustomKey) {
	RelLightAuthor light;
	light.name = "Light";
	ASSERT_TRUE(light.save());
	RelNote note{ ar::build<&RelNote::owner>(light) };
	ASSERT_TRUE(note.save());

	const auto owners{ ar::preload<&RelNote::owner>(query<RelNote>().all()) };
	ASSERT_EQ(1U, owners.size());
	EXPECT_EQ("Light", owners.at(light.code).name);
}

// ==============================================================================
// preload_children
// ==============================================================================

TEST_F(RelationsTest, PreloadChildrenWithoutPersistedParentsNeedsNoConnection) {
	const authors unsaved{ RelAuthor{}, RelAuthor{} };
	const int before{ calls() };
	config::instance().set_connections({}, "");

	EXPECT_TRUE(ar::preload_children<&RelBook::author_id>(authors{}).empty());
	EXPECT_TRUE(ar::preload_children<&RelBook::author_id>(unsaved).empty());
	EXPECT_EQ(before, calls());
}

TEST_F(RelationsTest, PreloadChildrenRejectsAPartlyLoadedParentBeforeAnySql) {
	const RelAuthor ann{ saved_author("Ann") };
	const RelAuthor partial{ *query<RelAuthor>().select("name").first() };
	const int before{ calls() };
	EXPECT_THROW(ar::preload_children<&RelBook::author_id>(authors{ partial }), ex::activerecord);
	EXPECT_THROW(ar::preload_children<&RelBook::author_id>(authors{ ann, partial }), ex::activerecord);
	EXPECT_EQ(before, calls());
}

TEST_F(RelationsTest, PreloadChildrenGroupsByParentInPrimaryKeyOrder) {
	const RelAuthor ann{ saved_author("Ann") };
	const RelAuthor bob{ saved_author("Bob") };
	const RelAuthor cid{ saved_author("Cid") };
	const RelBook z{ saved_book("Z", ann.id) }; // titles in reverse of id order
	const RelBook dune{ saved_book("Dune", bob.id) };
	const RelBook y{ saved_book("Y", ann.id) };
	const RelBook x{ saved_book("X", ann.id) };

	const int before{ calls() };
	const auto books_by_author{ ar::preload_children<&RelBook::author_id>(authors{ ann, bob, cid, ann, RelAuthor{} }) };
	EXPECT_EQ(before + 1, calls());
	EXPECT_EQ(3U, conn_->binds.back()); // ann, bob, cid; the duplicate and the unsaved parent are not bound
	ASSERT_EQ(2U, books_by_author.size());
	const std::vector<RelBook>& anns{ books_by_author.at(ann.id) };
	ASSERT_EQ(3U, anns.size());
	EXPECT_EQ(z.id, anns[0].id);
	EXPECT_EQ(y.id, anns[1].id);
	EXPECT_EQ(x.id, anns[2].id);
	EXPECT_EQ("Dune", books_by_author.at(bob.id).front().title);
	EXPECT_FALSE(books_by_author.contains(cid.id));
	EXPECT_FALSE(anns.front().is_new_record());
}

TEST_F(RelationsTest, PreloadChildrenThroughAnUntaggedFk) {
	const RelAuthor ann{ saved_author("Ann") };
	const RelAuthor bob{ saved_author("Bob") };
	RelBook edited{ saved_book("Edited", ann.id) };
	edited.editor_id = bob.id;
	ASSERT_TRUE(edited.save());
	saved_book("Unedited", ann.id); // NULL editor

	const auto edited_by{ ar::preload_children<&RelBook::editor_id>(authors{ ann, bob }) };
	ASSERT_EQ(1U, edited_by.size());
	ASSERT_EQ(1U, edited_by.at(bob.id).size());
	EXPECT_EQ("Edited", edited_by.at(bob.id).front().title);
}

class RelationsParentChunkTest : public RelationsTest {
  protected:
	// 1002 authors: 1000 in the first chunk, 2 in the second; one book for the first and one for the last
	void SetUp() override {
		RelationsTest::SetUp();
		transaction tx;
		for (int i = 0; i < 1002; ++i) {
			parents_.push_back(saved_author(std::format("A{}", i)));
		}
		saved_book("First", parents_.front().id);
		saved_book("Last", parents_.back().id);
		tx.commit();
	}

	std::vector<std::size_t> last_binds(std::size_t n) const {
		return { conn_->binds.end() - static_cast<std::ptrdiff_t>(n), conn_->binds.end() };
	}

	authors parents_{};
};

TEST_F(RelationsParentChunkTest, PreloadChildrenSplitsAt1000Parents) {
	const int before{ calls() };
	const auto books_by_author{ ar::preload_children<&RelBook::author_id>(parents_) };
	EXPECT_EQ(before + 2, calls());
	EXPECT_EQ((std::vector<std::size_t>{ 1000, 2 }), last_binds(2));
	ASSERT_EQ(2U, books_by_author.size());
	EXPECT_EQ("First", books_by_author.at(parents_.front().id).front().title);
	EXPECT_EQ("Last", books_by_author.at(parents_.back().id).front().title);
}

TEST_F(RelationsParentChunkTest, PreloadChildrenPropagatesAnErrorInTheSecondChunk) {
	const int before{ calls() };
	conn_->fail_at = before + 2;
	EXPECT_THROW(ar::preload_children<&RelBook::author_id>(parents_), ex::activerecord);
	EXPECT_EQ(before + 2, calls());
	EXPECT_EQ((std::vector<std::size_t>{ 1000, 2 }), last_binds(2));
}

TEST_F(RelationsTest, PreloadChildrenReadsASinglePassRangeOnce) {
	const RelAuthor ann{ saved_author("Ann") };
	const RelAuthor bob{ saved_author("Bob") };
	saved_book("A", ann.id);
	single_pass input{ authors{ ann, bob, ann } };

	const auto books_by_author{ ar::preload_children<&RelBook::author_id>(input) };
	EXPECT_EQ(1U, books_by_author.size());
	EXPECT_EQ(3, input.derefs);
	EXPECT_EQ(3, input.increments);
	EXPECT_THROW(input.begin(), std::logic_error);
}

TEST_F(RelationsTest, PreloadChildrenAcceptsViews) {
	const RelAuthor ann{ saved_author("Ann") };
	saved_book("A", ann.id);
	const authors parents{ ann };
	EXPECT_EQ(1U, ar::preload_children<&RelBook::author_id>(parents | std::views::transform(copy_author)).size());
	EXPECT_EQ(1U, ar::preload_children<&RelBook::author_id>(std::span{ parents }).size());
	EXPECT_EQ(1U, ar::preload_children<&RelBook::author_id>(parents | std::views::filter(any_author)).size());
}

TEST_F(RelationsTest, PreloadChildrenUntrackedModelsWithCustomKeys) {
	RelLightAuthor first;
	first.name = "First";
	ASSERT_TRUE(first.save());
	RelLightAuthor second;
	second.name = "Second";
	ASSERT_TRUE(second.save());
	ASSERT_TRUE(ar::build<&RelNote::owner>(first).save());

	// save() leaves a new record's primary key out, so explicit, out-of-order keys need raw SQL
	const std::string insert{ std::format("INSERT INTO rel_pages (page_no, {}) VALUES (?, ?)",
										  conn_->escape("order")) };
	for (const auto& [page_no, owner] : { std::pair{ 30, first.code }, std::pair{ 10, first.code },
										  std::pair{ 20, first.code }, std::pair{ 15, second.code } }) {
		conn_->query(insert, attribute_vector{ attribute{ std::int64_t{ page_no } }, attribute{ owner } });
	}

	const std::vector<RelLightAuthor> parents{ first, second };
	const auto notes{ ar::preload_children<&RelNote::owner>(parents) };
	ASSERT_EQ(1U, notes.size());
	EXPECT_EQ(1U, notes.at(first.code).size());

	const auto pages{ ar::preload_children<&RelPage::book>(parents) };
	EXPECT_TRUE(conn_->last_sql.ends_with("ORDER BY " + conn_->escape("page_no"))) << conn_->last_sql;
	std::vector<std::int64_t> first_pages;
	for (const RelPage& page : pages.at(first.code)) {
		first_pages.push_back(page.page_no);
	}
	EXPECT_EQ((std::vector<std::int64_t>{ 10, 20, 30 }), first_pages);
	ASSERT_EQ(1U, pages.at(second.code).size());
	EXPECT_EQ(15, pages.at(second.code).front().page_no);
}

} // namespace
