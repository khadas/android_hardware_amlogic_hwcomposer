/*
// Copyright(c) 2016 Amlogic Corporation
*/
#ifndef IPLATFORM_FACTORY_H_
#define IPLATFORM_FACTORY_H_


#include <IDisplayDevice.h>

namespace android {
namespace amlogic {


class IPlatFactory {

public:
    virtual ~IPlatFactory() {};
public:

    virtual IDisplayDevice* createDisplayDevice(int disp) = 0;
};
} // namespace amlogic
} // namespace android

#endif /* DATABUFFER_H__ */
