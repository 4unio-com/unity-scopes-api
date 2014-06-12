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
 * Authored by: James Henstridge <james.henstridge@canonical.com>
 *              Thomas Voß <thomas.voss@canonical.com>
 */

#include <unity/scopes/CannedQuery.h>
#include <unity/scopes/CategorisedResult.h>
#include <unity/scopes/Category.h>
#include <unity/scopes/Department.h>
#include <unity/scopes/PreviewReply.h>
#include <unity/scopes/Runtime.h>
#include <unity/scopes/ScopeBase.h>
#include <unity/scopes/SearchReply.h>

#include "scope.h"

#include <thread>

namespace testing
{

using namespace unity::scopes;

class ActivationShowingDash : public unity::scopes::ActivationQueryBase
{
public:
    ActivationShowingDash()
    {
    }

    unity::scopes::ActivationResponse activate() override
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{500});
        return unity::scopes::ActivationResponse{unity::scopes::ActivationResponse::ShowDash};
    }
};

class LongRunningActivation : public unity::scopes::ActivationQueryBase
{
public:
    LongRunningActivation()
    {
    }

    unity::scopes::ActivationResponse activate() override
    {
        return unity::scopes::ActivationResponse{unity::scopes::ActivationResponse::ShowDash};
    }
};

class Query : public unity::scopes::SearchQueryBase
{
public:
    Query(unity::scopes::CannedQuery const& query, unity::scopes::SearchMetadata const& metadata)
        : unity::scopes::SearchQueryBase(query, metadata)
    {
    }

    void cancelled() override
    {
    }

    void run(unity::scopes::SearchReplyProxy const& reply) override
    {
        Department::SPtr parent = Department::create("all", query(), "All Departments");
        Department::SPtr news_dep = Department::create("news", query(), "News");
        news_dep->set_subdepartments({Department::create("subdep1", query(), "Europe"), Department::create("subdep2", query(), "US")});
        parent->set_subdepartments({news_dep});
        reply->register_departments(parent);

        auto cat = reply->register_category("cat1", "Category 1", "");
        unity::scopes::CategorisedResult res(cat);
        res.set_uri("uri");
        res.set_title("title");
        res.set_art("art");
        res.set_dnd_uri("dnd_uri");
        reply->push(res);

        unity::scopes::CannedQuery q("scope-A", "foo", "dep1");
        unity::scopes::Annotation annotation(unity::scopes::Annotation::Type::Link);
        annotation.add_link("Link1", q);
        reply->push(annotation);
    }
};

class Preview : public unity::scopes::PreviewQueryBase
{
public:
    void cancelled() override
    {
    }

    void run(unity::scopes::PreviewReplyProxy const& reply) override
    {
        unity::scopes::PreviewWidgetList widgets;
        widgets.emplace_back(unity::scopes::PreviewWidget(R"({"id": "header", "type": "header", "title": "title", "subtitle": "author", "rating": "rating"})"));
        widgets.emplace_back(unity::scopes::PreviewWidget(R"({"id": "id", "type": "image", "art": "screenshot-url"})"));
        reply->push(widgets);
        reply->push("author", unity::scopes::Variant("Foo"));
        reply->push("rating", unity::scopes::Variant("Bar"));
    }
};

} // namespace testing

int testing::Scope::start(std::string const&, unity::scopes::RegistryProxy const &)
{
    return VERSION;
}

void testing::Scope::stop()
{
}

void testing::Scope::run()
{
}

unity::scopes::SearchQueryBase::UPtr testing::Scope::search(
        unity::scopes::CannedQuery const& query,
        unity::scopes::SearchMetadata const &metadata)
{
    return unity::scopes::SearchQueryBase::UPtr(new testing::Query(query, metadata));
}

unity::scopes::ActivationQueryBase::UPtr testing::Scope::activate(
        unity::scopes::Result const&,
        unity::scopes::ActionMetadata const&)
{
    return unity::scopes::ActivationQueryBase::UPtr{new testing::ActivationShowingDash()};
}

unity::scopes::ActivationQueryBase::UPtr testing::Scope::perform_action(
        unity::scopes::Result const&,
        unity::scopes::ActionMetadata const&,
        std::string const&,
        std::string const&)
{
    return unity::scopes::ActivationQueryBase::UPtr{new testing::LongRunningActivation()};
}

unity::scopes::PreviewQueryBase::UPtr testing::Scope::preview(
        unity::scopes::Result const&,
        unity::scopes::ActionMetadata const &)
{
    return unity::scopes::PreviewQueryBase::UPtr(new testing::Preview());
}
