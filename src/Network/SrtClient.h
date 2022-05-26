/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef NETWORK_SRTCLIENT_H
#define NETWORK_SRTCLIENT_H

#include <memory>
#include "Socket.h"

namespace toolkit {

//Tcp客户端，Socket对象默认开始互斥锁
class SrtClient : public std::enable_shared_from_this<SrtClient>, public SocketHelper {
public:
    using Ptr = std::shared_ptr<SrtClient>;
    SrtClient(const EventPoller::Ptr &poller = nullptr);
    ~SrtClient() override;

    /**
     * 开始连接tcp服务器
     * @param url 服务器ip或域名
     * @param port 服务器端口
     * @param timeout_sec 超时时间,单位秒
     * @param local_port 本地端口
     */
    virtual void startConnect(const std::string &url, uint16_t port, float timeout_sec = 5, uint16_t local_port = 0);
        
    /**
     * 主动断开连接
     * @param ex 触发onErr事件时的参数
     */
    void shutdown(const SockException &ex = SockException(Err_shutdown, "self shutdown")) override;

    /**
     * 连接中或已连接返回true，断开连接时返回false
     */
    virtual bool alive() const;

    /**
     * 设置网卡适配器,使用该网卡与服务器通信
     * @param local_ip 本地网卡ip
     */
    virtual void setNetAdapter(const std::string &local_ip);

protected:
    /**
     * 连接服务器结果回调
     * @param ex 成功与否
     */
    virtual void onConnect(const SockException &ex) = 0;

    /**
     * 收到数据回调
     * @param buf 接收到的数据(该buffer会重复使用)
     */
    virtual void onRecv(const Buffer::Ptr &buf) = 0;

    /**
     * 数据全部发送完毕后回调
     */
    virtual void onFlush() {}

    /**
     * 被动断开连接回调
     * @param ex 断开原因
     */
    virtual void onErr(const SockException &ex) = 0;

    /**
     * tcp连接成功后每2秒触发一次该事件
     */
    virtual void onManager() {}

private:
    void onSockConnect(const SockException &ex);

private:
    std::string _net_adapter = "::";
    // onManager 定时器
    std::shared_ptr<Timer> _timer;
    //对象个数统计
    ObjectStatistic<SrtClient> _statistic;
};


//用于实现TLS客户端的模板对象
template<typename TcpClientType>
class SrtClientImpl : public TcpClientType {
public:
    using Ptr = std::shared_ptr<SrtClientImpl>;

    template<typename ...ArgsType>
    SrtClientImpl(ArgsType &&...args) :TcpClientType(std::forward<ArgsType>(args)...) {}

    ~SrtClientImpl() override {
    }

    void onRecv(const Buffer::Ptr& buf) override {
        TcpClientType::onRecv(buf);
    }

    ssize_t send(Buffer::Ptr buf) override {
        return TcpClientType::send(std::move(buf));
    }

    void startConnect(const std::string& url, uint16_t port, float timeout_sec = 5, uint16_t local_port = 0) override {
        TcpClientType::startConnect(url, port, timeout_sec, local_port);
    }
protected:
    void onConnect(const SockException& ex) override {
        TcpClientType::onConnect(ex);
    }
};

} /* namespace toolkit */
#endif /* NETWORK_SRTCLIENT_H */
