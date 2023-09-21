/*
 * Copyright (c) 2022 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include "mode_policy.h"
#include "mode_private.h"

struct meson_policy {
    struct meson_policy_in input;
    struct meson_policy_out output;
    enum meson_mode_policy policy;
};

static struct meson_policy g_in[MESON_MODE_CON_MAX];

static bool test_mode = false;

#define GET_CURRENT_POLICY(connector) \
    struct meson_policy *mp = NULL; \
    if (connector < 0 || connector >= MESON_MODE_CON_MAX) \
        return -EINVAL; \
    mp = &g_in[connector];

static bool is_dv_prefer(struct meson_policy_in *input) {
    if (input == NULL)
        return false;

    struct meson_hdr_info *hdr_ptr = &input->hdr_info;

    /* not dv priority */
    if (hdr_ptr->hdr_priority != MESON_DOLBY_VISION_PRIORITY
        && hdr_ptr->hdr_priority != MESON_G_DV_HDR10_HLG
        && hdr_ptr->hdr_priority != MESON_G_DV_HDR10
        && hdr_ptr->hdr_priority != MESON_G_DV_HLG
        && hdr_ptr->hdr_priority != MESON_G_DV) {
        SYS_LOGI("not prefer dv, hdr_priority:%x", hdr_ptr->hdr_priority);
        return false;
    }

    /* not force dv */
    if (hdr_ptr->hdr_policy == MESON_HDR_POLICY_FORCE
        && hdr_ptr->hdr_force_mode != MESON_HDR_FORCE_MODE_DV) {
        SYS_LOGI("not force dv, hdr_policy:%d hdr_force_mode:%d\n", hdr_ptr->hdr_policy, hdr_ptr->hdr_force_mode);
        return false;
    }

    /* dv is enable and tv also support it */
    if (hdr_ptr->is_enable_dv && hdr_ptr->is_tv_supportDv)
        return true;

    return false;
}

static int32_t is_hdr_prefer(struct meson_policy_in *input) {
    if (input == NULL)
        return -EINVAL;

    struct meson_hdr_info *hdr_ptr = &(input->hdr_info);

    /* not prefer hdr */
    if (hdr_ptr->is_hdr_resolution_priority != 1) {
        SYS_LOGI("not prefer hdr is_hdr_resolution_priority:%d\n", hdr_ptr->is_hdr_resolution_priority);
        return 0;
    }
    /* not force hdr */
    if (hdr_ptr->hdr_policy == MESON_HDR_POLICY_FORCE
        && !(hdr_ptr->hdr_force_mode == MESON_HDR_FORCE_MODE_HDR10
        ||  hdr_ptr->hdr_force_mode == MESON_HDR_FORCE_MODE_HLG
        ||  hdr_ptr->hdr_force_mode == MESON_HDR_FORCE_MODE_HDR10PLUS)) {
        SYS_LOGI("not force hdr, hdr_policy:%d hdr_force_mode:%d\n", hdr_ptr->hdr_policy, hdr_ptr->hdr_force_mode);
        return 0;
    }

    /* policy is prefer dv or hdr and tv support hdr*/
    if (hdr_ptr->is_tv_supportHDR &&
        ((hdr_ptr->hdr_priority != MESON_SDR_PRIORITY)
        && (hdr_ptr->hdr_priority != MESON_G_SDR)))
        return 1;

    return 0;
}

static int32_t update_dv_type(struct meson_hdr_info *info) {
    char type[MESON_MODE_LEN];
    char dv_deepcolor[MESON_DV_MODE_LEN];

    /*
     * 1. update input amdolby vision info
     * 1.1 update current amdolby vision mode
     */
    strcpy(type, info->ubootenv_dv_type);
    /*
     * 1.2 update tv support amdolby vision deep color
     */
    strcpy(dv_deepcolor, info->dv_deepcolor);
    SYS_LOGI("ubootenv_dv_type %s dv_deepcolor:%s\n", type, dv_deepcolor);

    /*
     * 2. check tv support or not
     */
    if ((strstr(type, "1") != NULL) && strstr(dv_deepcolor, "DV_RGB_444_8BIT") != NULL) {
        return DOLBY_VISION_STD_ENABLE;
    } else if ((strstr(type, "2") != NULL) && strstr(dv_deepcolor, "LL_YCbCr_422_12BIT") != NULL) {
        return DOLBY_VISION_LL_YUV;
    } else if ((strstr(type, "3") != NULL) &&
            ((strstr(dv_deepcolor, "LL_RGB_444_12BIT") != NULL) ||
             (strstr(dv_deepcolor, "LL_RGB_444_10BIT") != NULL))) {
        return DOLBY_VISION_LL_RGB;
    } else if (strstr(type, "0") != NULL) {
        return DOLBY_VISION_DISABLE;
    }

    /*
     * 3. amdolby vision best policy:STD->LL_YUV->LL_RGB for netflix request
     * amdolby vision best policy:LL_YUV->STD->LL_RGB for amdolby vision request
     */
    if ((strstr(dv_deepcolor, "DV_RGB_444_8BIT") != NULL) ||
            (strstr(dv_deepcolor, "LL_YCbCr_422_12BIT") != NULL)) {
        if (strstr(dv_deepcolor, "DV_RGB_444_8BIT") != NULL) {
            return DOLBY_VISION_STD_ENABLE;
        } else if (strstr(dv_deepcolor, "LL_YCbCr_422_12BIT") != NULL) {
            return DOLBY_VISION_LL_YUV;
        }
    } else if ((strstr(dv_deepcolor, "LL_RGB_444_12BIT") != NULL) ||
            (strstr(dv_deepcolor, "LL_RGB_444_10BIT") != NULL)) {
        return DOLBY_VISION_LL_RGB;
    }

    return DOLBY_VISION_DISABLE;
}

