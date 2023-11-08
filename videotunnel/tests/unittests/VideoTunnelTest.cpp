/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: video tunnel functions for videotunnel device
 */

#define LOG_TAG "VideoTunnelTest"
#define TUNNEL_ID 1

#include <utils/Errors.h>
#include <cutils/properties.h>
#include <utils/Timers.h>

#include <gtest/gtest.h>

#include <VideoTunnelProducer.h>
#include <VideoTunnelConsumer.h>

#include <log/log.h>

using ::android::OK;

class VideoTunnelTest : public testing::Test {
public:

    virtual void SetUp()
    {
        mProducer = std::make_shared<VideoTunnelProducer>();
        mProducer->allocVideoTunnelId();

        int tunnelId = mProducer->getVideoTunnelId();
        mConsumer = std::make_shared<VideoTunnelConsumer>(tunnelId);
    }

    virtual void TearDown()
    {
        mProducer->freeVideoTunnelId();
    }

    bool compareRect(vt_rect_t rect1, vt_rect_t rect2) {
        if (rect1.left == rect2.left && rect1.top == rect2.top &&
            rect1.right == rect2.right && rect1.bottom == rect2.bottom)
            return true;
        else
            return false;
    }
protected:
    std::shared_ptr<VideoTunnelProducer> mProducer;
    std::shared_ptr<VideoTunnelConsumer> mConsumer;
};

TEST_F(VideoTunnelTest, connect_disconnect)
{
    EXPECT_EQ(OK, mProducer->producerConnect());

    EXPECT_EQ(OK, mConsumer->consumerConnect());
    EXPECT_EQ(OK, mConsumer->consumerDisconnect());
    EXPECT_EQ(OK, mProducer->producerDisconnect());
}

TEST_F(VideoTunnelTest, queueBuffer_withoutConnect)
{
    VTBufferItem queueItem;
    queueItem.allocateBuffer();

    EXPECT_EQ(-EINVAL, mProducer->queueBuffer(queueItem));
}

TEST_F(VideoTunnelTest, queueBuffer_withBadFd)
{
    VTBufferItem queueItem;
    queueItem.setBufferFd(2048);

    EXPECT_EQ(OK, mProducer->producerConnect());
    EXPECT_EQ(-EBADF, mProducer->queueBuffer(queueItem));
    EXPECT_EQ(OK, mProducer->producerDisconnect());
}

TEST_F(VideoTunnelTest, acquireBuffer_withoutConnect)
{
    VTBufferItem acquireItem;
    EXPECT_EQ(-EINVAL, mConsumer->acquireBuffer(acquireItem, false));
}

TEST_F(VideoTunnelTest, acquireBuffer)
{
    VTBufferItem acquireItem;
    EXPECT_EQ(OK, mConsumer->consumerConnect());
    EXPECT_EQ(-EAGAIN, mConsumer->acquireBuffer(acquireItem, false));
    EXPECT_EQ(OK, mConsumer->consumerDisconnect());
}

TEST_F(VideoTunnelTest, queueBuffer_acquireBuffer)
{
    VTBufferItem queueItem;
    queueItem.allocateBuffer();

    EXPECT_EQ(OK, mProducer->producerConnect());
    EXPECT_EQ(OK, mProducer->queueBuffer(queueItem));

    VTBufferItem acquireItem;
    EXPECT_EQ(OK, mConsumer->consumerConnect());
    EXPECT_EQ(OK, mConsumer->acquireBuffer(acquireItem));

    EXPECT_GE(acquireItem.getBufferFd(), 0);

    EXPECT_EQ(OK, mConsumer->consumerDisconnect());
    EXPECT_EQ(OK, mProducer->producerDisconnect());

    queueItem.releaseBuffer(true);
}

TEST_F(VideoTunnelTest, dequeueBuffer_withoutConnect)
{
    VTBufferItem dequeueItem;
    EXPECT_EQ(-EINVAL, mProducer->dequeueBuffer(dequeueItem, false));
}

