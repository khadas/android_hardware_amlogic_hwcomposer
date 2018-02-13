/*
 * Copyright (c) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef VARIABLE_MODE_MGR_H
#define VARIABLE_MODE_MGR_H

#include "HwcModeMgr.h"

/*
 * VariableModesMgr:
 * This class designed for removeable device(hdmi, cvbs)
 * to support real activeModes.
 * Config list will changed when device disconnect/connect.
 */
class VariableModeMgr : public  HwcModeMgr {
public:

};

#endif/*VARIABLE_MODE_MGR_H*/
