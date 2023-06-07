/*
 * Copyright (c) 2022 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <stdint.h>
#include <cutils/threads.h>

#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdint.h>

#include <zlib.h>

#ifdef MTD_OLD
# include <linux/mtd/mtd.h>
#else
# define  __user        /* nothing */
# include <mtd/mtd-user.h>
#endif

#include "mode_util.h"
#include "mode_ubootenv.h"

#define MAX_UBOOT_RWRETRY       5
#define CONFIG_ENV_SIZE   (64*1024)
#define CONFIG_ENV_OFFSET_REDUND (64*1024)

typedef struct env_image {
    uint32_t  crc;      /* CRC32 over data bytes*/
    char data[];        /* Environment data*/
} env_image_t;

typedef struct environment {
    void *image;
    uint32_t *crc;
    char *data;
} environment_t;

typedef struct env_attribute {
    struct env_attribute *next;
    char key[256];
    char value[4096];
} env_attribute_t;

static const char *PROFIX_UBOOTENV_VAR = "ubootenv.var.";

static char mEnvPartitionName[32];
static int mEnvPartitionSize;
static int mEnvSize;
static bool mEnvInitDone = false;
static environment_t mEnvData;
static env_attribute_t mEnvAttrHeader;
static pthread_mutex_t mEnvLock;

//TODO: support multi thread, thread safe

static struct env_attribute* meson_mode_parse_attribute() {
    char *proc = mEnvData.data;
    char *nextProc;
    env_attribute_t *attr = &mEnvAttrHeader;

    memset(attr, 0, sizeof(env_attribute_t));

    do {
        nextProc = proc + strlen(proc) + sizeof(char);
        SYS_LOGI("[ubootenv] process %s\n",proc);
        char *key = strchr(proc, (int)'=');
        if (key != NULL) {
            *key=0;
            strcpy(attr->key, proc);
            strcpy(attr->value, key + sizeof(char));
        } else {
            SYS_LOGI("[ubootenv] error need '=' skip this value\n");
        }

        if (!(*nextProc)) {
            SYS_LOGI("[ubootenv] process end \n");
            break;
        }
        proc = nextProc;

        attr->next = (env_attribute_t *)malloc(sizeof(env_attribute_t));
        if (attr->next == NULL) {
            SYS_LOGE("[ubootenv] parse attribute malloc error \n");
            break;
        }
        memset(attr->next, 0, sizeof(env_attribute_t));
        attr = attr->next;
    } while(1);
    mEnvInitDone = true;
    return &mEnvAttrHeader;
}

static int meson_mode_is_ubootenv(const char* prop_name) {
    if (!prop_name || !(*prop_name))
        return 0;

    if (!(*PROFIX_UBOOTENV_VAR))
        return 0;

    if (strncmp(prop_name, PROFIX_UBOOTENV_VAR, strlen(PROFIX_UBOOTENV_VAR)) == 0 &&
            strlen(prop_name) > strlen(PROFIX_UBOOTENV_VAR) )
        return 1;

    return 0;
}

/*
 * creat_args_flag : if true , if envvalue don't exists Creat it .
 * if false , if envvalue don't exists just exit .
 */
static int meson_mode_set_attribute(const char * key,  const char * value, bool createNew) {
    env_attribute_t *attr = &mEnvAttrHeader;
    env_attribute_t *last = attr;
    while (attr) {
        if (!strcmp(key, attr->key)) {
            strcpy(attr->value, value);
            return 2;
        }
        last = attr;
        attr = attr->next;
    }

    if (createNew) {
        SYS_LOGI("[ubootenv] ubootenv.var.%s not found, create it.\n", key);

        attr = (struct env_attribute *)malloc(sizeof(env_attribute_t));
        last->next = attr;
        memset(attr, 0, sizeof(env_attribute_t));
        strcpy(attr->key, key);
        strcpy(attr->value, value);
        return 1;
    }
    return 0;
}

