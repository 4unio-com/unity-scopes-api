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
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */

#include <unity/scopes/internal/smartscopes/HttpClientNetCpp.h>

#include <unity/UnityExceptions.h>

#include <core/net/http/client.h>
#include <core/net/http/request.h>
#include <core/net/http/response.h>
#include <core/net/http/status.h>

#include <iostream>
#include <unordered_map>

namespace net = core::net;
namespace http = core::net::http;

using namespace unity::scopes::internal::smartscopes;

namespace
{
struct Cancelable
{
    Cancelable() = default;
    Cancelable(const Cancelable&) = delete;
    Cancelable& operator=(const Cancelable&) = delete;

    void cancel()
    {
        cancelled.store(true);
    }

    bool is_cancelled() const
    {
        return cancelled.load();
    }

    std::atomic<bool> cancelled{false};
};

struct CancellationRegistry
{
    static CancellationRegistry& instance()
    {
        static CancellationRegistry cr;
        return cr;
    }

    std::pair<uint, std::shared_ptr<Cancelable>> add()
    {
        auto cancelable = std::make_shared<Cancelable>();
        auto id = add(cancelable);

        return std::make_pair(id, cancelable);
    }

    uint add(const std::shared_ptr<Cancelable>& cancelable)
    {
        std::lock_guard<std::mutex> lg(guard);

        uint result{++request_id};

        store.insert(std::make_pair(result, cancelable));

        return result;
    }

    void cancel_and_remove_for_id(uint id)
    {
        std::lock_guard<std::mutex> lg(guard);

        auto it = store.find(id);

        if (it != store.end())
        {
            it->second->cancel();
            store.erase(it);
        }
    }

    uint request_id{0};
    std::mutex guard;
    std::unordered_map<uint, std::shared_ptr<Cancelable>> store;
};
}

HttpClientNetCpp::HttpClientNetCpp(uint no_reply_timeout)
    : no_reply_timeout{no_reply_timeout},
      client{http::make_client()},
      worker([this]() { client->run(); })
{
}

HttpClientNetCpp::~HttpClientNetCpp()
{
    client->stop();

    if (worker.joinable())
        worker.join();
}

HttpResponseHandle::SPtr HttpClientNetCpp::get(std::string const& request_url)
{
    auto request = client->get(http::Request::Configuration::from_uri_as_string(request_url));

    request->set_timeout(std::chrono::milliseconds{no_reply_timeout});

    auto promise = std::make_shared<std::promise<std::string>>();
    std::shared_future<std::string> future(promise->get_future());

    auto id_and_cancelable = CancellationRegistry::instance().add();

    request->async_execute(
                http::Request::Handler()
                    .on_progress([id_and_cancelable](const http::Request::Progress&)
                    {
                        return id_and_cancelable.second->is_cancelled() ?
                                    http::Request::Progress::Next::abort_operation :
                                    http::Request::Progress::Next::continue_operation;
                    })
                    .on_response([promise, request](const http::Response& response)
                    {
                        if (response.status != http::Status::ok)
                        {
                            std::ostringstream msg;
                            msg << "HTTP request failed with: " << response.status << std::endl << response.body;
                            unity::ResourceException e(msg.str());

                            promise->set_exception(std::make_exception_ptr(e));
                        } else
                        {
                            promise->set_value(response.body);
                        }
                    })
                    .on_error([promise, request](const net::Error& e)
                    {
                        unity::ResourceException re(e.what());
                        promise->set_exception(std::make_exception_ptr(re));
                    }));

    return std::make_shared<HttpResponseHandle>(
                shared_from_this(),
                id_and_cancelable.first,
                future);
}

void HttpClientNetCpp::cancel_get(uint id)
{
    CancellationRegistry::instance().cancel_and_remove_for_id(id);
}

std::string HttpClientNetCpp::to_percent_encoding(std::string const& string)
{
    return client->url_escape(string);
}
