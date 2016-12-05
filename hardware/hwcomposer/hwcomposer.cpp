/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0

#include <hardware/hardware.h>

#include <fcntl.h>
#include <errno.h>

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/ioctl.h>

#include <hardware/hwcomposer.h>
#include <drv_display_sun4i.h>
#include <fb.h>
#include <EGL/egl.h>
#include <hardware_legacy/uevent.h>
#include <sys/resource.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <linux/netlink.h>
#include <poll.h>

#include "hwccomposer_priv.h"
#include <cutils/properties.h> 
#include <utils/Timers.h>

#define LOGV ALOGV
#define LOGD ALOGD
#define LOGE ALOGE

#define STATUS_LOG ALOGV
#define RECT_LOG ALOGV

#define BOOTVIDEO_SCREEN_ID 0
/*****************************************************************************/
static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device);

static struct hw_module_methods_t hwc_module_methods = {
    .open = hwc_device_open
};

hwc_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = 1,
        .hal_api_version = 0,
        .id = HWC_HARDWARE_MODULE_ID,
        .name = "Sample hwcomposer module",
        .author = "The Android Open Source Project",
        .methods = &hwc_module_methods,
    }
};

/*****************************************************************************/

static void dump_layer(hwc_layer_1_t const* l) {
    ALOGD("\ttype=%d, flags=%08x, handle=%p, tr=%02x, blend=%04x, {%d,%d,%d,%d}, {%d,%d,%d,%d}",
            l->compositionType, l->flags, l->handle, l->transform, l->blending,
            l->sourceCrop.left,
            l->sourceCrop.top,
            l->sourceCrop.right,
            l->sourceCrop.bottom,
            l->displayFrame.left,
            l->displayFrame.top,
            l->displayFrame.right,
            l->displayFrame.bottom);
}
static int hwc_video_open(sun4i_hwc_context_t *ctx, int screen_idx)
{
	unsigned long args[4]={0};

    ALOGD("#%s screen_idx:%d\n", __FUNCTION__, screen_idx);

    if(ctx->video_layerhdl[screen_idx] != 0)
    {        
        args[0]                         = screen_idx;
        args[1]                         = ctx->video_layerhdl[screen_idx];
        ioctl(ctx->dispfd, DISP_CMD_LAYER_OPEN,args);

        ctx->status[screen_idx] |= HWC_STATUS_OPENED;
        STATUS_LOG("#status[%d]=%d in %s", screen_idx, ctx->status[screen_idx], __FUNCTION__);
    }

	return 0;
}

static int hwc_video_close(sun4i_hwc_context_t *ctx, int screen_idx)
{
	unsigned long args[4]={0};

    ALOGD("#%s screen_idx:%d\n", __FUNCTION__, screen_idx);

    if(ctx->video_layerhdl[screen_idx] != 0)
    {
        args[0]                         = screen_idx;
        args[1]                         = ctx->video_layerhdl[screen_idx];
        ioctl(ctx->dispfd, DISP_CMD_LAYER_CLOSE,args);

        ctx->status[screen_idx] &= (HWC_STATUS_OPENED_MASK);
        STATUS_LOG("#status[%d]=%d in %s", screen_idx, ctx->status[screen_idx], __FUNCTION__);
    }
        
	return 0;
}

static int hwc_video_request(sun4i_hwc_context_t *ctx, int screen_idx, layerinitpara_t *layer_init_para)
{
    __disp_layer_info_t         layer_info;
    __disp_colorkey_t           ck;
    unsigned long               args[4]={0};
    int                         ret;

    ALOGD("#%s screen_idx:%d\n", __FUNCTION__, screen_idx);

    if(screen_idx == 0)
    {
        ioctl(ctx->mFD_fb[0], FBIOGET_LAYER_HDL_0, &ctx->ui_layerhdl[0]);
    }
    else
    {
        ioctl(ctx->mFD_fb[1], FBIOGET_LAYER_HDL_1, &ctx->ui_layerhdl[1]);
    }

    if(ctx->video_layerhdl[screen_idx] != 0)
    {
        //should not be here, something wrong????
        args[0] = screen_idx;
        args[1] = ctx->video_layerhdl[screen_idx];
        ctx->video_layerhdl[screen_idx] = (uint32_t)ioctl(ctx->dispfd, DISP_CMD_LAYER_RELEASE,args);
    }
    
    args[0] = screen_idx;
    ctx->video_layerhdl[screen_idx] = (uint32_t)ioctl(ctx->dispfd, DISP_CMD_LAYER_REQUEST,args);
    if(ctx->video_layerhdl[screen_idx] == 0)
    {
        ALOGD("####request layer failed in %s!\n", __FUNCTION__);
        return -1;
    }

    memset(&layer_info, 0, sizeof(__disp_layer_info_t));
    if (layer_init_para->h < 720)
    {
        layer_info.fb.cs_mode = DISP_BT601;
    }
    else
    {
        layer_info.fb.cs_mode = DISP_BT709;
    }
    layer_info.fb.mode = DISP_MOD_MB_UV_COMBINED;
    layer_info.fb.seq = DISP_SEQ_UVUV;
    switch(layer_init_para->format)
    {
        case HWC_FORMAT_MBYUV420:
            layer_info.fb.format = DISP_FORMAT_YUV420;
            layer_info.fb.mode = DISP_MOD_MB_UV_COMBINED;
            break;
        case HWC_FORMAT_MBYUV422:
            layer_info.fb.format = DISP_FORMAT_YUV422;
            layer_info.fb.mode = DISP_MOD_MB_UV_COMBINED;
            break;
        case HWC_FORMAT_YUV420PLANAR:
            layer_info.fb.format = DISP_FORMAT_YUV420;
            layer_info.fb.mode = DISP_MOD_NON_MB_PLANAR;
            layer_info.fb.seq = DISP_SEQ_P3210;
            break;
        case HWC_FORMAT_RGBA_8888:
            layer_info.fb.format = DISP_FORMAT_ARGB8888;
            layer_info.fb.mode = DISP_MOD_NON_MB_PLANAR;
            layer_info.fb.seq = DISP_SEQ_P3210;
            break;
        case HWC_FORMAT_DEFAULT:
		    layer_info.fb.format = DISP_FORMAT_YUV420;
		    //layer_info.fb.mode = DISP_MOD_MB_UV_COMBINED;
		    layer_info.fb.mode = DISP_MOD_NON_MB_UV_COMBINED;
		    layer_info.fb.seq = DISP_SEQ_VUVU;
		    break;
        default:
            layer_info.fb.format = DISP_FORMAT_YUV420;
            layer_info.fb.mode = DISP_MOD_NON_MB_UV_COMBINED;
            break;
    }
    layer_info.fb.br_swap         = 0;
    layer_info.fb.addr[0]         = 0;
    layer_info.fb.addr[1]         = 0;
    layer_info.fb.addr[2]         = 0;
    layer_info.fb.size.width      = layer_init_para->w;
    layer_info.fb.size.height     = layer_init_para->h;
    layer_info.mode               = DISP_LAYER_WORK_MODE_SCALER;
    layer_info.alpha_en           = 1;
    layer_info.alpha_val          = 0xff;
    layer_info.pipe               = 1;
    layer_info.src_win.x          = 0;
    layer_info.src_win.y          = 0;
    layer_info.src_win.width      = 1;
    layer_info.src_win.height     = 1;
    layer_info.scn_win.x          = 0;
    layer_info.scn_win.y          = 0;
    layer_info.scn_win.width      = 1;
    layer_info.scn_win.height     = 1;

    args[0]                         = screen_idx;
    args[1]                         = ctx->video_layerhdl[screen_idx];
    args[2]                         = (unsigned long) (&layer_info);
    args[3]                         = 0;
    ioctl(ctx->dispfd, DISP_CMD_LAYER_SET_PARA, args);

    args[0]                         = screen_idx;
    args[1]                         = ctx->video_layerhdl[screen_idx];
    ioctl(ctx->dispfd, DISP_CMD_LAYER_BOTTOM, args);
    
    ck.ck_min.alpha                 = 0xff;
    ck.ck_min.red                   = 0x00; //0x01;
    ck.ck_min.green                 = 0x00; //0x03;
    ck.ck_min.blue                  = 0x00; //0x05;
    ck.ck_max.alpha                 = 0xff;
    ck.ck_max.red                   = 0x00; //0x01;
    ck.ck_max.green                 = 0x00; //0x03;
    ck.ck_max.blue                  = 0x00; //0x05;
    ck.red_match_rule               = 2;
    ck.green_match_rule             = 2;
    ck.blue_match_rule              = 2;
    args[0]                         = screen_idx;
    args[1]                         = (unsigned long)&ck;
    ioctl(ctx->dispfd,DISP_CMD_SET_COLORKEY,(void*)args);

    args[0]                         = screen_idx;
    args[1]                         = ctx->ui_layerhdl[screen_idx];
    ioctl(ctx->dispfd,DISP_CMD_LAYER_CK_OFF,(void*)args);

    args[0]                         = screen_idx;
    args[1]                         = ctx->ui_layerhdl[screen_idx];
    args[2]                         = (unsigned long) (&layer_info);
    args[3]                         = 0;
    ret = ioctl(ctx->dispfd, DISP_CMD_LAYER_GET_PARA, args);
    if(ret < 0)
    {
        ALOGD("####DISP_CMD_LAYER_GET_PARA fail in %s, screen_idx:%d,hdl:%d\n", __FUNCTION__, screen_idx, ctx->ui_layerhdl[screen_idx]);
    }

    if(1)//layer_info.mode == DISP_LAYER_WORK_MODE_SCALER)
    {
        args[0]                     = screen_idx;
        args[1]                     = ctx->video_layerhdl[screen_idx];
        ioctl(ctx->dispfd,DISP_CMD_LAYER_CK_ON,(void*)args);
    }
    else
    {
        args[0]                     = screen_idx;
        args[1]                     = ctx->video_layerhdl[screen_idx];
        ioctl(ctx->dispfd,DISP_CMD_LAYER_CK_OFF,(void*)args);
    }

    args[0]                         = screen_idx;
    args[1]                         = ctx->ui_layerhdl[screen_idx];
    ioctl(ctx->dispfd, DISP_CMD_LAYER_ALPHA_OFF, args);    
    
    args[0]                         = screen_idx;
    args[1]                         = ctx->video_layerhdl[screen_idx];
    ret = ioctl(ctx->dispfd, DISP_CMD_VIDEO_START, args);

    ctx->w = layer_init_para->w;
    ctx->h = layer_init_para->h;
    ctx->format = layer_init_para->format;

    
    return 0;
}

