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

#include <unity/scopes/testing/Category.h>
#include <unity/scopes/testing/Result.h>
#include <unity/scopes/testing/ScopeMetadataBuilder.h>
#include <unity/scopes/testing/TypedScopeFixture.h>

#include <unity/scopes/testing/MockRegistry.h>
#include <unity/scopes/testing/MockPreviewReply.h>
#include <unity/scopes/testing/MockSearchReply.h>

#include <unity/scopes/CategoryRenderer.h>

#include <gtest/gtest.h>

#include "scope.h"

namespace
{
typedef unity::scopes::testing::TypedScopeFixture<testing::Scope> TestScopeFixture;

static const std::string scope_name{"does.not.exist.scope"};
static const std::string scope_query_string{"does.not.exist.scope.query_string"};

static const std::string default_locale{"C"};
static const std::string default_form_factor{"SuperDuperPhablet"};
}

TEST(Category, construction_passes_on_arguments)
{
    const std::string id{"does.not.exist.id"};
    const std::string title{"does.not.exist.title"};
    const std::string icon{"does.not.exist.icon"};
    const unity::scopes::CategoryRenderer renderer{};

    unity::scopes::testing::Category category
    {
        id,
        title,
        icon,
        renderer
    };

    EXPECT_EQ(id, category.id());
    EXPECT_EQ(title, category.title());
    EXPECT_EQ(icon, category.icon());
    EXPECT_EQ(renderer, category.renderer_template());
}

TEST(ScopeMetadataBuilder, construction_in_case_of_missing_mandatory_arguments_aborts)
{
    unity::scopes::testing::ScopeMetadataBuilder builder;
    builder.scope_name(scope_name)
           .display_name("does.not.exist.display_name")
           .description("does.not.exist.description");

    EXPECT_EXIT(builder(), ::testing::KilledBySignal(SIGABRT), ".*");
}

TEST_F(TestScopeFixture,
       creating_a_search_query_and_checking_for_pushed_results_works)
{
    using namespace ::testing;

    const std::string id{"does.not.exist.id"};
    const std::string title{"does.not.exist.title"};
    const std::string icon{"does.not.exist.icon"};
    const unity::scopes::CategoryRenderer renderer{};

    NiceMock<unity::scopes::testing::MockSearchReply> reply;
    EXPECT_CALL(reply, register_departments(_, _)).Times(1);
    EXPECT_CALL(reply, register_category(_, _, _, _))
            .Times(1)
            .WillOnce(
                Return(
                    unity::scopes::Category::SCPtr
                    {
                        new unity::scopes::testing::Category
                        {
                            id, title, icon, renderer
                        }
                    }));
    EXPECT_CALL(reply, register_annotation(_))
            .Times(1)
            .WillOnce(Return(true));
    EXPECT_CALL(reply, push(_))
            .Times(1)
            .WillOnce(Return(true));

    unity::scopes::SearchReplyProxy search_reply_proxy
    {
        &reply, [](unity::scopes::SearchReplyBase*) {}
    };

    unity::scopes::Query query{scope_name};
    query.set_query_string(scope_query_string);

    unity::scopes::SearchMetadata meta_data{default_locale, default_form_factor};

    auto search_query = scope->create_query(query, meta_data);
    ASSERT_NE(nullptr, search_query);
    search_query->run(search_reply_proxy);
}

TEST_F(TestScopeFixture, activating_a_result_works)
{
    using namespace ::testing;

    unity::scopes::ActionMetadata meta_data{default_locale, default_form_factor};
    unity::scopes::testing::Result result;
    auto activation = scope->activate(result, meta_data);

    EXPECT_NE(nullptr, activation);

    auto preview->run(preview_reply_proxy);
}