TEST_F(VideoTunnelTest, dequeueBuffer_noconsumer)
{
    VTBufferItem queueItem;
    queueItem.allocateBuffer();

    EXPECT_EQ(OK, mProducer->producerConnect());
    EXPECT_EQ(OK, mProducer->queueBuffer(queueItem));

    VTBufferItem dequeueItem;
    EXPECT_EQ(-ENOTCONN, mProducer->dequeueBuffer(dequeueItem, false));
    EXPECT_EQ(OK, mProducer->producerDisconnect());
}

TEST_F(VideoTunnelTest, cancelBuffer)
{
    VTBufferItem queueItem;
    queueItem.allocateBuffer();

    EXPECT_EQ(OK, mProducer->producerConnect());
    EXPECT_EQ(OK, mProducer->queueBuffer(queueItem));

    VTBufferItem dequeueItem;
    EXPECT_EQ(-ENOTCONN, mProducer->dequeueBuffer(dequeueItem));

    // cancelBuffer
    EXPECT_EQ(OK, mProducer->cancelBuffer());

    EXPECT_EQ(OK, mProducer->dequeueBuffer(dequeueItem));
    EXPECT_EQ(queueItem.getBufferFd(), dequeueItem.getBufferFd());

    EXPECT_EQ(dequeueItem.getReleaseFenceFd(), -1);

    EXPECT_EQ(OK, mProducer->producerDisconnect());

    queueItem.releaseBuffer(true);
}

TEST_F(VideoTunnelTest, dequeueBuffer_releaseBuffer)
{
    VTBufferItem queueItem;
    queueItem.allocateBuffer();

    EXPECT_EQ(OK, mProducer->producerConnect());
    EXPECT_EQ(OK, mProducer->queueBuffer(queueItem));

    VTBufferItem acquireItem;
    EXPECT_EQ(OK, mConsumer->consumerConnect());
    EXPECT_EQ(OK, mConsumer->acquireBuffer(acquireItem));
    EXPECT_GE(acquireItem.getBufferFd(), 0);
    EXPECT_EQ(OK, mConsumer->releaseBuffer(acquireItem));

    VTBufferItem dequeueItem;
    EXPECT_EQ(OK, mProducer->dequeueBuffer(dequeueItem));
    EXPECT_EQ(queueItem.getBufferFd(), dequeueItem.getBufferFd());

    EXPECT_EQ(OK, mConsumer->consumerDisconnect());
    EXPECT_EQ(OK, mProducer->producerDisconnect());

    queueItem.releaseBuffer(true);
}

// test of video cmd
TEST_F(VideoTunnelTest, sendVideoCmd_withoutConnect)
{
    vt_cmd cmd = VT_CMD_SET_VIDEO_STATUS;
    int data = 1;

    EXPECT_EQ(-EINVAL, mProducer->sendCmd(cmd, data));
}

TEST_F(VideoTunnelTest, sendVideoCmd)
{
    vt_cmd cmd = VT_CMD_SET_VIDEO_STATUS;
    int data = 1;

    EXPECT_EQ(OK, mProducer->producerConnect());
    EXPECT_EQ(OK, mProducer->sendCmd(cmd, data));
    EXPECT_EQ(OK, mProducer->producerDisconnect());
}

TEST_F(VideoTunnelTest, recvdVideoCmd_withoutConnect)
{
    vt_cmd cmd;
    struct vt_cmd_data data;

    EXPECT_EQ(-EINVAL, mConsumer->recvCmd(cmd, data));
}

TEST_F(VideoTunnelTest, recvdVideoCmd)
{
    vt_cmd cmd;
    struct vt_cmd_data data;

    EXPECT_EQ(OK, mConsumer->consumerConnect());
    EXPECT_EQ(-EAGAIN, mConsumer->recvCmd(cmd, data, false));
    EXPECT_EQ(OK, mConsumer->consumerDisconnect());
}