static int hwc_video_release(sun4i_hwc_context_t *ctx, int screen_idx)
{
	unsigned long args[4]={0};

    ALOGD("#hwc_video_release screen_idx:%d\n", screen_idx);

    if(ctx->video_layerhdl[screen_idx] != 0)
    {
        args[0]                         = screen_idx;
        args[1]                         = ctx->video_layerhdl[screen_idx];
        ioctl(ctx->dispfd, DISP_CMD_LAYER_CLOSE,args);

        args[0]                         = screen_idx;
        args[1]                         = ctx->video_layerhdl[screen_idx];
        ioctl(ctx->dispfd, DISP_CMD_VIDEO_STOP, args);

        usleep(20 * 1000);

        args[0]                         = screen_idx;
        args[1]                         = ctx->video_layerhdl[screen_idx];
        ioctl(ctx->dispfd, DISP_CMD_LAYER_RELEASE, args);

        ctx->video_layerhdl[screen_idx] = 0;
    }

    memset(&ctx->rect_in, 0, sizeof(hwc_rect_t));
    memset(&ctx->rect_out, 0, sizeof(hwc_rect_t));
    ctx->cur_3denable = 0;

    ctx->status[screen_idx] &= HWC_STATUS_HAVE_VIDEO;
    STATUS_LOG("#status[%d]=%d in %s", screen_idx, ctx->status[screen_idx], __FUNCTION__);
    
	return 0;
}


