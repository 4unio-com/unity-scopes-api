/*
 * Copyright (C) 2015 Canonical Ltd
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

#include <unity/scopes/CategorisedResult.h>
#include <unity/scopes/internal/RegistryObject.h>
#include <unity/scopes/internal/RuntimeImpl.h>
#include <unity/scopes/internal/ScopeImpl.h>
#include <unity/scopes/OptionSelectorFilter.h>

#include <boost/filesystem.hpp>
#include <gtest/gtest.h>

#include "CacheScope.h"

using namespace std;
using namespace unity::scopes;
using namespace unity::scopes::internal;

class Receiver : public SearchListenerBase
{
public:
    Receiver()
        : query_complete_(false)
    {
    }

    virtual void push(Department::SCPtr const& parent) override
    {
        lock_guard<mutex> lock(mutex_);
        dept_ = parent;
    }

    virtual void push(Filters const& filters, FilterState const& filter_state) override
    {
        lock_guard<mutex> lock(mutex_);
        filters_ = filters;
        filter_state_ = filter_state;
    }

    virtual void push(CategorisedResult result) override
    {
        lock_guard<mutex> lock(mutex_);
        result_ = make_shared<CategorisedResult>(result);
    }

    virtual void finished(CompletionDetails const& details) override
    {
        EXPECT_EQ(CompletionDetails::OK, details.status()) << details.message();
        lock_guard<mutex> lock(mutex_);
        query_complete_ = true;
        cond_.notify_one();
    }

    void wait_until_finished()
    {
        unique_lock<mutex> lock(mutex_);
        cond_.wait(lock, [this] { return this->query_complete_; });
        query_complete_ = false;
    }

    Department::SCPtr dept() const
    {
        lock_guard<mutex> lock(mutex_);
        return dept_;
    }

    Filters filters() const
    {
        lock_guard<mutex> lock(mutex_);
        return filters_;
    }

    FilterState filter_state() const
    {
        lock_guard<mutex> lock(mutex_);
        return filter_state_;
    }

    CategorisedResult::SCPtr result() const
    {
        lock_guard<mutex> lock(mutex_);
        return result_;
    }

private:
    Department::SCPtr dept_;
    Filters filters_;
    FilterState filter_state_;
    CategorisedResult::SCPtr result_;
    bool query_complete_;
    mutable mutex mutex_;
    condition_variable cond_;
};

class CacheScopeTest : public ::testing::Test
{
public:
    CacheScopeTest()
    {
        runtime_ = Runtime::create(TEST_RUNTIME_FILE);
        auto reg = runtime_->registry();
        auto meta = reg->get_metadata("CacheScope");
        scope_ = meta.proxy();
        meta = reg->get_metadata("AlwaysPushFromCacheScope");
        always_push_from_cache_scope_ = meta.proxy();
    }

    ScopeProxy scope() const
    {
        return scope_;
    }

    ScopeProxy always_push_from_cache_scope() const
    {
        return always_push_from_cache_scope_;
    }

private:
    Runtime::UPtr runtime_;
    ScopeProxy scope_;
    ScopeProxy always_push_from_cache_scope_;
};

TEST_F(CacheScopeTest, push_from_cache_without_cache_file)
{
    ::unlink(TEST_RUNTIME_PATH "/unconfined/AlwaysPushFromCacheScope/.surfacing_cache");
    auto receiver = make_shared<Receiver>();
    always_push_from_cache_scope()->search("", SearchMetadata("unused", "unused"), receiver);
    receiver->wait_until_finished();

    auto r = receiver->result();
    EXPECT_EQ(r, nullptr);
}

TEST_F(CacheScopeTest, non_surfacing_query)
{
    ::chmod(TEST_RUNTIME_PATH "/unconfined/CacheScope", 0700);
    ::unlink(TEST_RUNTIME_PATH "/unconfined/CacheScope/.surfacing_cache");
    auto receiver = make_shared<Receiver>();
    scope()->search("some query", SearchMetadata("unused", "unused"), receiver);
    receiver->wait_until_finished();

    auto r = receiver->result();
    EXPECT_EQ(r->title(), "some query");
    auto d = receiver->dept();
    EXPECT_EQ(d->id(), "");
    auto sd = *d->subdepartments().begin();
    EXPECT_EQ(sd->id(), "subsome query");

    // Non-empty query, so there must be no cache file.
    boost::system::error_code ec;
    EXPECT_FALSE(boost::filesystem::exists(TEST_RUNTIME_PATH "/unconfined/CacheScope/.surfacing_cache", ec));
}

// Stop warnings about unused return value from system()

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"

TEST_F(CacheScopeTest, surfacing_query)
{
    ::unlink(TEST_RUNTIME_PATH "/unconfined/CacheScope/.surfacing_cache");
    auto receiver = make_shared<Receiver>();
    scope()->search("", SearchMetadata("unused", "unused"), receiver);
    receiver->wait_until_finished();

    auto r = receiver->result();
    EXPECT_EQ(r->title(), "");
    auto d = receiver->dept();
    EXPECT_EQ(d->id(), "");
    auto sd = *d->subdepartments().begin();
    EXPECT_EQ(sd->id(), "sub");

    // Empty query, so there must be a cache file.
    boost::system::error_code ec;
    EXPECT_TRUE(boost::filesystem::exists(TEST_RUNTIME_PATH "/unconfined/CacheScope/.surfacing_cache", ec));
    system("cat " TEST_RUNTIME_PATH "/unconfined/CacheScope/.surfacing_cache");
}

#pragma GCC diagnostic push

// Run another non-surfacing query before checking that the cache contains the
// results of the last surfacing query.

TEST_F(CacheScopeTest, non_surfacing_query2)
{
    auto receiver = make_shared<Receiver>();
    scope()->search("some other query", SearchMetadata("unused", "unused"), receiver);
    receiver->wait_until_finished();

    auto r = receiver->result();
    EXPECT_EQ(r->title(), "some other query");
    auto d = receiver->dept();
    EXPECT_EQ(d->id(), "");
    auto sd = *d->subdepartments().begin();
    EXPECT_EQ(sd->id(), "subsome other query");

    // Cache file must still be there.
    boost::system::error_code ec;
    EXPECT_TRUE(boost::filesystem::exists(TEST_RUNTIME_PATH "/unconfined/CacheScope/.surfacing_cache", ec));
}

TEST_F(CacheScopeTest, push_from_cache)
{
    auto receiver = make_shared<Receiver>();
    scope()->search("", SearchMetadata("unused", "unused"), receiver);
    receiver->wait_until_finished();

    auto r = receiver->result();
    EXPECT_EQ(r->title(), "");
    auto d = receiver->dept();
    EXPECT_EQ(d->id(), "");
    auto sd = *d->subdepartments().begin();
    EXPECT_EQ(sd->id(), "sub");
    auto filters = receiver->filters();
    ASSERT_EQ(1, filters.size());
    auto f = *filters.begin();
    EXPECT_EQ("option_selector", f->filter_type());
    auto osf = dynamic_pointer_cast<OptionSelectorFilter const>(f);
    ASSERT_TRUE(osf != nullptr);
    EXPECT_EQ("Choose an option", osf->label());
    auto fs = receiver->filter_state();
    EXPECT_TRUE(fs.has_filter("f1"));
    EXPECT_TRUE(osf->has_active_option(fs));

    // Cache file must still be there.
    boost::system::error_code ec;
    EXPECT_TRUE(boost::filesystem::exists(TEST_RUNTIME_PATH "/unconfined/CacheScope/.surfacing_cache", ec));
}

TEST_F(CacheScopeTest, push_non_empty_from_cache)
{
    auto receiver = make_shared<Receiver>();
    scope()->search("non-empty from cache", SearchMetadata("unused", "unused"), receiver);
    receiver->wait_until_finished();

    // Cache file must still be there.
    boost::system::error_code ec;
    EXPECT_TRUE(boost::filesystem::exists(TEST_RUNTIME_PATH "/unconfined/CacheScope/.surfacing_cache", ec));
}

TEST_F(CacheScopeTest, write_failure)
{
    ::unlink(TEST_RUNTIME_PATH "/unconfined/CacheScope/.surfacing_cache");
    ::chmod(TEST_RUNTIME_PATH "/unconfined/CacheScope", 0);
    auto receiver = make_shared<Receiver>();
    scope()->search("", SearchMetadata("unused", "unused"), receiver);
    receiver->wait_until_finished();

    // Empty query, but cache file could not be created
    ::chmod(TEST_RUNTIME_PATH "/unconfined/CacheScope", 0700);
    boost::system::error_code ec;
    EXPECT_FALSE(boost::filesystem::exists(TEST_RUNTIME_PATH "/unconfined/CacheScope/.surfacing_cache", ec));
}

TEST_F(CacheScopeTest, surfacing_query_2)
{
    auto receiver = make_shared<Receiver>();
    scope()->search("", SearchMetadata("unused", "unused"), receiver);
    receiver->wait_until_finished();

    auto r = receiver->result();
    EXPECT_EQ(r->title(), "");
    auto d = receiver->dept();
    EXPECT_EQ(d->id(), "");
    auto sd = *d->subdepartments().begin();
    EXPECT_EQ(sd->id(), "sub");
    auto filters = receiver->filters();
    ASSERT_EQ(1, filters.size());
    auto f = *filters.begin();
    EXPECT_EQ("option_selector", f->filter_type());
    auto osf = dynamic_pointer_cast<OptionSelectorFilter const>(f);
    ASSERT_TRUE(osf != nullptr);
    EXPECT_EQ("Choose an option", osf->label());
    auto fs = receiver->filter_state();
    EXPECT_TRUE(fs.has_filter("f1"));
    EXPECT_TRUE(osf->has_active_option(fs));

    // New cache file must have been created
    boost::system::error_code ec;
    EXPECT_TRUE(boost::filesystem::exists(TEST_RUNTIME_PATH "/unconfined/CacheScope/.surfacing_cache", ec));
}

TEST_F(CacheScopeTest, read_failure)
{
    ::chmod(TEST_RUNTIME_PATH "/unconfined/CacheScope/.surfacing_cache", 0);
    auto receiver = make_shared<Receiver>();
    scope()->search("", SearchMetadata("unused", "unused"), receiver);
    receiver->wait_until_finished();
    EXPECT_EQ(receiver->result(), nullptr);
    ::chmod(TEST_RUNTIME_PATH "/unconfined/CacheScope/.surfacing_cache", 0600);

    // Cache file must still be there, but read will have failed.
    boost::system::error_code ec;
    EXPECT_TRUE(boost::filesystem::exists(TEST_RUNTIME_PATH "/unconfined/CacheScope/.surfacing_cache", ec));
}

// Stop warnings about unused return value from system()

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"

TEST_F(CacheScopeTest, missing_departments)
{
    // Get coverage on missing departments entry
    system("cp " TEST_SOURCE_PATH "/no_dept_cache " TEST_RUNTIME_PATH "/unconfined/CacheScope/.surfacing_cache");
    auto receiver = make_shared<Receiver>();
    scope()->search("", SearchMetadata("unused", "unused"), receiver);
    receiver->wait_until_finished();
    EXPECT_EQ(receiver->result(), nullptr);

    // Cache file must still be there, but decode will have failed.
    boost::system::error_code ec;
    EXPECT_TRUE(boost::filesystem::exists(TEST_RUNTIME_PATH "/unconfined/CacheScope/.surfacing_cache", ec));
}

TEST_F(CacheScopeTest, missing_categories)
{
    // Get coverage on missing categories entry
    system("cp " TEST_SOURCE_PATH "/no_cat_cache " TEST_RUNTIME_PATH "/unconfined/CacheScope/.surfacing_cache");
    auto receiver = make_shared<Receiver>();
    scope()->search("", SearchMetadata("unused", "unused"), receiver);
    receiver->wait_until_finished();
    EXPECT_EQ(receiver->result(), nullptr);

    // Cache file must still be there, but decode will have failed.
    boost::system::error_code ec;
    EXPECT_TRUE(boost::filesystem::exists(TEST_RUNTIME_PATH "/unconfined/CacheScope/.surfacing_cache", ec));
}

TEST_F(CacheScopeTest, missing_filters)
{
    // Get coverage on missing filters entry
    system("cp " TEST_SOURCE_PATH "/no_filters_cache " TEST_RUNTIME_PATH "/unconfined/CacheScope/.surfacing_cache");
    auto receiver = make_shared<Receiver>();
    scope()->search("", SearchMetadata("unused", "unused"), receiver);
    receiver->wait_until_finished();
    EXPECT_EQ(receiver->result(), nullptr);

    // Cache file must still be there, but decode will have failed.
    boost::system::error_code ec;
    EXPECT_TRUE(boost::filesystem::exists(TEST_RUNTIME_PATH "/unconfined/CacheScope/.surfacing_cache", ec));
}

TEST_F(CacheScopeTest, missing_filter_state)
{
    // Get coverage on missing filters_state entry
    system("cp " TEST_SOURCE_PATH "/no_filter_state_cache " TEST_RUNTIME_PATH "/unconfined/CacheScope/.surfacing_cache");
    auto receiver = make_shared<Receiver>();
    scope()->search("", SearchMetadata("unused", "unused"), receiver);
    receiver->wait_until_finished();
    EXPECT_EQ(receiver->result(), nullptr);

    // Cache file must still be there, but decode will have failed.
    boost::system::error_code ec;
    EXPECT_TRUE(boost::filesystem::exists(TEST_RUNTIME_PATH "/unconfined/CacheScope/.surfacing_cache", ec));
}

TEST_F(CacheScopeTest, missing_results)
{
    // Get coverage on missing results entry
    system("cp " TEST_SOURCE_PATH "/no_results_cache " TEST_RUNTIME_PATH "/unconfined/CacheScope/.surfacing_cache");
    auto receiver = make_shared<Receiver>();
    scope()->search("", SearchMetadata("unused", "unused"), receiver);
    receiver->wait_until_finished();
    EXPECT_EQ(receiver->result(), nullptr);

    // Cache file must still be there, but decode will have failed.
    boost::system::error_code ec;
    EXPECT_TRUE(boost::filesystem::exists(TEST_RUNTIME_PATH "/unconfined/CacheScope/.surfacing_cache", ec));
}

#pragma GCC diagnostic pop

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    int rc = 0;

    // Set the "TEST_DESKTOP_FILES_DIR" env var before forking as not to create desktop files in ~/.local
    putenv(const_cast<char*>("TEST_DESKTOP_FILES_DIR=" TEST_RUNTIME_PATH));

    auto rpid = fork();
    if (rpid == 0)
    {
        const char* const args[] = {"scoperegistry [CacheScope_test]", TEST_RUNTIME_FILE, nullptr};
        if (execv(TEST_REGISTRY_PATH "/scoperegistry", const_cast<char* const*>(args)) < 0)
        {
            perror("Error starting scoperegistry:");
        }
        return 1;
    }
    else if (rpid > 0)
    {
        rc = RUN_ALL_TESTS();
        kill(rpid, SIGTERM);
        waitpid(rpid, nullptr, 0);
    }
    else
    {
        perror("Failed to fork:");
    }

    return rc;
}
