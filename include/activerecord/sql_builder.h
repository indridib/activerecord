// ============================================================================
// sql_builder.h
//
// SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//

#pragma once

#include <activerecord.h>
#include <config.h>
#include <attribute.h>

#include <cassert>
#include <format>


namespace ar {


/**
* Helper class for building sql statements progmatically
*/
class sql_builder {

public:

	sql_builder(config::connection_ptr con, std::string_view table)
		: connection_(con)
		, table_(table)
	{}

	// - __toString
	// - to_s

	std::string to_string() const {
		switch(operation_) {
			case sql_select:	return build_select();
			case sql_insert:	return build_insert();
			case sql_update:	return build_update();
			case sql_delete:	return build_delete();
			default:
				assert(false);
		}
		return {};
	}

	// - bind_values

	const attribute_vector& get_bind_values() const noexcept { return bind_values_; }

	void add_order(std::string_view order) noexcept		{ order_ = order; }
	void add_group(std::string_view group) noexcept		{ group_ = group; }
	void add_having(std::string_view having) noexcept		{ having_ = having; }
	void add_select(std::string_view select) noexcept		{ operation_ = sql_select; select_ = select; }
	void add_joins(std::string_view joins) noexcept 		{ joins_ = joins; }
	void set_limit(long limit) noexcept						{ limit_ = limit; }
	void set_offset(long offset) noexcept					{ offset_ = offset; }

	void add_where(std::string_view str_where, const attribute_vector& values) noexcept
	{
		where_ = str_where;
		where_bind_values_ = values;
		bind_values_ = values;
	}

	void update(const string_vector& names, const attribute_vector& values) noexcept
	{
		operation_ = sql_update;
		keys_ = names;
		data_ = values;
	}

	void insert(const string_vector& names, const attribute_vector& values) noexcept
	{
		operation_ = sql_insert;
		keys_ = names;
		data_ = values;
	}

	// - delete
	// - reverse_order

protected:


	std::string build_select() const {
		// Reserve approximate size to reduce allocations
		std::size_t estimated_size{ 15 + select_.size() + table_.size() +
			joins_.size() + where_.size() + group_.size() +
			having_.size() + order_.size() + 50 };  // Extra for keywords

		std::string sql;
		sql.reserve(estimated_size);
		sql = std::format("SELECT {} FROM {}", select_, table_);

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

		if ( (offset_ >= 0) || (limit_ >= 0) ) {
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

		// Reserve space for columns string
		std::size_t columns_size{ 0 };
		for(const auto& key : keys_) {
			columns_size += key.size() + 4;  // key + quotes + ", "
		}

		std::string columns;
		columns.reserve(columns_size);
		for(auto iter = keys_.begin(); iter != keys_.end(); iter++) {
			if (iter != keys_.begin()) {
				columns += ", ";
			}
			columns += connection_->escape(*iter);
		}

		// Build placeholders: "?" for 1 column, "?,?" for 2, etc.
		std::size_t placeholders_size = keys_.size() * 2;  // "?," per key
		std::string placeholders;
		placeholders.reserve(placeholders_size);
		for(size_t i = 0; i < keys_.size(); i++) {
			if (i > 0) placeholders += ",";
			placeholders += "?";
		}

		std::string sql = std::format("INSERT INTO {}({}) VALUES({})",
			table_, columns, placeholders);

		bind_values_ = data_;
		return sql;
	}

	std::string build_update() const {
		// Reserve space for SET clause
		std::size_t set_size{ 0 };
		for(const auto& key : keys_) {
			set_size += key.size() + 8;  // key + " = ?, "
		}

		std::string set_clause;
		set_clause.reserve(set_size);
		for(auto iter = keys_.begin(); iter != keys_.end(); iter++) {
			if (iter != keys_.begin()) {
				set_clause += ", ";
			}
			set_clause += std::format("{} = ?", connection_->escape(*iter));
		}

		std::string sql = std::format("UPDATE {} SET {}", table_, set_clause);

		if (!where_.empty()) {
			sql += std::format(" WHERE {}", where_);
		}

		if (connection_->accepts_limit_and_order_for_update_and_delete()) {
			if ( (offset_ >= 0) || (limit_ >= 0) ) {
				connection_->limit(sql, offset_, limit_);
			}
		}

		attribute_vector where_values{where_bind_values_};
		bind_values_.clear();
		bind_values_.reserve(data_.size() + where_values.size());
		bind_values_.insert(bind_values_.end(), data_.begin(), data_.end());
		bind_values_.insert(bind_values_.end(), where_values.begin(), where_values.end());
		return sql;
	}
	std::string build_delete() const {
		// TODO(issue-6): pending implementation.
		return "";
	}


	config::connection_ptr connection_;		///< a reference to the database connection to use 
	std::string table_;

	enum sql_type { sql_select, sql_insert, sql_update, sql_delete };
	sql_type operation_{sql_select};
	std::string select_{"*"};
	std::string joins_;
	std::string order_;
	std::string group_;
	std::string having_;
	long limit_{-1L};
	long offset_{-1L}; 

	// for where
	std::string where_;
	attribute_vector where_bind_values_;
	mutable attribute_vector bind_values_;  // mutable for lazy initialization in const methods

	// for insert/update
	string_vector keys_;
	mutable attribute_vector data_;  // mutable for lazy initialization in const methods
	std::string sequence_;
};


// FixMe: need to implement iterator begin()/end() to be able to get single 
// result
template <class T>
class query : public sql_builder {
public:
	query()
		: sql_builder(config::instance().get_connection(), T::st_table_name())
		{};

