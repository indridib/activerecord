// ============================================================================
// connection_factory.h
//
// DSN/URL driven connection construction
// ============================================================================

#pragma once

#include <connection.h>
#include <memory>
#include <string_view>

namespace ar {

std::shared_ptr<connection> make_connection(std::string_view url);

} // namespace ar
