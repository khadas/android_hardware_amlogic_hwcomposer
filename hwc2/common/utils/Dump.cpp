/*
// Copyright (c) 2016 Amlogic Corporation
*/

#include <stdarg.h>
#include <stdio.h>

#include <Dump.h>

namespace android {
namespace amlogic {

Dump::Dump(char *buf, int len)
    : mBuf(buf),
      mLen(len)
{

}

Dump::~Dump()
{

}

void Dump::append(const char *fmt, ...)
{
    int len;

    if (!mBuf || !mLen)
        return;

    va_list ap;
    va_start(ap, fmt);
    len = vsnprintf(mBuf, mLen, fmt, ap);
    va_end(ap);

    mLen -= len;
    mBuf += len;
}

} // namespace amlogic
} // namespace android
