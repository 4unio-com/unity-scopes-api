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
 * Authored by: Marcus Tomlinson <marcus.tomlinson@canonical.com>
 */

#ifndef UNITY_SCOPES_INTERNAL_SMARTSCOPES_HTTPCLIENTINTERFACE_H
#define UNITY_SCOPES_INTERNAL_SMARTSCOPES_HTTPCLIENTINTERFACE_H

#include <unity/util/DefinesPtrs.h>
#include <unity/util/NonCopyable.h>

#include <future>

namespace unity
{

namespace scopes
{

namespace internal
{

namespace smartscopes
{

class HttpResponseHandle;

class HttpClientInterface : public std::enable_shared_from_this<HttpClientInterface>
{
public:
    NONCOPYABLE(HttpClientInterface);
    UNITY_DEFINES_PTRS(HttpClientInterface);

    HttpClientInterface() = default;
    virtual ~HttpClientInterface() = default;

    virtual std::shared_ptr<HttpResponseHandle> get(std::string const& request_url, uint port) = 0;

    virtual std::string to_percent_encoding(std::string const& string) = 0;

private:
    friend class HttpResponseHandle;

    virtual void cancel_get(uint session_id) = 0;
};

class HttpResponseHandle
{
public:
    NONCOPYABLE(HttpResponseHandle);
    UNITY_DEFINES_PTRS(HttpResponseHandle);

    HttpResponseHandle(HttpClientInterface::SPtr client, uint session_id, std::shared_future<std::string> future)
        : client_(client)
        , session_id_(session_id)
        , future_(future)
    {
    }

    ~HttpResponseHandle()
    {
        cancel();
    }

    void wait()
    {
        future_.wait();
    }

    std::string get()
    {
        return future_.get();
    }

    void cancel()
    {
        client_->cancel_get(session_id_);
    }

private:
    std::shared_ptr<HttpClientInterface> client_;
    uint session_id_;
    std::shared_future<std::string> future_;
};

}  // namespace smartscopes

}  // namespace internal

}  // namespace scopes

}  // namespace unity

#endif  // UNITY_SCOPES_INTERNAL_SMARTSCOPES_HTTPCLIENTINTERFACE_H
