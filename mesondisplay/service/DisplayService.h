/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#pragma once

#include "DisplayAdapter.h"
#include <json/json.h>
#include <vendor/amlogic/display/meson_display_ipc/1.0/IMesonDisplayIPC.h>
#include <stdbool.h>
#include <pthread.h>
#include "utile.h"
#include <stack>

//Limite the server recursion.
const int RECURSION_LIMIT  = 10;

namespace meson {
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_handle;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::vendor::amlogic::display::meson_display_ipc::V1_0::IMesonDisplayIPC;
using ::android::hardware::hidl_string;
using ::android::hardware::Return;
using namespace std;

template <typename T>
class SafeStack {
public:
    SafeStack() {
        int ret = pthread_mutex_init(&mutex, NULL);
        if (ret)
            DEBUG_INFO("mutex init with error!");
    };
    ~SafeStack() {
        pthread_mutex_destroy(&mutex);
    };
    void push(const T e) {
        pthread_mutex_lock(&mutex);
        m_stack.push(e);
        pthread_mutex_unlock(&mutex);
    };
    void pop() {
        pthread_mutex_lock(&mutex);
        if (!m_stack.empty())
            m_stack.pop();
        pthread_mutex_unlock(&mutex);
    };
    size_t size() {
        size_t size;
        pthread_mutex_lock(&mutex);
        size = m_stack.size();
        pthread_mutex_unlock(&mutex);
        return size;
    };
    bool empty() {
        bool ret;
        pthread_mutex_lock(&mutex);
        ret = m_stack.empty();
        pthread_mutex_unlock(&mutex);
        return ret;
    };
    void top(T& t) {
        pthread_mutex_lock(&mutex);
        t = m_stack.top();
        pthread_mutex_unlock(&mutex);
    };
private:
   pthread_mutex_t mutex;
   std::stack<T> m_stack;
};

class MesonIpcServer : public IMesonDisplayIPC{
public:
    MesonIpcServer();
    virtual ~MesonIpcServer() = default;
    // Methods from ::android::hidl::base::V1_0::IBase follow.
    Return<void> debug(const hidl_handle &fd, const hidl_vec<hidl_string> &args) override;

    // Methods from ::vendor::amlogic::display::meson_display_ipc::V1_0::IMesonDisplayIPC follow.
    Return<void> send_msg_wait_reply(const hidl_string& msg_in, send_msg_wait_reply_cb _hidl_cb) override;
    Return<void> send_msg(const hidl_string& msg_in) override;
    virtual void  message_handle(Json::Value& in, Json::Value& out);
    virtual Return<void> captureDisplayScreen(const int32_t displayId,
            const int32_t layerId, captureDisplayScreen_cb hidl_cb) override;

private:
    bool check_recursion_record_and_push(const std::string& str);
    SafeStack<std::string> recursion_record;
    DISALLOW_COPY_AND_ASSIGN(MesonIpcServer);
};


class DisplayServer: public MesonIpcServer {
public:
    void message_handle(Json::Value& in, Json::Value& out) override;
    DisplayServer(std::unique_ptr<DisplayAdapter>& adapter);
    DisplayServer() = default;
    Return<void> debug(const hidl_handle &fd, const hidl_vec<hidl_string> &args) override;
    Return<void> captureDisplayScreen(const int32_t displayId,
            const int32_t layerId, captureDisplayScreen_cb hidl_cb) override;

private:
    std::unique_ptr<DisplayAdapter> adapter;
    DISALLOW_COPY_AND_ASSIGN(DisplayServer);
};

} //namespace meson
