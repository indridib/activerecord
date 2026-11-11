// ============================================================================
// relations.h
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// Relationships between typed models: belongs_to, has_many, has_one, build and preload
//

#pragma once

#include <activerecord/activerecord.h>
#include <activerecord/attribute.h>
#include <activerecord/config.h>
#include <activerecord/sql_builder.h>
#include <activerecord/typed_model.h>
#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ar {

namespace detail {

template <auto Fk>
concept member_fk = std::is_member_object_pointer_v<decltype(Fk)>;

template <auto Fk> using owner_t = typename member_pointer_traits<decltype(Fk)>::owner_type;

template <auto Fk, class Column> consteval bool maps_member() {
	if constexpr (std::is_same_v<std::remove_cv_t<decltype(Column::member_ptr)>, decltype(Fk)>) {
		return Column::member_ptr == Fk;
	} else {
		return false;
	}
}

template <auto Fk, class Columns> struct fk_matches;
template <auto Fk, class... Columns> struct fk_matches<Fk, std::tuple<Columns...>> {
	static constexpr std::array<bool, sizeof...(Columns)> mapped{ maps_member<Fk, Columns>()... };

	static consteval std::size_t count() {
		std::size_t n{ 0 };
		for (const bool m : mapped) {
			n += m ? 1 : 0;
		}
		return n;
	}

	static consteval std::size_t index() {
		for (std::size_t i = 0; i < mapped.size(); ++i) {
			if (mapped[i]) {
				return i;
			}
		}
		return mapped.size();
	}
};

template <auto Fk> using owner_columns_t = decltype(owner_t<Fk>::columns());

template <auto Fk> constexpr std::size_t fk_count{ fk_matches<Fk, owner_columns_t<Fk>>::count() };

template <auto Fk>
using fk_column_t = std::tuple_element_t<fk_matches<Fk, owner_columns_t<Fk>>::index(), owner_columns_t<Fk>>;

/** The referenced model of the FK column (references<U>()), or void. */
template <auto Fk> using fk_ref_t = std::remove_cvref_t<typename fk_column_t<Fk>::reference_type>;

/**
 * A member pointer of a typed model, mapped by exactly one column of its columns(),
 * holding std::int64_t or std::optional<std::int64_t>. Checked in this order.
 */
template <auto Fk>
concept relation_fk = member_fk<Fk> && typed_model_type<owner_t<Fk>> && (fk_count<Fk> == 1) &&
					  std::same_as<typename fk_column_t<Fk>::value_type, std::int64_t>;

template <auto Fk, class Explicit>
using parent_t =
	std::conditional_t<std::is_void_v<std::remove_cvref_t<Explicit>>, fk_ref_t<Fk>, std::remove_cvref_t<Explicit>>;

/** belongs_to's parent: the explicit type or references<U>(), equal if both, and a typed model. */
template <auto Fk, class Explicit>
concept belongs_to_parent = (std::is_void_v<std::remove_cvref_t<Explicit>> || std::is_void_v<fk_ref_t<Fk>> ||
							 std::same_as<std::remove_cvref_t<Explicit>, fk_ref_t<Fk>>) &&
							typed_model_type<parent_t<Fk, Explicit>>;

/** The parent of has_many/has_one/build: a typed model, equal to references<U>() if declared. */
template <auto Fk, class Parent>
concept inverse_parent =
	typed_model_type<Parent> && (std::is_void_v<fk_ref_t<Fk>> || std::same_as<Parent, fk_ref_t<Fk>>);

template <auto Fk> constexpr const char* fk_name() {
	return std::get<fk_matches<Fk, owner_columns_t<Fk>>::index()>(owner_t<Fk>::columns()).name;
}

inline std::string quoted(std::string_view name) {
	return config::instance().get_connection()->escape(name);
}

/** The FK of a child: its member, but a persisted child must have loaded the column. */
template <auto Fk> std::optional<std::int64_t> loaded_fk(const owner_t<Fk>& child) {
	if (!child.is_new_record() && !child.attributes().contains(std::string{ fk_name<Fk>() })) {
		throw ex::activerecord("{}.{} was not loaded", owner_t<Fk>::st_table_name(), fk_name<Fk>());
	}
	return child.*Fk;
}

/** The stored primary key of a persisted parent; never its public member. */
inline std::int64_t loaded_id(const model& parent) {
	const std::string pk{ parent.get_primary_key() };
	const auto iter{ parent.attributes().find(pk) };
	if (iter == parent.attributes().end()) {
		throw ex::activerecord("{}.{} was not loaded", parent.table_name(), pk);
	}
	if (iter->second.is_null()) {
		throw ex::activerecord("{}.{} is NULL", parent.table_name(), pk);
	}
	if (iter->second.type() != type_integer) {
		throw ex::activerecord("{}.{} is not an integer", parent.table_name(), pk);
	}
	return iter->second.as_integer();
}

/** Ids per preload query: under SQL Server's 2100 parameters and the default limits of the others. */
constexpr std::size_t preload_chunk{ 1000 };

} // namespace detail

