/*
// Copyright (c) 2014 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// This file is modified by Amlogic, Inc. 2017.01.17.
*/

#include <HwcTrace.h>
#include <Hwcomposer.h>
#include <PrimaryDevice.h>
#include <Utils.h>
#include <SysTokenizer.h>

namespace android {
namespace amlogic {

PrimaryDevice::PrimaryDevice(Hwcomposer& hwc, IComposeDeviceFactory * controlFactory)
    : PhysicalDevice(DEVICE_PRIMARY, hwc, controlFactory),
    pConfigPath(DISPLAY_CFG_FILE),
    mDisplayType(DISPLAY_TYPE_MBOX)
{
    DTRACE("display mode config path: %s", pConfigPath);

    CTRACE();
}

PrimaryDevice::~PrimaryDevice()
{
    CTRACE();
}

bool PrimaryDevice::initialize()
{
    parseConfigFile();
    updateDisplayInfo(mDefaultMode);

    if (!PhysicalDevice::initialize()) {
        DEINIT_AND_RETURN_FALSE("failed to initialize physical device");
    }

    mSignalHpd = false;

    UeventObserver *observer = Hwcomposer::getInstance().getUeventObserver();
    if (observer) {
        observer->registerListener(
            Utils::getHotplugUeventEnvelope(),
            hotplugEventListener,
            this);
        observer->registerListener(
            Utils::getModeChangeUeventEnvelope(),
            modeChangeEventListener,
            this);
    } else {
        ETRACE("Uevent observer is NULL");
    }

    return true;
}

void PrimaryDevice::deinitialize()
{
    PhysicalDevice::deinitialize();
}

void PrimaryDevice::hotplugEventListener(void *data, bool status)
{
    PrimaryDevice *pThis = (PrimaryDevice*)data;
    if (pThis) {
        pThis->hotplugListener(0);
        if (status) pThis->mSignalHpd = true;
    }
}

void PrimaryDevice::modeChangeEventListener(void *data, bool status)
{
    PrimaryDevice *pThis = (PrimaryDevice*)data;
    DTRACE("mode change event: %d", status);

    if (status && pThis) {
        pThis->changeModeDetectThread();
    }
}

void PrimaryDevice::hotplugListener(bool connected)
{
    CTRACE();
    ETRACE("hotpug event: %d", connected);

    updateHotplugState(connected);

    // update display configs
    // onHotplug(getDisplayId(), connected);

    // notify sf to refresh.
    getDevice().refresh(getDisplayId());
}

int PrimaryDevice::parseConfigFile()
{
    const char* WHITESPACE = " \t\r";

    SysTokenizer* tokenizer;
    int status = SysTokenizer::open(pConfigPath, &tokenizer);
    if (status) {
        ETRACE("Error %d opening display config file %s.", status, pConfigPath);
    } else {
        while (!tokenizer->isEof()) {
            ITRACE("Parsing %s: %s", tokenizer->getLocation(), tokenizer->peekRemainderOfLine());

            tokenizer->skipDelimiters(WHITESPACE);
            if (!tokenizer->isEol() && tokenizer->peekChar() != '#') {

                char *token = tokenizer->nextToken(WHITESPACE);
                if (!strcmp(token, DEVICE_STR_MBOX)) {
                    mDisplayType = DISPLAY_TYPE_MBOX;
                } else if (!strcmp(token, DEVICE_STR_TV)) {
                    mDisplayType = DISPLAY_TYPE_TV;
                } else {
                    DTRACE("%s: Expected keyword, got '%s'.", tokenizer->getLocation(), token);
                    break;
                }
                tokenizer->skipDelimiters(WHITESPACE);
                tokenizer->nextToken(WHITESPACE);
                tokenizer->skipDelimiters(WHITESPACE);
                strcpy(mDefaultMode, tokenizer->nextToken(WHITESPACE));
            }

            tokenizer->nextLine();
        }
        delete tokenizer;
    }
    return status;
}

void PrimaryDevice::changeModeDetectThread()
{
    pthread_t id;
    int ret = pthread_create(&id, NULL, changeModeDetect, this);
    if (ret != 0)
        ETRACE("Create changeModeDetect error!\n");
}

void* PrimaryDevice::changeModeDetect(void* data)
{
    PrimaryDevice *pThis = (PrimaryDevice*)data;
    bool modeChanged = false;
    char lastMode[32];
    Utils::getSysfsStr(SYSFS_DISPLAY_MODE, lastMode);
    do {
        modeChanged = Utils::checkSysfsStatus(SYSFS_DISPLAY_MODE, lastMode, 32);
        usleep(500 * 1000);
    } while (!modeChanged);

    if (pThis->mSignalHpd) {
        pThis->setOsdMouse();
        pThis->hotplugListener(1);
        pThis->mSignalHpd = false;
    }
    return NULL;
}

} // namespace amlogic
} // namespace android
