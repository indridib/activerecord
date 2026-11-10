// ============================================================================
// relations_test.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// Unit tests for belongs_to, has_many, has_one and build (SQLite in memory)
//

#include <activerecord/adapters/sqlite_adapter.h>
#include <activerecord/config.h>
#include <activerecord/relations.h>
#include <activerecord/schema.h>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

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

// Counts every query
class counting_sqlite : public sqlite_adapter {
  public:
	counting_sqlite() : sqlite_adapter(":memory:") {}
	std::unique_ptr<statement> query(const std::string& sql, const attribute_vector& values = {}) override {
		++calls;
		return sqlite_adapter::query(sql, values);
	}
	int calls{ 0 };
};

class RelationsTest : public ::testing::Test {
  protected:
	void SetUp() override {
		conn_ = std::make_shared<counting_sqlite>();
		conn_->foreign_keys(true);
		config::instance().set_connections({ { "default", conn_ } }, "default");
		create_tables<RelAuthor, RelBook, RelProfile, RelLightAuthor, RelNote>();
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

} // namespace
