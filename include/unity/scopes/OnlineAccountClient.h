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

#ifndef UNITY_SCOPES_ONLINEACCOUNTCLIENT_H
#define UNITY_SCOPES_ONLINEACCOUNTCLIENT_H

#include <unity/scopes/CannedQuery.h>
#include <unity/scopes/PreviewWidget.h>
#include <unity/scopes/Result.h>
#include <unity/util/NonCopyable.h>

#include <memory>

namespace unity
{

namespace scopes
{

namespace internal
{
class OnlineAccountClientImpl;
}

/**
\brief A simple interface for integrating online accounts access and monitoring into scopes.

Each instantiation of this class targets a particular account service as specified on construction.
*/

class OnlineAccountClient final
{
public:
    /// @cond
    NONCOPYABLE(OnlineAccountClient);
    /// @endcond

    /**
    \brief A container for details about a service's status and authorization parameters.
    */
    struct ServiceStatus
    {
        uint account_id;            ///< A unique ID of the online account parenting this service.
        bool service_enabled;       ///< True if this service is enabled.
        std::string client_id;      ///< "ConsumerKey" / "ClientId" OAuth (1 / 2) parameter.
        std::string client_secret;  ///< "ClientSecret" / "ConsumerSecret" OAuth (1 / 2) parameter.
        std::string access_token;   ///< "AccessToken" OAuth parameter.
        std::string token_secret;   ///< "TokenSecret" OAuth parameter.
        std::string error;          ///< Error message (empty if no error occurred).
    };

    /**
    \brief Indicates whether an external main loop already exists, or one should be created internally.

    A running main loop is essential in order to receive service update callbacks. When in doubt, set
    RunInExternalMainLoop and just use refresh_service_statuses() and get_service_statuses() to obtain
    service statuses.
    */
    enum MainLoopSelect
    {
        RunInExternalMainLoop,  ///< An external main loop already exists or the service update
                                ///  callback is not required.
        CreateInternalMainLoop  ///< An external main loop does not exist and the service update
                                ///  callback is required.
    };

    /**
    \brief Create OnlineAccountClient for the specified account service.
    \param service_name The name of the service (E.g. "com.ubuntu.scopes.youtube_youtube").
    \param service_type The type of service (E.g. "sharing").
    \param provider_name The name of the service provider (E.g. "google").
    \param main_loop_select Indicates whether or not an external main loop exists
                            (see OnlineAccountClient::MainLoopSelect).
    */
    OnlineAccountClient(std::string const& service_name,
                        std::string const& service_type,
                        std::string const& provider_name,
                        MainLoopSelect main_loop_select = RunInExternalMainLoop);

    /// @cond
    ~OnlineAccountClient();
    /// @endcond

    /**
    \brief Function signature for the service update callback.
    \see set_service_update_callback
    */
    typedef std::function<void(ServiceStatus const&)> ServiceUpdateCallback;

    /**
    \brief Set the callback function to be invoked when a service status changes.
    \param callback The external callback function.
    */
    void set_service_update_callback(ServiceUpdateCallback callback);

    /**
    \brief Refresh all service statuses.

    <b>WARNING</b>: If a service update callback is set, this method will invoke that callback for each
    service monitored. Therefore, DO NOT call this method from within your callback function!
    */
    void refresh_service_statuses();

    /**
    \brief Get statuses for all services matching the name, type and provider specified on
           construction.
    \return A list of service statuses.
    */
    std::vector<ServiceStatus> get_service_statuses();

    /**
    \brief Indicates what action to take when the login process completes.
    */
    enum PostLoginAction
    {
        Unknown,                                ///< An action unknown to the run-time was used.
        DoNothing,                              ///< Simply return to the scope with no further action.
        InvalidateResults,                      ///< Invalidate the scope results.
        ContinueActivation,                     ///< Continue with regular result / widget activation.
        LastActionCode_ = ContinueActivation    ///< Dummy end marker.
    };

    /**
    \brief Register a result that requires the user to be logged in.
    \param result The result item that requires account access.
    \param query The scope's current query.
    \param login_passed_action The action to take upon a successful login.
    \param login_failed_action The action to take upon an unsuccessful login.
    */
    void register_account_login_item(Result& result,
                                     CannedQuery const& query,
                                     PostLoginAction login_passed_action,
                                     PostLoginAction login_failed_action);

    /**
    \brief Register a widget that requires the user to be logged in.
    \param widget The widget item that requires account access.
    \param login_passed_action The action to take upon a successful login.
    \param login_failed_action The action to take upon an unsuccessful login.
    */
    void register_account_login_item(PreviewWidget& widget,
                                     PostLoginAction login_passed_action,
                                     PostLoginAction login_failed_action);

private:
    std::unique_ptr<internal::OnlineAccountClientImpl> p;
};

} // namespace scopes

} // namespace unity

#endif
