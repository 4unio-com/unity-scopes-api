/*
 * Copyright (C) 2013 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Michi Henning <michi.henning@canonical.com>
 */

#include <unity/api/scopes/internal/ConfigBase.h>

#include <unity/api/scopes/ScopeExceptions.h>
#include <unity/util/IniParser.h>

#include <sstream>

using namespace std;

namespace unity
{

namespace api
{

namespace scopes
{

namespace internal
{

ConfigBase::ConfigBase(string const& configfile) :
    parser_(make_shared<util::IniParser>(configfile.c_str())),
    configfile_(configfile)
{
}

ConfigBase::~ConfigBase() noexcept
{
}

shared_ptr<util::IniParser> ConfigBase::parser() const noexcept
{
    return parser_;
}

void ConfigBase::throw_ex(string const& reason) const
{
    ostringstream s;
    s << "\"" << configfile_ << "\": " << reason;
    throw ConfigException(s.str());
}

} // namespace internal

} // namespace scopes

} // namespace api

} // namespace unity