/**
 * The parent a child's FK points to (Rails belongs_to): std::optional<Parent>, nullopt for a NULL FK
 * (no query) or a missing row. Parent is references<U>() of the FK column, or given explicitly.
 * Throws ex::activerecord if a persisted child did not load the FK column.
 */
template <auto Fk, class Parent = void, class Child>
	requires detail::relation_fk<Fk> && std::same_as<std::remove_cvref_t<Child>, detail::owner_t<Fk>> &&
			 detail::belongs_to_parent<Fk, Parent>
auto belongs_to(const Child& child) {
	using P = detail::parent_t<Fk, Parent>;
	const std::optional<std::int64_t> fk{ detail::loaded_fk<Fk>(child) };
	if (!fk.has_value()) {
		return std::optional<P>{};
	}
	return query<P>()
		.where(std::format("{} = ?", detail::quoted(P::st_primary_key())), attribute_vector{ attribute{ *fk } })
		.first();
}

/**
 * The children whose FK points to parent (Rails has_many): an unexecuted query<Child> to chain on.
 * For an unsaved parent the query matches nothing. Throws ex::activerecord if a persisted parent's
 * primary key is not loaded, NULL or not an integer.
 */
template <auto Fk, class Parent>
	requires detail::relation_fk<Fk> && detail::inverse_parent<Fk, Parent>
auto has_many(const Parent& parent) {
	using Child = detail::owner_t<Fk>;
	if (parent.is_new_record()) {
		query<Child> none;
		none.where("1 = 0");
		return none;
	}
	const std::int64_t id{ detail::loaded_id(parent) };
	query<Child> children;
	children.where(std::format("{} = ?", detail::quoted(detail::fk_name<Fk>())), attribute_vector{ attribute{ id } });
	return children;
}

/**
 * The child whose FK points to parent (Rails has_one): the one with the lowest primary key if there
 * are several, nullopt if none or if parent is unsaved (no query). Throws like has_many.
 */
template <auto Fk, class Parent>
	requires detail::relation_fk<Fk> && detail::inverse_parent<Fk, Parent>
auto has_one(const Parent& parent) {
	using Child = detail::owner_t<Fk>;
	if (parent.is_new_record()) {
		return std::optional<Child>{};
	}
	const std::int64_t id{ detail::loaded_id(parent) };
	query<Child> children;
	children.where(std::format("{} = ?", detail::quoted(detail::fk_name<Fk>())), attribute_vector{ attribute{ id } })
		.order(detail::quoted(Child::st_primary_key()));
	return children.first();
}

/**
 * A new, unsaved child with only its FK set to parent's id (Rails build); no query.
 * Throws ex::activerecord for an unsaved parent, or one whose primary key is not loaded.
 */
template <auto Fk, class Parent>
	requires detail::relation_fk<Fk> && detail::inverse_parent<Fk, Parent>
auto build(const Parent& parent) {
	if (parent.is_new_record()) {
		throw ex::activerecord("build on an unsaved {}", Parent::st_table_name());
	}
	const std::int64_t id{ detail::loaded_id(parent) };
	detail::owner_t<Fk> child{};
	child.*Fk = id;
	return child;
}

/**
 * The parents of many children in one query per 1000 distinct FKs (Rails preload): a map from the
 * parent's primary key to a copy of the parent. NULL FKs are skipped and missing parents are absent.
 * Children is read once; empty or all-NULL input sends no SQL. Throws like belongs_to, before any SQL.
 */
template <auto Fk, class Parent = void, std::ranges::input_range Children>
	requires detail::relation_fk<Fk> &&
			 std::same_as<std::remove_cv_t<std::ranges::range_value_t<Children>>, detail::owner_t<Fk>> &&
			 std::convertible_to<std::ranges::range_reference_t<Children>, const detail::owner_t<Fk>&> &&
			 detail::belongs_to_parent<Fk, Parent>
auto preload(Children&& children) {
	using P = detail::parent_t<Fk, Parent>;
	std::vector<std::int64_t> ids{};
	for (auto&& child : children) {
		if (const std::optional<std::int64_t> fk{ detail::loaded_fk<Fk>(child) }) {
			ids.push_back(*fk);
		}
	}
	std::ranges::sort(ids);
	const auto duplicates{ std::ranges::unique(ids) };
	ids.erase(duplicates.begin(), duplicates.end());

	std::unordered_map<std::int64_t, P> parents{};
	if (ids.empty()) {
		return parents;
	}
	const std::string pk{ detail::quoted(P::st_primary_key()) };
	for (std::size_t first = 0; first < ids.size(); first += detail::preload_chunk) {
		const std::size_t last{ std::min(ids.size(), first + detail::preload_chunk) };
		attribute_vector chunk{};
		chunk.reserve(last - first);
		for (std::size_t i = first; i < last; ++i) {
			chunk.emplace_back(ids[i]);
		}
		for (P& parent : query<P>().where_in(pk, chunk).all()) {
			const std::int64_t id{ detail::loaded_id(parent) };
			parents.insert_or_assign(id, std::move(parent));
		}
	}
	return parents;
}

} // namespace ar
