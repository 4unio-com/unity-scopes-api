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
 * Authored by: Pawel Stolowski <pawel.stolowski@canonical.com>
 */

#ifndef UNITY_SCOPES_ACTIVATIONRESPONSE_H
#define UNITY_SCOPES_ACTIVATIONRESPONSE_H

#include <unity/scopes/Variant.h>
#include <unity/scopes/CannedQuery.h>
#include <memory>

namespace unity
{

namespace scopes
{

namespace internal
{
class ActivationResponseImpl;
}

/**
\brief Carries response to a Result activation request.
*/
class ActivationResponse final
{
public:
    /*!
     \brief Status of an unity::scopes::ScopeBase::activate or unity::scopes::ScopeBase::perform_action request.
     */
    enum Status
    {
        NotHandled,  /**< Activation of this result wasn't handled by the scope */
        ShowDash,    /**< Activation of this result was handled, show the Dash */
        HideDash,    /**< Activation of this result was handled, hide the Dash */
        ShowPreview, /**< Preview should be requested for this result */
        PerformQuery /**< Perform new search. This state is implied if creating ActivationResponse with CannedQuery object and is invalid otherwise */
    };

    /**
    \brief Creates ActivationResponse with given status.
    Throws unity::InvalidArgumentException if status is Status::PerformQuery - to
    create ActivationResponse of that type, use ActivationResponse(CannedQuery const&)
    constructor.
    \param status The activation status.
    */
    ActivationResponse(Status status);

    /**
    \brief Creates ActivationResponse with activation status of Status::PerformQuery and a search query to be executed.
    \param query The search query to be executed by client.
     */
    ActivationResponse(CannedQuery const& query);

    /// @cond
    ~ActivationResponse();
    ActivationResponse(ActivationResponse const& other);
    ActivationResponse(ActivationResponse&& other);
    ActivationResponse& operator=(ActivationResponse const& other);
    ActivationResponse& operator=(ActivationResponse&& other);
    /// @endcond

    /**
    \brief Get activation status.
    \return The activation status.
    */
    ActivationResponse::Status status() const;

    /**
     \brief Attach arbitrary data to this response.

     The attached data will be sent back to the scope if status of this response is Status::ShowPreview.
     \param data arbitrary value attached to response
     */
    void set_scope_data(Variant const& data);

    /**
     \brief Get data attached to this response object.
     \return The data attached to response.
     */
    Variant scope_data() const;

    /**
     \brief CannedQuery to be executed if status is Status::PerformQuery.

     This method throws unity::LogicException is status of this ActivationResponse is different than Status::PerformQuery.
     \return The query to be executed by the client.
    */
    CannedQuery query() const;

    /// @cond
    VariantMap serialize() const;
    /// @endcond

private:
    std::unique_ptr<internal::ActivationResponseImpl> p;
    ActivationResponse(internal::ActivationResponseImpl* pimpl);
    friend class internal::ActivationResponseImpl;
};

} // namespace scopes

} // namespace unity

#endif
