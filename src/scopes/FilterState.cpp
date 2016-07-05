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

#include <unity/scopes/FilterState.h>
#include <unity/UnityExceptions.h>
#include <unity/scopes/internal/FilterStateImpl.h>

namespace unity
{

namespace scopes
{

FilterState::FilterState()
    : p(new internal::FilterStateImpl())
{
}

/// @cond

FilterState::FilterState(FilterState const& other)
    : p(new internal::FilterStateImpl(*(other.p)))
{
}

FilterState::FilterState(internal::FilterStateImpl *pimpl)
    : p(pimpl)
{
}

FilterState::~FilterState() = default;

FilterState::FilterState(FilterState &&) = default;
FilterState& FilterState::operator=(FilterState &&) = default;

FilterState& FilterState::operator=(FilterState const& other)
{
    if (this != &other)
    {
        p.reset(new internal::FilterStateImpl(*(other.p)));
    }
    return *this;
}

VariantMap FilterState::serialize() const
{
    return p->serialize();
}

FilterState FilterState::deserialize(VariantMap const& var) {
    return internal::FilterStateImpl::deserialize(var);
}

/// @endcond

bool FilterState::has_filter(std::string const& id) const
{
    return p->has_filter(id);
}

void FilterState::remove(std::string const& id)
{
    p->remove(id);
}

bool FilterState::has_filters() const
{
    return p->has_filters();
}

} // namespace scopes

} // namespace unity
