// ============================================================================
// ddl_integration_test.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// Statement plans, table_exists() and foreign-key enforcement on every adapter
//

#include "test_adapters.h"

#include <activerecord/schema.h>
#include <activerecord/typed_model.h>
#include <algorithm>
#include <cstdint>
#include <format>
#include <gtest/gtest.h>
#include <limits>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef AR_SQLITE
#include <activerecord/adapters/sqlite_adapter.h>
#endif

using namespace ar;
using namespace ar::test;

namespace {

struct DdlAuthor : typed_model<DdlAuthor> {
	static constexpr auto table_name{ "ddl_authors" };
	static constexpr auto primary_key{ "id" };
	std::int64_t id{};
	std::string name{};
	static constexpr auto columns() {
		return ar::columns(col<&DdlAuthor::id>("id"), col<&DdlAuthor::name>("name").limit(50).unique());
	}
};

struct DdlPublisher : typed_model<DdlPublisher> {
	static constexpr auto table_name{ "ddl_publishers" };
	static constexpr auto primary_key{ "id" };
	std::int64_t id{};
	std::string name{};
	static constexpr auto columns() {
		return ar::columns(col<&DdlPublisher::id>("id"), col<&DdlPublisher::name>("name"));
	}
};

// The primary key is not the first column on purpose
struct DdlBook : typed_model<DdlBook> {
	static constexpr auto table_name{ "ddl_books" };
	static constexpr auto primary_key{ "id" };
	std::string title{};
	std::int64_t id{};
	std::optional<std::string> summary{};
	double price{};
	bool in_print{};
	int pages{};
	datetime published{};
	std::int64_t author_id{};
	std::optional<std::int64_t> publisher_id{};
	static constexpr auto columns() {
		return ar::columns(
			col<&DdlBook::title>("title").limit(100).index(), col<&DdlBook::id>("id"),
			col<&DdlBook::summary>("summary").text(), col<&DdlBook::price>("price").decimal(10, 2).default_value(0),
			col<&DdlBook::in_print>("in_print").default_value(true),
			col<&DdlBook::pages>("pages").integer().default_value(-5), col<&DdlBook::published>("published"),
			col<&DdlBook::author_id>("author_id").references<DdlAuthor>().on_delete(fk_action::cascade),
			col<&DdlBook::publisher_id>("publisher_id")
				.references<DdlPublisher>()
				.on_delete(fk_action::set_null)
				.on_update(fk_action::cascade));
	}
};

std::vector<std::string> expected_book_plan(AdapterType type) {
	switch (type) {
	case AdapterType::SQLite:
		return {
			"CREATE TABLE `ddl_books` (`title` TEXT NOT NULL, `id` INTEGER PRIMARY KEY AUTOINCREMENT, "
			"`summary` TEXT NULL, `price` NUMERIC(10,2) NOT NULL DEFAULT 0, `in_print` BOOLEAN NOT NULL DEFAULT 1, "
			"`pages` INTEGER NOT NULL DEFAULT -5, `published` INTEGER NOT NULL, `author_id` INTEGER NOT NULL, "
			"`publisher_id` INTEGER NULL, FOREIGN KEY (`author_id`) REFERENCES `ddl_authors` (`id`) ON DELETE "
			"CASCADE, FOREIGN KEY (`publisher_id`) REFERENCES `ddl_publishers` (`id`) ON DELETE SET NULL ON "
			"UPDATE CASCADE)",
			"CREATE INDEX `ix_9_ddl_books_title` ON `ddl_books` (`title`)"
		};
	case AdapterType::MySQL:
		return {
			"CREATE TABLE `ddl_books` (`title` VARCHAR(100) NOT NULL, `id` BIGINT AUTO_INCREMENT PRIMARY KEY, "
			"`summary` TEXT NULL, `price` DECIMAL(10,2) NOT NULL DEFAULT 0, `in_print` BOOLEAN NOT NULL DEFAULT 1, "
			"`pages` INT NOT NULL DEFAULT -5, `published` BIGINT NOT NULL, `author_id` BIGINT NOT NULL, "
			"`publisher_id` BIGINT NULL, FOREIGN KEY (`author_id`) REFERENCES `ddl_authors` (`id`) ON DELETE "
			"CASCADE, FOREIGN KEY (`publisher_id`) REFERENCES `ddl_publishers` (`id`) ON DELETE SET NULL ON "
			"UPDATE CASCADE) ENGINE=InnoDB",
			"CREATE INDEX `ix_9_ddl_books_title` ON `ddl_books` (`title`)"
		};
	case AdapterType::PostgreSQL:
		return {
			R"sql(CREATE TABLE "ddl_books" ("title" VARCHAR(100) NOT NULL, "id" BIGSERIAL PRIMARY KEY, )sql"
			R"sql("summary" TEXT NULL, "price" NUMERIC(10,2) NOT NULL DEFAULT 0, "in_print" BOOLEAN NOT NULL )sql"
			R"sql(DEFAULT TRUE, "pages" INTEGER NOT NULL DEFAULT -5, "published" BIGINT NOT NULL, "author_id" )sql"
			R"sql(BIGINT NOT NULL, "publisher_id" BIGINT NULL, FOREIGN KEY ("author_id") REFERENCES "ddl_authors" )sql"
			R"sql(("id") ON DELETE CASCADE, FOREIGN KEY ("publisher_id") REFERENCES "ddl_publishers" ("id") ON )sql"
			"DELETE SET NULL ON UPDATE CASCADE)",
			R"sql(CREATE INDEX "ix_9_ddl_books_title" ON "ddl_books" ("title"))sql"
		};
	case AdapterType::MSSQL:
		return { "CREATE TABLE [ddl_books] ([title] VARCHAR(100) NOT NULL, [id] BIGINT IDENTITY(1,1) PRIMARY KEY, "
				 "[summary] VARCHAR(MAX) NULL, [price] DECIMAL(10,2) NOT NULL DEFAULT 0, [in_print] BIT NOT NULL "
				 "DEFAULT 1, [pages] INT NOT NULL DEFAULT -5, [published] BIGINT NOT NULL, [author_id] BIGINT NOT "
				 "NULL, [publisher_id] BIGINT NULL, FOREIGN KEY ([author_id]) REFERENCES [ddl_authors] ([id]) ON "
				 "DELETE CASCADE, FOREIGN KEY ([publisher_id]) REFERENCES [ddl_publishers] ([id]) ON DELETE SET NULL "
				 "ON UPDATE CASCADE)",
				 "CREATE INDEX [ix_9_ddl_books_title] ON [ddl_books] ([title])" };
	}
	return {};
}

column_schema string_column(std::string name, std::size_t length) {
	return column_schema{ std::move(name), column_type::string, length };
}

table_schema limits_schema(std::vector<column_schema> extra) {
	table_schema schema{ "ddl_limits", "id", { column_schema{ "id" } }, {} };
	for (auto& column : extra) {
		schema.columns.push_back(std::move(column));
	}
	return schema;
}

bool index_exists(AdapterType type, connection& conn, std::string_view table, std::string_view index) {
	switch (type) {
	case AdapterType::SQLite:
		return conn
			.query("SELECT 1 FROM sqlite_master WHERE type = 'index' AND tbl_name = ? AND name = ?",
				   { attribute{ table }, attribute{ index } })
			->fetch();
	case AdapterType::MySQL:
		return conn
			.query("SELECT 1 FROM information_schema.statistics WHERE table_schema = DATABASE() AND "
				   "table_name = ? AND index_name = ?",
				   { attribute{ table }, attribute{ index } })
			->fetch();
	case AdapterType::PostgreSQL:
		return conn
			.query("SELECT 1 FROM pg_indexes WHERE schemaname = current_schema() AND tablename = ? AND "
				   "indexname = ?",
				   { attribute{ table }, attribute{ index } })
			->fetch();
	case AdapterType::MSSQL:
		return conn
			.query("SELECT 1 FROM sys.indexes WHERE object_id = OBJECT_ID(?) AND name = ?",
				   { attribute{ table }, attribute{ index } })
			->fetch();
	}
	return false;
}

} // namespace

