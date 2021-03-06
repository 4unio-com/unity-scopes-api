/*
 * Copyright (C) 2015 Canonical Ltd
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
 * Authored by: Xavi Garcia <xavi.garcia.mena@canonical.com>
 */

#include <unity/scopes/qt/QScopeBaseAPI.h>
#include <unity/scopes/qt/QSearchQueryBaseAPI.h>
#include <unity/scopes/qt/internal/QScopeBaseAPIImpl.h>

#include <sstream>
#include <fstream>

namespace sc = unity::scopes;
using namespace std;
using namespace unity::scopes::qt;

/// @cond
QScopeBaseAPI::QScopeBaseAPI(FactoryFunc const& creator)
    : p(new internal::QScopeBaseAPIImpl(creator))
{
}

void QScopeBaseAPI::start(string const& start_args)
{
    p->start(start_args);
}

void QScopeBaseAPI::stop()
{
    p->stop();
}

sc::SearchQueryBase::UPtr QScopeBaseAPI::search(const sc::CannedQuery& query, const sc::SearchMetadata& metadata)
{
    // Boilerplate construction of Query
    return p->search(query, metadata);
}

sc::PreviewQueryBase::UPtr QScopeBaseAPI::preview(sc::Result const& result, sc::ActionMetadata const& metadata)
{
    // Boilerplate construction of Preview
    return p->preview(result, metadata);
}
/// @endcond