/* amdolby vision mode to color format */
static void update_dv_attr(const char *deepcolor, int dolbyvision_type, char * dv_attr) {
    int dv_type = dolbyvision_type;

    switch (dv_type) {
        case DOLBY_VISION_STD_ENABLE:
            strcpy(dv_attr, "444,8bit");
            break;
        case DOLBY_VISION_LL_YUV:
            strcpy(dv_attr, "422,12bit");
            break;
        case DOLBY_VISION_LL_RGB:
            if (strstr(deepcolor, "LL_RGB_444_12BIT") != NULL) {
                strcpy(dv_attr, "444,12bit");
            } else if (strstr(deepcolor, "LL_RGB_444_10BIT") != NULL) {
                strcpy(dv_attr, "444,10bit");
            }
            break;
        default:
            strcpy(dv_attr, "444,8bit");
            break;
    }

    SYS_LOGI("dv_type :%d dv_attr:%s", dv_type, dv_attr);
}

/*
 * find the index of mode base the hdmi resolution priority table
 * TODO: refactor
 */
static int32_t find_resolution_index(const char *mode, int flag) {
    int32_t validMode = 0;
    if (strlen(mode) > 0) {
        for (int i = 0; i < ARRAY_SIZE(DISPLAY_MODE_LIST); i++) {
            if (strcmp(mode, DISPLAY_MODE_LIST[i]) == 0) {
                validMode = 1;
                break;
            }
        }
    }
    if (!validMode) {
        SYS_LOGI("the resolveResolution mode [%s] is not valid\n", mode);
        return -1;
    }

    /*
     * frame rate priority than resolution
     * ex:1080p60hz prefer to 2160p30hz
     */
    if (flag == MESON_POLICY_FRAMERATE) {
        for (int64_t index = 0; index < sizeof(MODE_FRAMERATE_FIRST)/sizeof(char *); index++) {
            if (strcmp(mode, MODE_FRAMERATE_FIRST[index]) == 0) {
                return index;
            }
        }
    } else {
        /*
         * resolution priority than frame rate
         * ex:2160p30hz prefer to 1080p60hz
         */
        for (int64_t index = 0; index < sizeof(MODE_RESOLUTION_FIRST)/sizeof(char *); index++) {
            if (strcmp(mode, MODE_RESOLUTION_FIRST[index]) == 0) {
                return index;
            }
        }
    }
    return -1;
}


static bool is_dv_support_mode(char *mode) {
    bool validMode = false;
    if (strlen(mode) != 0 && strstr(mode, "hz") != NULL) {
        if ((strstr(mode, "480p") == NULL) && (strstr(mode, "576p") == NULL)
        && (strstr(mode, "smpte") == NULL) && (strstr(mode, "4096") == NULL)
        && (strstr(mode, "i") == NULL)) {
            validMode = true;
        }
    }

    return validMode;
}

static void update_dv_mode(char *dv_maxmode,
                    char *cur_outputmode,
                    int dv_type,
                    char *final_displaymode,
                    enum meson_mode_policy policy) {
    char dv_displaymode[MESON_MODE_LEN] = {0};

    /*
     * 1. update tv support amdolby vision resolution
     */
    for (int i = DV_MODE_LIST_SIZE - 1; i >= 0; i--) {
        if (strstr(dv_maxmode, DV_MODE_LIST[i]) != NULL) {
            strcpy(dv_displaymode, DV_MODE_LIST[i]);
            break;
        }
    }

    /*
     * 2. find prefer amdolby vision resolution
     */
    if (policy == MESON_POLICY_BEST) {
        /* 2.1 best policy enable case */
        if (!strcmp(dv_displaymode, DV_MODE_4K2K60HZ)) {
            /* TV support amdolby vision 2160p60hz case */
            if (dv_type == DOLBY_VISION_LL_RGB) {
                /* amdolby vision LL RGB(rgb 10/12bit) only support 1080p60hz */
                strcpy(final_displaymode, DV_MODE_1080P);
            } else {
                /* other amdolby visin mode,use 2160p60hz */
                strcpy(final_displaymode, DV_MODE_4K2K60HZ);
            }
        } else {
            /* TV support amdolby vision non 2160p60hz case */
            if (!strcmp(dv_displaymode, DV_MODE_4K2K30HZ) ||
                    !strcmp(dv_displaymode, DV_MODE_4K2K25HZ) ||
                    !strcmp(dv_displaymode, DV_MODE_4K2K24HZ)) {
                /*
                 * TV support amdolby vision support 2160p30hz or 2160p25hz or 2160p24hz
                 * 1080p60hz prefer to 2160p30hz 2160p25hz 2160p24hz
                 */
                strcpy(final_displaymode, DV_MODE_1080P);
            } else {
                /*
                 * TV support amdolby vision non 2160p30hz 2160p25hz 2160p24hz
                 * use tv support amdolby vision resolution
                 */
                strcpy(final_displaymode, dv_displaymode);
            }
        }
    } else {
        /*
         * 2.1 best policy disable case
         * smpte(3840x2160@XXhz) and i timing not support amdolby vision
         * hdmi output resolution need small than amdolby vision resolution
         * x:amdolby vision support 1080p60hz,only can output small 1080p60hz resolution
         */
        if ((find_resolution_index(cur_outputmode, MESON_POLICY_RESOLUTION) >
                    find_resolution_index(dv_displaymode, MESON_POLICY_RESOLUTION)) ||
                !is_dv_support_mode(cur_outputmode)) {
            strcpy(final_displaymode, dv_displaymode);
        } else {
            strcpy(final_displaymode, cur_outputmode);
        }
    }

    SYS_LOGI("final_displaymode:%s, cur_outputmode:%s, dv_displaymode:%s", final_displaymode, cur_outputmode, dv_displaymode);
}