class DdlTest : public AdapterTest {
  protected:
	table_list table_definitions(AdapterType /*type*/) const override { return {}; }

	void TearDown() override {
		for (const auto& name : created_ | std::views::reverse) {
			drop(name);
		}
		AdapterTest::TearDown();
	}

	void drop(const std::string& name) const {
		try {
			adapter_->query("DROP TABLE IF EXISTS " + adapter_->escape(name), {});
		} catch (...) { // NOLINT(bugprone-empty-catch) -- best-effort cleanup
		}
	}

	// Runs every statement of the plan, as create_table<T>() will
	void create(const table_schema& schema) {
		drop(schema.name);
		for (const std::string& sql : adapter_->create_table_plan(schema)) {
			adapter_->query(sql, {});
		}
		created_.push_back(schema.name);
	}

	void create_library() {
		create(table_schema_for<DdlAuthor>());
		create(table_schema_for<DdlPublisher>());
		create(table_schema_for<DdlBook>());
	}

	std::vector<std::string> created_;
};

TEST_P(DdlTest, BookPlanIsExact) {
	EXPECT_EQ(expected_book_plan(adapter_type()), adapter_->create_table_plan(table_schema_for<DdlBook>()));
}

TEST_P(DdlTest, PlanRunsAndRowsRoundTrip) {
	create_library();

	DdlAuthor author;
	author.name = "Ann";
	ASSERT_TRUE(author.save());
	DdlPublisher publisher;
	publisher.name = "Press";
	ASSERT_TRUE(publisher.save());

	DdlBook book;
	book.title = "Tables";
	book.summary = "About tables";
	book.price = 12.5;
	book.in_print = true;
	book.pages = 300;
	book.published = datetime{ 2024, 6, 15, 14, 30, 0 };
	book.author_id = author.id;
	book.publisher_id = publisher.id;
	ASSERT_TRUE(book.save());

	const DdlBook loaded{ query<DdlBook>().find(book.id) };
	EXPECT_EQ("Tables", loaded.title);
	EXPECT_EQ(std::optional<std::string>{ "About tables" }, loaded.summary);
	EXPECT_EQ(12.5, loaded.price);
	EXPECT_TRUE(loaded.in_print);
	EXPECT_EQ(300, loaded.pages);
	EXPECT_EQ(book.published.timestamp(), loaded.published.timestamp());
	EXPECT_EQ(author.id, loaded.author_id);
	EXPECT_EQ(publisher.id, loaded.publisher_id);
}