static int hwc_video_3d_mode(sun4i_hwc_context_t *ctx, int screen_idx, video3Dinfo_t *_3d_info)
{
    __disp_layer_info_t         layer_info;
    __disp_output_type_t        cur_out_type;
    __disp_tv_mode_t            cur_hdmi_mode;
    unsigned long               args[4]={0};
    int                         ret = -1;

    ALOGD("#%s, screen_idx:%d, src:%d, out:%d, w:%d, h:%d, format:0x%x\n", 
        __FUNCTION__, screen_idx, _3d_info->src_mode, _3d_info->display_mode, _3d_info->width, _3d_info->height, _3d_info->format);

    args[0] = screen_idx;
    cur_out_type = (__disp_output_type_t)ioctl(ctx->dispfd,DISP_CMD_GET_OUTPUT_TYPE,(unsigned long)args);
    if(cur_out_type == DISP_OUTPUT_TYPE_HDMI)
    {
        args[0] = screen_idx;
        cur_hdmi_mode = (__disp_tv_mode_t)ioctl(ctx->dispfd,DISP_CMD_HDMI_GET_MODE,(unsigned long)args);

        if(cur_hdmi_mode == DISP_TV_MOD_1080P_24HZ_3D_FP || cur_hdmi_mode == DISP_TV_MOD_720P_50HZ_3D_FP || cur_hdmi_mode == DISP_TV_MOD_720P_60HZ_3D_FP)
        {
            if((_3d_info->display_mode != HWC_3D_OUT_MODE_HDMI_3D_1080P24_FP) && (_3d_info->display_mode != HWC_3D_OUT_MODE_HDMI_3D_720P50_FP) 
                && (_3d_info->display_mode != HWC_3D_OUT_MODE_HDMI_3D_720P60_FP))
            {
                // 3d to 2d
                __disp_layer_info_t         ui_layer_info;
                
                args[0]                         = screen_idx;
                args[1]                         = ctx->ui_layerhdl[screen_idx];
                args[2]                         = (unsigned long) (&ui_layer_info);
                ret = ioctl(ctx->dispfd, DISP_CMD_LAYER_GET_PARA, args);

                ui_layer_info.scn_win.x = ctx->org_scn_win.x;
                ui_layer_info.scn_win.y = ctx->org_scn_win.y;
                ui_layer_info.scn_win.width = ctx->org_scn_win.width;
                ui_layer_info.scn_win.height = ctx->org_scn_win.height;
                
                args[0]                         = screen_idx;
                args[1]                         = ctx->ui_layerhdl[screen_idx];
                args[2]                         = (unsigned long) (&ui_layer_info);
                ret = ioctl(ctx->dispfd, DISP_CMD_LAYER_SET_PARA, args);
                
                args[0] = screen_idx;
                ret = ioctl(ctx->dispfd,DISP_CMD_HDMI_OFF,(unsigned long)args);

                args[0] = screen_idx;
                args[1] = ctx->org_hdmi_mode;
                ioctl(ctx->dispfd,DISP_CMD_HDMI_SET_MODE,(unsigned long)args);
                
                args[0] = screen_idx;
                ret = ioctl(ctx->dispfd,DISP_CMD_HDMI_ON,(unsigned long)args);
            }
        }
        else
        {
            if(_3d_info->display_mode == HWC_3D_OUT_MODE_HDMI_3D_1080P24_FP || _3d_info->display_mode == HWC_3D_OUT_MODE_HDMI_3D_720P50_FP 
                || _3d_info->display_mode == HWC_3D_OUT_MODE_HDMI_3D_720P60_FP)
            {
                // 2d to 3d
                ctx->org_hdmi_mode = cur_hdmi_mode;
                
                __disp_layer_info_t 		ui_layer_info;
                
                args[0]                         = screen_idx;
                args[1]                         = ctx->ui_layerhdl[screen_idx];
                args[2]                         = (unsigned long) (&ui_layer_info);
                ret = ioctl(ctx->dispfd, DISP_CMD_LAYER_GET_PARA, args);
                
                ctx->org_scn_win.x = ui_layer_info.scn_win.x;
                ctx->org_scn_win.y = ui_layer_info.scn_win.y;
                ctx->org_scn_win.width = ui_layer_info.scn_win.width;
                ctx->org_scn_win.height = ui_layer_info.scn_win.height;
                
                ui_layer_info.scn_win.x = 0;
                ui_layer_info.scn_win.y = 0;
                if(_3d_info->display_mode == HWC_3D_OUT_MODE_HDMI_3D_1080P24_FP)
                {
                    ui_layer_info.scn_win.width = 1920;
                    ui_layer_info.scn_win.height = 1080;
                }
                else
                {
                    ui_layer_info.scn_win.width = 1280;
                    ui_layer_info.scn_win.height = 720;
                }
                args[0]                         = screen_idx;
                args[1]                         = ctx->ui_layerhdl[screen_idx];
                args[2]                         = (unsigned long) (&ui_layer_info);
                ret = ioctl(ctx->dispfd, DISP_CMD_LAYER_SET_PARA, args);
                
                args[0] = screen_idx;
                ret = ioctl(ctx->dispfd,DISP_CMD_HDMI_OFF,(unsigned long)args);
                
                args[0] = screen_idx;
                if(_3d_info->display_mode == HWC_3D_OUT_MODE_HDMI_3D_1080P24_FP)
                {
                    args[1] = DISP_TV_MOD_1080P_24HZ_3D_FP;
                }
                else if(_3d_info->display_mode == HWC_3D_OUT_MODE_HDMI_3D_720P50_FP)
                {
                    args[1] = DISP_TV_MOD_720P_50HZ_3D_FP;
                }
                else if(_3d_info->display_mode == HWC_3D_OUT_MODE_HDMI_3D_720P60_FP)
                {
                    args[1] = DISP_TV_MOD_720P_60HZ_3D_FP;
                }
                ioctl(ctx->dispfd,DISP_CMD_HDMI_SET_MODE,(unsigned long)args);
                
                args[0] = screen_idx;
                ret = ioctl(ctx->dispfd,DISP_CMD_HDMI_ON,(unsigned long)args);
            }
        }
    }

    args[0] = screen_idx;
    args[1] = ctx->video_layerhdl[screen_idx];
    args[2] = (unsigned long)&layer_info;
    ret = ioctl(ctx->dispfd, DISP_CMD_LAYER_GET_PARA, args);
    if(ret < 0)
    {
        ALOGD("####DISP_CMD_LAYER_GET_PARA fail in %s, screen_idx:%d,hdl:%d\n",__FUNCTION__, screen_idx, ctx->video_layerhdl[screen_idx]);
    }

    layer_info.fb.size.width = _3d_info->width;
    layer_info.fb.size.height = _3d_info->height;
    layer_info.src_win.x = 0;
    layer_info.src_win.y = 0;
    layer_info.src_win.width = _3d_info->width;
    layer_info.src_win.height = _3d_info->height;
    
    if(_3d_info->format == HWC_FORMAT_RGBA_8888)//·ЦЙ«
    {
        layer_info.fb.mode = DISP_MOD_NON_MB_PLANAR;
        layer_info.fb.format = DISP_FORMAT_ARGB8888;
        layer_info.fb.seq = DISP_SEQ_P3210;
    }
    else if(_3d_info->format == HWC_FORMAT_YUV420PLANAR)
    {
        layer_info.fb.mode = DISP_MOD_NON_MB_PLANAR;
        layer_info.fb.format = DISP_FORMAT_YUV420;
        layer_info.fb.seq = DISP_SEQ_P3210;
    }
    else if(_3d_info->format == HWC_FORMAT_MBYUV422)
    {
        layer_info.fb.mode = DISP_MOD_MB_UV_COMBINED;
        layer_info.fb.format = DISP_FORMAT_YUV422;
        layer_info.fb.seq = DISP_SEQ_UVUV;
    }
    else
    {
        layer_info.fb.mode = DISP_MOD_MB_UV_COMBINED;
        layer_info.fb.format = DISP_FORMAT_YUV420;
        layer_info.fb.seq = DISP_SEQ_UVUV;
    }

    if(_3d_info->src_mode == HWC_3D_SRC_MODE_NORMAL)
    {
        layer_info.fb.b_trd_src = 0;
    }
    else
    {
        layer_info.fb.b_trd_src = 1;
        layer_info.fb.trd_mode = (__disp_3d_src_mode_t)_3d_info->src_mode;
        layer_info.fb.trd_right_addr[0] = layer_info.fb.addr[0];
        layer_info.fb.trd_right_addr[1] = layer_info.fb.addr[1];
        layer_info.fb.trd_right_addr[2] = layer_info.fb.addr[2];
    }
    if(_3d_info->display_mode == HWC_3D_OUT_MODE_HDMI_3D_1080P24_FP || _3d_info->display_mode == HWC_3D_OUT_MODE_HDMI_3D_720P50_FP 
        || _3d_info->display_mode == HWC_3D_OUT_MODE_HDMI_3D_720P60_FP)
    {
        if(cur_out_type == DISP_OUTPUT_TYPE_HDMI)
        {
            layer_info.b_trd_out = 1;
            layer_info.out_trd_mode = DISP_3D_OUT_MODE_FP;
        }
        else
        {
            layer_info.fb.b_trd_src = 0;
            layer_info.b_trd_out = 0;
        }
    }
    else if(_3d_info->display_mode == HWC_3D_OUT_MODE_ORIGINAL || _3d_info->display_mode == HWC_3D_OUT_MODE_ANAGLAGH)
    {
        layer_info.fb.b_trd_src = 0;
        layer_info.b_trd_out = 0;
    }
    else if(_3d_info->display_mode == HWC_3D_OUT_MODE_LI || _3d_info->display_mode == HWC_3D_OUT_MODE_CI_1 || _3d_info->display_mode == HWC_3D_OUT_MODE_CI_2 || 
            _3d_info->display_mode == HWC_3D_OUT_MODE_CI_3 || _3d_info->display_mode == HWC_3D_OUT_MODE_CI_4)
    {
        layer_info.b_trd_out = 1;
        layer_info.out_trd_mode = (__disp_3d_out_mode_t)_3d_info->display_mode;
    }
    else
    {
        layer_info.b_trd_out = 0;
    }

    if(layer_info.b_trd_out)
    {
        unsigned int w,h;
        
        args[0] = screen_idx;
        w = ioctl(ctx->dispfd,DISP_CMD_SCN_GET_WIDTH,(unsigned long)args);
        h = ioctl(ctx->dispfd,DISP_CMD_SCN_GET_HEIGHT,(unsigned long)args);

        layer_info.scn_win.x = 0;
        layer_info.scn_win.y = 0;
        layer_info.scn_win.width = w;
        layer_info.scn_win.height = h;

        ctx->rect_out.left = 0;
        ctx->rect_out.top = 0;
        ctx->rect_out.right = w;
        ctx->rect_out.bottom = h;
    }
    else
    {
        layer_info.scn_win.x = ctx->rect_out_active[screen_idx].left;
        layer_info.scn_win.y = ctx->rect_out_active[screen_idx].top;
        layer_info.scn_win.width = ctx->rect_out_active[screen_idx].right - ctx->rect_out_active[screen_idx].left;
        layer_info.scn_win.height = ctx->rect_out_active[screen_idx].bottom - ctx->rect_out_active[screen_idx].top;
    }
    
    ALOGV("b_3d_src:%d, 3d_src_mode:%d, b_3d_out:%d, 3d_out_mode:%d", layer_info.fb.b_trd_src, layer_info.fb.trd_mode, layer_info.b_trd_out, layer_info.out_trd_mode);
    args[0] = screen_idx;
    args[1] = ctx->video_layerhdl[screen_idx];
    args[2] = (unsigned long)&layer_info;
    ioctl(ctx->dispfd, DISP_CMD_LAYER_SET_PARA, args);

    ctx->cur_3d_w = _3d_info->width;
    ctx->cur_3d_h = _3d_info->height;
    ctx->cur_3d_format = _3d_info->format;
    ctx->cur_3d_src = _3d_info->src_mode;
    ctx->cur_3d_out = _3d_info->display_mode;
    ctx->cur_3denable = layer_info.b_trd_out;
    return 0;
}


