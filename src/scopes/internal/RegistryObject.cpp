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
 * Authored by: Marcus Tomlinson <marcus.tomlinson@canonical.com>
 */

#include <unity/scopes/internal/RegistryObject.h>

#include <unity/scopes/internal/MWRegistry.h>
#include <unity/scopes/internal/RuntimeImpl.h>
#include <unity/scopes/ScopeExceptions.h>
#include <unity/UnityExceptions.h>
#include <unity/util/ResourcePtr.h>

#include <unity/scopes/internal/max_align_clang_bug.h>  // TODO: remove this once clang 3.5.2 is released
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <core/posix/child_process.h>
#include <core/posix/exec.h>

#include <cassert>
#include <wordexp.h>

using namespace std;

namespace unity
{

namespace scopes
{

namespace internal
{

RegistryObject::RegistryObject(core::posix::ChildProcess::DeathObserver& death_observer,
                               Executor::SPtr const& executor,
                               MiddlewareBase::SPtr middleware)
    : death_observer_(death_observer),
      death_observer_connection_
      {
          death_observer_.child_died().connect([this](core::posix::ChildProcess const& cp)
          {
              on_process_death(cp);
          })
      },
      state_receiver_(new StateReceiverObject()),
      state_receiver_connection_
      {
          state_receiver_->state_received().connect([this](std::string const& id,
                                                    StateReceiverObject::State const& s)
          {
              on_state_received(id, s);
          })
      },
      executor_(executor)
{
    if (middleware)
    {
        try
        {
            publisher_ = middleware->create_publisher(middleware->runtime()->registry_identity());
        }
        catch (std::exception const& e)
        {
            std::cerr << "RegistryObject(): failed to create registry publisher: " << e.what() << endl;
        }
    }
}

RegistryObject::~RegistryObject()
{
    {
        // The destructor may be called from an arbitrary
        // thread, so we need a full fence here.
        lock_guard<decltype(mutex_)> lock(mutex_);
    }

    // kill all scope processes
    for (auto& scope_process : scope_processes_)
    {
        try
        {
            // at this point the registry middleware is shutting down, hence we will not receive
            // "ScopeStopping" states from dying scopes. We manually set it here as to avoid
            // outputting bogus error messages.
            if (is_scope_running(scope_process.first))
            {
                scope_process.second.update_state(ScopeProcess::Stopping);
                scope_process.second.kill();
            }
        }
        catch(std::exception const& e)
        {
            cerr << "RegistryObject::~RegistryObject(): " << e.what() << endl;
        }
    }
}

ScopeMetadata RegistryObject::get_metadata(std::string const& scope_id) const
{
    if (scope_id.empty())
    {
        // If the id is empty, it was sent as empty by the remote client.
        throw unity::InvalidArgumentException("RegistryObject::get_metadata(): Cannot search for scope with empty id");
    }

    // Look for the scope in both the local and the remote map.
    // Local scopes take precedence over remote ones of the same id.
    // (Ideally, this should never happen.)
    {
        lock_guard<decltype(mutex_)> lock(mutex_);
        auto const& scope_it = scopes_.find(scope_id);
        if (scope_it != scopes_.end())
        {
            return scope_it->second;
        }
    }
    // Unlock, so we don't call the remote registry while holding a lock.

    if (remote_registry_)
    {
        try
        {
            return remote_registry_->get_metadata(scope_id);
        }
        catch (std::exception const& e)
        {
            cerr << "cannot get metadata from remote registry: " << e.what() << endl;
            // TODO: log error
        }
    }

    throw NotFoundException("RegistryObject::get_metadata(): no such scope: ",  scope_id);
}

MetadataMap RegistryObject::list() const
{
    MetadataMap all_scopes;  // Local scopes
    {
        lock_guard<decltype(mutex_)> lock(mutex_);
        all_scopes = scopes_;  // Local scopes
    }
    // Unlock, so we don't call the remote registry while holding a lock.

    // If a remote scope has the same id as a local one,
    // this will not overwrite a local scope with a remote
    // one if they have the same id.
    if (remote_registry_)
    {
        try
        {
            MetadataMap remote_scopes = remote_registry_->list();
            all_scopes.insert(remote_scopes.begin(), remote_scopes.end());
        }
        catch (std::exception const& e)
        {
            cerr << "cannot get scopes list from remote registry: " << e.what() << endl;
            // TODO: log error
        }
    }

    return all_scopes;
}

ObjectProxy RegistryObject::locate(std::string const& identity)
{
    // If the id is empty, it was sent as empty by the remote client.
    if (identity.empty())
    {
        throw unity::InvalidArgumentException("RegistryObject::locate(): Cannot locate scope with empty id");
    }

    ObjectProxy proxy;
    ProcessMap::iterator proc_it;
    {
        lock_guard<decltype(mutex_)> lock(mutex_);

        auto scope_it = scopes_.find(identity);
        if (scope_it == scopes_.end())
        {
            throw NotFoundException("RegistryObject::locate(): Tried to locate unknown local scope", identity);
        }
        proxy = scope_it->second.proxy();

        proc_it = scope_processes_.find(identity);
        if (proc_it == scope_processes_.end())
        {
            throw NotFoundException("RegistryObject::locate(): Tried to exec unknown local scope", identity);
        }
    }

    // Exec after unlocking, so we can start processing another locate()
    proc_it->second.exec(death_observer_, executor_);

    return proxy;
}

bool RegistryObject::is_scope_running(std::string const& scope_id)
{
    lock_guard<decltype(mutex_)> lock(mutex_);

    auto it = scope_processes_.find(scope_id);
    if (it != scope_processes_.end())
    {
        return it->second.state() == ScopeProcess::ProcessState::Running;
    }

    throw NotFoundException("RegistryObject::is_scope_process_running(): no such scope: ",  scope_id);
}

bool RegistryObject::add_local_scope(std::string const& scope_id, ScopeMetadata const& metadata,
                                     ScopeExecData const& exec_data)
{
    if (scope_id.empty())
    {
        throw unity::InvalidArgumentException("RegistryObject::add_local_scope(): Cannot add scope with empty id");
    }
    if (scope_id.find('/') != std::string::npos)
    {
        throw unity::InvalidArgumentException("RegistryObject::add_local_scope(): Cannot create a scope with '/' in its id");
    }

    lock_guard<decltype(mutex_)> lock(mutex_);

    bool return_value = true;
    if (scopes_.find(scope_id) != scopes_.end())
    {
        scopes_.erase(scope_id);
        scope_processes_.erase(scope_id);
        return_value = false;
    }
    scopes_.insert(make_pair(scope_id, metadata));
    scope_processes_.insert(make_pair(scope_id, ScopeProcess(exec_data, publisher_)));

    if (publisher_)
    {
        // Send a blank message to subscribers to inform them that the registry has been updated
        publisher_->send_message("");
    }
    return return_value;
}

bool RegistryObject::remove_local_scope(std::string const& scope_id)
{
    // If the id is empty, it was sent as empty by the remote client.
    if (scope_id.empty())
    {
        throw unity::InvalidArgumentException("RegistryObject::remove_local_scope(): Cannot remove scope "
                                              "with empty id");
    }

    unique_lock<decltype(mutex_)> lock(mutex_);

    auto proc_it = scope_processes_.find(scope_id);
    if (proc_it != scope_processes_.end())
    {
        // Kill process after unlocking, so we can handle on_process_death
        lock.unlock();
        proc_it->second.kill();
        lock.lock();
    }

    scope_processes_.erase(scope_id);

    if (scopes_.erase(scope_id) == 1)
    {
        if (publisher_)
        {
            // Send a blank message to subscribers to inform them that the registry has been updated
            publisher_->send_message("");
        }
        return true;
    }

    return false;
}

void RegistryObject::set_remote_registry(MWRegistryProxy const& remote_registry)
{
    lock_guard<decltype(mutex_)> lock(mutex_);
    remote_registry_ = remote_registry;
}

StateReceiverObject::SPtr RegistryObject::state_receiver()
{
    return state_receiver_;
}

void RegistryObject::on_process_death(core::posix::ChildProcess const& process)
{
    lock_guard<decltype(mutex_)> lock(mutex_);

    // The death observer has signaled that a child has died.
    // Broadcast this message to each scope process until we have found the process in question.
    // (This is slightly more efficient than just connecting the signal to every scope process.)
    pid_t pid = process.pid();
    for (auto& scope_process : scope_processes_)
    {
        if (scope_process.second.on_process_death(pid))
        {
            break;
        }
    }
}

void RegistryObject::on_state_received(std::string const& scope_id, StateReceiverObject::State const& state)
{
    lock_guard<decltype(mutex_)> lock(mutex_);

    auto it = scope_processes_.find(scope_id);
    if (it != scope_processes_.end())
    {
        switch (state)
        {
            case StateReceiverObject::ScopeReady:
                it->second.update_state(ScopeProcess::ProcessState::Running);
                break;
            case StateReceiverObject::ScopeStopping:
                it->second.update_state(ScopeProcess::ProcessState::Stopping);
                break;
            default:
                std::cerr << "RegistryObject::on_state_received(): unknown state received from scope: " << scope_id;
        }
    }
    // simply ignore states from scopes the registry does not know about
}

RegistryObject::ScopeProcess::ScopeProcess(ScopeExecData exec_data, MWPublisher::SPtr publisher)
    : exec_data_(exec_data)
    , reg_publisher_(publisher)
    , manually_started_(false)
{
}

RegistryObject::ScopeProcess::ScopeProcess(ScopeProcess const& other)
    : ScopeProcess(other.exec_data_, other.reg_publisher_)
{
}

RegistryObject::ScopeProcess::~ScopeProcess()
{
    try
    {
        kill();
    }
    catch(std::exception const& e)
    {
        cerr << "RegistryObject::ScopeProcess::~ScopeProcess(): " << e.what() << endl;
    }
}

RegistryObject::ScopeProcess::ProcessState RegistryObject::ScopeProcess::state() const
{
    std::lock_guard<std::mutex> lock(process_mutex_);
    return state_;
}

void RegistryObject::ScopeProcess::update_state(ProcessState state)
{
    std::lock_guard<std::mutex> lock(process_mutex_);
    update_state_unlocked(state);
}

bool RegistryObject::ScopeProcess::wait_for_state(ProcessState state) const
{
    std::unique_lock<std::mutex> lock(process_mutex_);
    return wait_for_state(lock, state);
}

void RegistryObject::ScopeProcess::exec(
        core::posix::ChildProcess::DeathObserver& death_observer,
        Executor::SPtr executor)
{
    std::unique_lock<std::mutex> lock(process_mutex_);

    // 1. check if the scope is running.
    //  1.1. if scope already running, return.
    if (state_ == ScopeProcess::Running)
    {
        return;
    }
    //  1.2. if scope running but is “stopping”, wait for it to stop / kill it.
    else if (state_ == ScopeProcess::Stopping)
    {
        if (!wait_for_state(lock, ScopeProcess::Stopped))
        {
            cerr << "RegistryObject::ScopeProcess::exec(): Force killing process. Scope: \""
                 << exec_data_.scope_id << "\" did not stop after " << exec_data_.timeout_ms << " ms." << endl;
            try
            {
                kill(lock);
            }
            catch(std::exception const& e)
            {
                cerr << "RegistryObject::ScopeProcess::exec(): kill() failed: " << e.what() << endl;
            }
        }
    }

    // 2. exec the scope.
    update_state_unlocked(Starting);

    std::string program;
    std::vector<std::string> argv;

    // Check if this scope has specified a custom executable
    auto custom_exec_args = expand_custom_exec();
    if (!custom_exec_args.empty())
    {
        program = custom_exec_args[0];
        for (size_t i = 1; i < custom_exec_args.size(); ++i)
        {
            argv.push_back(custom_exec_args[i]);
        }
    }
    else
    {
        // No custom_exec was provided, use the default scoperunner
        program = exec_data_.scoperunner_path;
        argv.insert(argv.end(), {exec_data_.runtime_config, exec_data_.scope_config});
    }

    {
        // Copy current env vars into env.
        std::map<std::string, std::string> env;
        core::posix::this_process::env::for_each([&env](const std::string& key, const std::string& value)
        {
            env.insert(std::make_pair(key, value));
        });

        // Make sure we set LD_LIBRARY_PATH to include <lib_dir> and <lib_dir>/lib
        // before exec'ing the scope.
        auto scope_config_path = boost::filesystem::canonical(exec_data_.scope_config);
        string lib_dir = scope_config_path.parent_path().native();
        string scope_ld_lib_path = lib_dir + ":" + lib_dir + "/lib";
        string ld_lib_path = core::posix::this_process::env::get("LD_LIBRARY_PATH", "");
        if (!boost::algorithm::starts_with(ld_lib_path, lib_dir))
        {
            scope_ld_lib_path = scope_ld_lib_path + (ld_lib_path.empty() ? "" : (":" + ld_lib_path));
        }
        env["LD_LIBRARY_PATH"] = scope_ld_lib_path;  // Overwrite any LD_LIBRARY_PATH entry that may already be there.

        process_ = executor->exec(program, argv, env,
                                  core::posix::StandardStream::stdin | core::posix::StandardStream::stdout,
                                  exec_data_.confinement_profile);
        if (process_.pid() <= 0)
        {
            clear_handle_unlocked();
            throw unity::ResourceException("RegistryObject::ScopeProcess::exec(): Failed to exec scope via command: \""
                                           + exec_data_.scoperunner_path + " " + exec_data_.runtime_config + " "
                                           + exec_data_.scope_config + "\"");
        }
    }

    // 3. wait for scope to be "running".
    //  3.1. when ready, return.
    //  3.2. OR if timeout, kill process and throw.
    if (!wait_for_state(lock, ScopeProcess::Running))
    {
        try
        {
            kill(lock);
        }
        catch(std::exception const& e)
        {
            cerr << "RegistryObject::ScopeProcess::exec(): kill() failed: " << e.what() << endl;
        }
        throw unity::ResourceException("RegistryObject::ScopeProcess::exec(): exec aborted. Scope: \""
                                       + exec_data_.scope_id + "\" took longer than "
                                       + std::to_string(exec_data_.timeout_ms) + " ms to start.");
    }

    cout << "RegistryObject::ScopeProcess::exec(): Process for scope: \"" << exec_data_.scope_id << "\" started" << endl;

    // 4. add the scope process to the death observer
    death_observer.add(process_);
}

void RegistryObject::ScopeProcess::kill()
{
    std::unique_lock<std::mutex> lock(process_mutex_);
    kill(lock);
}

bool RegistryObject::ScopeProcess::on_process_death(pid_t pid)
{
    std::lock_guard<std::mutex> lock(process_mutex_);

    // check if this is the process reported to have died
    if (pid == process_.pid())
    {
        cout << "RegistryObject::ScopeProcess::on_process_death(): Process for scope: \"" << exec_data_.scope_id
             << "\" exited" << endl;
        clear_handle_unlocked();
        return true;
    }

    return false;
}

void RegistryObject::ScopeProcess::clear_handle_unlocked()
{
    process_ = core::posix::ChildProcess::invalid();
    update_state_unlocked(Stopped);
}

void RegistryObject::ScopeProcess::update_state_unlocked(ProcessState new_state)
{
    if (new_state == state_)
    {
        return;
    }
    else if (new_state == Running)
    {
        publish_state_change(true);

        if (state_ != Starting)
        {
            cout << "RegistryObject::ScopeProcess: Process for scope: \"" << exec_data_.scope_id
                 << "\" started manually" << endl;

            manually_started_ = true;
        }
    }
    else if (new_state == Stopped)
    {
        publish_state_change(false);

        if (state_ != Stopping)
        {
            cerr << "RegistryObject::ScopeProcess: Scope: \"" << exec_data_.scope_id
                 << "\" closed unexpectedly. Either the process crashed or was killed forcefully." << endl;
        }
    }
    else if (new_state == Stopping && manually_started_)
    {
        publish_state_change(false);

        cout << "RegistryObject::ScopeProcess: Manually started process for scope: \""
             << exec_data_.scope_id << "\" exited" << endl;

        new_state = Stopped;
        manually_started_ = false;
    }
    state_ = new_state;
    state_change_cond_.notify_all();
}

bool RegistryObject::ScopeProcess::wait_for_state(std::unique_lock<std::mutex>& lock, ProcessState state) const
{
    return state_change_cond_.wait_for(lock,
                                       std::chrono::milliseconds(exec_data_.timeout_ms),
                                       [this, &state]{return state_ == state;});
}

void RegistryObject::ScopeProcess::kill(std::unique_lock<std::mutex>& lock)
{
    if (state_ == Stopped)
    {
        return;
    }

    try
    {
        // first try to close the scope process gracefully
        process_.send_signal_or_throw(core::posix::Signal::sig_term);

        if (!wait_for_state(lock, ScopeProcess::Stopped))
        {
            std::error_code ec;

            cerr << "RegistryObject::ScopeProcess::kill(): Scope: \"" << exec_data_.scope_id
                 << "\" took longer than " << exec_data_.timeout_ms << " ms to exit gracefully. "
                 << "Killing the process instead." << endl;

            // scope is taking too long to close, send kill signal
            process_.send_signal(core::posix::Signal::sig_kill, ec);
        }

        // clear the process handle
        clear_handle_unlocked();
    }
    catch (std::exception const&)
    {
        cerr << "RegistryObject::ScopeProcess::kill(): Failed to kill scope: \""
             << exec_data_.scope_id << "\"" << endl;

        // clear the process handle
        // even on error, the previous handle will be unrecoverable at this point
        clear_handle_unlocked();
        throw;
    }
}

std::vector<std::string> RegistryObject::ScopeProcess::expand_custom_exec()
{
    // Check first that custom_exec has been set
    if (exec_data_.custom_exec.empty())
    {
        // custom_exec is empty, so simply return an empty vector
        return std::vector<std::string>();
    }

    wordexp_t exp;
    std::vector<std::string> command_args;

    // Split command into program and args
    if (wordexp(exec_data_.custom_exec.c_str(), &exp, WRDE_NOCMD) == 0)
    {
        util::ResourcePtr<wordexp_t*, decltype(&wordfree)> free_guard(&exp, wordfree);

        command_args.push_back(exp.we_wordv[0]);
        for (uint i = 1; i < exp.we_wordc; ++i)
        {
            std::string arg = exp.we_wordv[i];
            // Replace "%R" placeholders with the runtime config
            if (arg == "%R")
            {
                command_args.push_back(exec_data_.runtime_config);
            }
            // Replace "%S" placeholders with the scope config
            else if (arg == "%S")
            {
                command_args.push_back(exec_data_.scope_config);
            }
            // Else simply append next word as an argument
            else
            {
                command_args.push_back(arg);
            }
        }
    }
    else
    {
        // Something went wrong in parsing the command line, throw exception
        throw unity::ResourceException("RegistryObject::ScopeProcess::exec(): Invalid custom scoperunner command: \""
                                       + exec_data_.custom_exec + "\"");
    }

    return command_args;
}

void RegistryObject::ScopeProcess::publish_state_change(bool scope_started)
{
    if (scope_started)
    {
        if (reg_publisher_)
        {
            // Send a "started" message to subscribers to inform them that this scope (topic) has started
            reg_publisher_->send_message("started", exec_data_.scope_id);
        }
        if (exec_data_.debug_mode)
        {
            // If we're in debug mode, callback to the SDK via dbus (used to monitor scope lifecycle)
            std::string started_message = "dbus-send --type=method_call --dest=com.ubuntu.SDKAppLaunch "
                                          "/ScopeRegistryCallback com.ubuntu.SDKAppLaunch.ScopeLoaded "
                                          "string:" + exec_data_.scope_id + " "
                                          "uint64:" + std::to_string(process_.pid());
            if (std::system(started_message.c_str()) != 0)
            {
                std::cerr << "RegistryObject::ScopeProcess::publish_state_change(): Failed to execute SDK DBus callback" << endl;
            }
        }
    }
    else if (scope_started == false)
    {
        if (reg_publisher_)
        {
            // Send a "stopped" message to subscribers to inform them that this scope (topic) has stopped
            reg_publisher_->send_message("stopped", exec_data_.scope_id);
        }
        if (exec_data_.debug_mode)
        {
            // If we're in debug mode, callback to the SDK via dbus (used to monitor scope lifecycle)
            std::string stopped_message = "dbus-send --type=method_call --dest=com.ubuntu.SDKAppLaunch "
                                          "/ScopeRegistryCallback com.ubuntu.SDKAppLaunch.ScopeStopped "
                                          "string:" + exec_data_.scope_id;
            if (std::system(stopped_message.c_str()) != 0)
            {
                std::cerr << "RegistryObject::ScopeProcess::publish_state_change(): Failed to execute SDK DBus callback" << endl;
            }
        }
    }
}

} // namespace internal

} // namespace scopes

} // namespace unity
