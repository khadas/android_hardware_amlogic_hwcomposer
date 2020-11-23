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

    EXPECT_GT(acquireItem.getBufferFd(), 0);

    EXPECT_EQ(OK, mConsumer->consumerDisconnect());
    EXPECT_EQ(OK, mProducer->producerDisconnect());

    queueItem.releaseBuffer(true);
}

TEST_F(VideoTunnelTest, dequeueBuffer_withoutConnect)
{
    VTBufferItem dequeueItem;
    EXPECT_EQ(-EINVAL, mProducer->dequeueBuffer(dequeueItem, false));
}

TEST_F(VideoTunnelTest, dequeueBuffer)
{
    VTBufferItem queueItem;
    queueItem.allocateBuffer();

    EXPECT_EQ(OK, mProducer->producerConnect());
    EXPECT_EQ(OK, mProducer->queueBuffer(queueItem));

    VTBufferItem dequeueItem;
    EXPECT_EQ(-EAGAIN, mProducer->dequeueBuffer(dequeueItem, false));
    EXPECT_EQ(OK, mProducer->producerDisconnect());
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
    EXPECT_GT(acquireItem.getBufferFd(), 0);
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
    int data;
    int client;

    EXPECT_EQ(-EINVAL, mConsumer->recvCmd(cmd, data, client));
}

TEST_F(VideoTunnelTest, recvdVideoCmd)
{
    vt_cmd cmd;
    int data;
    int client;

    EXPECT_EQ(OK, mConsumer->consumerConnect());
    EXPECT_EQ(-EAGAIN, mConsumer->recvCmd(cmd, data, client, false));
    EXPECT_EQ(OK, mConsumer->consumerDisconnect());
}

TEST_F(VideoTunnelTest, send_recv_VideoCmd)
{
    vt_cmd cmdSend, cmdRecv;
    int dataSend, dataRecv;
    int clientRecv;

    EXPECT_EQ(OK, mProducer->producerConnect());

    cmdSend = VT_CMD_SET_VIDEO_STATUS;
    dataSend = 1;

    EXPECT_EQ(OK, mProducer->sendCmd(cmdSend, dataSend));

    EXPECT_EQ(OK, mConsumer->consumerConnect());
    EXPECT_EQ(OK, mConsumer->recvCmd(cmdRecv, dataRecv, clientRecv));

    EXPECT_EQ(cmdSend, cmdRecv);
    EXPECT_EQ(dataSend, dataRecv);
    EXPECT_EQ(getpid(), clientRecv);

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

    EXPECT_GT(acquireItem.getBufferFd(), 0);
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
    EXPECT_GT(acquireItem.getBufferFd(), 0);
    EXPECT_EQ(OK, mConsumer->releaseBuffer(acquireItem));

    VTBufferItem dequeueItem;
    EXPECT_EQ(OK, mProducer->dequeueBuffer(dequeueItem));
    EXPECT_EQ(queueItem.getBufferFd(), dequeueItem.getBufferFd());

    EXPECT_GT(dequeueItem.getReleaseFenceFd(), 0);

    EXPECT_EQ(OK, mConsumer->consumerDisconnect());
    EXPECT_EQ(OK, mProducer->producerDisconnect());

    queueItem.releaseBuffer(true);
}

