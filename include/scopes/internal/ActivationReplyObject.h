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

#ifndef UNITY_INTERNAL_ACTIVATIONREPLYOBJECT_H
#define UNITY_INTERNAL_ACTIVATIONREPLYOBJECT_H

#include <scopes/internal/ReplyObject.h>

namespace unity
{

namespace api
{

namespace scopes
{

namespace internal
{

class ActivationReplyObject : public ReplyObject
{
public:
    ActivationReplyObject(ActivationListener::SPtr const& receiver, RuntimeImpl const* runtime, std::string const& scope_name);
    virtual void process_data(VariantMap const& data) override;

private:
    ActivationListener::SPtr const receiver_;
};

} // namespace internal

} // namespace scopes

} // namespace api

} // namespace unity

#endif
