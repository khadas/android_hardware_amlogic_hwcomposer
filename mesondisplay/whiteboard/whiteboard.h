/*
 * Copyright (c) 2020 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef WHITEBOARD_H
#define WHITEBOARD_H
class WhiteBoard {
public:
    static int getWhiteBoardFd();
    static bool setWhiteBoardMode(bool mode);
    static bool getWhiteBoardMode(bool& mode);
    static bool setWBDisplayFrame(int x, int y);
    static bool hideVideoLayer(bool hide);
};
#endif
