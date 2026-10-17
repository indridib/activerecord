// ============================================================================
// model_test.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// Unit tests for the model class
//

#include <activerecord/activerecord.h>
#include <activerecord/config.h>
#include <activerecord/model.h>
#include <gtest/gtest.h>

using namespace ar;

// Mock connection for testing (satisfies the interface without real DB)
namespace {

class mock_connection : public connection {
  public:
	std::unique_ptr<statement> query(const std::string&, const attribute_vector&) override { return nullptr; }

	void limit(std::string& sql, long offset, long limit) const override {
		if (limit >= 0) {
			sql += " LIMIT " + std::to_string(limit);
			if (offset >= 0) {
				sql += " OFFSET " + std::to_string(offset);
			}
		}
	}

	std::size_t insert_id() const override { return 0; }

	void begin_transaction() override {}
	void commit() override {}
	void rollback() override {}
};

// Counts query() calls, to prove that rejected queries never reach the database
class counting_connection : public mock_connection {
  public:
	std::unique_ptr<statement> query(const std::string&, const attribute_vector&) override {
		++calls;
		return nullptr;
	}

	int calls{ 0 };
};

// Records the last SQL and reports one affected row, so update() and destroy() can run
class one_row_statement : public statement {
  public:
	void execute(const std::string&, const attribute_vector&) override {}
	std::size_t affected_rows() const noexcept override { return 1; }
};

class recording_connection : public mock_connection {
  public:
	std::unique_ptr<statement> query(const std::string& sql, const attribute_vector&) override {
		last_sql = sql;
		return std::make_unique<one_row_statement>();
	}

	std::string last_sql{};
};

class preflight_item : public model {
  public:
	static const char* st_table_name() { return "items"; }
	static const char* st_primary_key() { return "id"; }

	preflight_item() : model(st_table_name(), st_primary_key()) {}
};

} // namespace

// Test model class
class TestModel : public model {
  public:
	TestModel() : model("test_table", "id") {}

	using model::attributes_;
	using model::new_record_;
};

// Model whose primary key is not named "id"
class UidModel : public model {
  public:
	explicit UidModel(std::string_view primary_key = "uid") : model("accounts", primary_key) {}

	using model::attributes_;
	using model::new_record_;

	void make_persisted(const std::string& pk_name, const attribute& pk_value) {
		attributes_[pk_name] = pk_value;
		attributes_["name"] = "Alice";
		new_record_ = false;
		reset_dirty();
	}
};

// Test fixture to set up mock connection
class ModelTest : public ::testing::Test {
  protected:
	void SetUp() override {
		auto mock_conn = std::make_shared<mock_connection>();
		config::connection_map connections;
		connections["default"] = mock_conn;
		config::instance().set_connections(connections, "default");
	}
};

// ==============================================================================
// Basic Attribute Management Tests
// ==============================================================================

TEST_F(ModelTest, GetPrimaryKey) {
	TestModel m;
	EXPECT_EQ("id", m.get_primary_key());
}

TEST_F(ModelTest, GetTableName) {
	TestModel m;
	EXPECT_EQ("test_table", m.table_name());
}

TEST_F(ModelTest, SetAndGetAttribute) {
	TestModel m;
	m.set("name", "John");

	const attribute& attr = m.get("name");
	EXPECT_EQ("John", attr.as_string());
}

TEST_F(ModelTest, SetAndGetMultipleAttributes) {
	TestModel m;
	m.set("name", "John");
	m.set("age", 30);
	m.set("active", true);

	EXPECT_EQ("John", m.get("name").as_string());
	EXPECT_EQ(30, m.get("age").as_integer());
	EXPECT_TRUE(m.get("active").as_bool());
}

TEST_F(ModelTest, GetNonExistentAttributeThrows) {
	TestModel m;
	EXPECT_THROW(m.get("nonexistent"), ex::attribute_not_found);
}

TEST_F(ModelTest, BracketOperatorAccess) {
	TestModel m;
	m.set("name", "Jane");

	EXPECT_EQ("Jane", m["name"].as_string());
}

TEST_F(ModelTest, IssetExistingAttribute) {
	TestModel m;
	m.set("name", "John");

	EXPECT_TRUE(m.isset("name"));
}

