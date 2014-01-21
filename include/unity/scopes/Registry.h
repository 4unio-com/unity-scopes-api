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

#ifndef UNITY_SCOPES_REGISTRY_H
#define UNITY_SCOPES_REGISTRY_H

#include <unity/scopes/ObjectProxy.h>
#include <unity/scopes/RegistryProxyFwd.h>
#include <unity/scopes/ScopeMetadata.h>

#include <map>

namespace unity
{

namespace scopes
{

namespace internal
{
class RegistryImpl;
}

/**
\brief Map for scope name and metadata pairs.
*/
typedef std::map<std::string, ScopeMetadata> MetadataMap;

/**
\brief RegistryProxy provides access to the available scopes.
You can obtain a proxy to the registry by calling Runtime::registry().
*/

class UNITY_API Registry : public virtual ObjectProxy
{
public:
    /// @cond
    virtual ~Registry() noexcept;
    /// @endcond

    /**
    \brief Returns the metadata for the scope with the given name.
    @return The metadata for the scope. If no scope with the given name exists, get_metadata() throws NotFoundException.
    */
    ScopeMetadata get_metadata(std::string const& scope_name) const;

    /**
    \brief Returns a map containing the metadata for all scopes.
    */
    MetadataMap list() const;

    /**
    \brief Returns a map containing only those scopes for which predicate returns true.
    \param predicate a function object the must return true for each metadata item to be include in the map.
    \return The metadata items for which the predicate returned true.
    */
    MetadataMap list_if(std::function<bool(ScopeMetadata const& item)> predicate) const;

protected:
    Registry(internal::RegistryImpl* impl);          // Instantiated only by RegistryImpl
    friend class internal::RegistryImpl;

private:
    internal::RegistryImpl* fwd() const;
};

} // namespace scopes

} // namespace unity

#endif