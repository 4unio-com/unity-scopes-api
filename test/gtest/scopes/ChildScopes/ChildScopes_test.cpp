/*
 * Copyright (C) 2014 Canonical Ltd
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
 * Authored by: Marcus Tomlinson <marcus.tomlinson@canonical.com>
 */

#include <unity/scopes/ChildScope.h>

#include <unity/scopes/internal/RegistryObject.h>
#include <unity/scopes/internal/ScopeImpl.h>

#include "TestScope.h"

#include <boost/filesystem/operations.hpp>
#include <fstream>
#include <gtest/gtest.h>

using namespace boost;
using namespace testing;
using namespace unity::scopes;
using namespace unity::scopes::internal;

std::shared_ptr<::core::posix::SignalTrap> trap(::core::posix::trap_signals_for_all_subsequent_threads({::core::posix::Signal::sig_chld}));
std::unique_ptr<::core::posix::ChildProcess::DeathObserver> death_observer(::core::posix::ChildProcess::DeathObserver::create_once_with_signal_trap(trap));

class ChildScopesTest : public Test
{
public:
    ChildScopesTest()
    {
        // Run a test registry
        reg_rt_ = RuntimeImpl::create("TestRegistry", "Runtime.ini");
        auto reg_mw = reg_rt_->factory()->create("TestRegistry", "Zmq", "Zmq.ini");
        auto reg_obj(std::make_shared<RegistryObject>(*death_observer, std::make_shared<Executor>(), reg_mw));
        reg_mw->add_registry_object("TestRegistry", reg_obj);

        // Create a proxy to TestScope
        proxy_rt_ = RuntimeImpl::create("", "Runtime.ini");
        auto scope_mw = proxy_rt_->factory()->create("TestScope", "Zmq", "Zmq.ini");
        scope_mw->start();
        auto proxy = scope_mw->create_scope_proxy("TestScope");
        test_scope = ScopeImpl::create(proxy, "TestScope");
    }

    ~ChildScopesTest()
    {
        stop_test_scope();
    }

    void create_config_dir()
    {
        // Create an empty config directory for TestScope
        system::error_code ec;
        filesystem::create_directory(TEST_BIN_DIR "/TestScope", ec);
    }

    void remove_config_dir()
    {
        // Remove the config directory for TestScope
        system::error_code ec;
        filesystem::remove_all(TEST_BIN_DIR "/TestScope", ec);
    }

    void write_empty_config()
    {
        // open repository for output
        std::ofstream repo_file(TEST_BIN_DIR "/TestScope/child-scopes.json");
        repo_file << "";
        repo_file.close();
    }

    void write_corrupt_config()
    {
        // open repository for output
        std::ofstream repo_file(TEST_BIN_DIR "/TestScope/child-scopes.json");
        repo_file << "ag;hasd;glasd;glkasdhg;klasdf;k;";
        repo_file.close();
    }

    void write_partially_corrupt_config()
    {
        // open repository for output
        std::ofstream repo_file(TEST_BIN_DIR "/TestScope/child-scopes.json");
        repo_file << R"([{"id":"ScopeB","enabled":true},{"id":"ScopeB"},{"id":"ScopeC","enabled":"false"}])";
        repo_file.close();
    }

