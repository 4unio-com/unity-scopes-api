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
 * Authored by: Pawel Stolowski <pawel.stolowski@canonical.com>
*/

#include <unity/scopes/Department.h>
#include <unity/scopes/internal/DepartmentImpl.h>
#include <unity/scopes/CannedQuery.h>

namespace unity
{

namespace scopes
{

Department::Department(internal::DepartmentImpl *impl)
    : p(impl)
{
}

/// @cond
Department::Department(Department const& other)
    : p(new internal::DepartmentImpl(*(other.p)))
{
}

Department::Department(Department&&) = default;

Department::~Department() = default;

Department& Department::operator=(Department const& other)
{
    if (this != &other)
    {
        p.reset(new internal::DepartmentImpl(*(other.p)));
    }
    return *this;
}

Department& Department::operator=(Department&&) = default;

Department::UPtr Department::create(CannedQuery const& query, std::string const& label)
{
    return std::unique_ptr<Department>(new Department(new internal::DepartmentImpl(query, label)));
}

Department::UPtr Department::create(std::string const& department_id, CannedQuery const& query, std::string const& label)
{
    return std::unique_ptr<Department>(new Department(new internal::DepartmentImpl(department_id, query, label)));
}

VariantMap Department::serialize() const
{
    return p->serialize();
}

/// @endcond

void Department::set_has_subdepartments()
{
    p->set_has_subdepartments();
}

void Department::set_subdepartments(DepartmentList const& departments)
{
    p->set_subdepartments(departments);
}

void Department::set_alternate_label(std::string const& label)
{
    p->set_alternate_label(label);
}

std::string Department::id() const
{
    return p->id();
}

std::string Department::label() const
{
    return p->label();
}

std::string Department::alternate_label() const
{
    return p->alternate_label();
}

CannedQuery Department::query() const
{
    return p->query();
}

bool Department::has_subdepartments() const
{
    return p->has_subdepartments();
}

DepartmentList Department::subdepartments() const
{
    return p->subdepartments();
}

} // namespace scopes

} // namespace unity
