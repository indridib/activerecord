// ============================================================================
// sql_builder.h
//
// SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//

#pragma once

#include <activerecord.h>
#include <attribute.h>
#include <cassert>
#include <cctype>
#include <config.h>
#include <format>
#include <iterator>
#include <memory>
#include <optional>

namespace ar {

/**
 * Helper class for building sql statements progmatically
 */
class sql_builder {

  public:
	sql_builder(config::connection_ptr con, std::string_view table) : connection_(con), table_(table) {}

	// - __toString
	// - to_s

	std::string to_string() const {
		switch (operation_) {
		case sql_select:
			return build_select();
		case sql_insert:
			return build_insert();
		case sql_update:
			return build_update();
		case sql_delete:
			return build_delete();
		default:
			assert(false);
		}
		return {};
	}

	// - bind_values

	const attribute_vector& get_bind_values() const noexcept { return bind_values_; }

	void add_order(std::string_view order) noexcept { order_ = order; }
	void add_group(std::string_view group) noexcept { group_ = group; }
	void add_having(std::string_view having) noexcept { having_ = having; }
	void add_select(std::string_view select) noexcept {
		operation_ = sql_select;
		select_ = select;
	}
	void add_joins(std::string_view joins) noexcept { joins_ = joins; }
	void set_limit(long limit) noexcept { limit_ = limit; }
	void set_offset(long offset) noexcept { offset_ = offset; }

	void add_where(std::string_view str_where, const attribute_vector& values) noexcept {
		where_ = str_where;
		where_bind_values_ = values;
		bind_values_ = values;
	}

	void update(const string_vector& names, const attribute_vector& values) noexcept {
		operation_ = sql_update;
		keys_ = names;
		data_ = values;
	}

	void insert(const string_vector& names, const attribute_vector& values) noexcept {
		operation_ = sql_insert;
		keys_ = names;
		data_ = values;
	}

	void remove() noexcept { operation_ = sql_delete; }

  protected:
	std::string build_select() const {
		std::string sql{ std::format("SELECT {} FROM {}", select_, table_) };

		if (!joins_.empty()) {
			sql += std::format(" {}", joins_);
		}

		if (!where_.empty()) {
			sql += std::format(" WHERE {}", where_);
		}

		if (!group_.empty()) {
			sql += std::format(" GROUP BY {}", group_);
		}

		if (!having_.empty()) {
			sql += std::format(" HAVING {}", having_);
		}

		if (!order_.empty()) {
			sql += std::format(" ORDER BY {}", order_);
		}

		if ((offset_ >= 0) || (limit_ >= 0)) {
			connection_->limit(sql, offset_, limit_);
		}

		bind_values_ = where_bind_values_;
		return sql;
	}

	std::string build_insert() const {
		if (keys_.empty()) {
			bind_values_.clear();
			return std::format("INSERT INTO {} {}", table_, connection_->default_values_insert_clause());
		}

		std::string columns;
		for (auto iter = keys_.begin(); iter != keys_.end(); iter++) {
			if (iter != keys_.begin()) {
				columns += ", ";
			}
			columns += connection_->escape(*iter);
		}

		std::string placeholders;
		for (size_t i = 0; i < keys_.size(); i++) {
			if (i > 0)
				placeholders += ",";
			placeholders += "?";
		}

		std::string sql{ std::format("INSERT INTO {}({}) VALUES({})", table_, columns, placeholders) };

		bind_values_ = data_;
		return sql;
	}

	std::string build_update() const {
		std::string set_clause;
		for (auto iter = keys_.begin(); iter != keys_.end(); iter++) {
			if (iter != keys_.begin()) {
				set_clause += ", ";
			}
			set_clause += std::format("{} = ?", connection_->escape(*iter));
		}

		std::string sql{ std::format("UPDATE {} SET {}", table_, set_clause) };

		if (!where_.empty()) {
			sql += std::format(" WHERE {}", where_);
		}

		if (connection_->accepts_limit_and_order_for_update_and_delete()) {
			if ((offset_ >= 0) || (limit_ >= 0)) {
				connection_->limit(sql, offset_, limit_);
			}
		}

		attribute_vector where_values{ where_bind_values_ };
		bind_values_.clear();
		bind_values_.reserve(data_.size() + where_values.size());
		bind_values_.insert(bind_values_.end(), data_.begin(), data_.end());
		bind_values_.insert(bind_values_.end(), where_values.begin(), where_values.end());
		return sql;
	}
	std::string build_delete() const {
		std::string sql{ std::format("DELETE FROM {}", table_) };

		if (!where_.empty()) {
			sql += std::format(" WHERE {}", where_);
		}

		if (!order_.empty()) {
			sql += std::format(" ORDER BY {}", order_);
		}

		if (connection_->accepts_limit_and_order_for_update_and_delete()) {
			if ((offset_ >= 0) || (limit_ >= 0)) {
				connection_->limit(sql, offset_, limit_);
			}
		}

		bind_values_ = where_bind_values_;
		return sql;
	}