TEST_F(ModelTest, IssetNonExistentAttribute) {
	TestModel m;
	EXPECT_FALSE(m.isset("nonexistent"));
}

// ==============================================================================
// Bulk Attribute Operations
// ==============================================================================

TEST_F(ModelTest, SetAttributes) {
	TestModel m;
	attribute_map attrs;
	attrs["name"] = "John";
	attrs["age"] = 25;
	attrs["email"] = "john@example.com";

	m.set_attributes(attrs);

	EXPECT_EQ("John", m.get("name").as_string());
	EXPECT_EQ(25, m.get("age").as_integer());
	EXPECT_EQ("john@example.com", m.get("email").as_string());
}

TEST_F(ModelTest, SetAttributesWithGuardTrue) {
	TestModel m;
	m.set("name", "Original");

	attribute_map attrs;
	attrs["name"] = "Updated";
	attrs["nonexistent"] = "New"; // This should throw

	EXPECT_THROW(m.set_attributes(attrs, false), ex::attribute_not_found); // allow_new_attributes=false throws
}

TEST_F(ModelTest, SetAttributesWithGuardFalse) {
	TestModel m;
	attribute_map attrs;
	attrs["new_field"] = "New Value";

	m.set_attributes(attrs, true); // allow_new_attributes=true (or use default)

	EXPECT_EQ("New Value", m.get("new_field").as_string());
}

TEST_F(ModelTest, GetAttributes) {
	TestModel m;
	m.set("name", "John");
	m.set("age", 30);

	const attribute_map& attrs = m.attributes();

	EXPECT_EQ(2, attrs.size());
	EXPECT_NE(attrs.end(), attrs.find("name"));
	EXPECT_NE(attrs.end(), attrs.find("age"));
}

// ==============================================================================
// Dirty Tracking Tests
// ==============================================================================

TEST_F(ModelTest, InitiallyNotDirty) {
	TestModel m;
	EXPECT_FALSE(m.is_dirty());
	EXPECT_TRUE(m.dirty_attributes().empty());
}

TEST_F(ModelTest, SettingAttributeMakesDirty) {
	TestModel m;
	m.set("name", "John");

	EXPECT_TRUE(m.is_dirty());
	EXPECT_TRUE(m.attribute_is_dirty("name"));
}

TEST_F(ModelTest, FlagDirty) {
	TestModel m;
	m.flag_dirty("test_field");

	EXPECT_TRUE(m.is_dirty());
	EXPECT_TRUE(m.attribute_is_dirty("test_field"));
}

TEST_F(ModelTest, ResetDirty) {
	TestModel m;
	m.set("name", "John");
	EXPECT_TRUE(m.is_dirty());

	m.reset_dirty();

	EXPECT_FALSE(m.is_dirty());
	EXPECT_FALSE(m.attribute_is_dirty("name"));
}

TEST_F(ModelTest, DirtyAttributesSet) {
	TestModel m;
	m.set("name", "John");
	m.set("age", 30);
	m.set("email", "john@example.com");

	const string_set& dirty = m.dirty_attributes();

	EXPECT_EQ(3, dirty.size());
	EXPECT_NE(dirty.end(), dirty.find("name"));
	EXPECT_NE(dirty.end(), dirty.find("age"));
	EXPECT_NE(dirty.end(), dirty.find("email"));
}

// ==============================================================================
// New Record Tests
// ==============================================================================

TEST_F(ModelTest, InitiallyIsNewRecord) {
	TestModel m;
	EXPECT_TRUE(m.is_new_record());
}

// ==============================================================================
// Readonly Mode Tests
// ==============================================================================

TEST_F(ModelTest, InitiallyNotReadonly) {
	TestModel m;
	EXPECT_FALSE(m.is_readonly());
}

TEST_F(ModelTest, SetReadonly) {
	TestModel m;
	m.readonly(true);

	EXPECT_TRUE(m.is_readonly());
}

TEST_F(ModelTest, VerifyNotReadonlyDoesNotThrow) {
	TestModel m;
	EXPECT_NO_THROW(m.verify_not_readonly("test_method"));
}

TEST_F(ModelTest, VerifyNotReadonlyThrowsWhenReadonly) {
	TestModel m;
	m.readonly(true);

	EXPECT_THROW(m.verify_not_readonly("test_method"), ex::readonly);
}