TEST_P(DdlTest, DefaultsApplyToRowsInsertedWithoutThem) {
	create_library();
	DdlAuthor author;
	author.name = "Ann";
	ASSERT_TRUE(author.save());
	adapter_->query(std::format("INSERT INTO {} ({}, {}, {}) VALUES (?, ?, ?)", adapter_->escape("ddl_books"),
								adapter_->escape("title"), adapter_->escape("published"),
								adapter_->escape("author_id")),
					{ "Raw", std::int64_t{ 0 }, author.id });

	const DdlBook loaded{ query<DdlBook>().where("title = ?", "Raw").first_or_throw() };
	EXPECT_EQ(0.0, loaded.price);
	EXPECT_TRUE(loaded.in_print);
	EXPECT_EQ(-5, loaded.pages);
	EXPECT_FALSE(loaded.publisher_id.has_value());
}

TEST_P(DdlTest, TableExists) {
	create_library();
	EXPECT_TRUE(adapter_->table_exists("ddl_books"));
	EXPECT_FALSE(adapter_->table_exists("ddl_missing"));
	for (const char* name : { "ddl_books'", "ddl_?", "ddl_%", "_", "%" }) {
		EXPECT_FALSE(adapter_->table_exists(name)) << name; // bound, never a pattern or SQL
	}
}

