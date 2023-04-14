// ============================================================================
// SQLApi_adapter.h
//
// SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//
// A SQLApi adapter class. Uses SQLAPI. See sqlapi.com

#pragma once

#include <activerecord.h>
#include <connection.h>

#include <SQLAPI.h>

namespace ar {

/**
* SQLAPI statement class 
*/
class SQLApi_statement : public statement {
public:
	SQLApi_statement(SAConnection *pConnection);
	void execute(const std::string& command, const attribute_vector& values) override;
	bool fetch() override;
	void get_attributes(attribute_map &attributes) override;

private:
	static attribute field_as_attribute(const SAField &field);

	SACommand cmd;
};


/**
* Adapter for SQL API
*/
class SQLApi_adapter : public connection {

public:

	SQLApi_adapter(const std::string& db, const std::string& user = {}, const std::string& password = {});
	virtual ~SQLApi_adapter();

	std::unique_ptr<statement> query(const std::string& sql, const attribute_vector& values = {}) override;

	void limit(std::string& sql, long offset, long limit) const override;

	virtual bool accepts_limit_and_order_for_update_and_delete(void) const { return true; }

private:
	SAConnection connection_;

};

} // namespace ar