static int hwc_video_set_frame_para(sun4i_hwc_context_t *ctx, int screen_idx, libhwclayerpara_t *overlaypara)
{
    __disp_video_fb_t      		video_info;
    unsigned long               args[4]={0};
    int                         ret;
        
	video_info.interlace         = (overlaypara->bProgressiveSrc?0:1);
	video_info.top_field_first   = overlaypara->bTopFieldFirst;
	video_info.addr[0]           = overlaypara->top_y;
	video_info.addr[1]           = overlaypara->top_c;
	video_info.addr[2]			 = overlaypara->bottom_y;
	video_info.addr_right[0]     = overlaypara->bottom_y;
	video_info.addr_right[1]     = overlaypara->bottom_c;
	video_info.addr_right[2]	 = 0;
	video_info.id                = overlaypara->number; 
	video_info.maf_valid         = overlaypara->maf_valid;
	video_info.pre_frame_valid   = overlaypara->pre_frame_valid;
	video_info.flag_addr         = overlaypara->flag_addr;
	video_info.flag_stride       = overlaypara->flag_stride;

	if((ctx->status[screen_idx] & HWC_STATUS_HAVE_FRAME) == 0)
	{
        __disp_layer_info_t         layer_info;

    	ALOGD("#first_frame ............");

    	args[0] 				= screen_idx;
    	args[1] 				= ctx->video_layerhdl[screen_idx];
    	args[2] 				= (unsigned long) (&layer_info);
    	args[3] 				= 0;
    	ret = ioctl(ctx->dispfd, DISP_CMD_LAYER_GET_PARA, args);
        if(ret < 0)
        {
            ALOGD("####DISP_CMD_LAYER_GET_PARA fail in %s, screen_idx:%d, hdl:%d\n",__FUNCTION__, screen_idx, ctx->video_layerhdl[screen_idx]);
        }

    	layer_info.fb.addr[0] 	= video_info.addr[0];
    	layer_info.fb.addr[1] 	= video_info.addr[1];
    	layer_info.fb.addr[2] 	= video_info.addr[2];
    	layer_info.fb.trd_right_addr[0] = video_info.addr_right[0];
    	layer_info.fb.trd_right_addr[1] = video_info.addr_right[1];
    	layer_info.fb.trd_right_addr[2] = video_info.addr_right[2];
    	args[0] 				= screen_idx;
    	args[1] 				= ctx->video_layerhdl[screen_idx];
    	args[2] 				= (unsigned long) (&layer_info);
    	args[3] 				= 0;
    	ret = ioctl(ctx->dispfd, DISP_CMD_LAYER_SET_PARA, args);

        ctx->status[screen_idx] |= HWC_STATUS_HAVE_FRAME;
        STATUS_LOG("#status[%d]=%d in %s", screen_idx, ctx->status[screen_idx], __FUNCTION__);
	}            

	args[0]					= screen_idx;
    args[1]                 = ctx->video_layerhdl[screen_idx];
	args[2]                 = (unsigned long)(&video_info);
	args[3]                 = 0;
	ret = ioctl(ctx->dispfd, DISP_CMD_VIDEO_SET_FB,args);

    memcpy(&ctx->cur_frame_para, overlaypara,sizeof(libhwclayerpara_t));

    
    return 0;
}

static int hwc_computer_rect(sun4i_hwc_context_t *ctx, int screen_idx, hwc_rect_t *rect_out, hwc_rect_t *rect_in)
{
    int		        ret;
    unsigned long   args[4]={0};
    int    screen_in_width;
    int    screen_in_height;
    int    temp_x,temp_y,temp_w,temp_h;
    int    x,y,w,h,mid_x,mid_y;
    int    force_full_screen = 0;

    if(rect_in->left >= rect_in->right || rect_in->top >= rect_in->bottom)
    {
        ALOGV("para error in hwc_computer_rect,(left:%d,right:%d,top:%d,bottom:%d)\n", rect_in->left, rect_in->right, rect_in->top, rect_in->bottom);
        return -1;
    }

    screen_in_width = ctx->screen_para.app_width[0];
    screen_in_height = ctx->screen_para.app_height[0];

    RECT_LOG("#1.0 in:[%d,%d,%d,%d] [%d,%d]\n", 
        rect_in->left, rect_in->top, rect_in->right - rect_in->left, rect_in->bottom-rect_in->top,screen_in_width, screen_in_height);

    
    if(ctx->cur_3denable == true)
    {
    	rect_out->left = 0;
    	rect_out->right = ctx->screen_para.width[screen_idx];
    	rect_out->top = 0;
    	rect_out->bottom = ctx->screen_para.height[screen_idx];
    	
    	return 0;
    }

    temp_x = rect_in->left;
    temp_w = rect_in->right - rect_in->left;
    temp_y = rect_in->top;
    temp_h = rect_in->bottom - rect_in->top;

	mid_x = temp_x + temp_w/2;
	mid_y = temp_y + temp_h/2;

	mid_x = mid_x * ctx->screen_para.valid_width[screen_idx] / screen_in_width;
	mid_y = mid_y * ctx->screen_para.valid_height[screen_idx] / screen_in_height;

    if(ctx->b_video_in_valid_area)
    {
        force_full_screen = 0;
    }
    else
    {
        if((rect_in->left==0 && rect_in->right==screen_in_width) || (rect_in->top==0 && rect_in->bottom==screen_in_height))
        {
            force_full_screen = 1;
        }
        else
        {
            force_full_screen = 0;
        }
    }

    if(force_full_screen)
    {
    	if((screen_in_width == ctx->screen_para.width[screen_idx]) && (screen_in_height == ctx->screen_para.height[screen_idx]))
    	{
    	    rect_out->left = rect_in->left;
    	    rect_out->right = rect_in->right;
    	    rect_out->top = rect_in->top;
    	    rect_out->bottom = rect_in->bottom;
    	    return 0;
    	}
    
   	 	mid_x += (ctx->screen_para.width[screen_idx] - ctx->screen_para.valid_width[screen_idx])/2;
   	 	mid_y += (ctx->screen_para.height[screen_idx] - ctx->screen_para.valid_height[screen_idx])/2;

    	if(mid_x * temp_h >= mid_y * temp_w)
    	{
    	    y = 0;
    	    h = mid_y * 2;
    	    w = h * temp_w / temp_h;
    	    x = mid_x - (w/2);
    	}
    	else
    	{
    	    x = 0;
    	    w = mid_x * 2;
    	    h = w * temp_h / temp_w;
    	    y = mid_y - (h/2);
    	}
	}
    else
    {
        w = temp_w * ctx->screen_para.valid_width[screen_idx] / screen_in_width;
        h = temp_h * ctx->screen_para.valid_height[screen_idx] / screen_in_height;
        x = mid_x - (w/2);
        y = mid_y - (h/2);
        x += (ctx->screen_para.width[screen_idx] - ctx->screen_para.valid_width[screen_idx])/2;
        y += (ctx->screen_para.height[screen_idx] - ctx->screen_para.valid_height[screen_idx])/2;
    }

	temp_x = x;
	temp_y = y;
	temp_w = w;
	temp_h = h;

    rect_out->left = temp_x;
    rect_out->right = temp_x + temp_w;
    rect_out->top = temp_y;
    rect_out->bottom = temp_y + temp_h;

	RECT_LOG("#1.1 out:[%d,%d,%d,%d] [%d,%d] [%d,%d]\n", temp_x, temp_y, temp_w, temp_h,
	    ctx->screen_para.width[screen_idx], ctx->screen_para.height[screen_idx], ctx->screen_para.valid_width[screen_idx], ctx->screen_para.valid_height[screen_idx]);

    return 0;
}

