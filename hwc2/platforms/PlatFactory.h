/*
// Copyright(c) 2016 Amlogic Corporation
*/

#ifndef MOOFPLATFORMFACTORY_H_
#define MOOFPLATFORMFACTORY_H_

#include <IPlatFactory.h>


namespace android {
namespace amlogic {

class PlatFactory : public  IPlatFactory {
public:
    PlatFactory();
    virtual ~PlatFactory();

    virtual IDisplayDevice* createDisplayDevice(int disp);

};

} //namespace amlogic
} //namespace android


#endif /* MOOFPLATFORMFACTORY_H_ */
