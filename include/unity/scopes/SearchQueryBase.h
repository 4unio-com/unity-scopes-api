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

#ifndef UNITY_SCOPES_SEARCHQUERYBASE_H
#define UNITY_SCOPES_SEARCHQUERYBASE_H

#include <unity/scopes/QueryBase.h>
#include <unity/scopes/QueryCtrlProxyFwd.h>
#include <unity/scopes/ScopeProxyFwd.h>
#include <unity/scopes/SearchReplyProxyFwd.h>
#include <unity/scopes/Variant.h>
#include <unity/scopes/SearchListenerBase.h>

#include <unity/util/DefinesPtrs.h>
#include <unity/util/NonCopyable.h>

namespace unity
{

namespace scopes
{

class SearchMetadata;

namespace internal
{

class QueryBaseImpl;
class QueryObject;

} // namespace internal

/**
\brief Abstract base class to represent a particular query.

A scope must return an instance of this class from its implementation of ScopeBase::create_query().

\note The constructor of the instance must complete in a timely manner. Do not perform anything in the
constructor that might block.
*/

class SearchQueryBase : public QueryBase
{
public:
    /// @cond
    NONCOPYABLE(SearchQueryBase);
    UNITY_DEFINES_PTRS(SearchQueryBase);
    /// @endcond

    /**
    \brief Called by scopes run time to start the query.

    Your implementation of run() can use the reply proxy to push results
    for the query. You can push results from within run(), in which case
    the query implicitly completes when run() returns. Alternatively,
    run() can store the reply proxy and return immediately. In this
    case, you can use the stored proxy to push results from another
    thread. It is safe to call `push()` from multiple threads without
    synchronization.

    The query completes either when run() returns, or when the
    last stored reply proxy goes out of scope (whichever happens
    last).

    \param reply The proxy on which to push results for the query.
    */
    virtual void run(SearchReplyProxy const& reply) = 0;

    /** @name Subquery methods
    The subsearch() methods are for use by aggregating scopes.
    When an aggregator passes a query to its child scopes, it should
    use subsearch() instead of the normal Scope::search()
    that would be called by a client. subsearch() takes care
    of automatically forwarding query cancellation to child scopes.
    This means that there is no need for an aggregating scope to
    explicitly forward cancellation to child scopes
    when its QueryBase::cancelled() method is called by the scopes
    run time.
    */
    //{@
    QueryCtrlProxy subsearch(ScopeProxy const& scope,
                                   std::string const& query_string,
                                   SearchListenerBase::SPtr const& reply);
    QueryCtrlProxy subsearch(ScopeProxy const& scope,
                                   std::string const& query_string,
                                   FilterState const& filter_state,
                                   SearchListenerBase::SPtr const& reply);
    QueryCtrlProxy subsearch(ScopeProxy const& scope,
                                   std::string const& query_string,
                                   std::string const& department_id,
                                   FilterState const& filter_state,
                                   SearchListenerBase::SPtr const& reply);
    QueryCtrlProxy subsearch(ScopeProxy const& scope,
                                   std::string const& query_string,
                                   std::string const& department_id,
                                   FilterState const& filter_state,
                                   SearchMetadata const& hints,
                                   SearchListenerBase::SPtr const& reply);
    //@}

    /**
     \brief Get canned query of this search request.

     \return canned query
     \throws unity::LogicException if canned query was not initialized (the default ctor was used).
     */
    CannedQuery query() const;

    /**
     \brief Get metadata for this search request.
     \return search metadata
     \throws unity::LogicException if search metadata was not initialized (the default ctor was used).
    */
    SearchMetadata search_metadata() const;

    /// @cond
    virtual ~SearchQueryBase();
    /// @endcond

protected:
    /**
     \brief Default constructor of SearchQueryBase.
     \deprecated This ctor is deprecated and will be removed with version 0.6.0. Please use non-default ctor.
     */
    SearchQueryBase();

    /// @cond
    SearchQueryBase(CannedQuery const& query, SearchMetadata const& metadata);
    /// @endcond
};

} // namespace scopes

} // namespace unity

#endif