TEST_F(ModelTest, CannotModifyPrimaryKeyOnPersistedRecord) {
	TestModel m;
	m.new_record_ = false;
	m.attributes_["id"] = 1; // set up persisted PK directly
	m.reset_dirty();

	EXPECT_THROW(m.set("id", 2, false), ex::readonly);
}

TEST_F(ModelTest, CustomPrimaryKeyCannotBeChangedOnPersistedRecord) {
	UidModel m;
	m.make_persisted("uid", 1);
	const attribute two{ 2 };

	EXPECT_THROW(m.set("id", two), ex::readonly);
	EXPECT_THROW(m.set("id", attribute{ 2 }), ex::readonly);
	EXPECT_THROW(m.set("uid", two), ex::readonly);
	EXPECT_THROW(m.set("uid", attribute{ 2 }), ex::readonly);
	const attribute_map by_alias{ { "id", 2 } };
	EXPECT_THROW(m.set_attributes(by_alias), ex::readonly);
	EXPECT_THROW(m.set_attributes(attribute_map{ { "id", 2 } }), ex::readonly);
	EXPECT_THROW(m.update_attributes(by_alias), ex::readonly);
	EXPECT_THROW(m.update_attribute("id", two), ex::readonly);

	EXPECT_EQ(1, m.get("uid").as_integer());
	EXPECT_FALSE(m.isset("id"));
	EXPECT_FALSE(m.is_dirty());
}

TEST_F(ModelTest, PersistedPrimaryKeyNotLoadedCannotBeSet) {
	UidModel m;
	m.make_persisted("uid", 1);
	m.attributes_.erase("uid");

	EXPECT_THROW(m.set("uid", 2), ex::readonly);
	EXPECT_FALSE(m.isset("uid"));
}

TEST_F(ModelTest, RealIdColumnNextToCustomPrimaryKeyStaysEditable) {
	UidModel m;
	m.make_persisted("uid", 1);
	m.attributes_["id"] = 10;

	m.set("id", 11);

	EXPECT_EQ(11, m.get("id").as_integer());
	EXPECT_EQ(1, m.get("uid").as_integer());
	EXPECT_TRUE(m.attribute_is_dirty("id"));
}

TEST_F(ModelTest, NewRecordSetIdWithCustomPrimaryKey) {
	UidModel without_pk;
	without_pk.set("id", 5);
	EXPECT_EQ(5, without_pk.attributes_.at("id").as_integer());
	EXPECT_FALSE(without_pk.isset("uid"));

	UidModel with_pk;
	with_pk.attributes_["uid"] = 1;
	with_pk.set("id", 5);
	EXPECT_EQ(5, with_pk.get("uid").as_integer());
	EXPECT_FALSE(with_pk.attributes_.contains("id"));
}

// ==============================================================================
// Validation Tests
// ==============================================================================

TEST_F(ModelTest, IsValidReturnsTrue) {
	TestModel m;
	// Default validate() implementation returns true
	EXPECT_TRUE(m.is_valid());
}

TEST_F(ModelTest, IsInvalidReturnsFalse) {
	TestModel m;
	// Default validate() implementation returns true, so is_invalid() returns false
	EXPECT_FALSE(m.is_invalid());
}

// ==============================================================================
// Serialization Tests
// ==============================================================================

TEST_F(ModelTest, ToJsonString) {
	TestModel m;
	m.set("id", 1);
	m.set("name", "John Doe");
	m.set("age", 30);

	std::string json = m.to_json_string();

	EXPECT_NE(std::string::npos, json.find("\"id\""));
	EXPECT_NE(std::string::npos, json.find("\"name\""));
	EXPECT_NE(std::string::npos, json.find("\"John Doe\""));
}

TEST_F(ModelTest, ToJsonStringWithOnly) {
	TestModel m;
	m.set("id", 1);
	m.set("name", "John");
	m.set("password", "secret");

	string_vector only = { "id", "name" };
	std::string json = m.to_json_string(only);

	EXPECT_NE(std::string::npos, json.find("\"id\""));
	EXPECT_NE(std::string::npos, json.find("\"name\""));
	EXPECT_EQ(std::string::npos, json.find("password"));
}