static int32_t dv_scene_process(struct meson_policy_in *input,
                                struct meson_policy_out *output,
                                enum meson_mode_policy policy) {
    /*
     * 1. update amdolby vision output type
     */
    int dv_type = update_dv_type(&input->hdr_info);

    output->dv_type = dv_type;
    SYS_LOGI("dv type:%d", output->dv_type);

    /*
     * 2. update amdolby vision output output mode to color format
     * 2.1 update amdolby vision deepcolor
     */
    char dv_attr[MESON_MODE_LEN] = {0};
    update_dv_attr(input->hdr_info.dv_deepcolor, dv_type, dv_attr);
    strcpy(output->deepcolor, dv_attr);
    SYS_LOGI("dv final_deepcolor:%s", output->deepcolor);

    /*
     * 2.2 update amdolby vision output resolution
     */
    char final_displaymode[MESON_MODE_LEN] = {0};
    char cur_displaymode[MESON_MODE_LEN] = {0};
    strcpy(cur_displaymode, input->cur_displaymode);

    update_dv_mode(input->hdr_info.dv_max_mode,
                   cur_displaymode,
                   dv_type,
                   final_displaymode,
                   policy);
    strcpy(output->displaymode, final_displaymode);
    SYS_LOGI("dv final_displaymode:%s", output->displaymode);

    return 0;
}

/* check resolution and color format support or not */
static bool mode_support_check(const char *mode, const char * color) {
    char outputmode[MESON_MODE_LEN] = {0};
    bool validmode = false;
    strcpy(outputmode, mode);
    strcat(outputmode, color);

    /* try support or not */
    validmode = meson_write_valid_mode_sys(DISPLAY_HDMI_VALID_MODE, outputmode);

    if (test_mode)
        return true;
    else
        return validmode;
}

/*
 * check 4k50/4k60 hdr support or not base driver edid
 */
static bool is_support_4kHDR(struct meson_policy_in *input,
                             struct meson_policy_out *output_info) {
    if (!output_info) {
        SYS_LOGE("output_info is NULL\n");
        return false;
    }

    const char **colorList = NULL;
    int colorList_length   = 0;

    /* use 4k hdr color format table */
    colorList = HDR_4K_COLOR_ATTRIBUTE_LIST;
    colorList_length = ARRAY_SIZE(HDR_4K_COLOR_ATTRIBUTE_LIST);

    /*
     * choose prefer color format and resolution for 4k hdr
     * modes_ptr:the list of TV support resolution from connector
     * dc_cap:the list of TV support color format from driver parse edid
     */
    for (int i = 0; i < colorList_length; i++) {
        if (strstr(input->con_info.dc_cap, colorList[i]) != NULL) {
            const char **resolutionList = NULL;
            int resolutionList_length   = 0;
            /* use 4k hdr resolution table */
            resolutionList = MODE_4K_LIST;
            resolutionList_length = ARRAY_SIZE(MODE_4K_LIST);
            for (int j = 0; j < resolutionList_length; j++) {
                meson_mode_info_t *modes_ptr = input->con_info.modes;
                for (int k = 0; k < input->con_info.modes_size; k ++) {
                    meson_mode_info_t *it = &modes_ptr[k];
                    if (strcmp(it->name, resolutionList[j]) == 0) {
                        if (mode_support_check(resolutionList[j], colorList[i])) {
                           SYS_LOGI("%s mode:[%s], deep color:[%s]\n", __FUNCTION__, resolutionList[j], colorList[i]);
                           strcpy(output_info->deepcolor, colorList[i]);
                           strcpy(output_info->displaymode, resolutionList[j]);
                           return true;
                        }
                    }
                }
            }
        }
    }

    SYS_LOGI("%s 4k hdr not support\n", __FUNCTION__);
    return false;
}

/* check non 4k hdr support or not */
static bool is_support_non4kHDR(struct meson_policy_in *input,
                                struct meson_policy_out *output_info) {
    if (!output_info) {
        SYS_LOGE("output_info is NULL\n");
        return false;
    }

    const char **colorList = NULL;
    int colorList_length = 0;

    /* use non 4k hdr color format table */
    colorList = HDR_NON4K_COLOR_ATTRIBUTE_LIST;
    colorList_length = ARRAY_SIZE(HDR_NON4K_COLOR_ATTRIBUTE_LIST);

    /*
     * choose prefer color format and resolution for non 4k hdr
     * modes_ptr:the list of TV support resolution from connector
     * dc_cap:the list of TV support color format from driver parse edid
     */
    for (int i = 0; i < colorList_length; i++) {
        if (strstr(input->con_info.dc_cap, colorList[i]) != NULL) {
            const char **resolutionList = NULL;
            int resolutionList_length = 0;
            /* use non 4k hdr resolution table */
            resolutionList = MODE_NON4K_LIST;
            resolutionList_length = ARRAY_SIZE(MODE_NON4K_LIST);
            for (int j = 0; j < resolutionList_length; j++) {
                meson_mode_info_t *modes_ptr = input->con_info.modes;
                for (int k = 0; k < input->con_info.modes_size; k ++) {
                    meson_mode_info_t *it = &modes_ptr[k];
                    if (strcmp(it->name, resolutionList[j]) == 0) {
                        if (mode_support_check(resolutionList[j], colorList[i])) {
                           SYS_LOGI("%s mode:[%s], deep color:[%s]\n", __FUNCTION__, resolutionList[j], colorList[i]);
                           strcpy(output_info->deepcolor, colorList[i]);
                           strcpy(output_info->displaymode, resolutionList[j]);
                           return true;
                        }
                    }
                }
            }
        }
    }

    SYS_LOGI("%s non 4k hdr not support\n", __FUNCTION__);
    return false;
}

static bool find_hdr_prefer_mode(struct meson_policy_in *input,
                                 struct meson_policy_out *output_info) {
    bool find = false;
    if (!output_info) {
        SYS_LOGE("output_info is NULL\n");
    } else {
        /*
         * box can support 4k case
         * find prefer 4k hdr resolution and color format base driver edid
         */
        if (input->con_info.is_support4k == true) {
            find = is_support_4kHDR(input, output_info);
        }

        /*
         * not find 4k hdr mode and find non 4k case
         * find prefer non 4k hdr resolution and color format base driver edid
         */
        if (find == false) {
            find = is_support_non4kHDR(input, output_info);
        }
    }

    return find;
}