	config::connection_ptr connection_; ///< a reference to the database connection to use
	std::string table_;

	enum sql_type { sql_select, sql_insert, sql_update, sql_delete };
	sql_type operation_{ sql_select };
	std::string select_{ "*" };
	std::string joins_;
	std::string order_;
	std::string group_;
	std::string having_;
	long limit_{ -1L };
	long offset_{ -1L };

	// for where
	std::string where_;
	attribute_vector where_bind_values_;
	mutable attribute_vector bind_values_; // mutable for lazy initialization in const methods

	// for insert/update
	string_vector keys_;
	mutable attribute_vector data_; // mutable for lazy initialization in const methods
	std::string sequence_;
};


template <class T> class query : public sql_builder {
  public:
	class iterator {
	  public:
		using iterator_category = std::input_iterator_tag;
		using value_type = T;
		using difference_type = std::ptrdiff_t;
		using pointer = const T*;
		using reference = const T&;

		iterator() = default;
		explicit iterator(std::shared_ptr<statement> stmt) : stmt_(std::move(stmt)) { advance(); }

		reference operator*() const noexcept { return current_; }
		pointer operator->() const noexcept { return &current_; }

		iterator& operator++() {
			advance();
			return *this;
		}

		iterator operator++(int) {
			iterator tmp{ *this };
			++(*this);
			return tmp;
		}

		friend bool operator==(const iterator& lhs, const iterator& rhs) noexcept {
			if (lhs.end_ && rhs.end_) {
				return true;
			}
			return lhs.end_ == rhs.end_ && lhs.stmt_ == rhs.stmt_;
		}

		friend bool operator!=(const iterator& lhs, const iterator& rhs) noexcept { return !(lhs == rhs); }

	  private:
		void advance() {
			if (!stmt_ || !stmt_->fetch()) {
				stmt_.reset();
				end_ = true;
				return;
			}

			current_ = T{};
			stmt_->get_attributes(current_.attributes_);
			current_.new_record_ = false;
			if constexpr (requires(T& item) { item.sync_from_attributes(); }) {
				current_.sync_from_attributes();
			}
			end_ = false;
		}

		std::shared_ptr<statement> stmt_{};
		T current_{};
		bool end_{ true };
	};

	query() : sql_builder(config::instance().get_connection(), T::st_table_name()) {};

	iterator begin() {
		return iterator{ std::shared_ptr<statement>{ connection_->query(to_string(), get_bind_values()) } };
	}

	iterator end() noexcept { return iterator{}; }

	/*
	 * Returns an array of records found
	 */
	std::vector<T> all() {
		std::vector<T> result;
		for (const auto& item : *this) {
			result.emplace_back(item);
		}
		return result;
	}

	/*
	 * Returns the record with the primary key id 'id'
	 */
	T find(long id) {
		add_where(std::format("{} = ?", T::st_primary_key()), { id });
		auto result{ first() };
		if (!result.has_value()) {
			throw ex::item_not_found("record not found");
		}
		return *result;
	}

	/*
	 * Returns multiple records with primary key id
	 */
	std::vector<T> find(const std::vector<long>& ids) {
		if (ids.empty()) {
			return {};
		}

		std::string placeholders;
		attribute_vector bind_values;

		for (std::size_t i = 0; i < ids.size(); ++i) {
			if (i > 0) {
				placeholders += ",";
			}
			placeholders += "?";
			bind_values.emplace_back(static_cast<int64_t>(ids[i]));
		}

		add_where(std::format("{} IN ({})", T::st_primary_key(), placeholders), bind_values);
		return all();
	}

	/*
	 * Returns a count number of items matching the query
	 */
	long count() {
		query<T> count_query{ *this };
		count_query.select("COUNT(*)");
		count_query.set_limit(-1);
		count_query.set_offset(-1);
		count_query.add_order({});
		auto stmt{ connection_->query(count_query.to_string(), count_query.get_bind_values()) };
		if (stmt->fetch()) {
			attribute_map attributes;
			stmt->get_attributes(attributes);
			auto iter{ attributes.begin() };
			if (iter != attributes.end()) {
				return static_cast<long>(iter->second.as_integer());
			}
		}
		return 0;
	}

	/*
	 * Returns true if a record matching the query exists
	 */
	bool exists() { return count() > 0; }

	/*
	 * Deletes all records matching the current query scope.
	 */
	long delete_all() {
		query<T> scoped_query{ *this };
		scoped_query.remove();
		auto stmt{ scoped_query.connection_->query(scoped_query.to_string(), scoped_query.get_bind_values()) };
		return static_cast<long>(stmt->affected_rows());
	}

	/*
	 * Returns the first record found or nullopt if not found.
	 */
	std::optional<T> first() {
		query<T> scoped_query{ *this };
		scoped_query.set_limit(1);
		auto stmt{ scoped_query.connection_->query(scoped_query.to_string(), scoped_query.get_bind_values()) };
		if (stmt->fetch()) {
			T item;
			stmt->get_attributes(item.attributes_);
			item.new_record_ = false;
			if constexpr (requires(T& value) { value.sync_from_attributes(); }) {
				item.sync_from_attributes();
			}
			return item;
		}
		return std::nullopt;
	}

	/*
	 * Returns the first record found or throws item_not_found if not found.
	 */
	T first_or_throw() {
		auto result{ first() };
		if (!result.has_value()) {
			throw ex::item_not_found("record not found");
		}
		return *result;
	}

	/*
	 * Returns the last record found or nullopt if not found.
	 */
	std::optional<T> last() {
		query<T> scoped_query{ *this };
		if (scoped_query.order_.empty()) {
			scoped_query.add_order(std::format("{} DESC", T::st_primary_key()));
		} else {
			scoped_query.add_order(reverse_order(scoped_query.order_));
		}
		scoped_query.set_limit(1);
		return scoped_query.first();
	}

	/*
	 * Returns the last record found or throws item_not_found if not found.
	 */
	T last_or_throw() {
		auto result{ last() };
		if (!result.has_value()) {
			throw ex::item_not_found("record not found");
		}
		return *result;
	}

	query& order(std::string_view order) {
		add_order(order);
		return *this;
	}
	query& group(std::string_view group) {
		add_group(group);
		return *this;
	}
	query& having(std::string_view having) {
		add_having(having);
		return *this;
	}
	query& select(std::string_view select) {
		add_select(select);
		return *this;
	}
	query& joins(std::string_view joins) {
		add_joins(joins);
		return *this;
	}

	query& where(std::string_view str_where, const attribute& values) {
		add_where(str_where, { values });
		return *this;
	}
	query& where(std::string_view str_where, const attribute_vector& values = {}) {
		add_where(str_where, values);
		return *this;
	}

	query& limit(long limit) {
		set_limit(limit);
		return *this;
	}
	query& offset(long offset) {
		set_offset(offset);
		return *this;
	}

  private:
	static std::string reverse_order(std::string_view value) {
		std::string result;
		std::size_t pos{ 0 };
		while (pos < value.size()) {
			std::size_t next{ value.find(',', pos) };
			std::string token =
				trim(value.substr(pos, next == std::string_view::npos ? value.size() - pos : next - pos));
			if (!token.empty()) {
				if (!result.empty()) {
					result += ", ";
				}
				if (ends_with_ci(token, " asc")) {
					token.replace(token.size() - 4, 4, " DESC");
				} else if (ends_with_ci(token, " desc")) {
					token.replace(token.size() - 5, 5, " ASC");
				} else {
					token += " DESC";
				}
				result += token;
			}
			if (next == std::string_view::npos) {
				break;
			}
			pos = next + 1;
		}
		return result;
	}

	static std::string trim(std::string_view value) {
		std::size_t start{ 0 };
		while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
			++start;
		}
		std::size_t end{ value.size() };
		while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
			--end;
		}
		return std::string(value.substr(start, end - start));
	}

	static bool ends_with_ci(std::string_view value, std::string_view suffix) {
		if (suffix.size() > value.size()) {
			return false;
		}
		const std::size_t offset{ value.size() - suffix.size() };
		for (std::size_t i = 0; i < suffix.size(); ++i) {
			const auto lhs{ static_cast<unsigned char>(value[offset + i]) };
			const auto rhs{ static_cast<unsigned char>(suffix[i]) };
			if (std::tolower(lhs) != std::tolower(rhs)) {
				return false;
			}
		}
		return true;
	}
};

} // namespace ar
