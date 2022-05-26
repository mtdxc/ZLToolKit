/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "SrtServer.h"
#include "Util/uv_errno.h"
#include "Util/onceToken.h"

using namespace std;

namespace toolkit {

INSTANCE_IMP(SessionMap)
StatisticImp(SrtServer)

SrtServer::SrtServer(const EventPoller::Ptr &poller) : Server(poller) {
    setOnCreateSocket(nullptr);
    _socket = createSocket(_poller);
    _socket->setOnBeforeAccept([this](const EventPoller::Ptr &poller) {
        return onBeforeAcceptConnection(poller);
    });
    _socket->setOnAccept([this](Socket::Ptr &sock, shared_ptr<void> &complete) {
        auto ptr = sock->getPoller().get();
        auto server = this;
        //ptr->async([server, sock, complete]() {
            //该tcp客户端派发给对应线程的SrtServer服务器
            server->onAcceptConnection(sock);
        //});
    });
}

SrtServer::~SrtServer() {
    InfoL << "close tcp server [" << _socket->get_local_ip() << "]:" << _socket->get_local_port();
    _timer.reset();
    //先关闭socket监听，防止收到新的连接
    _socket.reset();
    _session_map.clear();
}

uint16_t SrtServer::getPort() {
    if (!_socket) {
        return 0;
    }
    return _socket->get_local_port();
}

void SrtServer::setOnCreateSocket(Socket::onCreateSocket cb) {
    if (!cb) {
        cb = [](const EventPoller::Ptr &poller) {
            return Socket::createSocket(poller, false);
        };
    }
    _on_create_socket = std::move(cb);
}

Socket::Ptr SrtServer::onBeforeAcceptConnection(const EventPoller::Ptr &poller) {
    assert(_poller->isCurrentThread());
    // 此处改成自定义获取poller对象，防止负载不均衡
    return createSocket(EventPollerPool::Instance().getPoller(false));
}

// 接收到客户端连接请求
void SrtServer::onAcceptConnection(const Socket::Ptr &sock) {
    // assert(_poller->isCurrentThread());
    Ptr self = std::dynamic_pointer_cast<SrtServer>(shared_from_this());
    weak_ptr<SrtServer> weak_self = self;
    //创建一个TcpSession;这里实现创建不同的服务会话实例
    auto helper = _session_alloc(self, sock);
    auto session = helper->session();
    //把本服务器配置传递给TcpSession
    session->attachServer(*this);
    // @todo add lock
    _session_map.emplace(helper.get(), helper);

    weak_ptr<Session> weak_session = session;
    //会话接收数据事件
    sock->setOnRead([weak_session](const Buffer::Ptr &buf, struct sockaddr *, int) {
        //获取会话强应用
        auto strong_session = weak_session.lock();
        if (!strong_session) {
            return;
        }
        try {
            strong_session->onRecv(buf);
        } catch (SockException &ex) {
            strong_session->shutdown(ex);
        } catch (exception &ex) {
            strong_session->shutdown(SockException(Err_shutdown, ex.what()));
        }
    });

    SessionHelper *ptr = helper.get();
    //会话接收到错误事件
    sock->setOnErr([weak_self, weak_session, ptr](const SockException &err) {
        // 获取会话强应用
        auto strong_session = weak_session.lock();
        if (strong_session) {
            // 触发onError事件回调
            try {strong_session->onError(err);}
            catch(...){}
        }

        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }

        strong_self->_poller->async([weak_self, ptr]() {
            auto strong_self = weak_self.lock();
            if (strong_self) {
                strong_self->_session_map.erase(ptr);
            }
        }, false);
    });
}

void SrtServer::start_l(uint16_t port, const std::string &host, uint32_t backlog) {
    if (!_socket->listen(port, host.c_str(), backlog, true)) {
        // 创建tcp监听失败，可能是由于端口占用或权限问题
        throw std::runtime_error(StrPrinter << "listen on " << host << ":" << port << " failed:" << get_uv_errmsg(true));
    }

    startMangerTimer();

    InfoL << "TCP Server listening on [" << host << "]:" << port;
}

void SrtServer::startMangerTimer()
{
    // 新建一个定时器定时管理这些tcp会话
    weak_ptr<SrtServer> weak_self = std::dynamic_pointer_cast<SrtServer>(shared_from_this());
    _timer = std::make_shared<Timer>(2.0f, [weak_self]() -> bool {
        auto strong_self = weak_self.lock();
        if (strong_self) {
            strong_self->onManagerSession();
            return true;
        }
        else {
            return false;
        }
    }, _poller);
}

void SrtServer::onManagerSession() {
    assert(_poller->isCurrentThread());
    for (auto &pr : _session_map) {
        //遍历时，可能触发onErr事件(也会操作_session_map)        
        auto session = pr.second->session();
        session->async([session]() {
            try {
                session->onManager();
            }
            catch (exception& ex) {
                WarnL << ex.what();
            }
        });
    }
}

Socket::Ptr SrtServer::createSocket(const EventPoller::Ptr &poller) {
    return _on_create_socket(poller);
}

} /* namespace toolkit */