TEST_F(ModelTest, ToJsonStringWithExclude) {
	TestModel m;
	m.set("id", 1);
	m.set("name", "John");
	m.set("password", "secret");

	string_set exclude = { "password" };
	std::string json = m.to_json_string({}, exclude);

	EXPECT_NE(std::string::npos, json.find("\"id\""));
	EXPECT_NE(std::string::npos, json.find("\"name\""));
	EXPECT_EQ(std::string::npos, json.find("password"));
}

TEST_F(ModelTest, ToXmlString) {
	TestModel m;
	m.set("id", 1);
	m.set("name", "John Doe");

	std::string xml = m.to_xml_string();

	EXPECT_NE(std::string::npos, xml.find("<test_table>"));
	EXPECT_NE(std::string::npos, xml.find("</test_table>"));
	EXPECT_NE(std::string::npos, xml.find("<id>1</id>"));
	EXPECT_NE(std::string::npos, xml.find("<name>John Doe</name>"));
}

TEST_F(ModelTest, ToXmlStringWithCustomName) {
	TestModel m;
	m.set("id", 1);

	std::string xml = m.to_xml_string("user");

	EXPECT_NE(std::string::npos, xml.find("<user>"));
	EXPECT_NE(std::string::npos, xml.find("</user>"));
}

TEST_F(ModelTest, ToXmlStringWithExclude) {
	TestModel m;
	m.set("id", 1);
	m.set("name", "John");
	m.set("password", "secret");

	string_set exclude = { "password" };
	std::string xml = m.to_xml_string("user", {}, exclude);

	EXPECT_NE(std::string::npos, xml.find("<id>1</id>"));
	EXPECT_EQ(std::string::npos, xml.find("password"));
}

// ==============================================================================
// init_record_tracking() Tests
// ==============================================================================

TEST_F(ModelTest, InitRecordTrackingCreatesPlaceholders) {
	TestModel m;
	m.init_record_tracking(true);

	EXPECT_TRUE(m.isset("created_at"));
	EXPECT_TRUE(m.isset("updated_at"));
	EXPECT_TRUE(m.isset("version"));

	// All should be null placeholders
	EXPECT_TRUE(m.get("created_at").is_null());
	EXPECT_TRUE(m.get("updated_at").is_null());
	EXPECT_TRUE(m.get("version").is_null());
}

TEST_F(ModelTest, InitRecordTrackingDoesNotOverwriteExisting) {
	TestModel m;
	m.set("created_at", datetime{ 2024, 1, 1 });
	m.init_record_tracking(true);

	// Should NOT overwrite the existing value
	EXPECT_FALSE(m.get("created_at").is_null());
	EXPECT_EQ(2024, m.get("created_at").as_date().year());
}

TEST_F(ModelTest, InitRecordTrackingDoesNotFlagDirty) {
	TestModel m;
	m.reset_dirty(); // Clear any dirty state from construction
	m.init_record_tracking(true);

	// Placeholders should NOT be dirty
	EXPECT_FALSE(m.attribute_is_dirty("created_at"));
	EXPECT_FALSE(m.attribute_is_dirty("updated_at"));
	EXPECT_FALSE(m.attribute_is_dirty("version"));
}

TEST_F(ModelTest, NoTimestampsWhenAttributesMissing) {
	TestModel m;
	m.set("name", "test");

	// Without init_record_tracking(), these attributes don't exist
	EXPECT_FALSE(m.isset("created_at"));
	EXPECT_FALSE(m.isset("updated_at"));
	EXPECT_FALSE(m.isset("version"));
}

TEST_F(ModelTest, InitRecordTrackingIdempotent) {
	TestModel m;
	m.set("created_at", datetime{ 2024, 6, 15 });
	m.init_record_tracking(true); // Should not overwrite
	m.init_record_tracking(true); // Call again - should be safe

	EXPECT_EQ(2024, m.get("created_at").as_date().year());
	EXPECT_TRUE(m.get("updated_at").is_null());
	EXPECT_TRUE(m.get("version").is_null());
}

TEST_F(ModelTest, InitRecordTrackingSkipsPrimaryKeyName) {
	UidModel absent{ "version" };
	absent.init_record_tracking(true);
	EXPECT_TRUE(absent.isset("created_at"));
	EXPECT_TRUE(absent.isset("updated_at"));
	EXPECT_FALSE(absent.attributes_.contains("version"));

	UidModel present{ "updated_at" };
	present.attributes_["updated_at"] = 7;
	present.init_record_tracking(true);
	EXPECT_EQ(7, present.get("updated_at").as_integer());
	EXPECT_TRUE(present.get("version").is_null());
}

