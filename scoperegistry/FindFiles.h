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

#ifndef SCOPEREGISTRY_FINDFILES_H
#define SCOPEREGISTRY_FINDFILES_H

#include <vector>
#include <string>

namespace scoperegistry
{

// Return a vector of file names underneath a scope root install dir that have the given suffix.
// Files are searched for exactly "one level down", that is, if we have a directory structure.
//
// canonical/scopeA/myconfig.ini
// canonical/someScope/someScope.ini
//
// we get those two .ini files, but no .ini files in canonical or underneath
// further-nested directories.

std::vector<std::string> find_scope_config_files(std::string const& install_dir, std::string const& suffix);

// Return a vector of file names in dir with the given suffix.

std::vector<std::string> find_files(std::string const& dir, std::string const& suffix);

} // namespace scoperegistry

#endif
