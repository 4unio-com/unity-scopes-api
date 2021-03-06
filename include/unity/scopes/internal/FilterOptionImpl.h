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

#pragma once

#include <string>

namespace unity
{

namespace scopes
{

namespace internal
{

class FilterOptionImpl
{
public:
    FilterOptionImpl(std::string const& id, std::string const& label);
    std::string id() const;
    std::string label() const;
    void set_default_value(bool default_value);
    bool default_value() const;

private:
    std::string id_;
    std::string label_;
    bool default_value_;
};

} // namespace internal

} // namespace scopes

} // namespace unity
