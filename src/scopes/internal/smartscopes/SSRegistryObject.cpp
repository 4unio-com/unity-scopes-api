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

#include <unity/scopes/internal/smartscopes/SSRegistryObject.h>

#include <unity/scopes/internal/JsonCppNode.h>
#include <unity/scopes/internal/RegistryException.h>
#include <unity/scopes/internal/ScopeImpl.h>
#include <unity/scopes/internal/ScopeMetadataImpl.h>
#include <unity/scopes/internal/smartscopes/HttpClientQt.h>
#include <unity/scopes/ScopeExceptions.h>
#include <unity/UnityExceptions.h>

namespace unity
{

namespace scopes
{

namespace internal
{

namespace smartscopes
{

SSRegistryObject::SSRegistryObject(MiddlewareBase::SPtr middleware,
                                   std::string const& ss_scope_endpoint,
                                   uint max_http_sessions,
                                   uint no_reply_timeout,
                                   uint refresh_rate_in_min)
    : ssclient_(std::make_shared<SmartScopesClient>(
                    std::make_shared<HttpClientQt>(max_http_sessions, no_reply_timeout),
                    std::make_shared<JsonCppNode>()))
    , refresh_stopped_(false)
    , middleware_(middleware)
    , ss_scope_endpoint_(ss_scope_endpoint)
    , regular_refresh_timeout_(refresh_rate_in_min)
    , next_refresh_timeout_(refresh_rate_in_min)
{
    // get remote scopes then start the auto-refresh background thread
    get_remote_scopes();
    refresh_thread_ = std::thread(&SSRegistryObject::refresh_thread, this);
}

SSRegistryObject::~SSRegistryObject() noexcept
{
    // stop the refresh thread
    {
        std::lock_guard<std::mutex> lock(refresh_mutex_);

        refresh_stopped_ = true;
        refresh_cond_.notify_all();
    }

    refresh_thread_.join();
}

ScopeMetadata SSRegistryObject::get_metadata(std::string const& scope_name)
{
    // If the name is empty, it was sent as empty by the remote client.
    if (scope_name.empty())
    {
        throw unity::InvalidArgumentException("SSRegistryObject: Cannot search for scope with empty name");
    }

    std::lock_guard<std::mutex> lock(scopes_mutex_);

    auto const& it = scopes_.find(scope_name);
    if (it == scopes_.end())
    {
        throw NotFoundException("SSRegistryObject::get_metadata(): no such scope", scope_name);
    }
    return it->second;
}

MetadataMap SSRegistryObject::list()
{
    std::lock_guard<std::mutex> lock(scopes_mutex_);
    return scopes_;
}

ScopeProxy SSRegistryObject::locate(std::string const& /*scope_name*/)
{
    throw internal::RegistryException("SSRegistryObject::locate(): operation not available");
}

bool SSRegistryObject::has_scope(std::string const& scope_name) const
{
    std::lock_guard<std::mutex> lock(scopes_mutex_);

    auto const& it = scopes_.find(scope_name);
    if (it == scopes_.end())
    {
        return false;
    }
    return true;
}

std::string SSRegistryObject::get_base_url(std::string const& scope_name) const
{
    std::lock_guard<std::mutex> lock(scopes_mutex_);

    auto base_url = base_urls_.find(scope_name);
    if (base_url != end(base_urls_))
    {
        return base_url->second;
    }
    else
    {
        throw NotFoundException("SSRegistryObject::get_base_url(): no such scope", scope_name);
    }
}

SmartScopesClient::SPtr SSRegistryObject::get_ssclient() const
{
    return ssclient_;
}

void SSRegistryObject::refresh_thread()
{
    std::lock_guard<std::mutex> lock(refresh_mutex_);

    while (!refresh_stopped_)
    {
        refresh_cond_.wait_for(refresh_mutex_, std::chrono::minutes(next_refresh_timeout_));

        if (!refresh_stopped_)
        {
            get_remote_scopes();
        }
    }
}

void SSRegistryObject::get_remote_scopes()
{
    std::vector<RemoteScope> remote_scopes;

    try
    {
        // request remote scopes from smart scopes client
        remote_scopes = ssclient_->get_remote_scopes();
    }
    catch (unity::Exception const&)
    {
        // refresh again soon as get_remote_scopes failed
        next_refresh_timeout_ = 1;  ///! TODO config?
        return;
    }

    std::lock_guard<std::mutex> lock(scopes_mutex_);

    // clear current collection of remote scopes
    base_urls_.clear();
    scopes_.clear();

    // loop through all available scopes and add() each visible scope
    for (RemoteScope const& scope : remote_scopes)
    {
        if (scope.invisible)
        {
            continue;
        }

        // construct a ScopeMetadata with remote scope info
        std::unique_ptr<ScopeMetadataImpl> metadata(new ScopeMetadataImpl(nullptr));

        metadata->set_scope_name(scope.name);
        metadata->set_display_name(scope.name);
        metadata->set_description(scope.description);

        ScopeProxy proxy = ScopeImpl::create(middleware_->create_scope_proxy(scope.name, ss_scope_endpoint_),
                                             middleware_->runtime(),
                                             scope.name);

        metadata->set_proxy(proxy);

        auto meta = ScopeMetadataImpl::create(move(metadata));

        // add scope info to collection
        add(scope.name, std::move(meta), scope);
    }

    next_refresh_timeout_ = regular_refresh_timeout_;
}

bool SSRegistryObject::add(std::string const& scope_name, ScopeMetadata const& metadata, RemoteScope const& remotedata)
{
    if (scope_name.empty())
    {
        throw unity::InvalidArgumentException("SSRegistryObject: Cannot add scope with empty name");
    }

    // store the base url under a scope name key
    base_urls_[scope_name] = remotedata.base_url;

    // store the scope metadata in scopes_
    auto const& pair = scopes_.insert(make_pair(scope_name, metadata));
    if (!pair.second)
    {
        // Replace already existing entry with this one
        scopes_.erase(pair.first);
        scopes_.insert(make_pair(scope_name, metadata));
        return false;
    }
    return true;
}

}  // namespace smartscopes

}  // namespace internal

}  // namespace scopes

}  // namespace unity