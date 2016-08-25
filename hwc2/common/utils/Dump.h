/*
// Copyright (c) 2016 Amlogic Corporation
*/
#ifndef DUMP_H_
#define DUMP_H_

namespace android {
namespace amlogic {

class Dump {
public:
    Dump(char *buf, int len);
    ~Dump();

    void append(const char *fmt, ...);
private:
    char *mBuf;
    int mLen;
};

} // namespace amlogic
} // namespace android
#endif /* DUMP_H_ */
