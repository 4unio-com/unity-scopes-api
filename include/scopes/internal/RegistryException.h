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

#ifndef UNITY_API_SCOPES_INTERNAL_REGISTRYEXCEPTION_H
#define UNITY_API_SCOPES_INTERNAL_REGISTRYEXCEPTION_H

#include <unity/Exception.h>

namespace unity
{

namespace api
{

namespace scopes
{

namespace internal
{

class UNITY_API RegistryException : public unity::Exception
{
public:
    explicit RegistryException(std::string const& reason);
    RegistryException(RegistryException const&);
    RegistryException& operator=(RegistryException const&);
    virtual ~RegistryException() noexcept;

    virtual std::exception_ptr self() const override;
};

} // namespace internal

} // namespace scopes

} // namespace api

} // namespace unity

#endif
