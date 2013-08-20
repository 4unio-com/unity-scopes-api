/*
 * Copyright (C) 2013 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Michi Henning <michi.henning@canonical.com>
 */

#ifndef UNITY_INTERNAL_QUERYBASEIMPL_H
#define UNITY_INTERNAL_QUERYBASEIMPL_H

#include <unity/api/scopes/internal/QueryCtrlImpl.h>

#include <unity/api/scopes/ScopeProxyFwd.h>

#include <vector>

namespace unity
{

namespace api
{

namespace scopes
{

class ReplyBase;

namespace internal
{

class QueryBaseImpl final
{
public:
    QueryBaseImpl();
    ~QueryBaseImpl() noexcept;

    QueryCtrlProxy create_subquery(std::string const& query_string,
                                   ScopeProxy const& scope,
                                   std::shared_ptr<ReplyBase> const& reply);
    void cancel();

private:
    std::vector<QueryCtrlProxy> subqueries_;
};

} // namespace internal

} // namespace scopes

} // namespace api

} // namespace unity

#endif
