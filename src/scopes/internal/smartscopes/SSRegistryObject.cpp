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
#include <unity/scopes/internal/JsonSettingsSchema.h>
#include <unity/scopes/internal/RegistryException.h>
#include <unity/scopes/internal/ScopeImpl.h>
#include <unity/scopes/internal/ScopeMetadataImpl.h>
#include <unity/scopes/internal/smartscopes/HttpClientQt.h>
#include <unity/scopes/ScopeExceptions.h>
#include <unity/UnityExceptions.h>

#include <iostream>

static const char* c_dbussend_cmd = "dbus-send /com/canonical/unity/scopes com.canonical.unity.scopes.InvalidateResults string:smart-scopes";

namespace unity
{

namespace scopes
{

namespace internal
{

namespace smartscopes
{

SSRegistryObject::SSRegistryObject(MiddlewareBase::SPtr middleware,
                                   SSConfig const& ss_config,
                                   std::string const& ss_scope_endpoint,
                                   std::string const& sss_url,
                                   bool caching_enabled)
    : ssclient_(std::make_shared<SmartScopesClient>(
                    std::make_shared<HttpClientQt>(ss_config.http_reply_timeout() * 1000),  // need millisecs
                    std::make_shared<JsonCppNode>(), sss_url))
    , refresh_stopped_(false)
    , middleware_(middleware)
    , ss_scope_endpoint_(ss_scope_endpoint)
    , regular_refresh_timeout_(ss_config.reg_refresh_rate())
    , next_refresh_timeout_(ss_config.reg_refresh_rate())
    , failed_refresh_timeout_(ss_config.reg_refresh_fail_timeout())
    , caching_enabled_(caching_enabled)
{
    // get locale_ from LANGUAGE environment variable (if not available, the default is "").
    char const* locale = getenv("LANGUAGE");
    if (locale && *locale != '\0')
    {
        // LANGUAGE is a colon separated list of locales in order of priority.
        // We are only concerned with the highest priority locale, so we split it out here.
        locale_ = std::string(locale);
        size_t colon_pos = locale_.find(':');
        if (colon_pos != std::string::npos)
        {
            locale_.resize(colon_pos);
        }
    }

    // get remote scopes then start the auto-refresh background thread
    try
    {
        get_remote_scopes();
    }
    catch (std::exception const& e)
    {
        std::cerr << "SSRegistryObject: get_remote_scopes() failed: " << e.what();
    }

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

ScopeMetadata SSRegistryObject::get_metadata(std::string const& scope_id) const
{
    // If the id is empty, it was sent as empty by the remote client.
    if (scope_id.empty())
    {
        throw unity::InvalidArgumentException("SSRegistryObject: Cannot search for scope with empty id");
    }

    std::lock_guard<std::mutex> lock(scopes_mutex_);

    auto const& it = scopes_.find(scope_id);
    if (it == scopes_.end())
    {
        throw NotFoundException("SSRegistryObject::get_metadata(): no such scope", scope_id);
    }
    return it->second;
}

MetadataMap SSRegistryObject::list() const
{
    std::lock_guard<std::mutex> lock(scopes_mutex_);
    return scopes_;
}

ObjectProxy SSRegistryObject::locate(std::string const& /*identity*/)
{
    throw internal::RegistryException("SSRegistryObject::locate(): operation not available");
}

bool SSRegistryObject::is_scope_running(std::string const&)
{
    throw internal::RegistryException("SSRegistryObject::is_scope_running(): operation not available");
}

bool SSRegistryObject::has_scope(std::string const& scope_id) const
{
    std::lock_guard<std::mutex> lock(scopes_mutex_);

    auto const& it = scopes_.find(scope_id);
    if (it == scopes_.end())
    {
        return false;
    }
    return true;
}

std::string SSRegistryObject::get_base_url(std::string const& scope_id) const
{
    std::lock_guard<std::mutex> lock(scopes_mutex_);

    auto base_url = base_urls_.find(scope_id);
    if (base_url != end(base_urls_))
    {
        return base_url->second;
    }
    else
    {
        throw NotFoundException("SSRegistryObject::get_base_url(): no such scope", scope_id);
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
        refresh_cond_.wait_for(refresh_mutex_, std::chrono::seconds(next_refresh_timeout_));

        if (!refresh_stopped_)
        {
            try
            {
                get_remote_scopes();
            }
            catch (std::exception const& e)
            {
                std::cerr << "SSRegistryObject: get_remote_scopes() failed: " << e.what();
            }
        }
    }
}

// Must be called with refresh_mutex_ locked
void SSRegistryObject::get_remote_scopes()
{
    std::vector<RemoteScope> remote_scopes;

    try
    {
        // request remote scopes from smart scopes client
        if (ssclient_->get_remote_scopes(remote_scopes, locale_, caching_enabled_))
        {
            next_refresh_timeout_ = regular_refresh_timeout_;
        }
        else
        {
            next_refresh_timeout_ = failed_refresh_timeout_;
        }
    }
    catch (std::exception const&)
    {
        // refresh again soon as get_remote_scopes failed
        next_refresh_timeout_ = failed_refresh_timeout_;
        throw;
    }

    MetadataMap new_scopes_;
    std::map<std::string, std::string> new_base_urls_;

    // loop through all available scopes and add() each visible scope
    for (RemoteScope const& scope : remote_scopes)
    {
        try
        {
            // construct a ScopeMetadata with remote scope info
            std::unique_ptr<ScopeMetadataImpl> metadata(new ScopeMetadataImpl(nullptr));

            metadata->set_scope_id(scope.id);
            metadata->set_display_name(scope.name);
            metadata->set_description(scope.description);
            metadata->set_author(scope.author);

            if (scope.icon)
            {
                metadata->set_icon(*scope.icon);
            }

            if (scope.art)
            {
                metadata->set_art(*scope.art);
            }

            if (scope.appearance)
            {
                metadata->set_appearance_attributes(*scope.appearance);
            }

            if (scope.settings)
            {
                try
                {
                    auto schema = JsonSettingsSchema::create(*scope.settings);
                    metadata->set_settings_definitions(schema->definitions());
                }
                catch (ResourceException const& e)
                {
                    std::cerr << e.what() << std::endl;
                    std::cerr << "SSRegistryObject: ignoring invalid settings JSON for scope \"" << scope.id << "\"" << std::endl;
                }
            }

            if (scope.needs_location_data)
            {
                metadata->set_location_data_needed(*scope.needs_location_data);
            }

            metadata->set_invisible(scope.invisible);

            ScopeProxy proxy = ScopeImpl::create(middleware_->create_scope_proxy(scope.id, ss_scope_endpoint_),
                                                 middleware_->runtime(),
                                                 scope.id);

            metadata->set_proxy(proxy);

            auto meta = ScopeMetadataImpl::create(move(metadata));

            // add scope info to collection
            add(scope, std::move(meta), new_scopes_, new_base_urls_);
        }
        catch (std::exception const& e)
        {
            std::cerr << e.what() << std::endl;
            std::cerr << "SSRegistryObject: skipping scope \"" << scope.id << "\"" << std::endl;
        }
    }

    bool changed = false;
    // replace previous scopes with new ones
    {
        std::lock_guard<std::mutex> lock(scopes_mutex_);

        // check if base urls or list of available scopes has changed.
        // the urls is a map of (string, string), so rely on == operator.
        // when comparing scopes maps, only check if scope ids are same, i.e.
        // changes to scope metadata attributes are not detected.
        if (new_base_urls_ != base_urls_ ||
            new_scopes_.size() != scopes_.size())
        {
            changed = true;
        }
        else
        {
            for (auto const& p: new_scopes_)
            {
                if (scopes_.find(p.first) == scopes_.end())
                {
                    changed = true;
                    break;
                }
            }
        }

        // replace current collection of remote scopes
        base_urls_ = new_base_urls_;
        scopes_ = new_scopes_;
    }

    if (changed)
    {
        // something has changed, send invalidate signal
        int result = system(c_dbussend_cmd);
        (void)result;
    }
}

bool SSRegistryObject::add(RemoteScope const& remotedata, ScopeMetadata const& metadata, MetadataMap& scopes, std::map<std::string, std::string>& urls)
{
    if (metadata.scope_id().empty())
    {
        throw unity::InvalidArgumentException("SSRegistryObject: Cannot add scope with empty id");
    }

    // store the base url under a scope ID key
    urls[metadata.scope_id()] = remotedata.base_url;

    // store the scope metadata in scopes_
    auto const& pair = scopes.insert(make_pair(metadata.scope_id(), metadata));
    if (!pair.second)
    {
        // Replace already existing entry with this one
        scopes.erase(pair.first);
        scopes.insert(make_pair(metadata.scope_id(), metadata));
        return false;
    }
    return true;
}

}  // namespace smartscopes

}  // namespace internal

}  // namespace scopes

}  // namespace unity