TEST_P(DdlTest, NameBoundaries) {
	const std::string longest(max_name_length, 't'); // 63 bytes
	create(table_schema{ longest, "id", { column_schema{ "id" } }, {} });
	EXPECT_TRUE(adapter_->table_exists(longest));

	// ix_6_ddl_ix_ plus a 51-byte column name is a 63-byte index name
	table_schema indexed{ "ddl_ix", "id", { column_schema{ "id" }, string_column(std::string(51, 'c'), 10) }, {} };
	indexed.columns.back().index = true;
	create(indexed);
	EXPECT_TRUE(adapter_->table_exists("ddl_ix"));
}

TEST_P(DdlTest, ViewIsNotATable) {
	const std::string view{ adapter_->escape("ddl_view") };
	try {
		adapter_->query("DROP VIEW " + view, {});
	} catch (...) { // NOLINT(bugprone-empty-catch) -- the view may not exist
	}
	adapter_->query("CREATE VIEW " + view + " AS SELECT 1 AS x", {});
	EXPECT_FALSE(adapter_->table_exists("ddl_view"));
	adapter_->query("DROP VIEW " + view, {});
}

TEST_P(DdlTest, OtherSchemaIsNotTheCurrentOne) {
	if (adapter_type() != AdapterType::PostgreSQL) {
		return; // only PostgreSQL has a second schema available to the test user
	}
	adapter_->query("DROP SCHEMA IF EXISTS ddl_other CASCADE", {});
	adapter_->query("CREATE SCHEMA ddl_other", {});
	adapter_->query("CREATE TABLE ddl_other.ddl_shadow (x INTEGER)", {});
	EXPECT_FALSE(adapter_->table_exists("ddl_shadow"));
	adapter_->query("DROP SCHEMA ddl_other CASCADE", {});
}

TEST_P(DdlTest, ForeignKeysAreEnforced) {
#ifdef AR_SQLITE
	if (adapter_type() == AdapterType::SQLite) {
		auto& sqlite{ dynamic_cast<sqlite_adapter&>(*adapter_) };
		EXPECT_FALSE(sqlite.enforces_foreign_keys());
		sqlite.foreign_keys(true);
	}
#endif
	EXPECT_TRUE(adapter_->enforces_foreign_keys());
	create_library();

	DdlBook orphan;
	orphan.title = "Orphan";
	orphan.author_id = 999999;
	EXPECT_ANY_THROW(orphan.save());

	if (adapter_type() == AdapterType::MySQL) {
		adapter_->query("SET foreign_key_checks = 0", {});
		EXPECT_FALSE(adapter_->enforces_foreign_keys());
		adapter_->query("SET foreign_key_checks = 1", {});
		EXPECT_TRUE(adapter_->enforces_foreign_keys());
	}
}

TEST_P(DdlTest, DecimalScaleLimits) {
	const auto decimal = [](int precision, int scale) {
		return limits_schema({ column_schema{ "d", column_type::decimal, 0, precision, scale } });
	};
	if (adapter_type() == AdapterType::MySQL) {
		create(decimal(38, 30));
		EXPECT_THROW((void)adapter_->create_table_plan(decimal(38, 31)), ex::activerecord);
		EXPECT_THROW((void)adapter_->create_table_plan(decimal(38, 38)), ex::activerecord);
	} else {
		create(decimal(38, 38)); // only MySQL narrows the common limit
	}
	EXPECT_TRUE(adapter_->table_exists("ddl_limits"));
}