static int hwc_set_rect(hwc_composer_device_1_t *dev,
        size_t numDisplays, hwc_display_contents_1_t** displays)
{
    int 						ret = 0;
    sun4i_hwc_context_t   		*ctx = (sun4i_hwc_context_t *)dev;
    unsigned long               args[4]={0};
    int screen_idx;
    int have_overlay = 0;
    
	for (size_t i=0 ; i<displays[0]->numHwLayers ; i++)         
    {       
        if(displays[0]->hwLayers[i].compositionType == HWC_OVERLAY)
        {
            ALOGV("#hwc_set_rect\n");
            
            hwc_rect_t croprect;
            hwc_rect_t displayframe_src, displayframe_dst;
            __disp_layer_info_t         layer_info;
            
            if(ctx->rect_in.left != displays[0]->hwLayers[i].sourceCrop.left || ctx->rect_in.right != displays[0]->hwLayers[i].sourceCrop.right 
             ||ctx->rect_in.top != displays[0]->hwLayers[i].sourceCrop.top || ctx->rect_in.bottom != displays[0]->hwLayers[i].sourceCrop.bottom
             ||ctx->rect_out.left != displays[0]->hwLayers[i].displayFrame.left || ctx->rect_out.right != displays[0]->hwLayers[i].displayFrame.right
             ||ctx->rect_out.top != displays[0]->hwLayers[i].displayFrame.top || ctx->rect_out.bottom != displays[0]->hwLayers[i].displayFrame.bottom)
            {
                memcpy(&croprect, &displays[0]->hwLayers[i].sourceCrop, sizeof(hwc_rect_t));
                memcpy(&displayframe_src, &displays[0]->hwLayers[i].displayFrame, sizeof(hwc_rect_t));

                for(screen_idx=0; screen_idx<2; screen_idx++)
                {
                    if(((screen_idx == 0) && (ctx->mode==HWC_MODE_SCREEN0 || ctx->mode==HWC_MODE_SCREEN0_AND_SCREEN1 || ctx->mode==HWC_MODE_SCREEN0_GPU))
                    || ((screen_idx == 1) && (ctx->mode==HWC_MODE_SCREEN1 || ctx->mode==HWC_MODE_SCREEN0_TO_SCREEN1 || ctx->mode==HWC_MODE_SCREEN0_AND_SCREEN1)))
                    {
                        if(!(ctx->cur_3denable))
                        {
                            int screen_in_width;
                            int screen_in_height;

                            screen_in_width                     = ctx->screen_para.app_width[0];
                            screen_in_height                    = ctx->screen_para.app_height[0];

                            RECT_LOG("#0:src[%d,%d,%d,%d], dst[%d,%d,%d,%d]\n",
                                croprect.left, croprect.top, croprect.right-croprect.left, croprect.bottom-croprect.top,
                                displayframe_src.left, displayframe_src.top, displayframe_src.right-displayframe_src.left, displayframe_src.bottom-displayframe_src.top);
                            if(displayframe_src.left < 0)
                            {
                                croprect.left = croprect.left + ((0 - displayframe_src.left) * (croprect.right - croprect.left) / (displayframe_src.right - displayframe_src.left));
                                displayframe_src.left = 0;
                            }
                            if(displayframe_src.right > screen_in_width)
                            {
                                croprect.right = croprect.right - ((displayframe_src.right - screen_in_width) * (croprect.right - croprect.left) / (displayframe_src.right - displayframe_src.left));
                                displayframe_src.right = screen_in_width;
                            }
                            if(displayframe_src.top< 0)
                            {
                                croprect.top= croprect.top+ ((0 - displayframe_src.top) * (croprect.bottom- croprect.top) / (displayframe_src.bottom- displayframe_src.top));
                                displayframe_src.top= 0;
                            }
                            if(displayframe_src.bottom> screen_in_height)
                            {
                                croprect.bottom= croprect.bottom- ((displayframe_src.bottom- screen_in_height) * (croprect.bottom- croprect.top) / (displayframe_src.bottom- displayframe_src.top));
                                displayframe_src.bottom= screen_in_height;
                            }

                            RECT_LOG("#1:src[%d,%d,%d,%d], dst[%d,%d,%d,%d]\n",
                                croprect.left, croprect.top, croprect.right-croprect.left, croprect.bottom-croprect.top,
                                displayframe_src.left, displayframe_src.top, displayframe_src.right-displayframe_src.left, displayframe_src.bottom-displayframe_src.top);

                            ret = hwc_computer_rect(ctx,screen_idx, &displayframe_dst, &displayframe_src);
                            if(ret < 0)
                            {
                                return -1;
                            }
							if(ctx->layer_para_set == false)
                           	{
								return -1;
							}
                            RECT_LOG("#2:src[%d,%d,%d,%d], dst[%d,%d,%d,%d]\n",
                                croprect.left, croprect.top, croprect.right-croprect.left, croprect.bottom-croprect.top,
                                displayframe_dst.left, displayframe_dst.top, displayframe_dst.right-displayframe_dst.left, displayframe_dst.bottom-displayframe_dst.top);

                        	args[0] 				= screen_idx;
                        	args[1] 				= ctx->video_layerhdl[screen_idx];
                        	args[2] 				= (unsigned long) (&layer_info);
                        	args[3] 				= 0;
                        	ret = ioctl(ctx->dispfd, DISP_CMD_LAYER_GET_PARA, args);
                        	if(ret < 0)
                        	{
                    	        ALOGD("####DISP_CMD_LAYER_GET_PARA fail in hwc_set_rect, screen_idx:%d,hdl:%d\n",screen_idx,ctx->video_layerhdl[screen_idx]);
                                return -1;
                        	}

                        	layer_info.src_win.x = croprect.left;
                        	layer_info.src_win.y = croprect.top;
                        	layer_info.src_win.width = croprect.right - croprect.left;
                        	layer_info.src_win.height = croprect.bottom - croprect.top;
                            if(ctx->cur_3d_out == HWC_3D_OUT_MODE_ANAGLAGH)
                            {
                                if(ctx->cur_3d_src == HWC_3D_SRC_MODE_SSF || ctx->cur_3d_src == HWC_3D_SRC_MODE_SSH)
                                {
                                    layer_info.src_win.x /=2;
                                    layer_info.src_win.width /=2;
                                }
                                if(ctx->cur_3d_src == HWC_3D_SRC_MODE_TB)
                                {
                                    layer_info.src_win.y /=2;
                                    layer_info.src_win.height /=2;
                                }
                            }
                        	layer_info.scn_win.x = displayframe_dst.left;
                        	layer_info.scn_win.y = displayframe_dst.top;
                        	layer_info.scn_win.width = displayframe_dst.right - displayframe_dst.left;
                        	layer_info.scn_win.height = displayframe_dst.bottom - displayframe_dst.top;
                            
                        	args[0] 				= screen_idx;
                        	args[1] 				= ctx->video_layerhdl[screen_idx];
                        	args[2] 				= (unsigned long) (&layer_info);
                        	args[3] 				= 0;
                        	ioctl(ctx->dispfd, DISP_CMD_LAYER_SET_PARA, args);

                            ctx->rect_out_active[screen_idx].left = displayframe_dst.left;
                            ctx->rect_out_active[screen_idx].right = displayframe_dst.right;
                            ctx->rect_out_active[screen_idx].top = displayframe_dst.top;
                            ctx->rect_out_active[screen_idx].bottom = displayframe_dst.bottom;
                        }
                    }
                }            
                
                ctx->rect_in.left = displays[0]->hwLayers[i].sourceCrop.left;
                ctx->rect_in.right = displays[0]->hwLayers[i].sourceCrop.right;
                ctx->rect_in.top = displays[0]->hwLayers[i].sourceCrop.top;
                ctx->rect_in.bottom = displays[0]->hwLayers[i].sourceCrop.bottom;
                
                ctx->rect_out.left = displays[0]->hwLayers[i].displayFrame.left;
                ctx->rect_out.right = displays[0]->hwLayers[i].displayFrame.right;
                ctx->rect_out.top = displays[0]->hwLayers[i].displayFrame.top;
                ctx->rect_out.bottom = displays[0]->hwLayers[i].displayFrame.bottom;
            }
            have_overlay = 1;
        }
    }     

    //close video layer if there is not overlay
    for(screen_idx=0; screen_idx<2; screen_idx++)
    {
        if(((screen_idx == 0) && (ctx->mode==HWC_MODE_SCREEN0 || ctx->mode==HWC_MODE_SCREEN0_AND_SCREEN1 || ctx->mode==HWC_MODE_SCREEN0_GPU))
            || ((screen_idx == 1) && (ctx->mode == HWC_MODE_SCREEN1 || ctx->mode==HWC_MODE_SCREEN0_TO_SCREEN1 || ctx->mode==HWC_MODE_SCREEN0_AND_SCREEN1)))
        {
            if((have_overlay == 0) && (ctx->status[screen_idx] & HWC_STATUS_OPENED))
            {
                hwc_video_close(ctx, screen_idx);
            }
            else if((have_overlay) && ((ctx->status[screen_idx] & (HWC_STATUS_OPENED | HWC_STATUS_HAVE_FRAME)) == HWC_STATUS_HAVE_FRAME))
            {
                hwc_video_open(ctx, screen_idx);
            }
        }
    }

    return ret;
}

