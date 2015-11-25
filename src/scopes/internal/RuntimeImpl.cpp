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

#include <unity/scopes/internal/ChildScopesRepository.h>
#include <unity/scopes/internal/DfltConfig.h>
#include <unity/scopes/internal/Logger.h>
#include <unity/scopes/internal/MWStateReceiver.h>
#include <unity/scopes/internal/RegistryConfig.h>
#include <unity/scopes/internal/RegistryImpl.h>
#include <unity/scopes/internal/RuntimeConfig.h>
#include <unity/scopes/internal/ScopeBaseImpl.h>
#include <unity/scopes/internal/ScopeConfig.h>
#include <unity/scopes/internal/ScopeObject.h>
#include <unity/scopes/internal/SettingsDB.h>
#include <unity/scopes/internal/UniqueID.h>
#include <unity/scopes/internal/Utils.h>
#include <unity/scopes/ScopeBase.h>
#include <unity/scopes/ScopeExceptions.h>

#include <boost/filesystem.hpp>
#include <unity/UnityExceptions.h>
#include <unity/util/FileIO.h>

#include <cassert>
#include <cstring>
#include <future>

#include <sys/apparmor.h>

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
        lock_guard<mutex> lock(mutex_);  // TODO: shouldn't be necessary.

        destroyed_ = false;

        scope_id_ = scope_id;
        string log_file_basename = scope_id;
        if (scope_id_.empty())
        {
            UniqueID id;
            scope_id_ = "c-" + id.gen();
            log_file_basename = "client";  // Don't make lots of log files named like c-c4c758b100000000.
        }

        // Until we know where the scope's cache directory is,
        // we use a logger that logs to std::clog.
        logger_.reset(new Logger(scope_id_));

        // Create the middleware factory and get the registry identity and config filename.
        runtime_configfile_ = configfile;
        RuntimeConfig config(configfile);

        // Configure trace channels.
        try
        {
            logger_->enable_channels(config.trace_channels());
        }
        catch (...)
        {
            throw InvalidArgumentException("Runtime(): invalid trace channel name(s)");
        }

        // Now that we have the config, change the logger to log to a file.
        // If log_dir was explicitly set to the empty string, continue
        // logging to std::clog.
        log_dir_ = config.log_directory();
        if (!log_dir_.empty())
        {
            logger_->set_log_file(find_log_dir(log_file_basename) + "/" + log_file_basename,
                                  config.max_log_file_size(),
                                  config.max_log_dir_size());
        }

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
            BOOST_LOG_SEV(logger(), Logger::Warning) << "no registry configured";
            registry_identity_ = "";
        }
        else
        {
            // Create the registry proxy
            RegistryConfig reg_config(registry_identity_, registry_configfile_);
            auto registry_mw_proxy = middleware_->registry_proxy();
            registry_ = make_shared<RegistryImpl>(registry_mw_proxy);
        }

        cache_dir_ = config.cache_directory();
        app_dir_ = config.app_directory();
        config_dir_ = config.config_directory();
    }
    catch (unity::Exception const&)
    {
        destroy();
        string msg = "Cannot instantiate run time for " + (scope_id.empty() ? "client" : scope_id) +
                     ", config file: " + configfile;
        ConfigException ex(msg);
        BOOST_LOG(logger()) << ex.what();
        throw ex;
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
        BOOST_LOG(logger()) << "~RuntimeImpl(): " << e.what();
    }
    catch (...) // LCOV_EXCL_LINE
    {
        BOOST_LOG(logger()) << "~RuntimeImpl(): unknown exception";
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
    if (async_pool_)
    {
        async_pool_->destroy_once_empty();
    }

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
        middleware_factory_ = nullptr;
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

boost::log::sources::severity_channel_logger_mt<>& RuntimeImpl::logger() const
{
    return *logger_;
}

boost::log::sources::severity_channel_logger_mt<>& RuntimeImpl::logger(Logger::Channel channel) const
{
    return (*logger_)(channel);
}

namespace
{

class PromiseWrapper final
{
public:
    PromiseWrapper(std::promise<void> p)
        : p_(move(p))
    {
    }

    ~PromiseWrapper()
    {
        try
        {
            p_.set_value();
        }
        catch (std::future_error const&)
        {
        }
    }

    void set_value()
    {
        p_.set_value();
    }

private:
    std::promise<void> p_;
};

}

void RuntimeImpl::run_scope(ScopeBase* scope_base,
                            string const& scope_ini_file,
                            std::promise<void> ready_promise)
{
    if (!scope_base)
    {
        throw InvalidArgumentException("Runtime::run_scope(): scope_base cannot be nullptr");
    }

    // Create a middleware for this scope.
    RegistryConfig reg_conf(registry_identity_, registry_configfile_);
    auto mw = factory()->create(scope_id_, reg_conf.mw_kind(), reg_conf.mw_configfile());

    boost::filesystem::path scope_dir = boost::filesystem::canonical(scope_ini_file).parent_path();
    scope_base->p->set_scope_directory(scope_dir.native());

    {
        // Try to open the scope settings database, if any.
        string config_dir = config_dir_ + "/" + scope_id_;
        string settings_db = config_dir + "/settings.ini";

        string settings_schema = scope_dir.native() + "/" + scope_id_ + "-settings.ini";

        boost::system::error_code ec;
        if (boost::filesystem::exists(settings_schema, ec))
        {
            shared_ptr<SettingsDB> db(SettingsDB::create_from_ini_file(settings_db, settings_schema, logger()));
            scope_base->p->set_settings_db(db);
        }
        else
        {
            scope_base->p->set_settings_db(nullptr);
        }
    }

    scope_base->p->set_registry(registry_);
    scope_base->p->set_cache_directory(find_cache_dir());
    scope_base->p->set_app_directory(find_app_dir());
    scope_base->p->set_tmp_directory(find_tmp_dir());

    {
        // Configure the child scopes repository
        string child_scopes_repo = scope_base->cache_directory() + "/child-scopes.json";
        auto repo = make_shared<ChildScopesRepository>(child_scopes_repo, logger());
        scope_base->p->set_child_scopes_repo(repo);
    }

    try
    {
        scope_base->start(scope_id_);
    }
    catch (...)
    {
        throw unity::ResourceException("Scope " + scope_id_ +": exception from start()");
    }

    exception_ptr ep;  // Stores any exceptions that happen from now on.

    // Ensure the scope gets stopped.
    auto call_stop = [this, &ep](ScopeBase *scope_base)
    {
        try
        {
            scope_base->stop();
        }
        catch (...)
        {
            unity::ResourceException ex("Scope " + scope_id_ +": exception from stop()");
            ep = make_exception_ptr(ex);
        }
    };
    unity::util::ResourcePtr<ScopeBase*, decltype(call_stop)> cleanup_scope(scope_base, call_stop);

    // Make sure we satisfy the promise even if something goes wrong.
    PromiseWrapper promise(move(ready_promise));

    // Give a thread to the scope to do with as it likes. If the scope
    // doesn't want to use it and immediately returns from run(),
    // that's fine.
    auto run_future = std::async(launch::async, [scope_base] { scope_base->run(); });
    this_thread::yield();

    try
    {
        // Create a servant for the scope and register the servant.
        if (!scope_ini_file.empty())
        {
            // Check if this scope has requested debug mode, if so, disable the idle timeout
            ScopeConfig scope_config(scope_ini_file);
            int idle_timeout_ms = scope_config.debug_mode() ? -1 : scope_config.idle_timeout() * 1000;
            auto scope = unique_ptr<internal::ScopeObject>(new internal::ScopeObject(scope_base,
                                                                                     scope_config.debug_mode()));
            mw->add_scope_object(scope_id_, move(scope), idle_timeout_ms);
        }
        else
        {
            auto scope = unique_ptr<internal::ScopeObject>(new internal::ScopeObject(scope_base));
            mw->add_scope_object(scope_id_, move(scope));
        }

        // Inform the registry that this scope is now ready to process requests
        {
            auto reg_state_receiver = mw->create_registry_state_receiver_proxy("StateReceiver");
            reg_state_receiver->push_state(scope_id_, StateReceiverObject::State::ScopeReady);
        }

        promise.set_value();
        mw->wait_for_shutdown();
        cleanup_scope.dealloc();   // Causes ScopeBase::run() to return if the scope is properly written

        {
            // mw is now shut down, so we need a separate one to send the state update to the registry.
            auto sd_runtime = create(scope_id_ + "-shutdown", runtime_configfile_);
            auto sd_mw = sd_runtime->factory()->find(scope_id_ + "-shutdown", reg_conf.mw_kind());
            auto reg_state_receiver = sd_mw->create_registry_state_receiver_proxy("StateReceiver");
            // Inform the registry that this scope is shutting down
            reg_state_receiver->push_state(scope_id_, StateReceiverObject::State::ScopeStopping);
        }
    }
    catch (...)
    {
        unity::ResourceException ex("Scope " + scope_id_ +": failure during initialization");
        ex.remember(ep);  // ep will still be nullptr if something other than stop() threw.
        ep = make_exception_ptr(ex);
    }

    try
    {
        run_future.get();
    }
    catch (...)
    {
        unity::ResourceException ex("Scope " + scope_id_ +": exception from run()");
        ep = ex.remember(ep);
    }

    if (ep)
    {
        rethrow_exception(ep);
    }
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

string RuntimeImpl::cache_directory() const
{
    return find_cache_dir();
}

string RuntimeImpl::tmp_directory() const
{
    return find_tmp_dir();
}

string RuntimeImpl::config_directory() const
{
    return config_dir_;
}

string RuntimeImpl::demangled_id(string const& scope_id) const
{
    // For scopes that are in a click package together with an app,
    // such as YouTube, the scope id is <scope_id>_<app_id>
    // The app directory and cache directory names are the scope ID up to the first
    // underscore. For example, com.ubuntu.scopes.youtube_youtube is the
    // scope ID, but the cache dir name is com.ubuntu.scopes.youtube.
    auto id = scope_id;
    if (confined())
    {
        auto pos = id.find('_');
        if (pos != string::npos)
        {
            id = id.substr(0, pos);
        }
    }
    return id;
}

bool RuntimeImpl::confined() const
{
    auto is_confined = []
    {
        // Find out whether we are confined. aa_getcon() returns -1 with EACCESS in that case.
        char* con = nullptr;
        char* mode;
        int rc = aa_getcon(&con, &mode);
        int error = errno;
        if (rc == -1)
        {
            cerr << "aa_getcon failed, errno = " << error << endl;
        }
        // Only con (not mode) must be deallocated
        free(con);
        return rc == -1 && error == EACCES;
    };
    static bool confined = is_confined();
    return confined;
}

string RuntimeImpl::confinement_type() const
{
    return confined() ? "leaf-net" : "unconfined";
}

string RuntimeImpl::find_cache_dir() const
{
    // Create the cache_dir_/<confinement-type>/<id> directories if they don't exist.
    string dir = cache_dir_ + "/" + confinement_type();
    if (!confined())  // Avoid apparmor noise
    {
        make_directories(cache_dir_, 0700);
    }
    // A confined scope is allowed to create this dir.
    dir += "/" + demangled_id(scope_id_);
    make_directories(dir, 0700);
    return dir;
}

string RuntimeImpl::find_app_dir() const
{
    // Create the app_dir_/<id> directories if they don't exist.
    string dir = app_dir_ + "/" + demangled_id(scope_id_);
    if (!confined())  // Avoid apparmor noise
    {
        make_directories(dir, 0700);
    }
    return dir;
}

string RuntimeImpl::find_log_dir(string const& id) const
{
    string dir = log_dir_ + "/" + confinement_type();
    if (!confined())  // Avoid apparmore noise
    {
        make_directories(dir, 0700);
    }
    // A confined scope is allowed to create this dir.
    dir += "/" + demangled_id(id) + "/logs";
    make_directories(dir, 0700);
    return dir;
}

string RuntimeImpl::find_tmp_dir() const
{
    // Set tmp dir.
    // We need to create any directories under /run/user/<uid> because they might not
    // exist. We set the sticky bit because, without this, things in
    // /run/user may be deleted if not accessed for more than six hours.
    string dir = string("/run/user/") + std::to_string(geteuid()) + "/scopes/" + confinement_type();
    if (!confined())  // Avoid apparmor noise
    {
        make_directories(dir, 0700);
    }
    // A confined scope is allowed to create this dir.
    dir += "/" + scope_id_;  // Not demangled, use the real scope ID.
    make_directories(dir, 0700);
    return dir;
}

} // namespace internal

} // namespace scopes

} // namespace unity
