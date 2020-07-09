/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <pthread.h>
#include <sys/time.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>

#include "DisplayAdapter.h"

using meson::DisplayAdapter;
using std::unique_ptr;

static const char* short_option = "";
static const struct option long_option[] = {
    {"list-modes", no_argument, 0, 'l'},
    {"chang-mode", required_argument, 0, 'c'},
    {"get-property", required_argument, 0, 'g'},
    {"set-property", required_argument, 0, 's'},
    {"raw-cmd", required_argument, 0, 'r'},
    {"G", required_argument, 0, 'G'},
    {"S", required_argument, 0, 'S'},
    {"dump-display-attribute", no_argument, 0, 'd'},
    {0, 0, 0, 0}
};

static void print_usage(const char* name) {
    printf("Usage: %s [-lcrgs]\n"
            "Get or change the mode setting of the weston drm output.\n"
            "Options:\n"
            "       -l,--list-modes        \tlist connector support modes\n"
            "       -c,--change-mode MODE  \tchange connector current mode, MODE format like:%%dx%%d@%%d width,height,refresh\n"
            "       -d,--dump-display-attribute \tdump all display attribute\n"
            "       -g,--get-display-attribute  \"ATTRI_NAME\"\tget display attribute\n"
            "       -s,--set-display-attribute  \"ATTRI_NAME\"=value\tset display attribute\n"
            "       -G \"[ui-rect|display-mode]\"\tget [logic ui rect|display mode]\n"
            "       -S \"[ui-rect]\"\tset [logic ui rect]\n"
            "                               \t eg: \"Content Protection\" 1\n"
            "       -r,--raw-cmd           \tsend raw cmd\n", name);
}


int main(int argc, char* argv[]) {
    std::vector<meson::DisplayModeInfo> displayModeList;
    if (argc == 1) {
        print_usage(argv[0]);
        return 0;
    }
#ifndef RECOVERY_MODE
    unique_ptr<DisplayAdapter> client = meson::DisplayAdapterCreateRemote();
    DEBUG_INFO("Start client");
#else
    unique_ptr<DisplayAdapter> client = meson::DisplayAdapterCreateLocal(meson::DisplayAdapter::BackendType::DISPLAY_TYPE_FBDEV);
    DEBUG_INFO("Start recovery client");
#endif
    DisplayAdapter::ConnectorType type = DisplayAdapter::CONN_TYPE_HDMI;


    int opt;
    while ((opt = getopt_long_only(argc, argv, short_option, long_option, NULL)) != -1) {
        switch (opt) {
            case 'l':
                if (client->getSupportDisplayModes(displayModeList, type)) {
                    for (auto mode : displayModeList) {
                        printf("%s %u %u %u %u %f \n", mode.name.c_str(), mode.dpiX, mode.dpiY, mode.pixelW, mode.pixelH, mode.refreshRate);
                    }
                }
                break;
            case 'c':
                client->setDisplayMode(optarg, type);
                break;
            case 'g':
                {
                    std::string value;
                    client->getDisplayAttribute(optarg, value, type);
                    printf("%s\n", value.c_str());
                }
                break;
            case 's':
                if (optind + 1 > argc) {
                    printf("miss parameter\n");
                    break;
                }
                {
                    client->setDisplayAttribute(optarg, argv[optind], type);
                    printf("set %s to %s", optarg, argv[optind]);
                    std::string value;
                    client->getDisplayAttribute(optarg, value, type);
                    printf(", current value:%s\n", value.c_str());
                }
                optind++;
                break;
            case 'G':
                if (optarg == NULL)
                    break;
                if (0 == memcmp("display-mode", optarg, sizeof("display-mode"))) {
                    std::string mode;
                    client->getDisplayMode(mode, type);
                    printf("%s\n", mode.c_str());
                } else if (0 == memcmp("ui-rect", optarg, sizeof("ui-rect"))) {
                    meson::Rect rect;
                    client->getDisplayRect(rect, type);
                    printf("%s\n", rect.toString().c_str());
                } else {
                    NOTIMPLEMENTED;
                }
                break;
            case 'S':
                if (optarg == NULL)
                    break;
                {
                    if (0 == memcmp("display-mode", optarg, sizeof("display-mode"))) {
                        if (optind + 1 > argc) {
                            DEBUG_INFO("miss parameter");
                            break;
                        }
                        client->setDisplayMode(argv[optind], type);
                        optind++;
                    } else if (0 == memcmp("ui-rect", optarg, sizeof("ui-rect"))) {
                        meson::Rect rect;
                        if (optind + 4 > argc) {
                            DEBUG_INFO("miss rect parameter");
                            break;
                        }
                        rect.x = atoi(argv[optind]);
                        optind++;
                        rect.y = atoi(argv[optind]);
                        optind++;
                        rect.w = atoi(argv[optind]);
                        optind++;
                        rect.h = atoi(argv[optind]);
                        DEBUG_INFO("set ui to (%s)", rect.toString().c_str());
                        client->setDisplayRect(rect, type);
                        optind++;
                    } else {
                        NOTIMPLEMENTED;
                    }
                }
                break;
            case 'r':
                NOTIMPLEMENTED;
                break;
            case 'd':
                {
                    Json::Value json;
                    Json::StyledWriter  write;
                    client->dumpDisplayAttribute(json, type);
                    printf("Dump display attribute:\n%s", (write.write(json)).c_str());
                }
                break;
            default:
                print_usage(argv[0]);
        }
    };

    DEBUG_INFO("Exit client");
    return 0;
}