/*  attribute revert to sava data*/
static int meson_mode_format_attribute() {
    env_attribute_t *attr = &mEnvAttrHeader;
    char *data = mEnvData.data;
    memset(mEnvData.data, 0, mEnvSize);
    do {
        int len = sprintf(data, "%s=%s", attr->key, attr->value);
        if (len < (int)(sizeof(char)*3)) {
            SYS_LOGE("[ubootenv] Invalid env data key:%s, value:%s\n", attr->key, attr->value);
        }
        else
            data += len + sizeof(char);

        attr = attr->next;
    } while (attr);
    return 0;
}

//save value to storage flash
static int meson_mode_save_ubootenv() {
    int fd;
    int err;
    int lseeknum;

    meson_mode_format_attribute();
    *(mEnvData.crc) = crc32(0, (uint8_t *)mEnvData.data, mEnvSize);

    if ((fd = open (mEnvPartitionName, O_RDWR)) < 0) {
        SYS_LOGE("[ubootenv] open devices error\n");
        return -1;
    }

    if (strstr (mEnvPartitionName, "mtd")) {
        struct erase_info_user erase;
        struct mtd_info_user info;
        unsigned char *data = NULL;

        memset(&info, 0, sizeof(info));
        err = ioctl(fd, MEMGETINFO, &info);
        if (err < 0) {
            SYS_LOGE("[ubootenv] Get MTD info error\n");
            close(fd);
            return -4;
        }

        erase.start = 0;
        if (info.erasesize > ((unsigned int)mEnvPartitionSize * 2)) {
            data = (unsigned char*)malloc(info.erasesize);
            if (data == NULL) {
                SYS_LOGE("[ubootenv] Out of memory!!!\n");
                close(fd);
                return -5;
            }
            memset(data, 0, info.erasesize);
            err = read(fd, (void*)data, info.erasesize);
            if (err != (int)info.erasesize) {
                SYS_LOGE("[ubootenv] Read access failed !!!\n");
                free(data);
                close(fd);
                return -6;
            }
            memcpy(data, mEnvData.image, mEnvPartitionSize);
            memcpy(data + CONFIG_ENV_OFFSET_REDUND, mEnvData.image, mEnvPartitionSize);
            erase.length = info.erasesize;
        }
        else {
            erase.length = mEnvPartitionSize * 2;
        }

        err = ioctl (fd, MEMERASE,&erase);
        if (err < 0) {
            SYS_LOGE ("[ubootenv] MEMERASE %d\n",err);
            close(fd);
            free(data);
            return  -2;
        }

        if (info.erasesize > (unsigned int)mEnvPartitionSize) {
            lseek(fd, 0L, SEEK_SET);
            if (data != NULL)
                err = write(fd , data, info.erasesize);
            else
                SYS_LOGE("data is NULL\n");
        }
        else {
            err = write(fd ,mEnvData.image, mEnvPartitionSize);
            lseeknum = lseek(fd, CONFIG_ENV_OFFSET_REDUND, SEEK_SET);
            if (lseeknum != CONFIG_ENV_OFFSET_REDUND) {
                SYS_LOGE("[ubootenv] can not lseek, seek num = %d \n", lseeknum);
            }
            err = write(fd ,mEnvData.image, mEnvPartitionSize);
        }
         free(data);
    } else {
        //emmc and nand needn't erase
        lseek(fd, 0L, SEEK_SET);
        err = write(fd, mEnvData.image, mEnvPartitionSize);
        lseeknum = lseek(fd, CONFIG_ENV_OFFSET_REDUND, SEEK_SET);
        if (lseeknum != CONFIG_ENV_OFFSET_REDUND) {
            SYS_LOGE("[ubootenv] lseek error, seek num = %d \n", lseeknum);
        }
        err = write(fd ,mEnvData.image, mEnvPartitionSize);
    }

    close(fd);
    if (err < 0) {
        SYS_LOGE ("[ubootenv] write, size %d \n", mEnvPartitionSize);
        return -3;
    }
    return 0;
}

static void meson_mode_print_values() {
    env_attribute_t *attr = &mEnvAttrHeader;
    while (attr != NULL) {
        SYS_LOGI("[ubootenv] key: [%s], value: [%s]\n", attr->key, attr->value);
        attr = attr->next;
    }
}