TEST_P(DdlTest, MysqlRowSizeBudget) {
	if (adapter_type() != AdapterType::MySQL) {
		return; // MySQL-specific limit
	}
	const auto reject = [&](const table_schema& schema) {
		EXPECT_THROW((void)adapter_->create_table_plan(schema), ex::activerecord);
	};
	// BIGINT key (8) + VARCHAR(16381) (65524 + 2) = 65534 bytes
	create(limits_schema({ string_column("v", 16381) }));
	reject(limits_schema({ string_column("v", 16383) }));

	// The VARCHAR length prefix grows from 1 to 2 bytes after 63 characters: 8 + 65274 + 253 = 65535
	create(limits_schema({ string_column("v", 16318), string_column("w", 63) }));
	reject(limits_schema({ string_column("v", 16318), string_column("w", 64) }));

	// 8 + 65514 + DECIMAL(5,0) 3 + 9 BOOLEAN = 65534, plus 1 NULL-bitmap byte for up to 8 nullable columns
	const auto with_nullable = [](int nullable) {
		std::vector<column_schema> columns{ string_column("v", 16378),
											column_schema{ "d", column_type::decimal, 0, 5, 0 } };
		for (int i = 0; i < 9; ++i) {
			columns.push_back(column_schema{ std::format("b{}", i), column_type::boolean, 0, 0, 0, i < nullable });
		}
		return limits_schema(std::move(columns));
	};
	create(with_nullable(7));
	create(with_nullable(8));
	reject(with_nullable(9));

	// TEXT counts a conservative 12 bytes: 8 + 65514 + 12 = 65534
	create(limits_schema({ string_column("v", 16378), column_schema{ "t", column_type::text } }));
	reject(limits_schema({ string_column("v", 16378), column_schema{ "t", column_type::text },
						   column_schema{ "u", column_type::text } }));
}

TEST_P(DdlTest, IndexedColumnLimits) {
	const auto indexed = [](column_schema column) {
		column.index = true;
		return limits_schema({ std::move(column) });
	};
	switch (adapter_type()) {
	case AdapterType::MySQL:
		create(indexed(string_column("v", 768)));
		EXPECT_THROW((void)adapter_->create_table_plan(indexed(string_column("v", 769))), ex::activerecord);
		EXPECT_THROW((void)adapter_->create_table_plan(indexed(column_schema{ "t", column_type::text })),
					 ex::activerecord);
		break;
	case AdapterType::MSSQL:
		create(indexed(string_column("v", 1700)));
		EXPECT_THROW((void)adapter_->create_table_plan(indexed(string_column("v", 1701))), ex::activerecord);
		EXPECT_THROW((void)adapter_->create_table_plan(indexed(column_schema{ "t", column_type::text })),
					 ex::activerecord);
		create(limits_schema({ string_column("v", 8000) }));
		EXPECT_THROW((void)adapter_->create_table_plan(limits_schema({ string_column("v", 8001) })), ex::activerecord);
		break;
	case AdapterType::PostgreSQL:
		create(limits_schema({ string_column("v", 10485760) }));
		EXPECT_THROW((void)adapter_->create_table_plan(limits_schema({ string_column("v", 10485761) })),
					 ex::activerecord);
		break;
	case AdapterType::SQLite:
		create(indexed(column_schema{ "t", column_type::text })); // no limits
		break;
	}
	EXPECT_TRUE(adapter_->table_exists("ddl_limits"));
}

// ==============================================================================
// create_table / drop_table API
// ==============================================================================

TEST_P(DdlTest, CreateTablesAndDropTables) {
#ifdef AR_SQLITE
	if (adapter_type() == AdapterType::SQLite) {
		dynamic_cast<sqlite_adapter&>(*adapter_).foreign_keys(true);
	}
#endif
	for (const char* name : { "ddl_books", "ddl_publishers", "ddl_authors" }) {
		drop(name);
		created_.emplace_back(name); // cleanup if the test fails half-way
	}
	create_tables<DdlAuthor, DdlPublisher, DdlBook>();
	EXPECT_TRUE(adapter_->table_exists("ddl_books"));
	EXPECT_FALSE(create_table<DdlAuthor>({ .if_not_exists = true }));

	DdlAuthor author;
	author.name = "Ann";
	ASSERT_TRUE(author.save());
	DdlBook book;
	book.title = "API";
	book.author_id = author.id;
	ASSERT_TRUE(book.save());
	EXPECT_EQ("API", query<DdlBook>().find(book.id).title);

	drop_tables<DdlAuthor, DdlPublisher, DdlBook>(); // books first
	for (const char* name : { "ddl_books", "ddl_publishers", "ddl_authors" }) {
		EXPECT_FALSE(adapter_->table_exists(name)) << name;
	}
	EXPECT_NO_THROW((drop_tables<DdlAuthor, DdlPublisher, DdlBook>({ .if_exists = true })));
}

