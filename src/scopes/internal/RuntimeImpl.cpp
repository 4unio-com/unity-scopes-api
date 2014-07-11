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

#include <unity/scopes/internal/RuntimeImpl.h>
#include <unity/scopes/internal/ScopeBaseImpl.h>

#include <unity/scopes/internal/DfltConfig.h>
#include <unity/scopes/internal/MWStateReceiver.h>
#include <unity/scopes/internal/RegistryConfig.h>
#include <unity/scopes/internal/RegistryImpl.h>
#include <unity/scopes/internal/RuntimeConfig.h>
#include <unity/scopes/internal/ScopeConfig.h>
#include <unity/scopes/internal/ScopeObject.h>
#include <unity/scopes/internal/SettingsDB.h>
#include <unity/scopes/internal/UniqueID.h>
#include <unity/scopes/ScopeBase.h>
#include <unity/scopes/ScopeExceptions.h>
#include <unity/UnityExceptions.h>
#include <unity/util/FileIO.h>

#include <boost/filesystem.hpp>

#include <boost/filesystem.hpp>

#include <cassert>
#include <cstring>
#include <future>
#include <iostream> // TODO: remove this once logging is added

#include <sys/stat.h>

using namespace std;
using namespace unity::scopes;

namespace unity
{

namespace scopes
{

namespace internal
{

RuntimeImpl::RuntimeImpl(string const& scope_id, string const& configfile)
{
    try
    {
        lock_guard<mutex> lock(mutex_);

        destroyed_ = false;

        scope_id_ = scope_id;
        if (scope_id_.empty())
        {
            UniqueID id;
            scope_id_ = "c-" + id.gen();
        }

        // Create the middleware factory and get the registry identity and config filename.
        RuntimeConfig config(configfile);
        string default_middleware = config.default_middleware();
        string middleware_configfile = config.default_middleware_configfile();
        middleware_factory_.reset(new MiddlewareFactory(this));
        registry_configfile_ = config.registry_configfile();
        registry_identity_ = config.registry_identity();
        reap_expiry_ = config.reap_expiry();
        reap_interval_ = config.reap_interval();
        ss_configfile_ = config.ss_configfile();
        ss_registry_identity_ = config.ss_registry_identity();

        middleware_ = middleware_factory_->create(scope_id_, default_middleware, middleware_configfile);
        middleware_->start();

        async_pool_ = make_shared<ThreadPool>(1); // TODO: configurable pool size
        future_queue_ = make_shared<ThreadSafeQueue<future<void>>>();
        waiter_thread_ = std::thread([this]{ waiter_thread(future_queue_); });

        if (registry_configfile_.empty() || registry_identity_.empty())
        {
            cerr << "Warning: no registry configured" << endl;
            registry_identity_ = "";
        }
        else
        {
            // Create the registry proxy
            RegistryConfig reg_config(registry_identity_, registry_configfile_);
            auto registry_mw_proxy = middleware_->registry_proxy();
            registry_ = make_shared<RegistryImpl>(registry_mw_proxy, this);
        }

        data_dir_ = config.data_directory();

        string scope_type;
        cache_dir_ = find_cache_directory(scope_type);

        // Set tmp dir.
        if (scope_type.empty())
        {
            tmp_dir_ = "/tmp";
        }
        else
        {
            // We need to create any directories under /run/user/<uid> because they might not
            // exist. (/run/user/<uid> gets cleaned out periodically.)
            string path = string("/run/user/") + std::to_string(geteuid());
            path += "/scopes";
            ::mkdir(path.c_str(), 0700);
            path += "/" + scope_type;
            ::mkdir(path.c_str(), 0700);
            path += "/" + scope_id_;
            ::mkdir(path.c_str(), 0700);

            tmp_dir_ = path;
        }
    }
    catch (unity::Exception const& e)
    {
        destroy();
        string msg = "Cannot instantiate run time for " + (scope_id.empty() ? "client" : scope_id) +
                     ", config file: " + configfile;
        throw ConfigException(msg);
    }
}

RuntimeImpl::~RuntimeImpl()
{
    try
    {
        destroy();
    }
    catch (std::exception const& e) // LCOV_EXCL_LINE
    {
        cerr << "~RuntimeImpl(): " << e.what() << endl;
        // TODO: log error
    }
    catch (...) // LCOV_EXCL_LINE
    {
        cerr << "~RuntimeImpl(): unknown exception" << endl;
        // TODO: log error
    }
}

RuntimeImpl::UPtr RuntimeImpl::create(string const& scope_id, string const& configfile)
{
    return UPtr(new RuntimeImpl(scope_id, configfile));
}

void RuntimeImpl::destroy()
{
    lock_guard<mutex> lock(mutex_);

    if (destroyed_)
    {
        return;
    }
    destroyed_ = true;

    // Stop the reaper. Any ReplyObject instances that may still
    // be hanging around will be cleaned up when the server side
    // is shut down.
    if (reply_reaper_)
    {
        reply_reaper_->destroy();
        reply_reaper_ = nullptr;
    }

    // No more outgoing invocations.
    async_pool_ = nullptr;

    // Wait for any twoway operations that were invoked asynchronously to complete.
    if (future_queue_)
    {
        future_queue_->wait_until_empty();
        future_queue_->destroy();
        future_queue_->wait_for_destroy();
    }

    if (waiter_thread_.joinable())
    {
        waiter_thread_.join();
    }

    // Shut down server-side.
    if (middleware_)
    {
        middleware_->stop();
        middleware_->wait_for_shutdown();
        middleware_ = nullptr;
        middleware_factory_.reset(nullptr);
    }
}

string RuntimeImpl::scope_id() const
{
    lock_guard<mutex> lock(mutex_);
    return scope_id_;
}

MiddlewareFactory const* RuntimeImpl::factory() const
{
    lock_guard<mutex> lock(mutex_);

    if (destroyed_)
    {
        throw LogicException("factory(): Cannot obtain factory for already destroyed run time");
    }
    return middleware_factory_.get();
}

RegistryProxy RuntimeImpl::registry() const
{
    lock_guard<mutex> lock(mutex_);

    if (destroyed_)
    {
        throw LogicException("registry(): Cannot obtain registry for already destroyed run time");
    }
    if (!registry_)
    {
        throw ConfigException("registry(): no registry configured");
    }
    return registry_;
}

string RuntimeImpl::registry_configfile() const
{
    return registry_configfile_;  // Immutable
}

string RuntimeImpl::registry_identity() const
{
    return registry_identity_;  // Immutable
}

string RuntimeImpl::ss_configfile() const
{
    return ss_configfile_;  // Immutable
}

string RuntimeImpl::ss_registry_identity() const
{
    return ss_registry_identity_;  // Immutable
}

Reaper::SPtr RuntimeImpl::reply_reaper() const
{
    // We lazily create a reaper the first time we are asked for it, which happens when the first query is created.
    lock_guard<mutex> lock(mutex_);
    if (!reply_reaper_)
    {
        reply_reaper_ = Reaper::create(reap_interval_, reap_expiry_);
    }
    return reply_reaper_;
}

void RuntimeImpl::waiter_thread(ThreadSafeQueue<std::future<void>>::SPtr const& queue) const noexcept
{
    for (;;)
    {
        try
        {
            // Wait on the future from an async invocation.
            // wait_and_pop() throws runtime_error when queue is destroyed
            queue->wait_and_pop().get();
        }
        catch (std::runtime_error const&)
        {
            break;
        }
        catch (std::future_error const&)
        {
            // If the run time is shut down without waiting for outstanding
            // async invocations to complete, we get a future error because
            // the promise will be destroyed, so we ignore this.
        }
    }
}

ThreadPool::SPtr RuntimeImpl::async_pool() const
{
    lock_guard<mutex> lock(mutex_);
    return async_pool_;
}

ThreadSafeQueue<future<void>>::SPtr RuntimeImpl::future_queue() const
{
    return future_queue_;  // Immutable
}

void RuntimeImpl::run_scope(ScopeBase* scope_base, string const& scope_ini_file)
{
    run_scope(scope_base, "", scope_ini_file);
}

void RuntimeImpl::run_scope(ScopeBase* scope_base, string const& runtime_ini_file, string const& scope_ini_file)
{
    if (!scope_base)
    {
        throw InvalidArgumentException("Runtime::run_scope(): scope_base cannot be nullptr");
    }

    // Retrieve the registry middleware and create a proxy to its state receiver
    RegistryConfig reg_conf(registry_identity_, registry_configfile_);
    auto reg_runtime = create(registry_identity_, runtime_ini_file);
    auto reg_mw = reg_runtime->factory()->find(registry_identity_, reg_conf.mw_kind());
    auto reg_state_receiver = reg_mw->create_state_receiver_proxy("StateReceiver");

    auto mw = factory()->create(scope_id_, reg_conf.mw_kind(), reg_conf.mw_configfile());

    {
        boost::filesystem::path dir = boost::filesystem::canonical(scope_ini_file).parent_path();
        scope_base->p->set_scope_directory(dir.native());
    }

    // cache_dir_ will not be empty for a properly installed (real) scope,
    // but may be empty for some of the tests. It does not matter
    // unless the scope actually tries to write something there, in which
    // case it will get an error if it does.
    scope_base->p->set_cache_directory(cache_dir_);

    scope_base->p->set_tmp_directory(tmp_dir_);

    {
        // Try to open the scope settings database, if any.
        string settings_dir = data_dir_ + "/" + scope_id_;
        string scope_dir = scope_base->scope_directory();
        string settings_db = settings_dir + "/settings.db";
        string settings_schema = scope_dir + "/" + scope_id_ + "-settings.ini";
        if (boost::filesystem::exists(settings_schema))
        {
            // Make sure the settings directories exist. (No permission for group and others; data might be sensitive.)
            ::mkdir(data_dir_.c_str(), 0700);
            ::mkdir(settings_dir.c_str(), 0700);

            shared_ptr<SettingsDB> db(SettingsDB::create_from_ini_file(settings_db, settings_schema));
            scope_base->p->set_settings_db(db);
        }
        else
        {
            scope_base->p->set_settings_db(nullptr);
        }
    }

    scope_base->p->set_registry(registry_);

    // We are finally set up, initialize the scope.
    scope_base->start(scope_id_);

    // Ensure the scope gets stopped.
    unique_ptr<ScopeBase, void(*)(ScopeBase*)> cleanup_scope(scope_base, [](ScopeBase *scope_base) { scope_base->stop(); });

    // Give a thread to the scope to do with as it likes. If the scope
    // doesn't want to use it and immediately returns from run(),
    // that's fine.
    auto run_future = std::async(launch::async, [scope_base] { scope_base->run(); });

    // Create a servant for the scope and register the servant.
    auto scope = unique_ptr<internal::ScopeObject>(new internal::ScopeObject(this, scope_base));
    if (!scope_ini_file.empty())
    {
        ScopeConfig scope_config(scope_ini_file);
        mw->add_scope_object(scope_id_, move(scope), scope_config.idle_timeout() * 1000);
    }
    else
    {
        mw->add_scope_object(scope_id_, move(scope));
    }

    // Inform the registry that this scope is now ready to process requests
    reg_state_receiver->push_state(scope_id_, StateReceiverObject::State::ScopeReady);

    mw->wait_for_shutdown();

    // Inform the registry that this scope is shutting down
    reg_state_receiver->push_state(scope_id_, StateReceiverObject::State::ScopeStopping);

    run_future.get();
}

ObjectProxy RuntimeImpl::string_to_proxy(string const& s) const
{
    try
    {
        auto mw = middleware_factory_->find(s);
        assert(mw);
        return mw->string_to_proxy(s);
    }
    catch (...)
    {
        throw ResourceException("Runtime::string_to_proxy(): Cannot create proxy from string");
    }
}

string RuntimeImpl::proxy_to_string(ObjectProxy const& proxy) const
{
    try
    {
        if (proxy == nullptr)
        {
            return "nullproxy:";
        }
        return proxy->to_string();
    }
    catch (...)
    {
        throw ResourceException("Runtime::proxy_to_string(): Cannot stringify proxy");
    }
}

// Figure out what kind of scope we are dealing with by looking for
// the scope's writable area in the file system. For a properly-installed
// scope, there will be exactly one such directory. If we can't find any,
// we print a warning return the empty string. (This allows the tests to
// to run without having to create the directories.) If we find more
// than one such directory, we throw, because the scope is not installed
// correctly. Otherwise, we return the found directory and set the
// scope type to the found type.

string RuntimeImpl::find_cache_directory(string& type)
{
    static vector<string> const scope_types = { "unconfined", "leaf-net", "leaf-fs" };

    vector<boost::filesystem::path> found_dirs;
    string found_type;

    // Try and find one of the possible directories for the scope to store its
    // data files. If we find more than one, we throw.
    for (auto const& scope_type : scope_types)
    {
        boost::filesystem::path path = data_dir_ + "/" + scope_type + "/" + scope_id_;
        if (boost::filesystem::exists(path))
        {
            if (!boost::filesystem::is_directory(path))
            {
                cerr << "Warning: ignoring non-directory cache path: " << path.native() << endl;
            }
            else
            {
                found_type = scope_type;
                found_dirs.push_back(path);
            }
        }
    }

    if (found_dirs.empty())
    {
        cerr << "Warning: no cache directory found for scope " << scope_id_ << endl;
        type = "";
        return "";
    }

    if (found_dirs.size() > 1)
    {
        ostringstream err;
        err << "Runtime(): bad configuration: found more than one cache directory for scope " << scope_id_ << ":";
        for (auto const& path : found_dirs)
        {
            err << "\n  " << path;
        }
        throw ConfigException(err.str());
    }

    auto dir = found_dirs[0].native();
    auto perms = boost::filesystem::status(found_dirs[0]).permissions();
    if (perms & boost::filesystem::group_all || perms & boost::filesystem::others_all)
    {
        cerr << "Warning: ignoring cache directory accessible by group or others for " << scope_id_
             << ": " << dir << endl;
        type = "";
        return "";
    }

    type = found_type;
    return dir;
}

} // namespace internal

} // namespace scopes

} // namespace unity
