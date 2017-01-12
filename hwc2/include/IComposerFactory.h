/*
// Copyright(c) 2016 Amlogic Corporation
*/
#ifndef ICOMPOSER_FACTORY_H_
#define ICOMPOSER_FACTORY_H_


#include <IComposer.h>

namespace android {
namespace amlogic {


class IComposerFactory {

public:
    virtual ~IComposerFactory() {};

public:
    virtual IComposer* createComposer(String8 key) = 0;
};
} // namespace amlogic
} // namespace android

#endif /* ICOMPOSER_FACTORY_H_ */