TEST_P(DdlTest, IdsAboveInt32Max) {
	create(table_schema_for<DdlAuthor>());
	constexpr std::int64_t big{ std::int64_t{ std::numeric_limits<std::int32_t>::max() } + 10 };
	const attribute_vector row{ attribute{ big }, attribute{ std::string{ "Big" } } };
	const std::string insert{ "INSERT INTO ddl_authors (id, name) VALUES (?, ?)" };
	switch (adapter_type()) {
	case AdapterType::MSSQL:
		// One batch: a prepared statement's SET does not outlive it
		adapter_->query("SET IDENTITY_INSERT ddl_authors ON; " + insert + "; SET IDENTITY_INSERT ddl_authors OFF", row);
		break;
	case AdapterType::PostgreSQL:
		adapter_->query(insert, row);
		adapter_->query("SELECT setval('ddl_authors_id_seq', ?)", { attribute{ big } });
		break;
	default:
		adapter_->query(insert, row);
		break;
	}

	const auto found{ query<DdlAuthor>().find_optional(big) };
	ASSERT_TRUE(found.has_value());
	EXPECT_EQ("Big", found->name);
	EXPECT_EQ(big, query<DdlAuthor>().find(big).id);

	DdlAuthor next;
	next.name = "Next";
	ASSERT_TRUE(next.save());
	EXPECT_GT(next.id, big);
	EXPECT_EQ("Next", query<DdlAuthor>().find(next.id).name);
}

TEST_P(DdlTest, ForeignKeyTablesNeedEnforcement) {
	if (adapter_type() != AdapterType::SQLite && adapter_type() != AdapterType::MySQL) {
		return; // PostgreSQL and SQL Server always enforce new constraints
	}
	const std::vector<std::string>* statements{ nullptr };
	const std::shared_ptr<connection> conn{ make_counting(adapter_type(), statements) };
	config::instance().set_connections({ { "default", conn } }, "default");
	const auto enforce = [&](bool on) {
		if (adapter_type() == AdapterType::SQLite) {
			conn->query(on ? "PRAGMA foreign_keys = ON" : "PRAGMA foreign_keys = OFF", {});
		} else {
			conn->query(on ? "SET foreign_key_checks = 1" : "SET foreign_key_checks = 0", {});
		}
	};
	const auto creates = [&] {
		return std::ranges::count_if(*statements, [](const std::string& sql) { return sql.starts_with("CREATE"); });
	};
	for (const char* name : { "ddl_books", "ddl_publishers", "ddl_authors" }) {
		drop(name);
	}
	created_ = { "ddl_authors", "ddl_publishers", "ddl_books" };
	create_tables<DdlAuthor, DdlPublisher>(); // on the recording connection: SQLite's is its own database

	enforce(false);
	const auto before{ creates() };
	EXPECT_THROW(create_table<DdlBook>(), ex::config);
	EXPECT_EQ(before, creates());
	EXPECT_FALSE(conn->table_exists("ddl_books"));

	// An existing FK table is skipped without looking at enforcement
	enforce(true);
	create_table<DdlBook>();
	enforce(false);
	const std::size_t queries{ statements->size() };
	EXPECT_FALSE(create_table<DdlBook>({ .if_not_exists = true }));
	EXPECT_EQ(queries + 1, statements->size()); // the existence check only
}

TEST_P(DdlTest, InvalidSchemasSendNoQuery) {
	const std::vector<std::string>* statements{ nullptr };
	const std::shared_ptr<connection> conn{ make_counting(adapter_type(), statements) };
	config::instance().set_connections({ { "default", conn } }, "default");

	table_schema common{ limits_schema({ string_column("a.b", 10) }) };
	table_schema dialect{ limits_schema({ string_column("v", 10) }) };
	switch (adapter_type()) {
	case AdapterType::MySQL:
		dialect.columns.back().length = 16383;
		break;
	case AdapterType::MSSQL:
		dialect.columns.back().length = 8001;
		break;
	case AdapterType::PostgreSQL:
		dialect.columns.back().length = 10485761;
		break;
	case AdapterType::SQLite:
		dialect.columns.push_back(string_column("V", 10)); // SQLite has no dialect limits: an uppercase name
		break;
	}
	const std::size_t before{ statements->size() };
	EXPECT_THROW(create_table(common), ex::activerecord);
	EXPECT_THROW(create_table(dialect, { .if_not_exists = true }), ex::activerecord);
	EXPECT_THROW(ar::drop_table("a.b"), ex::activerecord); // ar:: — AdapterTest has its own drop_table
	EXPECT_EQ(before, statements->size());
}

