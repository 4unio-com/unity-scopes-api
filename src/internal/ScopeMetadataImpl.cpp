/*
 * Copyright (C) 2013 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the Lesser GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the Lesser GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Michi Henning <michi.henning@canonical.com>
 */

#include <scopes/internal/ScopeMetadataImpl.h>

#include <scopes/internal/MiddlewareBase.h>
#include <scopes/internal/ScopeImpl.h>
#include <scopes/ScopeExceptions.h>
#include <unity/UnityExceptions.h>

using namespace std;
using namespace unity;

namespace unity
{

namespace api
{

namespace scopes
{

namespace internal
{

ScopeMetadataImpl::ScopeMetadataImpl(MiddlewareBase* mw) :
    mw_(mw)
{
}

ScopeMetadataImpl::ScopeMetadataImpl(const VariantMap& variant_map, MiddlewareBase* mw) :
    mw_(mw)
{
    deserialize(variant_map);
}

ScopeMetadataImpl::ScopeMetadataImpl(ScopeMetadataImpl const& other) :
    scope_name_(other.scope_name_),
    proxy_(other.proxy_),
    display_name_(other.display_name_),
    description_(other.description_)
{
    if (other.art_)
    {
        art_.reset(new string(*other.art_));
    }
    if (other.icon_)
    {
        icon_.reset(new string(*other.icon_));
    }
    if (other.search_hint_)
    {
        search_hint_.reset(new string(*other.search_hint_));
    }
    if (other.hot_key_)
    {
        hot_key_.reset(new string(*other.hot_key_));
    }
}

ScopeMetadataImpl& ScopeMetadataImpl::operator=(ScopeMetadataImpl const& rhs)
{
    if (this != &rhs)
    {
        scope_name_ = rhs.scope_name_;
        proxy_ = rhs.proxy_;
        display_name_ = rhs.display_name_;
        description_ = rhs.description_;
        art_.reset(rhs.art_ ? new string(*rhs.art_) : nullptr);
        icon_.reset(rhs.icon_ ? new string(*rhs.icon_) : nullptr);
        search_hint_.reset(rhs.search_hint_ ? new string(*rhs.search_hint_) : nullptr);
        hot_key_.reset(rhs.hot_key_ ? new string(*rhs.hot_key_) : nullptr);
    }
    return *this;
}

std::string ScopeMetadataImpl::scope_name() const
{
    return scope_name_;
}

ScopeProxy ScopeMetadataImpl::proxy() const
{
    return proxy_;
}

std::string ScopeMetadataImpl::display_name() const
{
    return display_name_;
}

std::string ScopeMetadataImpl::description() const
{
    return description_;
}

std::string ScopeMetadataImpl::art() const
{
    if (art_)
    {
        return *art_;
    }
    throw NotFoundException("attribute not set", "art");
}

std::string ScopeMetadataImpl::icon() const
{
    if (icon_)
    {
        return *icon_;
    }
    throw NotFoundException("attribute not set", "icon");
}

std::string ScopeMetadataImpl::search_hint() const
{
    if (search_hint_)
    {
        return *search_hint_;
    }
    throw NotFoundException("attribute not set", "search_hint");
}

std::string ScopeMetadataImpl::hot_key() const
{
    if (hot_key_)
    {
        return *hot_key_;
    }
    throw NotFoundException("attribute not set", "hot_key");
}

void ScopeMetadataImpl::set_scope_name(std::string const& scope_name)
{
    scope_name_ = scope_name;
}

void ScopeMetadataImpl::set_proxy(ScopeProxy const& proxy)
{
    proxy_ = proxy;
}

void ScopeMetadataImpl::set_display_name(std::string const& display_name)
{
    display_name_ = display_name;
}

void ScopeMetadataImpl::set_description(std::string const& description)
{
    description_ = description;
}

void ScopeMetadataImpl::set_art(std::string const& art)
{
    art_.reset(new string(art));
}

void ScopeMetadataImpl::set_icon(std::string const& icon)
{
    icon_.reset(new string(icon));
}

void ScopeMetadataImpl::set_search_hint(std::string const& search_hint)
{
    search_hint_.reset(new string(search_hint));
}

void ScopeMetadataImpl::set_hot_key(std::string const& hot_key)
{
    hot_key_.reset(new string(hot_key));
}

namespace
{

void throw_on_empty(std::string const& name, std::string const& value)
{
    if (value.empty())
    {
        throw InvalidArgumentException("ScopeMetadata: required attribute '" + name + "' is empty");
    }
}

} // namespace

VariantMap ScopeMetadataImpl::serialize() const
{
    throw_on_empty("scope_name", scope_name_);
    if (!proxy_)
    {
        throw InvalidArgumentException("ScopeMetadataImpl::serialize(): required attribute 'proxy' is null");
    }
    throw_on_empty("display_name", display_name_);
    throw_on_empty("description", description_);

    VariantMap var;
    var["scope_name"] = scope_name_;
    VariantMap proxy;
    proxy["identity"] = proxy_->identity();
    proxy["endpoint"] = proxy_->endpoint();
    var["proxy"] = proxy;
    var["display_name"] = display_name_;
    var["description"] = description_;

    // Optional fields
    if (art_)
    {
        var["art"] = *art_;
    }
    if (icon_)
    {
        var["icon"] = *icon_;
    }
    if (search_hint_)
    {
        var["search_hint"] = *search_hint_;
    }
    if (hot_key_)
    {
        var["hot_key"] = *hot_key_;
    }

    return var;
}

namespace
{

VariantMap::const_iterator find_or_throw(VariantMap const& var, std::string const& name)
{
    auto it = var.find(name);
    if (it == var.end())
    {
        throw InvalidArgumentException("ScopeMetadata::deserialize(): required attribute '" + name + "' is missing");
    }
    return it;
}

} // namespace

void ScopeMetadataImpl::deserialize(VariantMap const& var)
{
    auto it = find_or_throw(var, "scope_name");
    scope_name_ = it->second.get_string();
    throw_on_empty("scope_name", scope_name_);

    it = find_or_throw(var, "proxy");
    auto proxy = it->second.get_dict();
    auto it2 = proxy.find("identity");
    if (it2 == proxy.end())
    {
        throw InvalidArgumentException("ScopeMetadataImpl::deserialize(): missing 'proxy.identity'");
    }
    auto identity = it2->second.get_string();
    throw_on_empty("proxy.identity", identity);
    it2 = proxy.find("endpoint");
    if (it2 == proxy.end())
    {
        throw InvalidArgumentException("ScopeMetadataImpl::deserialize(): missing 'proxy.endpoint'");
    }
    auto endpoint = it2->second.get_string();
    throw_on_empty("proxy.endpoint", endpoint);
    auto mw_proxy = mw_->create_scope_proxy(identity, endpoint);
    proxy_ = ScopeImpl::create(mw_proxy, mw_->runtime());

    it = find_or_throw(var, "display_name");
    display_name_ = it->second.get_string();

    it = find_or_throw(var, "description");
    description_ = it->second.get_string();

    // Optional fields

    it = var.find("art");
    if (it != var.end())
    {
        art_.reset(new string(it->second.get_string()));
    }

    it = var.find("icon");
    if (it != var.end())
    {
        icon_.reset(new string(it->second.get_string()));
    }

    it = var.find("search_hint");
    if (it != var.end())
    {
        search_hint_.reset(new string(it->second.get_string()));
    }

    it = var.find("hot_key");
    if (it != var.end())
    {
        hot_key_.reset(new string(it->second.get_string()));
    }
}

ScopeMetadata ScopeMetadataImpl::create(unique_ptr<ScopeMetadataImpl> impl)
{
    return ScopeMetadata(move(impl));
}

} // namespace internal

} // namespace scopes

} // namespace api

} // namespace unity