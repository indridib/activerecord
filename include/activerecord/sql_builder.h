// ============================================================================
// sql_builder.h
//
// SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//

#pragma once

#include <activerecord.h>
#include <model.h>

#include <cassert>


namespace ar {


/**
* Helper class for building sql statements progmatically
*/
class sql_builder {

public:

	sql_builder(config::connection_ptr con, const std::string& table)
		: connection_(con)
		, table_(table)
	{}

	// - __toString
	// - to_s

	std::string to_string(void) {
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

	const attribute_vector& get_bind_values(void) const noexcept { return bind_values_; }

	void add_order(const std::string& order) noexcept		{ order_ = order; }
	void add_group(const std::string& group) noexcept		{ group_ = group; }
	void add_having(const std::string& having) noexcept		{ having_ = having; }
	void add_select(const std::string& select) noexcept		{ operation_ = sql_select; select_ = select; }
	void add_joins(const std::string& joins) noexcept 		{ joins_ = joins; }
	void set_limit(long limit) noexcept						{ limit_ = limit; }
	void set_offset(long offset) noexcept					{ offset_ = offset; }

	void add_where(const std::string& str_where, const attribute_vector& values) noexcept
	{ 
		where_ = str_where;
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


	std::string build_select(void) const {
		std::string sql{ "SELECT " + select_ + " FROM " + table_};

		if (!joins_.empty()) {
			sql += " " + joins_;
		}

		if (!where_.empty()) {
			sql += " WHERE  " + where_;
		}

		if (!group_.empty()) {
			sql += " GROUP BY " + group_;
		}

		if (!having_.empty()) {
			sql += " HAVING " + having_;
		}

		if (!order_.empty()) {
			sql += " ORDER BY " + order_;
		}

		if ( (offset_ >= 0) || (limit_ >= 0) ) {
			connection_->limit(sql, offset_, limit_);
		}

		return sql;
	}

	std::string build_insert(void) {
		std::string sql{ "INSERT INTO " + table_ + "("};
		for(auto iter = keys_.begin(); iter != keys_.end(); iter++) {
			if (iter != keys_.begin()) {
				sql += ", ";
			}
			sql += connection_->escape(*iter);
		} 
		sql += ") VALUES(?";
		for(int i = 1; i < keys_.size(); i++) {
			sql += ",?";
		}
		sql += ")";
		bind_values_ = data_;
		return sql;
	}

	std::string build_update(void) {
		std::string sql{ "UPDATE " + table_ + " SET "};

		for(auto iter = keys_.begin(); iter != keys_.end(); iter++) {
			if (iter != keys_.begin()) {
				sql += ", ";
			}
			sql += connection_->escape(*iter) + " = ?";
		} 
		if (!where_.empty()) {
			sql += " WHERE  " + where_;
		}
		if (connection_->accepts_limit_and_order_for_update_and_delete()) {
			if ( (offset_ >= 0) || (limit_ >= 0) ) {
				connection_->limit(sql, offset_, limit_);
			}
		}
		data_.insert(data_.end(), bind_values_.begin(), bind_values_.end());
		bind_values_ = data_;
		return sql;
	}
	std::string build_delete(void) {
		//fixme:
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
	attribute_vector bind_values_;

	// for insert/update
	string_vector keys_;
	attribute_vector data_;
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
		add_where(std::string(T::st_primary_key()) + " = ?", {id});
		return first();
	}

	/*
	* Returns multiple records with primary key id
	*/
	std::vector<T> find(const std::vector<long>& ids) {
		throw ex:activerecord("not yet implemented");
	}


	/*
	* Returns a count number of items matching the query
	*/
	long count() {
		select("COUNT(*)");
		auto stmt{ connection_->query(to_string(), get_bind_values()) };
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
		// FIXME: if we have order revert it else add order on primary key DESC
		// and return first
		// return first();
		throw ex::activerecord("not yet implemented");
	}




	query& order(const std::string& order)		{ add_order(order); return *this; }
	query& group(const std::string& group)		{ add_group(group); return *this; }
	query& having(const std::string& having)	{ add_having(having); return *this; }
	query& select(const std::string& select)	{ add_select(select); return *this; }
	query& joins(const std::string& joins) 		{ add_joins(joins); return *this; }

	query& where(const std::string& str_where, const attribute& values)				{ add_where(str_where, {values}); return *this;}
	query& where(const std::string& str_where, const attribute_vector& values = {}) { add_where(str_where, values); return *this;}

	query& limit(long limit)					{ set_limit(limit);  return *this; }
	query& offset(long offset)					{ set_offset(offset); return *this; }

};


} // namespace ar