TEST_P(DdlTest, MixedCaseNamesAreRejected) {
	// The ORM writes table names unquoted; a quoted mixed-case table would be a different table on PostgreSQL
	for (const char* table : { "DdlMixed_Case", "ddl odd", "ddl'odd" }) {
		EXPECT_THROW(create_table(table_schema{ table, "id", { column_schema{ "id" } }, {} }), ex::activerecord)
			<< table;
	}
	EXPECT_THROW(
		create_table(table_schema{ "ddl_mixed", "id", { column_schema{ "id" }, string_column("Value", 10) }, {} }),
		ex::activerecord);
	EXPECT_FALSE(adapter_->table_exists("ddl_mixed"));
}

TEST_P(DdlTest, DefaultExtremes) {
	const auto real_default = [](std::string name, double value) {
		return column_schema{ std::move(name), column_type::real, 0, 0, 0, false, false, false, value };
	};
	const auto bigint_default = [](std::string name, std::int64_t value) {
		return column_schema{ std::move(name), column_type::bigint, 0, 0, 0, false, false, false, value };
	};
	created_.emplace_back("ddl_defaults");
	drop("ddl_defaults");
	create_table(
		table_schema{ "ddl_defaults",
					  "id",
					  { column_schema{ "id" }, real_default("r_max", std::numeric_limits<double>::max()),
						real_default("r_low", std::numeric_limits<double>::lowest()), real_default("r_tiny", 1e-300),
						bigint_default("i_min", std::numeric_limits<std::int64_t>::min()),
						bigint_default("i_max", std::numeric_limits<std::int64_t>::max()) },
					  {} });
	adapter_->query(
		std::format("INSERT INTO {} {}", adapter_->escape("ddl_defaults"), adapter_->default_values_insert_clause()),
		{});

	auto stmt{ adapter_->query(std::format("SELECT {}, {}, {}, {}, {} FROM {}", adapter_->escape("r_max"),
										   adapter_->escape("r_low"), adapter_->escape("r_tiny"),
										   adapter_->escape("i_min"), adapter_->escape("i_max"),
										   adapter_->escape("ddl_defaults")),
							   {}) };
	ASSERT_TRUE(stmt->fetch());
	attribute_map row;
	stmt->get_attributes(row);
	EXPECT_EQ(std::numeric_limits<double>::max(), row.at("r_max").as_real());
	EXPECT_EQ(std::numeric_limits<double>::lowest(), row.at("r_low").as_real());
	if (adapter_type() == AdapterType::SQLite) {
		// SQLite's parser for numbers in SQL text is not correctly rounded at extreme exponents
		EXPECT_DOUBLE_EQ(1e-300, row.at("r_tiny").as_real());
	} else {
		EXPECT_EQ(1e-300, row.at("r_tiny").as_real());
	}
	EXPECT_EQ(std::numeric_limits<std::int64_t>::min(), row.at("i_min").as_integer());
	EXPECT_EQ(std::numeric_limits<std::int64_t>::max(), row.at("i_max").as_integer());
}

TEST_P(DdlTest, SeparateIndexOnThePrimaryKey) {
	table_schema schema{ "ddl_pk_index", "id", { column_schema{ "id" } }, {} };
	schema.columns[0].index = true;
	created_.emplace_back(schema.name);
	drop(schema.name);
	create_table(schema);
	EXPECT_TRUE(index_exists(adapter_type(), *adapter_, schema.name, index_name(schema.name, "id")));
}

INSTANTIATE_TEST_SUITE_P(AllAdapters, DdlTest, ::testing::ValuesIn(enabled_adapters()), adapter_param_name);
