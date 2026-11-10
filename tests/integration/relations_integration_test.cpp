// ============================================================================
// relations_integration_test.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// belongs_to, has_many, has_one and build against every enabled adapter
//

#include "test_adapters.h"

#include <activerecord/relations.h>
#include <activerecord/schema.h>
#include <activerecord/typed_model.h>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#ifdef AR_SQLITE
#include <activerecord/adapters/sqlite_adapter.h>
#endif

using namespace ar;
using namespace ar::test;

namespace {

struct RiAuthor : typed_model<RiAuthor> {
	static constexpr auto table_name{ "ri_authors" };
	static constexpr auto primary_key{ "id" };
	std::int64_t id{};
	std::string name{};
	static constexpr auto columns() {
		return ar::columns(col<&RiAuthor::id>("id"), col<&RiAuthor::name>("name").limit(50));
	}
};

// Two FKs to RiAuthor; editor_id has no references<>, so no constraint
struct RiBook : typed_model<RiBook> {
	static constexpr auto table_name{ "ri_books" };
	static constexpr auto primary_key{ "id" };
	std::int64_t id{};
	std::string title{};
	std::int64_t author_id{};
	std::optional<std::int64_t> editor_id{};
	static constexpr auto columns() {
		return ar::columns(col<&RiBook::id>("id"), col<&RiBook::title>("title").limit(50),
						   col<&RiBook::author_id>("author_id").references<RiAuthor>(),
						   col<&RiBook::editor_id>("editor_id"));
	}
};

struct RiProfile : typed_model<RiProfile> {
	static constexpr auto table_name{ "ri_profiles" };
	static constexpr auto primary_key{ "id" };
	std::int64_t id{};
	std::string bio{};
	std::int64_t author_id{};
	static constexpr auto columns() {
		return ar::columns(col<&RiProfile::id>("id"), col<&RiProfile::bio>("bio").limit(50),
						   col<&RiProfile::author_id>("author_id").references<RiAuthor>());
	}
};

struct RiNode : typed_model<RiNode> {
	static constexpr auto table_name{ "ri_nodes" };
	static constexpr auto primary_key{ "id" };
	std::int64_t id{};
	std::string name{};
	std::optional<std::int64_t> parent_id{};
	static constexpr auto columns() {
		return ar::columns(col<&RiNode::id>("id"), col<&RiNode::name>("name").limit(50),
						   col<&RiNode::parent_id>("parent_id").references<RiNode>());
	}
};

// Record tracking off, a custom primary key name, and an FK column named after a keyword
struct RiShelf : typed_model<RiShelf, false> {
	static constexpr auto table_name{ "ri_shelves" };
	static constexpr auto primary_key{ "code" };
	std::int64_t code{};
	std::string label{};
	static constexpr auto columns() {
		return ar::columns(col<&RiShelf::code>("code"), col<&RiShelf::label>("label").limit(50));
	}
};

struct RiSlot : typed_model<RiSlot, false> {
	static constexpr auto table_name{ "ri_slots" };
	static constexpr auto primary_key{ "id" };
	std::int64_t id{};
	std::int64_t shelf{};
	static constexpr auto columns() {
		return ar::columns(col<&RiSlot::id>("id"), col<&RiSlot::shelf>("order").references<RiShelf>());
	}
};

class RelationsIntegrationTest : public AdapterTest {
  protected:
	table_list table_definitions(AdapterType /*type*/) const override { return {}; }

	void SetUp() override {
		AdapterTest::SetUp();
		if (HasFatalFailure()) {
			return;
		}
		// Record every query; on SQLite this is a fresh database, so the tables are created on it
		adapter_ = make_counting(adapter_type(), statements_);
		config::instance().set_connections({ { "default", adapter_ } }, "default");
#ifdef AR_SQLITE
		if (adapter_type() == AdapterType::SQLite) {
			dynamic_cast<sqlite_adapter&>(*adapter_).foreign_keys(true);
		}
#endif
		for (const char* name : { "ri_slots", "ri_shelves", "ri_nodes", "ri_profiles", "ri_books", "ri_authors" }) {
			drop_table(name);
		}
		tables_ = { "ri_authors", "ri_books", "ri_profiles", "ri_nodes", "ri_shelves", "ri_slots" };
		create_tables<RiAuthor, RiBook, RiProfile, RiNode, RiShelf, RiSlot>();
	}