static int hwc_set_init_para(sun4i_hwc_context_t *ctx,uint32_t value,int mode_change)
{
    layerinitpara_t				*layer_init_para = (layerinitpara_t *)value;
    unsigned int                screen_idx;

    ALOGD("####%s, mode:%d, w:%d, h:%d, format:%d\n",
        __FUNCTION__, ctx->mode, layer_init_para->w, layer_init_para->h, layer_init_para->format);
    
    for(screen_idx=0; screen_idx<2; screen_idx++)
    {
        if(((screen_idx == 0) && (ctx->mode==HWC_MODE_SCREEN0 || ctx->mode==HWC_MODE_SCREEN0_AND_SCREEN1 || ctx->mode==HWC_MODE_SCREEN0_GPU))
            || ((screen_idx == 1) && (ctx->mode==HWC_MODE_SCREEN1 || ctx->mode==HWC_MODE_SCREEN0_TO_SCREEN1 || ctx->mode==HWC_MODE_SCREEN0_AND_SCREEN1)))
        {
            hwc_video_request(ctx, screen_idx, layer_init_para);
        }
    }
    
    ctx->w = layer_init_para->w;
    ctx->h = layer_init_para->h;
    ctx->format = layer_init_para->format;
    memset(&ctx->rect_in, 0, sizeof(hwc_rect_t));
    memset(&ctx->rect_out, 0, sizeof(hwc_rect_t));
    ctx->cur_3denable = 0;

    ctx->status[0] = HWC_STATUS_HAVE_VIDEO;
    ctx->status[1] = HWC_STATUS_HAVE_VIDEO;
    STATUS_LOG("#status[0]=%d in %s", ctx->status[0], __FUNCTION__);
    STATUS_LOG("#status[1]=%d in %s", ctx->status[1], __FUNCTION__);

	return 0;
}

static int hwc_set_frame_para(sun4i_hwc_context_t *ctx,uint32_t value)
{
    libhwclayerpara_t           *overlaypara;
    int                         screen_idx;

    ALOGV("####hwc_set_frame_para\n");
    
    for(screen_idx=0; screen_idx<2; screen_idx++)
    {
        if(((screen_idx == 0) && (ctx->mode==HWC_MODE_SCREEN0 || ctx->mode==HWC_MODE_SCREEN0_AND_SCREEN1 || ctx->mode==HWC_MODE_SCREEN0_GPU))
            || ((screen_idx == 1) && (ctx->mode==HWC_MODE_SCREEN1 || ctx->mode==HWC_MODE_SCREEN0_TO_SCREEN1 || ctx->mode==HWC_MODE_SCREEN0_AND_SCREEN1)))
        {
            if(ctx->status[screen_idx] & HWC_STATUS_HAVE_VIDEO)
            {
                overlaypara = (libhwclayerpara_t*)value;
                
                hwc_video_set_frame_para(ctx, screen_idx, overlaypara);
            }
        }
    }
    
    return 0;
}


static int hwc_get_frame_id(sun4i_hwc_context_t *ctx)
{
    int                         ret = -1;
    unsigned long               args[4]={0};

    if(ctx->mode==HWC_MODE_SCREEN0 || ctx->mode==HWC_MODE_SCREEN0_GPU)
    {
    	args[0] = 0;
    	args[1] = ctx->video_layerhdl[0];
    	ret = ioctl(ctx->dispfd, DISP_CMD_VIDEO_GET_FRAME_ID, args);
    }
    else if(ctx->mode==HWC_MODE_SCREEN0_TO_SCREEN1 || ctx->mode==HWC_MODE_SCREEN1)
    {
        args[0] = 1;
        args[1] = ctx->video_layerhdl[1];
        ret = ioctl(ctx->dispfd, DISP_CMD_VIDEO_GET_FRAME_ID, args);
    }
    else if(ctx->mode == HWC_MODE_SCREEN0_AND_SCREEN1)
    {
        int ret0,ret1;
        
    	args[0] = 0;
    	args[1] = ctx->video_layerhdl[0];
    	ret0 = ioctl(ctx->dispfd, DISP_CMD_VIDEO_GET_FRAME_ID, args);

        args[0] = 1;
        args[1] = ctx->video_layerhdl[1];
        ret1 = ioctl(ctx->dispfd, DISP_CMD_VIDEO_GET_FRAME_ID, args);

        ret = (ret0<ret1)?ret0:ret1;
    }

    if(ret <0)
    {
        ALOGV("####hwc_get_frame_id return -1,mode:%d\n",ctx->mode);
    }
    return ret;
}

static int hwc_set3dmode(sun4i_hwc_context_t *ctx,int para)
{
	video3Dinfo_t *_3d_info = (video3Dinfo_t *)para;
    unsigned int                screen_idx;

    ALOGV("####%s\n", __FUNCTION__);
    
    for(screen_idx=0; screen_idx<2; screen_idx++)
    {
        if(((screen_idx == 0) && (ctx->mode==HWC_MODE_SCREEN0 || ctx->mode==HWC_MODE_SCREEN0_AND_SCREEN1 || ctx->mode==HWC_MODE_SCREEN0_GPU))
            || ((screen_idx == 1) && (ctx->mode==HWC_MODE_SCREEN1 || ctx->mode==HWC_MODE_SCREEN0_TO_SCREEN1 || ctx->mode==HWC_MODE_SCREEN0_AND_SCREEN1)))
        {
            hwc_video_3d_mode(ctx, screen_idx, _3d_info);
        }
    }

    return 0;
}

