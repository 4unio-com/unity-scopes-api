/* Copyright (C) 2013 Canonical Ltd
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

#include <scopes/internal/AnnotationObjectBaseImpl.h>

namespace unity
{

namespace api
{

namespace scopes
{

namespace internal
{

AnnotationObjectBaseImpl::AnnotationObjectBaseImpl(PlacementHint const& placement)
    : placement_hint_(placement)
{
}

AnnotationObjectBaseImpl::~AnnotationObjectBaseImpl()
{
}

PlacementHint AnnotationObjectBaseImpl::placement_hint() const
{
    return placement_hint_;
}

VariantMap AnnotationObjectBaseImpl::serialize() const
{
    VariantMap vm;
    vm["type"] = type_string();
    vm["placement"] = placement_hint_.serialize();
    serialize(vm);
    return vm;
}

} // namespace internal

} // namespace scopes

} // namespace api

} // namespace unity