TEST_F(VideoTunnelTest, send_recv_VideoCmd)
{
    vt_cmd cmdSend, cmdRecv;
    int dataSend;
    struct vt_cmd_data dataRecv;

    EXPECT_EQ(OK, mProducer->producerConnect());

    cmdSend = VT_CMD_SET_VIDEO_STATUS;
    dataSend = 1;

    EXPECT_EQ(OK, mProducer->sendCmd(cmdSend, dataSend));

    EXPECT_EQ(OK, mConsumer->consumerConnect());
    EXPECT_EQ(OK, mConsumer->recvCmd(cmdRecv, dataRecv));

    EXPECT_EQ(cmdSend, cmdRecv);
    EXPECT_EQ(dataSend, dataRecv.data);
    EXPECT_EQ(getpid(), dataRecv.client);

    cmdSend = VT_CMD_SET_GAME_MODE;
    dataSend = 1;

    EXPECT_EQ(OK, mProducer->sendCmd(cmdSend, dataSend));

    EXPECT_EQ(OK, mConsumer->consumerConnect());
    EXPECT_EQ(OK, mConsumer->recvCmd(cmdRecv, dataRecv));

    EXPECT_EQ(cmdSend, cmdRecv);
    EXPECT_EQ(dataSend, dataRecv.data);
    EXPECT_EQ(getpid(), dataRecv.client);

    EXPECT_EQ(OK, mProducer->producerDisconnect());
    EXPECT_EQ(OK, mConsumer->consumerDisconnect());
}

TEST_F(VideoTunnelTest, set_vt_mode)
{
    EXPECT_EQ(OK, mConsumer->setBlockMode(true));
    EXPECT_EQ(OK, mConsumer->setBlockMode(false));
}

TEST_F(VideoTunnelTest, timeStamp)
{
    nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
    VTBufferItem queueItem;
    queueItem.allocateBuffer();
    queueItem.setTimeStamp(now);

    EXPECT_EQ(OK, mProducer->producerConnect());
    EXPECT_EQ(OK, mProducer->queueBuffer(queueItem));

    VTBufferItem acquireItem;
    EXPECT_EQ(OK, mConsumer->consumerConnect());
    EXPECT_EQ(OK, mConsumer->acquireBuffer(acquireItem));

    EXPECT_GE(acquireItem.getBufferFd(), 0);
    EXPECT_EQ(acquireItem.getTimeStamp(), now);
    //EXPECT_EQ(acquireItem.getTimeStamp(), 5185103);

    EXPECT_EQ(OK, mConsumer->consumerDisconnect());
    EXPECT_EQ(OK, mProducer->producerDisconnect());

    queueItem.releaseBuffer(true);
}

TEST_F(VideoTunnelTest, dequeueBuffer_releaseBuffer_releaseFence)
{
    VTBufferItem queueItem;
    queueItem.allocateBuffer();

    EXPECT_EQ(OK, mProducer->producerConnect());
    EXPECT_EQ(OK, mProducer->queueBuffer(queueItem));

    VTBufferItem acquireItem;
    EXPECT_EQ(OK, mConsumer->consumerConnect());
    EXPECT_EQ(OK, mConsumer->acquireBuffer(acquireItem));
    EXPECT_GE(acquireItem.getBufferFd(), 0);
    EXPECT_EQ(OK, mConsumer->releaseBuffer(acquireItem));

    VTBufferItem dequeueItem;
    EXPECT_EQ(OK, mProducer->dequeueBuffer(dequeueItem));
    EXPECT_EQ(queueItem.getBufferFd(), dequeueItem.getBufferFd());

    EXPECT_GE(dequeueItem.getReleaseFenceFd(), -1);

    EXPECT_EQ(OK, mConsumer->consumerDisconnect());
    EXPECT_EQ(OK, mProducer->producerDisconnect());

    queueItem.releaseBuffer(true);
}