/* TODO: need refactor */
/* check if the edid support current hdmi mode */
static bool is_support_HdmiMode(struct meson_policy_in *input, char* mode) {
    if (!mode) {
        SYS_LOGE("mode is NULL\n");
        return false;
    } else {
        /* check current resolution support or not base connector mode list */
        meson_mode_info_t *modes_ptr = input->con_info.modes;
        for (int i = 0; i < input->con_info.modes_size; i ++) {
            meson_mode_info_t *it = &modes_ptr[i];
            if (strcmp(it->name, mode) == 0) {
                strcpy(mode, it->name);
                SYS_LOGI("mode: %s\n", mode);
                return true;
            }
        }

        SYS_LOGI("mode: %s not support\n", mode);

        return false;
    }
}

static void get_best_deepcolor(struct meson_policy_in *input,
                               const char *outputmode, char* colorAttribute) {
    char *pos = NULL;
    int length = 0;
    const char **colorList = NULL;
    char supportedColorList[MESON_MAX_STR_LEN];
    strcpy(supportedColorList, input->con_info.dc_cap);

    /*
     * if read /sys/class/amhdmitx/amhdmitx0/dc_cap is NULL
     * return and use default color format(444 8bit)
     */
    if (!strlen(supportedColorList)) {
        if (!strcmp(outputmode, MODE_4K2K60HZ) || !strcmp(outputmode, MODE_4K2K50HZ)
            || !strcmp(outputmode, MODE_4K2KSMPTE60HZ) || !strcmp(outputmode, MODE_4K2KSMPTE50HZ)) {
            strcpy(colorAttribute, MESON_DEFAULT_COLOR_FORMAT_4K);
        } else {
            strcpy(colorAttribute, MESON_DEFAULT_COLOR_FORMAT);
        }

        SYS_LOGE("dc_cap is NULL\n");
        return;
    }

    /*
     * 1. select the color format table for different resolution or scene.
     */
    if (!strcmp(outputmode, MODE_4K2K60HZ) || !strcmp(outputmode, MODE_4K2K50HZ)
        || !strcmp(outputmode, MODE_4K2KSMPTE60HZ) || !strcmp(outputmode, MODE_4K2KSMPTE50HZ)) {
        /* 2160p50hz 2160p60hz 3840x2160p60hz 3840x2160p50hz case */
        if (input->hdr_info.is_lowpower_mode) {
            colorList = COLOR_ATTRIBUTE_LIST3;
            length = ARRAY_SIZE(COLOR_ATTRIBUTE_LIST3);
        } else {
            colorList = COLOR_ATTRIBUTE_LIST1;
            length = ARRAY_SIZE(COLOR_ATTRIBUTE_LIST1);
        }
    } else {
        /* except 2160p60hz 2160p50hz 3840x2160p60hz 3840x2160p60hz case */
        if (input->hdr_info.is_lowpower_mode) {
            //8bit prefer to 10bit for low power mode
            //detail priority as table
            colorList = COLOR_ATTRIBUTE_LIST4;
            length = ARRAY_SIZE(COLOR_ATTRIBUTE_LIST4);
        } else if (is_hdr_prefer(input)) {
            /*
             * hdr non 4k color format priority table
             * for user change resolution case
             * exp:connector 2160p60 420 8bit TV,when switch to 1080p60,
             * 10bit first for hdr,switch 2160p60,only 420 8bit.
             */
            colorList = COLOR_ATTRIBUTE_LIST2;
            length = ARRAY_SIZE(COLOR_ATTRIBUTE_LIST2);
        } else {
            //sdr non 4k color format priority table
            colorList = SDR_NON4K_COLOR_ATTRIBUTE_LIST;
            length = ARRAY_SIZE(SDR_NON4K_COLOR_ATTRIBUTE_LIST);
        }
    }

    /*
     * 2. select the preferred color format base resolution
     */
    for (int i = 0; i < length; i++) {
        if ((pos = strstr(supportedColorList, colorList[i])) != NULL) {
            //check resolution+color format support or not base driver edid
            if (mode_support_check(outputmode, colorList[i])) {
                SYS_LOGI("support current mode:[%s], deep color:[%s]\n", outputmode, colorList[i]);
                strcpy(colorAttribute, colorList[i]);
                break;
            }
        }
    }

    /*
     * TODO: what about not find
     */
}

