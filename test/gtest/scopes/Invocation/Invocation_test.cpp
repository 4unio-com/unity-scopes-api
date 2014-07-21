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
 * Authored by: Michi Henning <michi.henning@canonical.com>
 */

#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include <mutex>

#include <unity/scopes/ActionMetadata.h>
#include <unity/scopes/CategorisedResult.h>
#include <unity/scopes/internal/MWScope.h>
#include <unity/scopes/internal/RegistryObject.h>
#include <unity/scopes/internal/RuntimeImpl.h>
#include <unity/scopes/internal/ScopeImpl.h>
#include <unity/scopes/ListenerBase.h>
#include <unity/scopes/QueryCtrl.h>
#include <unity/scopes/Runtime.h>
#include <unity/scopes/SearchMetadata.h>
#include <unity/UnityExceptions.h>

#include <gtest/gtest.h>

#include "EmptyScope.h"
#include "TestScope.h"

using namespace std;
using namespace unity::scopes;
using namespace unity::scopes::internal;

class TestReceiver : public SearchListenerBase
{
public:
    TestReceiver()
        : query_complete_(false)
    {
    }

    virtual void push(CategorisedResult /* result */) override
    {
    }

    virtual void finished(ListenerBase::Reason reason, string const& error_message) override
    {
        lock_guard<mutex> lock(mutex_);
        reason_ = reason;
        error_message_ = error_message;
        query_complete_ = true;
        cond_.notify_one();
    }

    void wait_until_finished()
    {
        unique_lock<mutex> lock(mutex_);
        cond_.wait(lock, [this] { return this->query_complete_; });
        query_complete_ = false;
    }

    ListenerBase::Reason reason()
    {
        lock_guard<mutex> lock(mutex_);
        return reason_;
    }

    string error_message()
    {
        lock_guard<mutex> lock(mutex_);
        return error_message_;
    }

private:
    bool query_complete_;
    ListenerBase::Reason reason_;
    string error_message_;
    mutex mutex_;
    condition_variable cond_;
};

std::shared_ptr<core::posix::SignalTrap> trap(core::posix::trap_signals_for_all_subsequent_threads({core::posix::Signal::sig_chld}));
std::unique_ptr<core::posix::ChildProcess::DeathObserver> death_observer(core::posix::ChildProcess::DeathObserver::create_once_with_signal_trap(trap));

RuntimeImpl::SPtr run_test_registry()
{
    RuntimeImpl::SPtr runtime = RuntimeImpl::create("TestRegistry", "Runtime.ini");
    MiddlewareBase::SPtr middleware = runtime->factory()->create("TestRegistry", "Zmq", "Zmq.ini");
    RegistryObject::SPtr ro(std::make_shared<RegistryObject>(*death_observer, std::make_shared<Executor>(), middleware));
    middleware->add_registry_object("TestRegistry", ro);
    return runtime;
}

// Check that invoking on a scope after a timeout exception from a previous
// invocation works correctly. This tests that a failed socket is removed
// from the connection pool in ZmqObject::invoke_twoway_().

TEST(Invocation, timeout)
{
    auto reg_rt = run_test_registry();
    auto rt = internal::RuntimeImpl::create("", "Runtime.ini");
    auto mw = rt->factory()->create("TestScope", "Zmq", "Zmq.ini");
    mw->start();
    auto proxy = mw->create_scope_proxy("TestScope");
    auto scope = internal::ScopeImpl::create(proxy, rt.get(), "TestScope");

    auto receiver = make_shared<TestReceiver>();

    // First call must time out.
    scope->search("test", SearchMetadata("unused", "unused"), receiver);
    receiver->wait_until_finished();

    EXPECT_EQ(ListenerBase::Error, receiver->reason());
    EXPECT_EQ("unity::scopes::TimeoutException: Request timed out after 500 milliseconds", receiver->error_message());

    // Wait another two seconds, so TestScope is finally able to receive another request.
    this_thread::sleep_for(chrono::seconds(2));

    // Second call must succeed
    scope->search("test", SearchMetadata("unused", "unused"), receiver);
    receiver->wait_until_finished();

    EXPECT_EQ(ListenerBase::Finished, receiver->reason());
    EXPECT_EQ("", receiver->error_message());
}

class NullReceiver : public SearchListenerBase
{
public:
    NullReceiver()
    {
    }

    virtual void push(CategorisedResult /* result */) override
    {
    }

    virtual void finished(ListenerBase::Reason /* reason */, string const& /* error_message */) override
    {
    }
};

TEST(Invocation, shutdown_with_outstanding_async)
{
    auto rt = internal::RuntimeImpl::create("", "Runtime.ini");
    auto mw = rt->factory()->create("EmptyScope", "Zmq", "Zmq.ini");
    mw->start();
    auto proxy = mw->create_scope_proxy("EmptyScope");
    auto scope = internal::ScopeImpl::create(proxy, rt.get(), "EmptyScope");

    auto receiver = make_shared<NullReceiver>();

    // Fire a bunch of searches and do *not* wait for them complete.
    // This tests that we correctly shut down the run time if there
    // are outstanding async invocations.
    for (int i = 0; i < 100; ++i)
    {
        scope->search("test", SearchMetadata("unused", "unused"), receiver);
    }
}

void testscope_thread(Runtime::SPtr const& rt, string const& runtime_ini_file)
{
    TestScope scope;
    rt->run_scope(&scope, runtime_ini_file, "");
}

void nullscope_thread(Runtime::SPtr const& rt, string const& runtime_ini_file)
{
    EmptyScope scope;
    rt->run_scope(&scope, runtime_ini_file, "");
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    Runtime::SPtr tsrt = move(Runtime::create_scope_runtime("TestScope", "Runtime.ini"));
    std::thread testscope_t(testscope_thread, tsrt, "Runtime.ini");

    Runtime::SPtr esrt = move(Runtime::create_scope_runtime("EmptyScope", "Runtime.ini"));
    std::thread emptyscope_t(nullscope_thread, esrt, "Runtime.ini");

    // Give threads some time to bind to endpoints, to avoid getting ObjectNotExistException
    // from a synchronous remote call.
    this_thread::sleep_for(chrono::milliseconds(200));

    auto rc = RUN_ALL_TESTS();

    tsrt->destroy();
    testscope_t.join();

    esrt->destroy();
    emptyscope_t.join();

    return rc;
}
