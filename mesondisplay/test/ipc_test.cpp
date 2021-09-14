/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "DisplayService.h"
#include "DisplayClient.h"
#include <sys/wait.h>
#include <unistd.h>
#include <hidl/HidlTransportSupport.h>
#include <binder/ProcessState.h>
#include <binder/IPCThreadState.h>
#include <utils/RefBase.h>
#include <binder/IServiceManager.h>
#include <utils/String16.h>

class ipcserver: public meson::MesonIpcServer {
    public:
        void message_handle(Json::Value& in, Json::Value& out) {
            UNUSED(in);
            UNUSED(out);
            DEBUG_INFO("Get message:%s", meson::JsonValue2String(in).c_str());
        };
        ipcserver() {
            DEBUG_INFO("ipcserver created");
            if (registerAsService() != android::OK) {
                DEBUG_INFO("RegisterAsServer failed(%d)!", registerAsService());
            }
        };
        virtual ~ipcserver() = default;
};

void print_help(const char* cmd) {
    printf("Useage:\n %s server : start a ipc server \n%s : start a ipc client test.", cmd, cmd);
};

int main(int argc, char* argv[]) {
    UNUSED(argv);
    print_help(argv[0]);
    if (argc == 1) {
        //IPC client
        Json::Value a = "test";
        const char* json = "{ \"property\" : \"value\" }";
        bool ok = meson::String2JsonValue(json, a);
        if (!ok) {
            DEBUG_INFO("format error!");
        }

        auto client = &meson::DisplayClient::getInstance();
        while (true) {
            DEBUG_INFO("begin");
            DEBUG_INFO("Send=>%s", meson::JsonValue2String(a).c_str());
            sleep(3);
            client->send_request(a);
            sleep(3);
        }
    } else {
        //IPC Server
        android::ProcessState::initWithDriver("/dev/vndbinder");
        android::hardware::configureRpcThreadpool(16, false);

        static ipcserver* m_server = new ipcserver();
        UNUSED(m_server);
        android::IPCThreadState::self()->joinThreadPool();
    }
    return 0;
};