static bool hdr_scene_process(struct meson_policy_in *input,
                              struct meson_policy_out *output_info,
                              enum meson_mode_policy policy) {
    bool find = false;

    SYS_LOGI("policy:%d isBestColor:%d state:%d", policy, input->con_info.is_bestcolorspace, input->state);

    if ((input->state == MESON_SCENE_STATE_INIT) ||
        (input->state == MESON_SCENE_STATE_POWER)) {
        if (policy == MESON_POLICY_BEST && input->con_info.is_bestcolorspace) {
            /*
             * best policy enable case
             * and except from third apk or framework set mode.
             */
            find = find_hdr_prefer_mode(input, output_info);
        } else if (policy == MESON_POLICY_BEST || policy == MESON_POLICY_RESOLUTION || policy == MESON_POLICY_FRAMERATE) {
            const char **resolutionList = NULL;
            int resolutionList_length   = 0;
            if (policy == MESON_POLICY_BEST) {
                resolutionList        = MODE_FRAMERATE_FIRST;
                resolutionList_length = ARRAY_SIZE(MODE_FRAMERATE_FIRST);
            } else {
                resolutionList        = MODE_RESOLUTION_FIRST;
                resolutionList_length = ARRAY_SIZE(MODE_RESOLUTION_FIRST);
            }

            for (int j = resolutionList_length - 1; j >= 0 ; j--) {
                /* if current mode support,use current mode */
                meson_mode_info_t *modes_ptr = input->con_info.modes;

                for (int i = 0; i < input->con_info.modes_size; i ++) {
                    meson_mode_info_t *it = &modes_ptr[i];

                    if (!strcmp(it->name, resolutionList[j])) {
                        if (mode_support_check(resolutionList[j], input->con_info.ubootenv_colorattr)) {
                            SYS_LOGI("%s mode:[%s], deep color:[%s]\n", __FUNCTION__, resolutionList[j], input->con_info.ubootenv_colorattr);
                            strcpy(output_info->deepcolor, input->con_info.ubootenv_colorattr);
                            strcpy(output_info->displaymode, resolutionList[j]);
                            find = true;
                            break;
                        }
                    }
                }

                if (find) {
                    break;
                }
            }
        } else if (input->con_info.is_bestcolorspace) {
            const char **colorList = NULL;
            int colorList_length   = 0;

            if (!strcmp(input->cur_displaymode, MODE_4K2K60HZ) || !strcmp(input->cur_displaymode, MODE_4K2K50HZ)
            || !strcmp(input->cur_displaymode, MODE_4K2KSMPTE60HZ) || !strcmp(input->cur_displaymode, MODE_4K2KSMPTE50HZ)
            || !strcmp(input->cur_displaymode, MODE_4K2K100HZ) || !strcmp(input->cur_displaymode, MODE_4K2K120HZ)
            || !strcmp(input->cur_displaymode, MODE_8K4K60HZ) || !strcmp(input->cur_displaymode, MODE_8K4K50HZ)
            || !strcmp(input->cur_displaymode, MODE_8K4K48HZ) || !strcmp(input->cur_displaymode, MODE_8K4K30HZ)
            || !strcmp(input->cur_displaymode, MODE_8K4K25HZ) || !strcmp(input->cur_displaymode, MODE_8K4K24HZ)) {
                //2160p50hz 2160p60hz 3840x2160p60hz 3840x2160p50hz case
                //use 4k color format table
                colorList        = COLOR_ATTRIBUTE_LIST1;
                colorList_length = ARRAY_SIZE(COLOR_ATTRIBUTE_LIST1);
            } else {
                //except 2160p60hz 2160p50hz 3840x2160p60hz 3840x2160p60hz case
                //use non 4k color format table
                colorList        = COLOR_ATTRIBUTE_LIST2;
                colorList_length = ARRAY_SIZE(COLOR_ATTRIBUTE_LIST2);
            }

            for (int j = 0; j < colorList_length; j++) {
                if (strstr(input->con_info.dc_cap, colorList[j]) != NULL) {
                    if (mode_support_check(input->cur_displaymode, colorList[j])) {
                        SYS_LOGI("%s mode:[%s], deep color:[%s]\n", __FUNCTION__, input->cur_displaymode, colorList[j]);
                        strcpy(output_info->deepcolor, colorList[j]);
                        strcpy(output_info->displaymode, input->cur_displaymode);
                        find = true;
                        break;
                   }
              }
            }
        } else {
            //1.check mode+color format support or not
            if (mode_support_check(input->cur_displaymode, input->con_info.ubootenv_colorattr)) {
                SYS_LOGI("support current mode:[%s], deep color:[%s]\n", input->cur_displaymode, input->con_info.ubootenv_colorattr);
                strcpy(output_info->deepcolor, input->con_info.ubootenv_colorattr);
                strcpy(output_info->displaymode, input->cur_displaymode);
                find = true;
            } else if (is_support_HdmiMode(input, input->cur_displaymode)) {
                SYS_LOGI("support current mode:[%s]\n", input->cur_displaymode);
                /*
                 * 2.check cur_displaymode support or not
                 * if displaymode support ,and find best color format base mode.
                 */
                char color_attribute[MESON_MODE_LEN] = {0};
                get_best_deepcolor(input, input->cur_displaymode, color_attribute);
                strcpy(output_info->deepcolor, color_attribute);
                strcpy(output_info->displaymode, input->cur_displaymode);
                find = true;
            }
        }

        //not find support mode and colorspace and try best policy
        if (!find && !(policy == MESON_POLICY_BEST && input->con_info.is_bestcolorspace)) {
            //best policy case
            find = find_hdr_prefer_mode(input, output_info);
        }
     } else {
         //switch UI case
         //1.check cur_displaymode + ubootenv.var.colorattribute support or not
         // and except from third apk or framework set mode.
         if (mode_support_check(input->cur_displaymode, input->con_info.ubootenv_colorattr)
             && !input->con_info.is_bestcolorspace) {
             SYS_LOGI("support current mode:[%s], deep color:[%s]\n", input->cur_displaymode, input->con_info.ubootenv_colorattr);
             strcpy(output_info->deepcolor, input->con_info.ubootenv_colorattr);
             strcpy(output_info->displaymode, input->cur_displaymode);
             find = true;
         } else if (is_support_HdmiMode(input, input->cur_displaymode)) {
             SYS_LOGI("support current mode:[%s]\n", input->cur_displaymode);
             /*
              * 2.check cur_displaymode support or not
              * if displaymode support ,and find best color format base mode.
              */
             char color_attribute[MESON_MODE_LEN] = {0};
             get_best_deepcolor(input, input->cur_displaymode, color_attribute);
             strcpy(output_info->deepcolor, color_attribute);
             strcpy(output_info->displaymode, input->cur_displaymode);
             find = true;
         } else {
             //3.find best hdr prefer mode
             find = find_hdr_prefer_mode(input, output_info);
         }
    }

    return find;
}


