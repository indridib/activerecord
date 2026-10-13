// ============================================================================
// connection_factory.h
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// DSN/URL driven connection construction
//

#pragma once

#include <activerecord/connection.h>
#include <memory>
#include <string_view>

namespace ar {

std::shared_ptr<connection> make_connection(std::string_view url);

} // namespace ar
