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

#ifndef UNITY_INTERNAL_ACTIVATIONRESPONSEIMPL_H
#define UNITY_INTERNAL_ACTIVATIONRESPONSEIMPL_H

#include <unity/scopes/ActivationResponse.h>

namespace unity
{

namespace scopes
{

namespace internal
{

class ActivationResponseImpl final
{
public:
    ActivationResponseImpl(ActivationResponse::Status status);
    ActivationResponseImpl(VariantMap const& var);
    ~ActivationResponseImpl() = default;

    ActivationResponseImpl(ActivationResponseImpl const& other) = default;
    ActivationResponseImpl(ActivationResponseImpl&& other) = default;
    ActivationResponseImpl& operator=(ActivationResponseImpl const& other) = default;
    ActivationResponseImpl& operator=(ActivationResponseImpl && other) = default;

    ActivationResponse::Status status() const;
    void setHints(VariantMap const& hints);
    VariantMap hints() const;

    VariantMap serialize() const;

    static ActivationResponse create(VariantMap const& var);

private:
    ActivationResponse::Status status_;
    VariantMap hints_;
};

} // namespace internal

} // namespace scopes

} // namespace unity

#endif