static void get_highest_mode_by_policy(struct meson_policy_in *input,
        char *mode, enum meson_mode_policy policy) {
    meson_mode_info_t *modes_ptr = input->con_info.modes;
    meson_mode_info_t *config_ptr = NULL;

    for (int i = 0; i < input->con_info.modes_size; i ++) {
        meson_mode_info_t *it = &modes_ptr[i];

        /* not select smpte and interlace mode */
        if ((strstr(it->name, "smpte") != NULL) || (strstr(it->name, "i") != NULL))
            continue;

        if (!config_ptr) {
            config_ptr = it;
            continue;
        }

        if (policy == MESON_POLICY_BEST || policy == MESON_POLICY_FRAMERATE)  {
            /*
             * frame rate policy: choose the mode which has the highest refresh rate
             * If the refresh rate is same, then find the hightest resolution
             */
            if (config_ptr->refresh_rate < it->refresh_rate) {
                config_ptr = it;
            } else if (config_ptr->refresh_rate == it->refresh_rate) {
                if (config_ptr->pixel_w < it->pixel_w && config_ptr->pixel_h < it->pixel_h)
                    config_ptr = it;
            }
        } else if (policy == MESON_POLICY_RESOLUTION) {
            if (config_ptr->pixel_h < it->pixel_h) {
                config_ptr = it;
            } else if (config_ptr->pixel_h == it->pixel_h) {
                if (config_ptr->pixel_w < it->pixel_w) {
                    config_ptr = it;
                } else if (config_ptr->refresh_rate < it->refresh_rate) {
                    config_ptr = it;
                }
            }
        }
    }

    if (config_ptr) {
        strcpy(mode, config_ptr->name);
    }
}

static void get_hdmi_outputmode(struct meson_policy_in *input,
                                char *mode, enum meson_mode_policy policy) {
    /* Fall back to 480p if EDID can't be parsed */
    if (strcmp(input->con_info.edid_parsing, "ok")) {
        strcpy(mode, MESON_DEFAULT_HDMI_MODE);
        SYS_LOGE("EDID parsing error detected\n");
        return;
    }

    if (policy == MESON_POLICY_INVALID) {
        /* if current mode support,use current mode */
        meson_mode_info_t *modes_ptr = input->con_info.modes;

        for (int i = 0; i < input->con_info.modes_size; i ++) {
            meson_mode_info_t *it = &modes_ptr[i];

            if (!strcmp(it->name, input->cur_displaymode)) {
                strcpy(mode, input->cur_displaymode);
                return;
            }
        }
         /* if current mode not support,find best prefer resolution base driver edid */
        get_highest_mode_by_policy(input, mode, MESON_POLICY_BEST);
    } else {
        get_highest_mode_by_policy(input, mode, policy);
    }

    SYS_LOGI("set HDMI mode to %s\n", mode);
}

static void get_hdmi_color_attr(struct meson_policy_in *input,
                                const char *outputmode,
                                char *color_attr,
                                enum meson_mode_policy policy __unused) {
    char supportedColorList[MESON_MAX_STR_LEN];
    strcpy(supportedColorList, input->con_info.dc_cap);

    /*
     * if read /sys/class/amhdmitx/amhdmitx0/dc_cap is NULL.
     * use default color format
     */
    if (!strlen(supportedColorList)) {
        if (!strcmp(outputmode, MODE_4K2K60HZ) || !strcmp(outputmode, MODE_4K2K50HZ)
            || !strcmp(outputmode, MODE_4K2KSMPTE60HZ) || !strcmp(outputmode, MODE_4K2KSMPTE50HZ)) {
            strcpy(color_attr, MESON_DEFAULT_COLOR_FORMAT_4K);
        } else {
            strcpy(color_attr, MESON_DEFAULT_COLOR_FORMAT);
        }

        SYS_LOGE("Error!!! Do not find sink color list, use default color attribute:%s\n", color_attr);
        return;
    }

    /*
     * use ubootenv.var.colorattribute when best color space policy is disable
     * will check resolution + color format be support TV EDID
     */
    if (input->con_info.is_bestcolorspace == false) {
        char colorTemp[MESON_MODE_LEN] = {0};
        strcpy(colorTemp, input->con_info.ubootenv_colorattr);
        if (mode_support_check(outputmode, colorTemp)) {
            strcpy(color_attr, input->con_info.ubootenv_colorattr);
        } else {
            get_best_deepcolor(input, outputmode, color_attr);
        }
    } else {
        /*
         * best policy enable case
         * select the preferred color format base outputmode(resolution)
         */
        get_best_deepcolor(input, outputmode, color_attr);
    }

    //1.if colorAttr is NULL above steps, will defines a initial value to it
    //2.edid_parsing ng,will defines a initial value to it
    if (!strstr(color_attr, "bit")
        || strcmp(input->con_info.edid_parsing, "ok")) {
        strcpy(color_attr, MESON_DEFAULT_COLOR_FORMAT);
    }

    SYS_LOGI("get hdmi color attribute : [%s], outputmode is: [%s] , and support color list is: [%s]\n",
        color_attr, outputmode, supportedColorList);
}


static void update_deepcolor(struct meson_policy_in *input,
                             const char* outputmode,
                             char* color,
                             enum meson_mode_policy policy) {
    if (input->con_info.is_deepcolor == true) {
        /* deep color(10/12bit) enable case */
        get_hdmi_color_attr(input, outputmode, color, policy);
    } else {
        /* deep color disable case */
        strcpy(color, "default");
    }

    SYS_LOGI("colorAttribute = %s\n", color);
}

