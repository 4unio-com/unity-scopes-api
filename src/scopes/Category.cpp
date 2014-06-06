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

#include <unity/scopes/Category.h>
#include <unity/scopes/internal/CategoryImpl.h>

namespace unity
{

namespace scopes
{

//! @cond

Category::Category(std::string const& id, std::string const& title, std::string const &icon, CategoryRenderer const& renderer_template)
    : p(new internal::CategoryImpl(id, title, icon, renderer_template))
{
}

Category::Category(VariantMap const& variant_map)
    : p(new internal::CategoryImpl(variant_map))
{
}

Category::Category(internal::CategoryImpl *impl)
    : p(impl)
{
}

Category::~Category()
{
}

std::string Category::id() const
{
    return p->id();
}

std::string Category::icon() const
{
    return p->icon();
}

CategoryRenderer const& Category::renderer_template() const
{
    return p->renderer_template();
}

std::string Category::title() const
{
    return p->title();
}

Category::TapBehavior Category::tap_behavior() const
{
    return p->tap_behavior();
}

Category::TapBehavior Category::long_press_behavior() const
{
    return p->long_press_behavior();
}

VariantMap Category::serialize() const
{
    return p->serialize();
}

//! @endcond

} // namespace scopes

} // namespace unity
