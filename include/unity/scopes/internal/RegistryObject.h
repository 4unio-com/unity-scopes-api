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

#ifndef UNITY_SCOPES_INTERNAL_REGISTRYOBJECT_H
#define UNITY_SCOPES_INTERNAL_REGISTRYOBJECT_H

#include <unity/scopes/internal/Executor.h>
#include <unity/scopes/internal/MiddlewareBase.h>
#include <unity/scopes/internal/MWPublisher.h>
#include <unity/scopes/internal/MWRegistryProxyFwd.h>
#include <unity/scopes/internal/RegistryObjectBase.h>
#include <unity/scopes/internal/StateReceiverObject.h>

#include <condition_variable>
#include <mutex>
#include <thread>

namespace unity
{

namespace scopes
{

namespace internal
{

class RegistryObject : public RegistryObjectBase
{
public:
    struct ScopeExecData
    {
        ScopeExecData() = default;
        ScopeExecData(std::initializer_list<std::string>) = delete;
        std::string scope_id;
        std::string scoperunner_path;
        std::string runtime_config;
        std::string scope_config;
        std::string confinement_profile;
        int timeout_ms;
    };

public:
    UNITY_DEFINES_PTRS(RegistryObject);

    RegistryObject(core::posix::ChildProcess::DeathObserver& death_observer, Executor::SPtr const& executor,
                   MiddlewareBase::SPtr middleware);
    virtual ~RegistryObject();

    // Remote operation implementations
    virtual ScopeMetadata get_metadata(std::string const& scope_id) const override;
    virtual MetadataMap list() const override;
    virtual ObjectProxy locate(std::string const& identity) override;
    virtual bool is_scope_running(std::string const& scope_id) override;

    // Local methods
    bool add_local_scope(std::string const& scope_id, ScopeMetadata const& scope,
                         ScopeExecData const& scope_exec_data);
    bool remove_local_scope(std::string const& scope_id);
    void set_remote_registry(MWRegistryProxy const& remote_registry);

    StateReceiverObject::SPtr state_receiver();

private:
    void on_process_death(core::posix::Process const& process);
    void on_state_received(std::string const& scope_id, StateReceiverObject::State const& state);

    class ScopeProcess
    {
    public:
        enum ProcessState
        {
            Stopped, Starting, Running, Stopping
        };

        ScopeProcess(ScopeExecData exec_data);
        ScopeProcess(ScopeProcess const& other);
        ~ScopeProcess();

        ProcessState state() const;
        void update_state(ProcessState state);
        bool wait_for_state(ProcessState state) const;

        void exec(core::posix::ChildProcess::DeathObserver& death_observer,
                  Executor::SPtr executor);
        void kill();

        bool on_process_death(pid_t pid);

    private:
        // the following methods must be called with process_mutex_ locked
        void clear_handle_unlocked();
        void update_state_unlocked(ProcessState state);

        bool wait_for_state(std::unique_lock<std::mutex>& lock, ProcessState state) const;
        void kill(std::unique_lock<std::mutex>& lock);

    private:
        const ScopeExecData exec_data_;
        ProcessState state_ = Stopped;
        mutable std::mutex process_mutex_;
        mutable std::condition_variable state_change_cond_;
        core::posix::ChildProcess process_ = core::posix::ChildProcess::invalid();
    };

private:
    core::posix::ChildProcess::DeathObserver& death_observer_;
    core::ScopedConnection death_observer_connection_;

    StateReceiverObject::SPtr state_receiver_;
    core::ScopedConnection state_receiver_connection_;

    Executor::SPtr executor_;

    MetadataMap scopes_;
    typedef std::map<std::string, ScopeProcess> ProcessMap;
    ProcessMap scope_processes_;
    MWRegistryProxy remote_registry_;
    mutable std::mutex mutex_;

    MWPublisher::UPtr publisher_;
};

} // namespace internal

} // namespace scopes

} // namespace unity

#endif