static void sdr_scene_process(struct meson_policy_in *input,
                              struct meson_policy_out *output_info,
                              enum meson_mode_policy policy) {
    SYS_LOGI("%s", __func__);
    /*
     * 1. choose resolution and frame rate
     */
    if ((input->state == MESON_SCENE_STATE_INIT) || (input->state == MESON_SCENE_STATE_POWER)) {
        /*
         * boot/hot plug/suspend/resmue case
         * choose resolution, frame rate base driver edid
         */
        char outputmode[MESON_MODE_LEN] = {0};

        if (MESON_SINK_TYPE_NONE != input->con_info.sink_type) {
            /* hdmi connect */
            get_hdmi_outputmode(input, outputmode, policy);
        } else {
            /* hdmi not connect */
            strcpy(outputmode, input->con_info.ubootenv_cvbsmode);
        }
        /* not find prefer resolution,use default resolution */
        if (strlen(outputmode) == 0) {
            strcpy(outputmode, MESON_DEFAULT_HDMI_MODE);
        }

        strcpy(output_info->displaymode, outputmode);
        SYS_LOGI("final_displaymode:%s\n", output_info->displaymode);
    } else if (input->state == MESON_SCENE_STATE_SWITCH) {
        /*
         * user/framework change scene
         * doesn't read hdmi info for ui switch scene
         * use user want to set resolution and frame rate
         */
        strcpy(output_info->displaymode, input->cur_displaymode);
        SYS_LOGI("final_displaymode:%s\n", output_info->displaymode);
    }

    /*
     * 2. choose color format, bit-depth
     */
    char color_attribute[MESON_MODE_LEN] = {0};
    update_deepcolor(input, output_info->displaymode, color_attribute, policy);
    strcpy(output_info->deepcolor, color_attribute);
    SYS_LOGI("final_deepcolor = %s\n", output_info->deepcolor);
}

/*
 * Set the mode policy
 *
 * @param connector         [in] connector type info
 * @param policy            [in] meson policy
 *
 * Return of a value other than 0 means an error has occurred:
 * -EINVAL - invalid connector type
 */
int32_t meson_mode_set_policy(int32_t connector, const meson_mode_policy_e policy) {
    GET_CURRENT_POLICY(connector);
    SYS_LOGI("connector:%d to policy:%d", connector, policy);
    mp->policy = policy;
    return 0;
}

/*
 * set mode policy input
 *
 * @param connector         [in] connector type info
 * @param input             [in] meson policy input, like hdr info, connector info
 *
 * Return of a value other than 0 means an error has occurred:
 * -EINVAL - invalid connector type or input
 */
int32_t meson_mode_set_policy_input(int32_t connector, const struct meson_policy_in *input) {
    GET_CURRENT_POLICY(connector);

    if (input == NULL)
        return -EINVAL;

    mp->input = *input;
    return 0;
}

/*
 * get mode policy output
 * @param connector         [in] connector type info
 * @param output            [out] meson policy output: mode info, colorspace
 *
 * Return of a value other than 0 means an error has occurred:
 * -EINVAL - invalid connector type or input
 */
int32_t meson_mode_get_policy_output(int32_t connector, struct meson_policy_out *output) {
    GET_CURRENT_POLICY(connector);
    struct meson_policy_in *input = &mp->input;
    enum meson_mode_policy policy = mp->policy;

    /* no output */
    if (output ==  NULL)
        return -EINVAL;

    struct meson_policy_out  scene_output_info;
    memset(&scene_output_info, 0, sizeof(scene_output_info));

    /*
     * 1. amdolby vision scene process
     *    only for tv support dv and box enable dv
     */
    if (is_dv_prefer(input) == true) {
        dv_scene_process(input, &scene_output_info, policy);
    } else if (input->hdr_info.is_enable_dv) {
        /* for enable amdolby vision core when first boot connecting non dv tv */
        output->dv_type = DOLBY_VISION_STD_ENABLE;
    } else {
        /* for UI disable amdolby vision core and boot keep the status */
        output->dv_type = DOLBY_VISION_DISABLE;
    }

    /*
     * 2. hdr/sdr scene process
     *    and decide final display mode and deepcolor
     */
    if (is_dv_prefer(input) == true) {
        strcpy(output->displaymode, scene_output_info.displaymode);
        strcpy(output->deepcolor, scene_output_info.deepcolor);
        output->dv_type = scene_output_info.dv_type;
    } else if (is_hdr_prefer(input) == 1) {
        hdr_scene_process(input, &scene_output_info, policy);
        strcpy(output->displaymode, scene_output_info.displaymode);
        strcpy(output->deepcolor, scene_output_info.deepcolor);
    } else {
        sdr_scene_process(input, &scene_output_info, policy);
        strcpy(output->displaymode, scene_output_info.displaymode);
        strcpy(output->deepcolor, scene_output_info.deepcolor);
    }

    /*
     * 3. not find outputmode and use default mode
     */
    if (strlen(output->displaymode) == 0) {
        strcpy(output->displaymode, MESON_DEFAULT_HDMI_MODE);
    }

    /*
     * 4. not find color space and use default mode
     */
    if (!strstr(output->deepcolor, "bit")) {
        strcpy(output->deepcolor, MESON_DEFAULT_COLOR_FORMAT);
    }

    mp->output = *output;

    SYS_LOGI("final_displaymode:%s, final_deepcolor:%s, dv_type:%d\n",
        output->displaymode, output->deepcolor, output->dv_type);

    return 0;
}

/*
 * @param enable         [in] enable test mode or not
 *
 */
void meson_mode_set_test_mode(const bool enable) {
    test_mode = enable;
}


/*
 * @param type: dv,hdr,sdr
 */