TEST_F(ModelTest, InitRecordTrackingOffDoesNotCreatePlaceholders) {
	TestModel m;
	m.init_record_tracking(false);

	EXPECT_FALSE(m.isset("created_at"));
	EXPECT_FALSE(m.isset("updated_at"));
	EXPECT_FALSE(m.isset("version"));
}

// ==============================================================================
// Query preflight Tests: rejected before any SQL is sent
// ==============================================================================

class QueryPreflightTest : public ::testing::Test {
  protected:
	void SetUp() override {
		config::connection_map connections;
		connections["default"] = connection_;
		config::instance().set_connections(connections, "default");
	}

	std::shared_ptr<counting_connection> connection_{ std::make_shared<counting_connection>() };
};

TEST_F(QueryPreflightTest, DeleteAllWithBoundsSendsNoSql) {
	EXPECT_THROW(query<preflight_item>().limit(1).delete_all(), ex::activerecord);
	EXPECT_THROW(query<preflight_item>().offset(0).delete_all(), ex::activerecord);
	EXPECT_THROW(query<preflight_item>().limit(1).offset(1).delete_all(), ex::activerecord);
	EXPECT_EQ(0, connection_->calls);
}

TEST_F(QueryPreflightTest, GroupedCountAndExistsSendNoSql) {
	EXPECT_THROW(query<preflight_item>().group("name").count(), ex::activerecord);
	EXPECT_THROW(query<preflight_item>().group("name").exists(), ex::activerecord);
	EXPECT_EQ(0, connection_->calls);
}

// ==============================================================================
// Metadata columns used as primary key
// ==============================================================================

class MetadataPrimaryKeyTest : public ::testing::Test {
  protected:
	void SetUp() override {
		config::connection_map connections;
		connections["default"] = connection_;
		config::instance().set_connections(connections, "default");
	}

	std::shared_ptr<recording_connection> connection_{ std::make_shared<recording_connection>() };
};

TEST_F(MetadataPrimaryKeyTest, VersionPrimaryKeyUpdateAndDeleteHaveNoLockingClause) {
	UidModel m{ "version" };
	m.make_persisted("version", 1);

	m.set("name", "Bob");
	EXPECT_TRUE(m.save());
	EXPECT_EQ(1, m.get("version").as_integer());
	EXPECT_TRUE(connection_->last_sql.ends_with(" WHERE version = ?")) << connection_->last_sql;

	EXPECT_TRUE(m.destroy());
	EXPECT_EQ("DELETE FROM accounts WHERE version = ?", connection_->last_sql);
}

TEST_F(MetadataPrimaryKeyTest, OrdinaryVersionStillLocks) {
	UidModel m;
	m.make_persisted("uid", 1);
	m.attributes_["version"] = 3;

	m.set("name", "Bob");
	EXPECT_TRUE(m.save());
	EXPECT_EQ(4, m.get("version").as_integer());
	EXPECT_TRUE(connection_->last_sql.ends_with(" WHERE uid = ? AND version = ?")) << connection_->last_sql;

	EXPECT_TRUE(m.destroy());
	EXPECT_EQ("DELETE FROM accounts WHERE uid = ? AND version = ?", connection_->last_sql);
}

TEST_F(MetadataPrimaryKeyTest, InsertKeepsSuppliedMetadataPrimaryKey) {
	for (const char* pk : { "created_at", "updated_at", "version" }) {
		UidModel m{ pk };
		m.init_record_tracking(true);
		m.set(pk, 7);
		m.set("name", "Bob");
		EXPECT_TRUE(m.save()) << pk;
		EXPECT_EQ(7, m.get(pk).as_integer()) << pk; // the mock reports no insert id, so nothing replaces it
	}

	UidModel m{ "version" };
	m.init_record_tracking(true);
	m.set("version", 7);
	m.set("name", "Bob");
	EXPECT_TRUE(m.save());
	EXPECT_FALSE(m.get("created_at").is_null());
	EXPECT_FALSE(m.get("updated_at").is_null());
}
