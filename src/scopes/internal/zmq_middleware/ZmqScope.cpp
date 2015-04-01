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

#include <unity/scopes/internal/zmq_middleware/ZmqScope.h>

#include <unity/scopes/internal/QueryCtrlImpl.h>
#include <unity/scopes/internal/RuntimeImpl.h>
#include <scopes/internal/zmq_middleware/capnproto/Scope.capnp.h>
#include <unity/scopes/internal/zmq_middleware/VariantConverter.h>
#include <unity/scopes/internal/zmq_middleware/ZmqException.h>
#include <unity/scopes/internal/zmq_middleware/ZmqQueryCtrl.h>
#include <unity/scopes/internal/zmq_middleware/ZmqReceiver.h>
#include <unity/scopes/internal/zmq_middleware/ZmqReply.h>
#include <unity/scopes/Result.h>
#include <unity/scopes/CannedQuery.h>

using namespace std;

namespace unity
{

namespace scopes
{

namespace internal
{

namespace zmq_middleware
{

/*

dictionary<string, Value> ValueDict;

interface QueryCtrl;
interface Reply;

interface Scope
{
    QueryCtrl* search(string query, ValueDict hints, Reply* replyProxy);
    QueryCtrl* activate(string query, ValueDict hints, Reply* replyProxy);
    QueryCtrl* perform_action(string query, ValueDict hints, string action_id, Reply* replyProxy);
    QueryCtrl* preview(string query, ValueDict hints, Reply* replyProxy);
    ChildScopeList child_scopes();
    bool set_child_scopes(ChildScopeList const& child_scopes);
    bool debug_mode();
};

*/

ZmqScope::ZmqScope(ZmqMiddleware* mw_base,
                   string const& endpoint,
                   string const& identity,
                   string const& category,
                   int64_t timeout) :
    MWObjectProxy(mw_base),
    ZmqObjectProxy(mw_base, endpoint, identity, category, RequestMode::Twoway, timeout),
    MWScope(mw_base)
{
}

ZmqScope::~ZmqScope()
{
}

QueryCtrlProxy ZmqScope::search(CannedQuery const& query,
                                VariantMap const& hints,
                                VariantMap const& context,
                                MWReplyProxy const& reply)
{
    capnp::MallocMessageBuilder request_builder;
    auto reply_proxy = dynamic_pointer_cast<ZmqReply>(reply);
    {
        auto request = make_request_(request_builder, "search");
        auto in_params = request.initInParams().getAs<capnproto::Scope::CreateQueryRequest>();
        auto q = in_params.initQuery();
        to_value_dict(query.serialize(), q);
        auto h = in_params.initHints();
        to_value_dict(hints, h);
        auto p = in_params.initReplyProxy();
        p.setEndpoint(reply_proxy->endpoint().c_str());
        p.setIdentity(reply_proxy->identity().c_str());
        p.setCategory(reply_proxy->target_category().c_str());
        auto d = in_params.initContext();
        to_value_dict(context, d);
    }

    auto future = mw_base()->twoway_pool()->submit([&] { return this->invoke_scope_(request_builder); });

    auto out_params = future.get();
    auto response = out_params.reader->getRoot<capnproto::Response>();
    throw_if_runtime_exception(response);

    auto proxy = response.getPayload().getAs<capnproto::Scope::CreateQueryResponse>().getReturnValue();
    ZmqQueryCtrlProxy p(new ZmqQueryCtrl(mw_base(),
                                         proxy.getEndpoint().cStr(),
                                         proxy.getIdentity().cStr(),
                                         proxy.getCategory().cStr()));
    return make_shared<QueryCtrlImpl>(p, reply_proxy);
}

QueryCtrlProxy ZmqScope::activate(VariantMap const& result, VariantMap const& hints, MWReplyProxy const& reply)
{
    capnp::MallocMessageBuilder request_builder;
    auto reply_proxy = dynamic_pointer_cast<ZmqReply>(reply);
    {
        auto request = make_request_(request_builder, "activate");
        auto in_params = request.initInParams().getAs<capnproto::Scope::ActivationRequest>();
        auto res = in_params.initResult();
        to_value_dict(result, res);
        auto h = in_params.initHints();
        to_value_dict(hints, h);
        auto p = in_params.initReplyProxy();
        p.setEndpoint(reply_proxy->endpoint().c_str());
        p.setIdentity(reply_proxy->identity().c_str());
    }

    auto future = mw_base()->twoway_pool()->submit([&] { return this->invoke_scope_(request_builder); });

    auto out_params = future.get();
    auto response = out_params.reader->getRoot<capnproto::Response>();
    throw_if_runtime_exception(response);

    auto proxy = response.getPayload().getAs<capnproto::Scope::CreateQueryResponse>().getReturnValue();
    ZmqQueryCtrlProxy p(new ZmqQueryCtrl(mw_base(),
                                         proxy.getEndpoint().cStr(),
                                         proxy.getIdentity().cStr(),
                                         proxy.getCategory().cStr()));
    return make_shared<QueryCtrlImpl>(p, reply_proxy);
}

QueryCtrlProxy ZmqScope::perform_action(VariantMap const& result,
        VariantMap const& hints, std::string const& widget_id, std::string const& action_id, MWReplyProxy const& reply)
{
    capnp::MallocMessageBuilder request_builder;
    auto reply_proxy = dynamic_pointer_cast<ZmqReply>(reply);
    {
        auto request = make_request_(request_builder, "perform_action");
        auto in_params = request.initInParams().getAs<capnproto::Scope::ActionActivationRequest>();
        auto res = in_params.initResult();
        to_value_dict(result, res);
        auto h = in_params.initHints();
        to_value_dict(hints, h);
        in_params.setWidgetId(widget_id);
        in_params.setActionId(action_id);
        auto p = in_params.initReplyProxy();
        p.setEndpoint(reply_proxy->endpoint().c_str());
        p.setIdentity(reply_proxy->identity().c_str());
    }

    auto future = mw_base()->twoway_pool()->submit([&] { return this->invoke_scope_(request_builder); });

    auto out_params = future.get();
    auto response = out_params.reader->getRoot<capnproto::Response>();
    throw_if_runtime_exception(response);

    auto proxy = response.getPayload().getAs<capnproto::Scope::CreateQueryResponse>().getReturnValue();
    ZmqQueryCtrlProxy p(new ZmqQueryCtrl(mw_base(),
                                         proxy.getEndpoint().cStr(),
                                         proxy.getIdentity().cStr(),
                                         proxy.getCategory().cStr()));
    return make_shared<QueryCtrlImpl>(p, reply_proxy);
}

QueryCtrlProxy ZmqScope::preview(VariantMap const& result, VariantMap const& hints, MWReplyProxy const& reply)
{
    capnp::MallocMessageBuilder request_builder;
    auto reply_proxy = dynamic_pointer_cast<ZmqReply>(reply);
    {
        auto request = make_request_(request_builder, "preview");
        auto in_params = request.initInParams().getAs<capnproto::Scope::PreviewRequest>();
        auto res = in_params.initResult();
        to_value_dict(result, res);
        auto h = in_params.initHints();
        to_value_dict(hints, h);
        auto p = in_params.initReplyProxy();
        p.setEndpoint(reply_proxy->endpoint().c_str());
        p.setIdentity(reply_proxy->identity().c_str());
    }

    auto future = mw_base()->twoway_pool()->submit([&] { return this->invoke_scope_(request_builder); });

    auto out_params = future.get();
    auto response = out_params.reader->getRoot<capnproto::Response>();
    throw_if_runtime_exception(response);

    auto proxy = response.getPayload().getAs<capnproto::Scope::CreateQueryResponse>().getReturnValue();
    ZmqQueryCtrlProxy p(new ZmqQueryCtrl(mw_base(),
                                         proxy.getEndpoint().cStr(),
                                         proxy.getIdentity().cStr(),
                                         proxy.getCategory().cStr()));
    return make_shared<QueryCtrlImpl>(p, reply_proxy);
}

ChildScopeList ZmqScope::child_scopes()
{
    capnp::MallocMessageBuilder request_builder;
    make_request_(request_builder, "child_scopes");

    auto future = mw_base()->twoway_pool()->submit([&] { return this->invoke_scope_(request_builder); });

    auto out_params = future.get();
    auto response = out_params.reader->getRoot<capnproto::Response>();
    throw_if_runtime_exception(response);

    auto list = response.getPayload().getAs<capnproto::Scope::ChildScopesResponse>().getReturnValue();

    ChildScopeList child_scope_list;
    for (size_t i = 0; i < list.size(); ++i)
    {
        string id = list[i].getId();
        bool enabled = list[i].getEnabled();

        set<string> keywords;
        auto keywords_capn = list[i].getKeywords();
        for (auto const& kw : keywords_capn)
        {
            keywords.emplace(kw);
        }

        ///!child_scope_list.push_back( ChildScope{id, enabled, keywords} );
    }
    return child_scope_list;
}

bool ZmqScope::set_child_scopes(ChildScopeList const& child_scopes)
{
    capnp::MallocMessageBuilder request_builder;
    auto request = make_request_(request_builder, "set_child_scopes");

    auto in_params = request.initInParams().getAs<capnproto::Scope::SetChildScopesRequest>();
    auto list = in_params.initChildScopes(child_scopes.size());

    for (size_t i = 0; i < child_scopes.size(); ++i)
    {
        list[i].setId(child_scopes[i].id);
        list[i].setEnabled(child_scopes[i].enabled);

        auto keywords = list[i].initKeywords(child_scopes[i].keywords.size());
        int j = 0;
        for (auto const& kw : child_scopes[i].keywords)
        {
            keywords.set(j++, kw);
        }
    }

    auto future = mw_base()->twoway_pool()->submit([&] { return this->invoke_scope_(request_builder); });
    auto out_params = future.get();
    auto r = out_params.reader->getRoot<capnproto::Response>();
    throw_if_runtime_exception(r);

    auto response = r.getPayload().getAs<capnproto::Scope::SetChildScopesResponse>();
    return response.getReturnValue();
}

bool ZmqScope::debug_mode()
{
    lock_guard<std::mutex> lock(debug_mode_mutex_);

    // We only need to retrieve the debug mode state once, so we cache it in debug_mode_
    if (!debug_mode_)
    {
        capnp::MallocMessageBuilder request_builder;
        make_request_(request_builder, "debug_mode");

        auto future = mw_base()->twoway_pool()->submit([&] { return this->invoke_twoway_(request_builder); });
        auto out_params = future.get();
        auto response = out_params.reader->getRoot<capnproto::Response>();
        throw_if_runtime_exception(response);

        auto debug_mode_response = response.getPayload().getAs<capnproto::Scope::DebugModeResponse>();
        debug_mode_.reset(new bool(debug_mode_response.getReturnValue()));
    }

    return *debug_mode_;
}

ZmqObjectProxy::TwowayOutParams ZmqScope::invoke_scope_(capnp::MessageBuilder& in_params)
{
    // Check if this scope has requested debug mode, if so, disable two-way timeout and set
    // locate timeout to 15s.
    if (debug_mode())
    {
        return this->invoke_twoway_(in_params, -1, 15000);
    }
    return this->invoke_twoway_(in_params);
}

} // namespace zmq_middleware

} // namespace internal

} // namespace scopes

} // namespace unity