static int hwc_set_3d_parallax(sun4i_hwc_context_t *ctx,uint32_t value)
{
    __disp_layer_info_t         layer_info;
    unsigned int                screen_idx;
    int                         ret = -1;
    unsigned long               args[4]={0};

    ALOGD("####%s value:%d\n", __FUNCTION__, value);
    
    for(screen_idx=0; screen_idx<2; screen_idx++)
    {
        if(((screen_idx == 0) && (ctx->mode==HWC_MODE_SCREEN0 || ctx->mode==HWC_MODE_SCREEN0_AND_SCREEN1 || ctx->mode==HWC_MODE_SCREEN0_GPU))
            || ((screen_idx == 1) && (ctx->mode==HWC_MODE_SCREEN1 || ctx->mode==HWC_MODE_SCREEN0_TO_SCREEN1 || ctx->mode==HWC_MODE_SCREEN0_AND_SCREEN1)))
        {
            args[0] = screen_idx;
            args[1] = ctx->video_layerhdl[screen_idx];
            args[2] = (unsigned long)&layer_info;
            ret = ioctl(ctx->dispfd, DISP_CMD_LAYER_GET_PARA, args);
            if(ret < 0)
            {
                ALOGD("####DISP_CMD_LAYER_GET_PARA fail in %s, screen_idx:%d, hdl:%d\n",__FUNCTION__, screen_idx, ctx->video_layerhdl[screen_idx]);
            }

            if(layer_info.fb.b_trd_src && (layer_info.fb.trd_mode==DISP_3D_SRC_MODE_SSF || layer_info.fb.trd_mode==DISP_3D_SRC_MODE_SSH) && layer_info.b_trd_out)
            {
                args[0] = screen_idx;
                args[1] = ctx->video_layerhdl[screen_idx];
                args[2] = (unsigned long)&layer_info;
                ret = ioctl(ctx->dispfd, DISP_CMD_LAYER_SET_PARA, args);
            }
        }
    }
    
    return 0;
}


static int hwc_prepare(hwc_composer_device_1_t *dev,
        size_t numDisplays, hwc_display_contents_1_t** displays) 
{
	for (size_t i=0 ; i<displays[0]->numHwLayers ; i++) 
    {
        if((displays[0]->hwLayers[i].format == HWC_FORMAT_MBYUV420)
            ||(displays[0]->hwLayers[i].format == HWC_FORMAT_MBYUV422)
            ||(displays[0]->hwLayers[i].format == HWC_FORMAT_YUV420PLANAR)
            ||(displays[0]->hwLayers[i].format == HWC_FORMAT_DEFAULT))
    	{
        	displays[0]->hwLayers[i].compositionType = HWC_OVERLAY;
    	}
    	else
    	{
        	displays[0]->hwLayers[i].compositionType = HWC_FRAMEBUFFER;
    	}
    }

	hwc_set_rect(dev, numDisplays, displays);

    return 0;
}

static int hwc_set(hwc_composer_device_1_t *dev,
        size_t numDisplays, hwc_display_contents_1_t** displays)
{
    //for (size_t i=0 ; i<list->numHwLayers ; i++) {
    //    dump_layer(&list->hwLayers[i]);
    //}
    EGLBoolean sucess = eglSwapBuffers((EGLDisplay)displays[0]->dpy,
            (EGLSurface)displays[0]->sur);
    if (!sucess) {
        return HWC_EGL_ERROR;
    }
    return 0;  //hwc_set_rect(dev, numDisplays, displays);
}

static int hwc_set_mode(sun4i_hwc_context_t *ctx, e_hwc_mode_t mode)
{
    unsigned int screen_idx;

    ALOGD("####%s mode:%d\n", __FUNCTION__, mode);

    if(mode == ctx->mode)
    {
        ALOGV("####mode not change\n");
        return 0;
    }
    
    for(screen_idx=0; screen_idx<2; screen_idx++)
    {        
        if(((screen_idx == 0) && (mode==HWC_MODE_SCREEN0 || mode==HWC_MODE_SCREEN0_AND_SCREEN1 || mode==HWC_MODE_SCREEN0_GPU))
            || ((screen_idx == 1) && (mode==HWC_MODE_SCREEN1 || mode==HWC_MODE_SCREEN0_TO_SCREEN1 || mode==HWC_MODE_SCREEN0_AND_SCREEN1)))
        {
        }
        else if((ctx->status[screen_idx] & HWC_STATUS_HAVE_VIDEO) && (ctx->video_layerhdl[screen_idx]))
        {
            hwc_video_release(ctx, screen_idx);
        }
    }

    for(screen_idx=0; screen_idx<2; screen_idx++)
    {        
        if(((screen_idx == 0) && (mode==HWC_MODE_SCREEN0 || mode==HWC_MODE_SCREEN0_AND_SCREEN1 || mode==HWC_MODE_SCREEN0_GPU))
            || ((screen_idx == 1) && (mode==HWC_MODE_SCREEN1 || mode==HWC_MODE_SCREEN0_TO_SCREEN1 || mode==HWC_MODE_SCREEN0_AND_SCREEN1)))
        {
            if((ctx->status[screen_idx] & HWC_STATUS_HAVE_VIDEO) && (ctx->video_layerhdl[screen_idx] == 0))
            {
                layerinitpara_t layer_init_para;
                video3Dinfo_t _3d_info;
            
                layer_init_para.w = ctx->w;
                layer_init_para.h = ctx->h;
                layer_init_para.format = ctx->format;
                hwc_video_request(ctx, screen_idx, &layer_init_para);
            
                hwc_video_set_frame_para(ctx, screen_idx, &ctx->cur_frame_para);
                
                _3d_info.width = ctx->cur_3d_w;
                _3d_info.height = ctx->cur_3d_h;
                _3d_info.format = ctx->cur_3d_format;
                _3d_info.src_mode = ctx->cur_3d_src;
                _3d_info.display_mode = ctx->cur_3d_out;
                hwc_video_3d_mode(ctx, screen_idx, &_3d_info);
            }
        }
    }


    ctx->mode = mode;

    return 0;
}

static int hwc_set_screen_para(sun4i_hwc_context_t *ctx,uint32_t value)
{
    screen_para_t *screen_info = (screen_para_t *)value;
    
    ALOGV("####hwc_set_screen_para,%d,%d,%d,%d,%d,%d",screen_info->app_width[0],screen_info->app_height[0],
        screen_info->width[0],screen_info->height[0],screen_info->valid_width[0],screen_info->valid_height[0]);
    
    memcpy(&ctx->screen_para,screen_info,sizeof(screen_para_t));
    
    return 0;
}

static int hwc_set_show(sun4i_hwc_context_t *ctx,uint32_t enable)
{
    int screen_idx;

    if(enable == 0)
    {    
    	ALOGD("####%s enable:%d", __FUNCTION__, enable);

        for(screen_idx=0; screen_idx<2; screen_idx++)
        {
            if(((screen_idx == 0) && (ctx->mode==HWC_MODE_SCREEN0 || ctx->mode==HWC_MODE_SCREEN0_AND_SCREEN1 || ctx->mode==HWC_MODE_SCREEN0_GPU))
                || ((screen_idx == 1) && (ctx->mode==HWC_MODE_SCREEN1 || ctx->mode==HWC_MODE_SCREEN0_TO_SCREEN1 || ctx->mode==HWC_MODE_SCREEN0_AND_SCREEN1)))
            {
                hwc_video_release(ctx, screen_idx);
            }
        }
        
        ctx->status[0] = 0;
        ctx->status[1] = 0;
        STATUS_LOG("#status[0]=%d in %s", ctx->status[0], __FUNCTION__);
        STATUS_LOG("#status[1]=%d in %s", ctx->status[1], __FUNCTION__);
    }
    return 0;
}

static int hwc_set_layer_top_bottom(sun4i_hwc_context_t *ctx,int para)
{
	int                         ret;

	if(ctx->video_layerhdl[BOOTVIDEO_SCREEN_ID])
	{
		unsigned long               args[4]={0};

		args[0]							= BOOTVIDEO_SCREEN_ID;
		args[1]                 		= ctx->video_layerhdl[BOOTVIDEO_SCREEN_ID];
		args[2]                 		= 0;
		args[3]                 		= 0;
		if(para == HWC_LAYER_SETTOP)
		{
			ret                     	= ioctl(ctx->dispfd, DISP_CMD_LAYER_TOP,args);
			if(ret != 0)
			{
				//open display layer failed, need send play end command, and exit
				ALOGE("Set video display Top failed!\n");
				return NULL;
			}
		}
		else if(para == HWC_LAYER_SETBOTTOM)
		{
			ret                     	= ioctl(ctx->dispfd, DISP_CMD_LAYER_BOTTOM,args);
			if(ret != 0)
			{
				//open display layer failed, need send play end command, and exit
				ALOGE("Set video display Bottom failed!\n");
				return NULL;
			}
		}

	}
	else
	{
		ALOGE("current handle is Null!\n");
		return NULL;
	}

	return ret;
}