	std::size_t queries() const { return statements_->size(); }

	static RiAuthor author(const std::string& name) {
		RiAuthor saved;
		saved.name = name;
		EXPECT_TRUE(saved.save());
		return saved;
	}

	static RiBook book(const std::string& title, std::int64_t author_id) {
		RiBook saved;
		saved.title = title;
		saved.author_id = author_id;
		EXPECT_TRUE(saved.save());
		return saved;
	}

	const std::vector<std::string>* statements_{ nullptr };
};

TEST_P(RelationsIntegrationTest, BelongsTo) {
	const RiAuthor ann{ author("Ann") };
	const RiBook dune{ query<RiBook>().find(book("Dune", ann.id).id) };
	EXPECT_EQ("Ann", ar::belongs_to<&RiBook::author_id>(dune)->name);

	const std::size_t before{ queries() };
	EXPECT_FALSE((ar::belongs_to<&RiBook::editor_id, RiAuthor>(dune).has_value())); // NULL: no query
	EXPECT_EQ(before, queries());

	RiBook unsaved;
	unsaved.author_id = 999999;
	EXPECT_FALSE(ar::belongs_to<&RiBook::author_id>(unsaved).has_value());
	RiBook edited{ dune };
	edited.editor_id = 999999;
	EXPECT_FALSE((ar::belongs_to<&RiBook::editor_id, RiAuthor>(edited).has_value()));
}

TEST_P(RelationsIntegrationTest, HasMany) {
	const RiAuthor ann{ author("Ann") };
	const RiAuthor bob{ author("Bob") };
	book("C", ann.id);
	book("A", ann.id);
	book("B", ann.id);
	book("Other", bob.id);

	const std::size_t before{ queries() };
	auto books{ ar::has_many<&RiBook::author_id>(ann) };
	EXPECT_EQ(before, queries());
	const auto ordered{ books.order("title").all() };
	ASSERT_EQ(3U, ordered.size());
	EXPECT_EQ("A", ordered.front().title);
	EXPECT_EQ("C", ordered.back().title);
	EXPECT_EQ(3, ar::has_many<&RiBook::author_id>(ann).count());
	EXPECT_EQ(1, ar::has_many<&RiBook::author_id>(ann).where("title > ?", attribute{ "B" }).count());
	EXPECT_EQ(2U, ar::has_many<&RiBook::author_id>(ann).order("title").limit(2).all().size());

	const RiAuthor unsaved;
	const std::size_t unsaved_before{ queries() };
	auto none{ ar::has_many<&RiBook::author_id>(unsaved) };
	EXPECT_EQ(unsaved_before, queries());
	EXPECT_TRUE(none.all().empty());
}

TEST_P(RelationsIntegrationTest, HasOne) {
	const RiAuthor ann{ author("Ann") };
	EXPECT_FALSE(ar::has_one<&RiProfile::author_id>(ann).has_value());

	RiProfile first;
	first.bio = "first";
	first.author_id = ann.id;
	ASSERT_TRUE(first.save());
	EXPECT_EQ("first", ar::has_one<&RiProfile::author_id>(ann)->bio);

	RiProfile second;
	second.bio = "second";
	second.author_id = ann.id;
	ASSERT_TRUE(second.save());
	EXPECT_EQ(first.id, ar::has_one<&RiProfile::author_id>(ann)->id); // the lower primary key

	const RiAuthor unsaved;
	const std::size_t before{ queries() };
	EXPECT_FALSE(ar::has_one<&RiProfile::author_id>(unsaved).has_value());
	EXPECT_EQ(before, queries());
}

TEST_P(RelationsIntegrationTest, BuildThenSave) {
	const RiAuthor ann{ query<RiAuthor>().find(author("Ann").id) };
	const std::size_t before{ queries() };
	RiBook built{ ar::build<&RiBook::author_id>(ann) };
	EXPECT_EQ(before, queries());
	EXPECT_TRUE(built.is_new_record());
	built.title = "Built";
	ASSERT_TRUE(built.save());

	const auto books{ ar::has_many<&RiBook::author_id>(ann).all() };
	ASSERT_EQ(1U, books.size());
	EXPECT_EQ("Built", books.front().title);
	EXPECT_EQ(ann.id, books.front().author_id);
}

TEST_P(RelationsIntegrationTest, SelfReference) {
	RiNode root;
	root.name = "root";
	ASSERT_TRUE(root.save());
	RiNode leaf{ ar::build<&RiNode::parent_id>(root) };
	leaf.name = "leaf";
	ASSERT_TRUE(leaf.save());

	EXPECT_EQ("root", ar::belongs_to<&RiNode::parent_id>(leaf)->name);
	EXPECT_FALSE(ar::belongs_to<&RiNode::parent_id>(root).has_value());
	EXPECT_EQ(1, ar::has_many<&RiNode::parent_id>(root).count());
	EXPECT_EQ(0, ar::has_many<&RiNode::parent_id>(leaf).count());
}

TEST_P(RelationsIntegrationTest, CustomKeyAndKeywordColumn) {
	RiShelf shelf;
	shelf.label = "top";
	ASSERT_TRUE(shelf.save());
	RiSlot slot{ ar::build<&RiSlot::shelf>(shelf) };
	ASSERT_TRUE(slot.save());

	EXPECT_EQ(1, ar::has_many<&RiSlot::shelf>(shelf).count());
	EXPECT_EQ(slot.id, ar::has_one<&RiSlot::shelf>(shelf)->id);
	EXPECT_EQ("top", ar::belongs_to<&RiSlot::shelf>(query<RiSlot>().find(slot.id))->label);
}

TEST_P(RelationsIntegrationTest, IdsAboveInt32Max) {
	constexpr std::int64_t big{ std::int64_t{ std::numeric_limits<std::int32_t>::max() } + 10 };
	const attribute_vector row{ attribute{ big }, attribute{ "Big" } };
	const std::string insert{ "INSERT INTO ri_authors (id, name) VALUES (?, ?)" };
	if (adapter_type() == AdapterType::MSSQL) {
		adapter_->query("SET IDENTITY_INSERT ri_authors ON; " + insert + "; SET IDENTITY_INSERT ri_authors OFF", row);
	} else {
		adapter_->query(insert, row);
	}
	const RiAuthor big_author{ query<RiAuthor>().find(big) };
	const RiBook saved{ book("Big book", big) };

	EXPECT_EQ(big, ar::belongs_to<&RiBook::author_id>(saved)->id);
	EXPECT_EQ(1, ar::has_many<&RiBook::author_id>(big_author).count());
	EXPECT_EQ(big, ar::build<&RiBook::author_id>(big_author).author_id);
}

TEST_P(RelationsIntegrationTest, PartialLoadsThrowBeforeAnyQuery) {
	const RiAuthor ann{ author("Ann") };
	const RiBook saved{ book("Dune", ann.id) };
	const RiBook partial_book{ *query<RiBook>().select("id, title").where("id = ?", attribute{ saved.id }).first() };
	const RiAuthor partial_author{ *query<RiAuthor>().select("name").first() };

	const std::size_t before{ queries() };
	EXPECT_THROW(ar::belongs_to<&RiBook::author_id>(partial_book), ex::activerecord);
	EXPECT_THROW(ar::has_many<&RiBook::author_id>(partial_author), ex::activerecord);
	EXPECT_THROW(ar::has_one<&RiProfile::author_id>(partial_author), ex::activerecord);
	EXPECT_THROW(ar::build<&RiBook::author_id>(partial_author), ex::activerecord);
	EXPECT_EQ(before, queries());
}

INSTANTIATE_TEST_SUITE_P(AllAdapters, RelationsIntegrationTest, ::testing::ValuesIn(enabled_adapters()),
						 adapter_param_name);

} // namespace
