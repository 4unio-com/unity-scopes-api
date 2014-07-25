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

#ifndef UNITY_SCOPES_REPLY_H
#define UNITY_SCOPES_REPLY_H

#include <unity/scopes/Object.h>
#include <unity/scopes/OperationInfo.h>
#include <unity/scopes/ReplyProxyFwd.h>

#include <exception>

namespace unity
{

namespace scopes
{

/**
\brief Allows query termination to be sent to the source of a query.
*/

class Reply : public virtual Object
{
public:
    /**
    \brief Informs the source of a query that the query results are complete.

    Calling finished() informs the source of a query that the final result for the query
    was sent, that is, that the query is complete.
    Multiple calls to finished() and calls to error() after finished() was called are ignored.
    The destructor implicitly calls finished() if a Reply goes out of scope without
    a prior call to finished(). Similarly, QueryBase::run() implicitly calls finished() when
    it returns, provided there are no more reply proxies in scope. In other words, calling
    finished() is optional. The scopes run time ensures that the call happens automatically,
    either when the last reply proxy goes out of scope, or when QueryBase::run() returns
    (whichever happens last).
    */
    virtual void finished() = 0;

    /**
    \brief Informs the source of a query that the query was terminated due to an error.

    Multiple calls to error() and calls to finished() after error() was called are ignored.
    \param ex An exception_ptr indicating the cause of the error. If ex is a `std::exception`,
              the return value of `what()` is made available to the query source. Otherwise,
              the query source receives `"unknown exception"`.
    */
    virtual void error(std::exception_ptr ex) = 0;

    /**
    \brief Informs the source of a query that additional info regarding the reply is available.

    Calling info() does not terminate the query, it simply informs the source that something
    interesting occured during execution of the query (usually effecting the results returned in
    some way).

    Multiple calls to info() for each condition are legal.
    \param info_code Indicates the cause for the call to info().
    \param info_message Contains further details about the info code (optional).
    */
    virtual void info(OperationInfo const& op_info) = 0;

    /**
    \brief Destroys a Reply.

    If a Reply goes out of scope without a prior call to finished(), the destructor implicitly calls finished(),
    provided QueryBase::run() has returned.
    */
    virtual ~Reply();

protected:
    /// @cond
    Reply();
    /// @endcond
};

} // namespace scopes

} // namespace unity

#endif