    void start_test_scope()
    {
        // Run TestScope in a separate thread
        scope_rt_ = Runtime::create_scope_runtime("TestScope", "Runtime.ini");
        scope_t_ = std::thread([this]
        {
            TestScope scope;
            scope_rt_->run_scope(&scope, "");
        });

        // Give the scope some time to bind to its endpoint
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    void stop_test_scope()
    {
        if (scope_rt_)
        {
            scope_rt_->destroy();
        }
        if (scope_t_.joinable())
        {
            scope_t_.join();
        }
    }

protected:
    ScopeProxy test_scope;

private:
    RuntimeImpl::UPtr reg_rt_;
    RuntimeImpl::UPtr proxy_rt_;
    Runtime::UPtr scope_rt_;
    std::thread scope_t_;
};

TEST_F(ChildScopesTest, get_set_ordered_list)
{
    // Create an empty config directory for TestScope
    remove_config_dir();
    create_config_dir();

    // Start TestScope
    start_test_scope();

    // 1st TestScope::find_child_scopes() returns: "A,B,C"
    // No order has been set yet, so we should just get: "A,B,C"
    ChildScopeList return_list = test_scope->child_scopes();
    ASSERT_EQ(3, return_list.size());
    EXPECT_EQ("ScopeA", return_list[0].id);
    EXPECT_FALSE(return_list[0].enabled);
    EXPECT_EQ("ScopeB", return_list[1].id);
    EXPECT_FALSE(return_list[1].enabled);
    EXPECT_EQ("ScopeC", return_list[2].id);
    EXPECT_TRUE(return_list[2].enabled);

    // Set order to: "B,E,A"
    ChildScopeList list;
    list.emplace_back(ChildScope{"ScopeB", false});
    list.emplace_back(ChildScope{"ScopeE", false});
    list.emplace_back(ChildScope{"ScopeA", true});
    EXPECT_TRUE(test_scope->set_child_scopes(list));

    // 2nd TestScope::find_child_scopes() returns: "D,A,B,C,E"
    // with order: "B,E,A", we should get: "B,E,A,D,C"
    return_list = test_scope->child_scopes();
    ASSERT_EQ(5, return_list.size());
    EXPECT_EQ("ScopeB", return_list[0].id);
    EXPECT_FALSE(return_list[0].enabled);
    EXPECT_EQ("ScopeE", return_list[1].id);
    EXPECT_FALSE(return_list[1].enabled);
    EXPECT_EQ("ScopeA", return_list[2].id);
    EXPECT_TRUE(return_list[2].enabled);
    EXPECT_EQ("ScopeD", return_list[3].id);
    EXPECT_FALSE(return_list[3].enabled);
    EXPECT_EQ("ScopeC", return_list[4].id);
    EXPECT_TRUE(return_list[4].enabled);

    // 3rd+ TestScope::find_child_scopes() returns: "D,A,B"
    // with order: "B,E,A,D,C", we should get: "B,A,D"
    return_list = test_scope->child_scopes();
    ASSERT_EQ(3, return_list.size());
    EXPECT_EQ("ScopeB", return_list[0].id);
    EXPECT_FALSE(return_list[0].enabled);
    EXPECT_EQ("ScopeA", return_list[1].id);
    EXPECT_TRUE(return_list[1].enabled);
    EXPECT_EQ("ScopeD", return_list[2].id);
    EXPECT_FALSE(return_list[2].enabled);

    // Set order to: "B,D,X,A"
    list.clear();
    list.emplace_back(ChildScope{"ScopeB", false});
    list.emplace_back(ChildScope{"ScopeD", true});
    list.emplace_back(ChildScope{"ScopeX", true});
    list.emplace_back(ChildScope{"ScopeA", false});
    EXPECT_TRUE(test_scope->set_child_scopes(list));

    // 3rd+ TestScope::find_child_scopes() returns: "D,A,B"
    // with order: "B,D,X,A", we should get: "B,D,A"
    return_list = test_scope->child_scopes();
    return_list = test_scope->child_scopes();
    ASSERT_EQ(3, return_list.size());
    EXPECT_EQ("ScopeB", return_list[0].id);
    EXPECT_FALSE(return_list[0].enabled);
    EXPECT_EQ("ScopeD", return_list[1].id);
    EXPECT_TRUE(return_list[1].enabled);
    EXPECT_EQ("ScopeA", return_list[2].id);
    EXPECT_FALSE(return_list[2].enabled);
}

TEST_F(ChildScopesTest, existing_config)
{
    // Use config from previous test
    // Start TestScope
    start_test_scope();

    // 1st TestScope::find_child_scopes() returns: "A,B,C"
    // with order: "B,D,A" (from previous test), we should get: "B,A,C"
    ChildScopeList return_list = test_scope->child_scopes();
    ASSERT_EQ(3, return_list.size());
    EXPECT_EQ("ScopeB", return_list[0].id);
    EXPECT_FALSE(return_list[0].enabled);
    EXPECT_EQ("ScopeA", return_list[1].id);
    EXPECT_FALSE(return_list[1].enabled);
    EXPECT_EQ("ScopeC", return_list[2].id);
    EXPECT_TRUE(return_list[2].enabled);
}

TEST_F(ChildScopesTest, no_config_dir)
{
    // Remove the config directory for TestScope
    remove_config_dir();

    // Start TestScope
    start_test_scope();

    // Set order to: "B,A,C" (should fail)
    ChildScopeList list;
    list.emplace_back(ChildScope{"ScopeB", false});
    list.emplace_back(ChildScope{"ScopeA", false});
    list.emplace_back(ChildScope{"ScopeC", true});
    EXPECT_FALSE(test_scope->set_child_scopes(list));

    // 1st TestScope::find_child_scopes() returns: "A,B,C"
    // No order was set, so we should just get: "A,B,C"
    ChildScopeList return_list = test_scope->child_scopes();
    ASSERT_EQ(3, return_list.size());
    EXPECT_EQ("ScopeA", return_list[0].id);
    EXPECT_FALSE(return_list[0].enabled);
    EXPECT_EQ("ScopeB", return_list[1].id);
    EXPECT_FALSE(return_list[1].enabled);
    EXPECT_EQ("ScopeC", return_list[2].id);
    EXPECT_TRUE(return_list[2].enabled);
}

TEST_F(ChildScopesTest, corrupted_config)
{
    // Create an empty config directory for TestScope
    remove_config_dir();
    create_config_dir();

    // Partially corrupt config (Sets order to: "B")
    write_partially_corrupt_config();
    start_test_scope();

    // 1st TestScope::find_child_scopes() returns: "A,B,C"
    // with partial order: "B", we should get: "B,A,C"
    ChildScopeList return_list = test_scope->child_scopes();
    ASSERT_EQ(3, return_list.size());
    EXPECT_EQ("ScopeB", return_list[0].id);
    EXPECT_TRUE(return_list[0].enabled);
    EXPECT_EQ("ScopeA", return_list[1].id);
    EXPECT_FALSE(return_list[1].enabled);
    EXPECT_EQ("ScopeC", return_list[2].id);
    EXPECT_TRUE(return_list[2].enabled);

    // Empty config
    write_empty_config();
    stop_test_scope();
    start_test_scope();

    // Check that nothing crashes and we simply get the list from the scope as is
    return_list = test_scope->child_scopes();
    ASSERT_EQ(3, return_list.size());
    EXPECT_EQ("ScopeA", return_list[0].id);
    EXPECT_FALSE(return_list[0].enabled);
    EXPECT_EQ("ScopeB", return_list[1].id);
    EXPECT_FALSE(return_list[1].enabled);
    EXPECT_EQ("ScopeC", return_list[2].id);
    EXPECT_TRUE(return_list[2].enabled);

    // Corrupt config
    write_corrupt_config();
    stop_test_scope();
    start_test_scope();

    // Check that nothing crashes and we simply get the list from the scope as is
    return_list = test_scope->child_scopes();
    ASSERT_EQ(3, return_list.size());
    EXPECT_EQ("ScopeA", return_list[0].id);
    EXPECT_FALSE(return_list[0].enabled);
    EXPECT_EQ("ScopeB", return_list[1].id);
    EXPECT_FALSE(return_list[1].enabled);
    EXPECT_EQ("ScopeC", return_list[2].id);
    EXPECT_TRUE(return_list[2].enabled);
}
