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
 * Authored by: Pawel Stolowski <pawel.stolowski@canonical.com>
 */

#include <gtest/gtest.h>
#include <scopes/Query.h>
#include <unity/UnityExceptions.h>

using namespace unity::api::scopes;
using namespace unity::api::scopes::internal;

// basic test of Query setters and getters
TEST(Query, basic)
{
    {
        Query q("scope-A");
        EXPECT_EQ("scope-A", q.scope_name());
        EXPECT_EQ("", q.query_string());
        EXPECT_EQ("", q.department_id());
    }
    {
        Query q("scope-A");
        q.set_query_string("foo");
        q.set_department_id("dep1");
        EXPECT_EQ("scope-A", q.scope_name());
        EXPECT_EQ("foo", q.query_string());
        EXPECT_EQ("dep1", q.department_id());
    }
}

TEST(Query, comparison)
{
    {
        Query a("scope-A", "foo", "dep1");
        Query b("scope-A", "foo", "dep2");
        EXPECT_FALSE(a == b);
        EXPECT_TRUE(a != b);
    }
    {
        Query a("scope-A", "foo", "dep1");
        Query b("scope-A", "bar", "dep1");
        EXPECT_FALSE(a == b);
        EXPECT_TRUE(a != b);
    }
    {
        Query a("scope-A", "foo", "dep1");
        Query b("scope-B", "foo", "dep1");
        EXPECT_FALSE(a == b);
        EXPECT_TRUE(a != b);
    }
    {
        Query a("scope-A", "foo", "dep1");
        Query b("scope-A", "foo", "dep1");
        EXPECT_TRUE(a == b);
        EXPECT_FALSE(a != b);
    }
}

// test of serialization into a canned query string
TEST(Query, to_string)
{
    {
        Query q("scope-A");
        q.set_query_string("foo");
        q.set_department_id("dep1");
        EXPECT_EQ("scope://scope-A?q=foo&department=dep1", q.to_string());
    }
    {
        Query q("scope-A");
        EXPECT_EQ("scope://scope-A?q=", q.to_string());
    }
}

TEST(Query, serialize)
{
    Query q("scope-A");
    q.set_query_string("foo");
    q.set_department_id("dep1");

    auto var = q.serialize();
    EXPECT_EQ("scope-A", var["scope"].get_string());
    EXPECT_EQ("dep1", var["department_id"].get_string());
    EXPECT_EQ("foo", var["query_string"].get_string());
}

TEST(Query, exceptions)
{
    EXPECT_THROW(Query(""), unity::InvalidArgumentException);
}