static int hwc_setlayerorder(hwc_composer_device_1_t *dev,
        size_t numDisplays, hwc_display_contents_1_t** displays, uint32_t para)
{
	sun4i_hwc_context_t   		*ctx = (sun4i_hwc_context_t *)dev;

	//bootvideo = true;
	if(displays[0]->numHwLayers == 1)
	{
		displays[0]->hwLayers[0].format = HWC_FORMAT_MBYUV420;
	}
	hwc_prepare(dev, numDisplays, displays);
	//hwc_set_rect(dev, numDisplays, displays);

	return hwc_set_layer_top_bottom(ctx, para);
}

static int hwc_setparameter(hwc_composer_device_1 *dev,uint32_t param,uint32_t value)
{
	int 						ret = 0;
    sun4i_hwc_context_t   		*ctx = (sun4i_hwc_context_t *)dev;

    if(param == HWC_LAYER_SETINITPARA)
    {
    	ret = hwc_set_init_para(ctx,value, 0);
    }
    else if(param == HWC_LAYER_SETFRAMEPARA)
    {
    	ret = hwc_set_frame_para(ctx,value);
    }
    else if(param == HWC_LAYER_GETCURFRAMEPARA)
    {
    	ret = hwc_get_frame_id(ctx);
    }
    else if(param == HWC_LAYER_SETMODE)
    {
        ret = hwc_set_mode(ctx, (e_hwc_mode_t)value);
    }
	else if(param == HWC_LAYER_SET3DMODE)
	{
	    ret = hwc_set3dmode(ctx,value);
	}
	else if(param == HWC_LAYER_SET_3D_PARALLAX)
	{
	    ret = hwc_set_3d_parallax(ctx,value);
	}
	else if(param == HWC_LAYER_SET_SCREEN_PARA)
	{
	    ret = hwc_set_screen_para(ctx,value);
	}
	else if(param == HWC_LAYER_SHOW)
	{
	    ret = hwc_set_show(ctx,value);
	}

    return ( ret );
}

static uint32_t hwc_getparameter(hwc_composer_device_1 *dev,uint32_t param)
{
    return 0;
}

static void *hwc_vsync_thread(void *data)
{
    #define HZ 60
    struct timespec spec;
    int err = 0;
    nsecs_t period, now, next_vsync, sleep, next_fake_vsync = 0;
    hwc_context_t *ctx = (hwc_context_t *)data;
    int fb = open("/dev/graphics/fb0", O_RDWR);
    if(fb < 0) {
        ALOGE("failed to open fb0\n");
        return NULL;
    }

    setpriority(PRIO_PROCESS, 0, HAL_PRIORITY_URGENT_DISPLAY);

    while (true) {
        period = 1000000000/HZ;
        now = systemTime(CLOCK_MONOTONIC);
        next_vsync = next_fake_vsync;
        sleep = next_vsync - now;
        if (sleep < 0) {
            // we missed, find where the next vsync should be
            sleep = (period - ((now - next_vsync) % period));
            next_vsync = now + sleep;
        }
        next_fake_vsync = next_vsync + period;

        spec.tv_sec  = next_vsync / 1000000000;
        spec.tv_nsec = next_vsync % 1000000000;

        do {
            err = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &spec, NULL);
        } while (err < 0 && errno == EINTR);

/*      arg = 0;
        //ioctl(fb, FBIO_WAITFORVSYNC, &arg);
*/
        if (ctx->vsync_enabled)
            ctx->procs->vsync(ctx->procs, 0, next_vsync);
    }

    close(fb);

    return NULL;
}

static int hwc_blank(hwc_composer_device_1* a, int b, int c)
{
    /* STUB */
    return 0;
}

static int hwc_eventControl(hwc_composer_device_1* dev, int dpy, int event,
                            int enabled)
{
    struct hwc_context_t* ctx = (sun4i_hwc_context_t*) dev;

    switch (event) {
    case HWC_EVENT_VSYNC:
        ctx->vsync_enabled = !!enabled;
        return 0;
    }
    return -EINVAL;
}

static void hwc_registerProcs(struct hwc_composer_device_1* dev,
        hwc_procs_t const* procs)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    ctx->procs = const_cast<hwc_procs_t *>(procs);
}

static int hwc_init(sun4i_hwc_context_t *ctx)
{
	unsigned long               arg[4]={0};
    __disp_init_t init_para;

    ctx->dispfd = open("/dev/disp", O_RDWR);
    if (ctx->dispfd < 0)
    {
        ALOGE("Failed to open disp device, ret:%d, errno: %d\n", ctx->dispfd, errno);
        return  -1;
    }
    
    ctx->mFD_fb[0] = open("/dev/graphics/fb0", O_RDWR);
    if (ctx->mFD_fb[0] < 0)
    {
        ALOGE("Failed to open fb0 device, ret:%d, errno:%d\n", ctx->mFD_fb[0], errno);
        return  -1;
    }
    
    ctx->mFD_fb[1] = open("/dev/graphics/fb1", O_RDWR);
    if (ctx->mFD_fb[1] < 0)
    {
        ALOGE("Failed to open fb1 device, ret:%d, errno:%d\n", ctx->mFD_fb[1], errno);
        return  -1;
    }
    
    arg[0] = (unsigned long)&init_para;
    ioctl(ctx->dispfd,DISP_CMD_GET_DISP_INIT_PARA,(unsigned long)arg);

    if(init_para.disp_mode == DISP_INIT_MODE_TWO_DIFF_SCREEN_SAME_CONTENTS)
    {
    	ctx->mode = HWC_MODE_SCREEN0_GPU;
	}
	else
	{
	    ctx->mode = HWC_MODE_SCREEN0;
	}

	ctx->b_video_in_valid_area = 0;

    return 0;
}


static int hwc_device_close(struct hw_device_t *dev)
{
    //sun4i_hwc_context_t* ctx = (sun4i_hwc_context_t*)dev;

    //hwc_release(ctx);
    return 0;
}

/*****************************************************************************/

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device)
{
    int status = -EINVAL;
    if (!strcmp(name, HWC_HARDWARE_COMPOSER)) 
    {
        sun4i_hwc_context_t *dev;
        dev = (sun4i_hwc_context_t*)malloc(sizeof(*dev));

        /* initialize our state here */
        memset(dev, 0, sizeof(*dev));

        /* initialize the procs */
        dev->device.common.tag      = HARDWARE_DEVICE_TAG;
        dev->device.common.version  = HWC_DEVICE_API_VERSION_1_0;  //0
        dev->device.common.module   = const_cast<hw_module_t*>(module);
        dev->device.common.close    = hwc_device_close;

        dev->device.prepare         = hwc_prepare;
        dev->device.set             = hwc_set;
        dev->device.blank           = hwc_blank;
        dev->device.eventControl    = hwc_eventControl;
        dev->device.registerProcs   = hwc_registerProcs;
        dev->device.setparameter    = hwc_setparameter;
        dev->device.getparameter    = hwc_getparameter;
        dev->device.setlayerorder    = hwc_setlayerorder;

        *device = &dev->device.common;
        status = 0;
        dev->layer_para_set = false;

        hwc_init(dev);

        if(dev->device.common.version == HWC_DEVICE_API_VERSION_1_0)
        {
            status = pthread_create(&dev->vsync_thread, NULL, hwc_vsync_thread, dev);
            if (status) {
	        ALOGE("%s::pthread_create() failed : %s", __func__, strerror(status));
	        status = -status;
            }
        }
    }
    return status;
}