	/*
	 * Returns an array of records found
	 */
	std::vector<T> all() { 
		std::vector<T> result;
		auto stmt{ connection_->query(to_string(), get_bind_values()) };
		while(stmt->fetch()) {
			T item;
			stmt->get_attributes(item.attributes_);
			item.new_record_ = false;
			result.emplace_back(item);
		}
		return result;
	}

	/*
	* Returns the record with the primary key id 'id'
	*/
	T find(long id) {
		add_where(std::format("{} = ?", T::st_primary_key()), {id});
		return first();
	}

	/*
	* Returns multiple records with primary key id
	*/
	std::vector<T> find(const std::vector<long>& ids) {
		// TODO(issue-6): pending implementation.
		throw ex::activerecord("not yet implemented");
	}


	/*
	* Returns a count number of items matching the query
	*/
	long count() {
		query<T> count_query{*this};
		count_query.select("COUNT(*)");
		auto stmt{ connection_->query(count_query.to_string(), count_query.get_bind_values()) };
		if (stmt->fetch()) {
			attribute_map attributes;
			stmt->get_attributes(attributes);
			auto iter = attributes.begin();
			if (iter != attributes.end()) {
				return static_cast<long>(iter->second.as_integer());
			}
		}
		return 0;
	}

	/*
	* Returns true if a record matching the query exists
	*/
	bool exists() {	
		return count() > 0;
	}


	/*
	* Returns the first record found or "null if not found" (fixme: how unique_ptr?)
	*/
	T first() {
		auto stmt{ connection_->query(to_string(), get_bind_values()) };
		if (stmt->fetch()) {
			T item;
			stmt->get_attributes(item.attributes_);
			item.new_record_ = false;
			return item;
		}
		throw ex::item_not_found("record not found");
	}

	/*
	* Returns the last record found or "null if not found" (fixme: how unique_ptr?)
	*/
	T last() {
		// TODO(issue-6): pending implementation.
		// FIXME: if we have order revert it else add order on primary key DESC
		// and return first
		// return first();
		throw ex::activerecord("not yet implemented");
	}




	query& order(std::string_view order)		{ add_order(order); return *this; }
	query& group(std::string_view group)		{ add_group(group); return *this; }
	query& having(std::string_view having)	{ add_having(having); return *this; }
	query& select(std::string_view select)	{ add_select(select); return *this; }
	query& joins(std::string_view joins) 		{ add_joins(joins); return *this; }

	query& where(std::string_view str_where, const attribute& values)				{ add_where(str_where, {values}); return *this;}
	query& where(std::string_view str_where, const attribute_vector& values = {}) { add_where(str_where, values); return *this;}

	query& limit(long limit)					{ set_limit(limit);  return *this; }
	query& offset(long offset)					{ set_offset(offset); return *this; }

};


} // namespace ar

