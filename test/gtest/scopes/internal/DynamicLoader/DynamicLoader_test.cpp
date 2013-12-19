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

#include <scopes/internal/DynamicLoader.h>
#include <unity/UnityExceptions.h>

#include <gtest/gtest.h>
#include <boost/regex.hpp>  // Use Boost implementation until http://gcc.gnu.org/bugzilla/show_bug.cgi?id=53631 is fixed.
#include <scope-api-testconfig.h>

using namespace std;
using namespace unity::scopes::internal;

namespace
{
char const* goodlib = TEST_BUILD_ROOT "/gtest/scopes/internal/DynamicLoader/libtestlib.so";
char const* badlib = TEST_BUILD_ROOT "/gtest/scopes/internal/DynamicLoader/libbadtestlib.so";
}

// Basic test.

TEST(DynamicLoader, basic)
{
    {
        DynamicLoader::UPtr dl = DynamicLoader::create(goodlib);

        DynamicLoader::VoidFunc f = dl->find_function("test_function");
        EXPECT_NE(nullptr, f);
        f();                    // Call it just to be sure it works

        int* p = static_cast<int*>(dl->find_variable("test_variable"));
        EXPECT_NE(nullptr, p);
        *p = 42;                // Assign it just to be sure it works
    }

    {
        // For coverage testing
        DynamicLoader::UPtr dl = DynamicLoader::create(goodlib,
                                                       DynamicLoader::Binding::lazy,
                                                       DynamicLoader::Unload::noclose);
    }
}

// Make sure that lazy and immediate binding work as intended.

TEST(DynamicLoader, flags)
{
    {
        DynamicLoader::UPtr dl = DynamicLoader::create(badlib);
        DynamicLoader::VoidFunc f = dl->find_function("test_function");   // Must work despite unreslved().
        EXPECT_NE(nullptr, f);
    }

    try
    {
        // Must fail because of unresolved symbol.
        DynamicLoader::UPtr dl = DynamicLoader::create(badlib, DynamicLoader::Binding::now);
    }
    catch (unity::ResourceException const& e)
    {
        boost::regex r("unity::ResourceException: .*/libbadtestlib.so: undefined symbol: foo");
        EXPECT_TRUE(boost::regex_match(e.to_string(), r));
    }
}

TEST(DynamicLoader, exceptions)
{
    {
        try
        {
            DynamicLoader::UPtr dl = DynamicLoader::create("no_such_lib");
        }
        catch (unity::ResourceException const& e)
        {
            EXPECT_EQ("unity::ResourceException: no_such_lib: cannot open shared object file: No such file or directory",
                      e.to_string());
        }
    }

    {
        DynamicLoader::UPtr dl = DynamicLoader::create(goodlib);
        try
        {
            dl->find_function("no_such_symbol");
        }
        catch (unity::ResourceException const& e)
        {
            boost::regex r("unity::ResourceException: .*/libtestlib.so: undefined symbol: no_such_symbol");
            EXPECT_TRUE(boost::regex_match(e.to_string(), r));
        }
    }
}
