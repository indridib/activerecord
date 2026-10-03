// ============================================================================
// connection_factory.cpp
//
// DSN/URL driven connection construction
// ============================================================================

#include <activerecord/connection_factory.h>

#ifdef AR_SQLITE
#include <activerecord/adapters/sqlite_adapter.h>
#endif
#ifdef AR_MYSQL
#include <activerecord/adapters/mysql_adapter.h>
#endif
#ifdef AR_POSTGRES
#include <activerecord/adapters/postgres_adapter.h>
#endif
#ifdef AR_MSSQL
#include <activerecord/adapters/mssql_adapter.h>
#endif

#include <string>

namespace ar {

namespace {
std::string required_field(const std::optional<std::string>& field, std::string_view name, std::string_view protocol) {
	if (!field.has_value() || field->empty()) {
		throw ex::config("Invalid {} connection URL: missing {}", protocol, name);
	}
	return *field;
}
} // namespace

std::shared_ptr<connection> make_connection(std::string_view url) {
	const connection::connection_url_info info = connection::parse_connection_url(url);
	const std::string& protocol = info.protocol;

	if (protocol == "sqlite") {
#ifdef AR_SQLITE
		const std::string file{ required_field(info.file, "file", protocol) };
		return std::make_shared<sqlite_adapter>(file);
#else
		throw ex::config("Protocol '{}' is not enabled in this build", protocol);
#endif
	}

	if (protocol == "mysql") {
#ifdef AR_MYSQL
		const std::string host{ required_field(info.host, "host", protocol) };
		const std::string user{ info.user.value_or("") };
		const std::string password{ info.password.value_or("") };
		const std::string database{ required_field(info.database, "database", protocol) };
		const unsigned int port = static_cast<unsigned int>(info.port.value_or(3306));
		return std::make_shared<mysql_adapter>(host, user, password, database, port);
#else
		throw ex::config("Protocol '{}' is not enabled in this build", protocol);
#endif
	}

	if (protocol == "postgres" || protocol == "postgresql") {
#ifdef AR_POSTGRES
		const std::string host{ required_field(info.host, "host", protocol) };
		const std::string user{ info.user.value_or("") };
		const std::string password{ info.password.value_or("") };
		const std::string database{ required_field(info.database, "database", protocol) };
		const unsigned int port = static_cast<unsigned int>(info.port.value_or(5432));
		return std::make_shared<postgres_adapter>(host, user, password, database, port);
#else
		throw ex::config("Protocol '{}' is not enabled in this build", protocol);
#endif
	}

	if (protocol == "mssql" || protocol == "sqlserver") {
#ifdef AR_MSSQL
		if (auto dsn_iter = info.query.find("dsn"); dsn_iter != info.query.end()) {
			return std::make_shared<mssql_adapter>(dsn_iter->second, info.user.value_or(""),
												   info.password.value_or(""));
		}
		const std::string server{ required_field(info.host, "host", protocol) };
		const std::string database{ required_field(info.database, "database", protocol) };
		const std::string user{ info.user.value_or("") };
		const std::string password{ info.password.value_or("") };
		const unsigned int port = static_cast<unsigned int>(info.port.value_or(1433));
		return std::make_shared<mssql_adapter>(server, database, user, password, port);
#else
		throw ex::config("Protocol '{}' is not enabled in this build", protocol);
#endif
	}

	throw ex::config("Unsupported connection protocol '{}'", protocol);
}

} // namespace ar
