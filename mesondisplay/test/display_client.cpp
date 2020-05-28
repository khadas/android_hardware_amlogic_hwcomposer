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
    {0, 0, 0, 0}
};

static void print_usage(const char* name) {
    printf("Usage: %s [-lcrgs]\n"
            "Get or change the mode setting of the weston drm output.\n"
            "Options:\n"
            "       -l,--list-modes        \tlist connector support modes\n"
            "       -c,--change-mode MODE  \tchange connector current mode, MODE format like:%%dx%%d@%%d width,height,refresh\n"
            "       -g,--get-connector-property \"PROPERTY\"\tget connector property\n"
            "       -s,--set-connector-property \"PROPERTY\"=value\tset connector property\n"
            "       -G \"[ui-rect|display-mode]\"\tget [logic ui rect|display mode]\n"
            "       -S \"[ui-rect]\"\tset [logic ui rect]\n"
            "                               \t eg: \"Content Protection\"=1\n"
            "       -r,--raw-cmd           \tsend raw cmd\n", name);
}


int main(int argc, char* argv[]) {
    std::vector<meson::DisplayModeInfo> displayModeList;
    if (argc == 1) {
        print_usage(argv[0]);
        return 0;
    }
    unique_ptr<DisplayAdapter> client = meson::DisplayAdapterCreateRemote();
    DisplayAdapter::ConnectorType type = DisplayAdapter::CONN_TYPE_HDMI;
    DEBUG_INFO("Start client");

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
                NOTIMPLEMENTED;
                break;
            case 's':
                NOTIMPLEMENTED;
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
            default:
                print_usage(argv[0]);
        }
    };

    DEBUG_INFO("Exit client");
    return 0;
}
