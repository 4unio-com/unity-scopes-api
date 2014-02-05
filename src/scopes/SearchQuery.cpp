/*
 * Copyright (C) 2013 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Michi Henning <michi.henning@canonical.com>
 */

#include <unity/scopes/SearchQuery.h>

#include <unity/scopes/internal/QueryBaseImpl.h>

using namespace std;

namespace unity
{

namespace scopes
{

SearchQuery::SearchQuery() : QueryBase()
{
}

SearchQuery::~SearchQuery()
{
}

QueryCtrlProxy SearchQuery::create_subquery(ScopeProxy const& scope,
                                            string const& query_string,
                                            VariantMap const& hints,
                                            shared_ptr<SearchListener> const& reply)
{
    return p->create_subquery(scope, query_string, hints, reply);
}

QueryCtrlProxy SearchQuery::create_subquery(ScopeProxy const& scope,
                                            std::string const& query_string,
                                            FilterState const& filter_state,
                                            VariantMap const& hints,
                                            SearchListener::SPtr const& reply)
{
    return p->create_subquery(scope, query_string, filter_state, hints, reply);
}

QueryCtrlProxy SearchQuery::create_subquery(ScopeProxy const& scope,
                                            std::string const& query_string,
                                            std::string const& department_id,
                                            FilterState const& filter_state,
                                            VariantMap const& hints,
                                            SearchListener::SPtr const& reply)
{
    return p->create_subquery(scope, query_string, department_id, filter_state, hints, reply);
}

} // namespace scopes

} // namespace unity