static int meson_mode_read_ubootenv() {
    int fd;
    int flag = 0;
    int seeknum;
    if ((fd = open(mEnvPartitionName, O_RDONLY)) < 0) {
        SYS_LOGE("[ubootenv] open devices error: %s\n", strerror(errno));
        return -1;
    }

    char *addr = (char *)malloc(mEnvPartitionSize);
    if (addr == NULL) {
        SYS_LOGE("[ubootenv] Not enough memory for environment (%u bytes)\n", mEnvPartitionSize);
        close(fd);
        return -2;
    }

    memset(addr, 0, mEnvPartitionSize);
    mEnvData.image = addr;
    struct env_image *image = (struct env_image *)addr;
    mEnvData.crc = &(image->crc);
    mEnvData.data = image->data;

    int ret = read(fd ,mEnvData.image, mEnvPartitionSize);

    SYS_LOGI("ubootenv open success ret %d mEnvPartitionName %s fd %d mEnvPartitionSize %d ",ret,mEnvPartitionName,fd,mEnvPartitionSize);

    if (ret == (int)mEnvPartitionSize) {
        uint32_t crcCalc = crc32(0, (uint8_t *)mEnvData.data, mEnvSize);
            SYS_LOGE("[ubootenv] CRC Check  save_crc=%08x, crcCalc = %08x \n",
                *mEnvData.crc, crcCalc);
        if (crcCalc != *(mEnvData.crc)) {
            SYS_LOGE("[ubootenv] CRC Check error  save_crc=%08x, crcCalc = %08x \n",
                *mEnvData.crc, crcCalc);
            flag = -3;
        }
    } else {
        SYS_LOGE("[ubootenv] read error 0x%x \n",ret);
        flag = -5;
    }

    if (flag != 0) {
        SYS_LOGE("first env error, try second....\n");
        seeknum = lseek(fd, CONFIG_ENV_OFFSET_REDUND, SEEK_SET);
        if (seeknum != CONFIG_ENV_OFFSET_REDUND) {
            SYS_LOGE("[ubootenv] lseek error, seeknum = %d \n", seeknum);
        }
        int ret2 = read(fd ,mEnvData.image, CONFIG_ENV_OFFSET_REDUND);
        if (ret2 == (int)mEnvPartitionSize) {
            uint32_t crcCalc = crc32(0, (uint8_t *)mEnvData.data, mEnvSize);
            if (crcCalc != *(mEnvData.crc)) {
                SYS_LOGE("[ubootenv] CRC2 Check  save_crc=%08x, crcCalc = %08x \n",
                    *mEnvData.crc, crcCalc);
                close(fd);
                return -3;
            }
        }
    }

    meson_mode_parse_attribute();
    meson_mode_print_values();

    close(fd);
    return 0;
}

static const char * meson_mode_get_uenv(const char * key) {
    env_attribute_t *attr = &mEnvAttrHeader;
    while (attr) {
        if (!strcmp(key, attr->key)) {
            SYS_LOGI("get ubootenv [ubootenv] key: [%s], value: [%s]\n", attr->key, attr->value);
            return attr->value;
        }
        attr = attr->next;
    }
    return NULL;
}

