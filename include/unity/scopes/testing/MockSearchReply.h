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
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */

#ifndef UNITY_SCOPES_TESTING_MOCK_SEARCHREPLY_H
#define UNITY_SCOPES_TESTING_MOCK_SEARCHREPLY_H

#include <unity/scopes/CategorisedResult.h>
#include <unity/scopes/SearchReply.h>
#include <unity/scopes/Department.h>

#include <unity/scopes/testing/MockObject.h>

#include <gmock/gmock.h>

namespace unity
{

namespace scopes
{

namespace testing
{

/// @cond

class MockSearchReply : public unity::scopes::SearchReply, public virtual MockObject
{
public:
    MockSearchReply() = default;

    // From Reply
    MOCK_METHOD0(finished, void());
    MOCK_METHOD1(error, void(std::exception_ptr));

    // From SearchReply
    MOCK_METHOD1(register_departments, void(Department::SCPtr const&));
    MOCK_METHOD4(register_category,
                 Category::SCPtr(std::string const&,
                                 std::string const&,
                                 std::string const&,
                                 CategoryRenderer const&));
    MOCK_METHOD1(register_category, void(Category::SCPtr category));
    MOCK_METHOD1(lookup_category, Category::SCPtr(std::string const&));
    MOCK_METHOD1(push, bool(CategorisedResult const&));
    MOCK_METHOD2(push, bool(Filters const&, FilterState const&));
    MOCK_METHOD1(push, bool(experimental::Annotation const& annotation));
};

/// @endcond

} // namespace testing

} // namespace scopes

} // namespace unity

#endif