TEST_F(VideoTunnelTest, color_cmd)
{
    vt_cmd cmdRecv;
    struct vt_cmd_data dataRecv;

    vt_color_cmd cmdSend = VT_CMD_COLOR_BLACK;
    vt_color_data dataSend = VT_CMD_COLOR_DATA_ONCE;

    // not connect, if there is no consumer
    EXPECT_EQ(-ENOTCONN, mProducer->setSolidColor(cmdSend, dataSend));

    EXPECT_EQ(OK, mConsumer->consumerConnect());
    EXPECT_EQ(OK, mProducer->setSolidColor(cmdSend, dataSend));

    EXPECT_EQ(OK, mConsumer->recvCmd(cmdRecv, dataRecv, false));

    EXPECT_EQ(VT_CMD_SET_COLOR_BLACK, cmdRecv);
    EXPECT_EQ(dataSend, dataRecv.data);
    EXPECT_EQ(getpid(), dataRecv.client);
    EXPECT_EQ(OK, mConsumer->consumerDisconnect());
}

TEST_F(VideoTunnelTest, set_sourceCrop_cmd)
{
    vt_cmd cmdRecv;
    struct vt_cmd_data dataRecv;
    vt_rect_t rect = {0, 0, 600, 600};

    EXPECT_EQ(OK, mConsumer->consumerConnect());
    EXPECT_EQ(OK, mProducer->setSourceCrop(rect));
    EXPECT_EQ(OK, mConsumer->recvCmd(cmdRecv, dataRecv, false));

    EXPECT_EQ(VT_CMD_SET_SOURCE_CROP, cmdRecv);
    EXPECT_TRUE(compareRect(rect, dataRecv.rect));
    EXPECT_EQ(OK, mConsumer->consumerDisconnect());
}

TEST_F(VideoTunnelTest, set_displayFrame_cmd)
{
    vt_cmd cmdRecv;
    struct vt_cmd_data dataRecv;
    vt_rect_t rect = {0, 0, 500, 500};

    EXPECT_EQ(OK, mConsumer->consumerConnect());
    EXPECT_EQ(OK, mProducer->setDisplayFrame(rect));
    EXPECT_EQ(OK, mConsumer->recvCmd(cmdRecv, dataRecv, false));

    EXPECT_EQ(VT_CMD_SET_DISPLAY_FRAME, cmdRecv);
    EXPECT_TRUE(compareRect(rect, dataRecv.rect));
    EXPECT_EQ(OK, mConsumer->consumerDisconnect());
}

TEST_F(VideoTunnelTest, ask_refresh_cmd)
{
    VTBufferItem queueItem;
    queueItem.allocateBuffer();

    EXPECT_EQ(OK, mProducer->producerConnect());
    EXPECT_EQ(OK, mProducer->queueBuffer(queueItem));

    VTBufferItem acquireItem;
    EXPECT_EQ(OK, mConsumer->consumerConnect());
    EXPECT_EQ(OK, mConsumer->acquireBuffer(acquireItem));
    EXPECT_GE(acquireItem.getBufferFd(), 0);

    EXPECT_EQ(OK, mConsumer->askRefresh());
    EXPECT_EQ(OK, mConsumer->releaseBuffer(acquireItem));
    EXPECT_EQ(OK, mConsumer->consumerDisconnect());

    VTBufferItem acquireItemRefresh;
    EXPECT_EQ(OK, mConsumer->consumerConnect());
    EXPECT_EQ(OK, mConsumer->acquireBuffer(acquireItemRefresh));

    EXPECT_EQ(acquireItem.getBufferFd(), acquireItemRefresh.getBufferFd());
    EXPECT_EQ(OK, mConsumer->consumerDisconnect());
    EXPECT_EQ(OK, mProducer->producerDisconnect());

    queueItem.releaseBuffer(true);
}