int meson_mode_init_ubootenv() {
    const char *NAND_ENV = "/dev/nand_env";
    const char *BLOCK_ENV = "/dev/block/env";//for update from P
    const char *BLOCK_ENV_BYNAME = "/dev/block/by-name/env";//normally use that
    const char *BLOCK_UBOOT_ENV = "/dev/block/by-name/ubootenv";

    struct stat st;
    mEnvPartitionSize = CONFIG_ENV_SIZE;
    mEnvSize = mEnvPartitionSize - sizeof(uint32_t);

    if (!stat(NAND_ENV, &st)) {
        strcpy (mEnvPartitionName, NAND_ENV);
    }
    else if (!stat(BLOCK_ENV, &st)) {
        strcpy (mEnvPartitionName, BLOCK_ENV);
    }
    else if (!stat(BLOCK_ENV_BYNAME, &st)) {
        strcpy (mEnvPartitionName, BLOCK_ENV_BYNAME);
    }
    else if (!stat(BLOCK_UBOOT_ENV, &st)) {
        int fd;
        struct mtd_info_user info;

        strcpy (mEnvPartitionName, BLOCK_UBOOT_ENV);
        if ((fd = open(mEnvPartitionName, O_RDWR)) < 0) {
            SYS_LOGE("[ubootenv] open device(%s) error\n", mEnvPartitionName );
            return -2;
        }

        memset(&info, 0, sizeof(info));
        int err = ioctl(fd, MEMGETINFO, &info);
        if (err < 0) {
            SYS_LOGE("[ubootenv] get MTD info error\n" );
            close(fd);
            return -3;
        }
        close(fd);

        //mEnvEraseSize = info.erasesize;//0x20000;//128K
        mEnvPartitionSize = info.size;//0x8000;
        mEnvSize = mEnvPartitionSize - sizeof(long);
    }

    //the first four bytes are crc value, others are data
    SYS_LOGI("[ubootenv] using %s with size(%d) (%d)", mEnvPartitionName, mEnvPartitionSize, mEnvSize);

    int i = 0;
    int ret = -1;

    pthread_mutex_lock(&mEnvLock);
    while (i < MAX_UBOOT_RWRETRY && ret < 0) {
        i ++;
        ret = meson_mode_read_ubootenv();
        if (ret < 0)
            SYS_LOGE("[ubootenv] Cannot read %s: %d.\n", mEnvPartitionName, ret);
        if (ret < -2)
            free(mEnvData.image);
    }
    pthread_mutex_unlock(&mEnvLock);

    if (i >= MAX_UBOOT_RWRETRY) {
        SYS_LOGE("[ubootenv] read %s failed \n", mEnvPartitionName);
        return -2;
    }

    return 0;
}

int meson_mode_set_ubootenv(const char* name, const char* value) {
    if (!mEnvInitDone) {
        SYS_LOGE("[ubootenv] bootenv do not init\n");
        meson_mode_init_ubootenv();
    }

    SYS_LOGI("[ubootenv] set_ubootenv name [%s]: value [%s] \n", name, value);
    const char* envName = NULL;
    if (strcmp(name, "ubootenv.var.bootcmd") == 0) {
        envName = "bootcmd";
    }
    else {
        if (!meson_mode_is_ubootenv(name)) {
            //should assert here.
            SYS_LOGE("[ubootenv] %s is not a ubootenv variable.\n", name);
            return -2;
        }
        envName = name + strlen(PROFIX_UBOOTENV_VAR);
    }

    const char *envValue = meson_mode_get_uenv(envName);
    if (!envValue)
        envValue = "";

    if (!strcmp(value, envValue))
        return 0;

    pthread_mutex_lock(&mEnvLock);
    meson_mode_set_attribute(envName, value, true);

    int i = 0;
    int ret = -1;
    while (i < MAX_UBOOT_RWRETRY && ret < 0) {
        i ++;
        ret = meson_mode_save_ubootenv();
        if (ret < 0)
            SYS_LOGE("[ubootenv] Cannot write %s: %d.\n", mEnvPartitionName, ret);
    }

    if (i < MAX_UBOOT_RWRETRY) {
        SYS_LOGI("[ubootenv] Save ubootenv to %s succeed!\n", mEnvPartitionName);
    }

    pthread_mutex_unlock(&mEnvLock);

    return ret;
}

const char * meson_mode_get_ubootenv(const char * key) {
    if (!mEnvInitDone) {
        SYS_LOGE("[ubootenv] don't init done\n");
        meson_mode_init_ubootenv();
    }

    pthread_mutex_lock(&mEnvLock);
    const char* envName = key + strlen(PROFIX_UBOOTENV_VAR);
    const char* envValue = meson_mode_get_uenv(envName);
    pthread_mutex_unlock(&mEnvLock);
    return envValue;
}

void meson_mode_ubootenv_dump(int fd) {
    env_attribute_t *attr = &mEnvAttrHeader;

    while (attr != NULL) {
        dprintf(fd, "[ubootenv] key: [%s], value: [%s]\n", attr->key, attr->value);
        attr = attr->next;
    }
}