int32_t meson_mode_support_mode(int32_t connector, int32_t type, char *mode) {
    SYS_LOGI("%s type:%d mode:%s", __func__, type, mode);
    GET_CURRENT_POLICY(connector);
    struct meson_policy_in *input = &mp->input;

    int32_t ret = -EINVAL;
    meson_mode_info_t *modes_ptr = input->con_info.modes;
    meson_mode_info_t *config_ptr = NULL;

    for (int i = 0; i < input->con_info.modes_size; i ++) {
        meson_mode_info_t *it = &modes_ptr[i];

        if (!strcmp(it->name, mode)) {
            config_ptr = it;
            break;
        }
    }
    if (!config_ptr) {
        SYS_LOGI("%s could not find mode:%s", __func__, mode);
        return -EINVAL;
    }

    int length = 0;
    const char **colorList = NULL;
    char supportedColorList[MESON_MAX_STR_LEN];
    strcpy(supportedColorList, input->con_info.dc_cap);

    if (type == MESON_HDR10_PRIORITY) {
        //1. select the color format table for different resolution
        if (config_ptr->pixel_w >= 2160) {
            //resolution support 420 case
            colorList = HDR_4K_COLOR_ATTRIBUTE_LIST;
            length = ARRAY_SIZE(HDR_4K_COLOR_ATTRIBUTE_LIST);
        } else {
            colorList = HDR_NON4K_COLOR_ATTRIBUTE_LIST;
            length    = ARRAY_SIZE(HDR_NON4K_COLOR_ATTRIBUTE_LIST);
        }
        //2. check support or not
        for (int i = 0; i < length; i++) {
            if (strstr(supportedColorList, colorList[i]) != NULL) {
                //check resolution+color format support or not base driver edid
                if (mode_support_check(mode, colorList[i])) {
                    SYS_LOGI("support current mode:[%s], deep color:[%s]\n", mode, colorList[i]);
                    ret = 0;
                    break;
                }
            }
        }
    } else if (type == MESON_DOLBY_VISION_PRIORITY) {
        // 1. update tv support amdolby vision resolution
        char dv_displaymode[MESON_MODE_LEN] = {0};
        for (int i = DV_MODE_LIST_SIZE - 1; i >= 0; i--) {
            if (strstr(input->hdr_info.dv_max_mode, DV_MODE_LIST[i]) != NULL) {
                strcpy(dv_displaymode, DV_MODE_LIST[i]);
                break;
            }
        }

        meson_mode_info_t *current_ptr = NULL;
        meson_mode_info_t *max_dv_ptr = NULL;
        meson_mode_info_t *modes_ptr = input->con_info.modes;
        for (int i = 0; i < input->con_info.modes_size; i ++) {
            meson_mode_info_t *it = &modes_ptr[i];

            if (!strcmp(it->name, dv_displaymode)) {
                max_dv_ptr = it;
            }
            if (!strcmp(it->name, mode)) {
                current_ptr =it;
            }
        }

        if (!current_ptr || !max_dv_ptr) {
            SYS_LOGI("could not find max dv or current mode");
            return -EINVAL;
        }

        if ((current_ptr->pixel_w > max_dv_ptr->pixel_w) ||
                (current_ptr->pixel_w == max_dv_ptr->pixel_w &&
                    current_ptr->refresh_rate -1  > max_dv_ptr->refresh_rate)) {
            SYS_LOGI("dv not support current mode :%s (%dx%d@%.2f), max dv: %s(%dx%d@%.2f)",
                    current_ptr->name, current_ptr->pixel_w,
                    current_ptr->pixel_h, current_ptr->refresh_rate,
                    max_dv_ptr->name, max_dv_ptr->pixel_w,
                    max_dv_ptr->pixel_h, max_dv_ptr->refresh_rate);
            return -EINVAL;
        }

        int dv_type = update_dv_type(&input->hdr_info);
        char dv_attr[MESON_MODE_LEN] = {0};
        update_dv_attr(input->hdr_info.dv_deepcolor, dv_type, dv_attr);
        if (mode_support_check(mode, dv_attr)) {
            SYS_LOGI("support current mode:[%s], deep color:[%s]\n", mode, dv_attr);
            ret = 0;
        }
    } else if (type == MESON_SDR_PRIORITY) {
        // no check for sdr
        ret = 0;
    }

    return ret;
}

const char *meson_hdrPriorityToString(int32_t type) {
    const char * typeStr;
    switch (type) {
        case MESON_DOLBY_VISION_PRIORITY:
            typeStr = "DV Priority";
            break;
        case MESON_HDR10_PRIORITY:
            typeStr = "HDR10 Priority";
            break;
        case MESON_SDR_PRIORITY:
            typeStr = "SDR priority";
            break;
        case MESON_G_DV_HDR10_HLG:
            typeStr = "DV_HDR10_HLG priority";
            break;
        case MESON_G_DV_HDR10:
            typeStr = "DV_HDR10 priority";
            break;
        case MESON_G_DV_HLG:
            typeStr = "DV_HLG priority";
            break;
        case MESON_G_HDR10_HLG:
            typeStr = "HDR10_HLG priority";
            break;
        case MESON_G_DV:
            typeStr = "DV priority";
            break;
        case MESON_G_HDR10:
            typeStr = "HDR10 priority";
            break;
        case MESON_G_HLG:
            typeStr = "HLG priority";
            break;
        case MESON_G_SDR:
            typeStr = "SDR priority";
            break;
        default :
            typeStr = "INVALID";
            break;
    }
    return typeStr;
}

const char *meson_hdrPolicyToString(int32_t type) {
    const char * typeStr;
    switch (type) {
        case MESON_HDR_POLICY_SINK:
            typeStr = "follow sink";
            break;
        case MESON_HDR_POLICY_SOURCE:
            typeStr = "follow source";
            break;
        case MESON_HDR_POLICY_FORCE:
            typeStr = "force";
            break;
        default:
            typeStr = "INVALID";
            break;
    }
    return typeStr;
}

const char *meson_dvModeTypeToString(const char *dvMode) {
    const char * typeStr;
    if (strstr(dvMode, "current dv_mode = HDR10")) {
        typeStr = MESON_FORCE_MODE_TYPE[MESON_HDR_FORCE_MODE_HDR10];
    } else if (strstr(dvMode, "current dv_mode = IPT_TUNNEL")) {
        typeStr = MESON_FORCE_MODE_TYPE[MESON_HDR_FORCE_MODE_DV];
    } else if (strstr(dvMode, "current dv_mode = SDR8") ||
                  strstr(dvMode, "current dv_mode = SDR10")) {
        typeStr = MESON_FORCE_MODE_TYPE[MESON_HDR_FORCE_MODE_SDR];
    } else {
        typeStr = MESON_FORCE_MODE_TYPE[MESON_HDR_FORCE_MODE_INVALID];
    }
    return typeStr;
}
