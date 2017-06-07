/*
 * DXVA2 HW acceleration.
 *
 * copyright (c) 2010 Laurent Aimar
 *
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <assert.h>
#include <string.h>
#include <initguid.h>

#include "libavutil/common.h"
#include "libavutil/log.h"
#include "libavutil/time.h"

#include "avcodec.h"
#include "dxva2_internal.h"

/* define all the GUIDs used directly here,
 to avoid problems with inconsistent dxva2api.h versions in mingw-w64 and different MSVC version */
DEFINE_GUID(ff_DXVA2_ModeMPEG2_VLD,      0xee27417f, 0x5e28,0x4e65,0xbe,0xea,0x1d,0x26,0xb5,0x08,0xad,0xc9);
DEFINE_GUID(ff_DXVA2_ModeMPEG2and1_VLD,  0x86695f12, 0x340e,0x4f04,0x9f,0xd3,0x92,0x53,0xdd,0x32,0x74,0x60);
DEFINE_GUID(ff_DXVA2_ModeH264_E,         0x1b81be68, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(ff_DXVA2_ModeH264_F,         0x1b81be69, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(ff_DXVADDI_Intel_ModeH264_E, 0x604F8E68, 0x4951,0x4C54,0x88,0xFE,0xAB,0xD2,0x5C,0x15,0xB3,0xD6);
DEFINE_GUID(ff_DXVA2_ModeVC1_D,          0x1b81beA3, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(ff_DXVA2_ModeVC1_D2010,      0x1b81beA4, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(ff_DXVA2_ModeHEVC_VLD_Main,  0x5b11d51b, 0x2f4c,0x4452,0xbc,0xc3,0x09,0xf2,0xa1,0x16,0x0c,0xc0);
DEFINE_GUID(ff_DXVA2_ModeHEVC_VLD_Main10,0x107af0e0, 0xef1a,0x4d19,0xab,0xa8,0x67,0xa1,0x63,0x07,0x3d,0x13);
DEFINE_GUID(ff_DXVA2_NoEncrypt,          0x1b81beD0, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(ff_GUID_NULL,                0x00000000, 0x0000,0x0000,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00);
DEFINE_GUID(ff_IID_IDirectXVideoDecoderService, 0xfc51a551,0xd5e7,0x11d9,0xaf,0x55,0x00,0x05,0x4e,0x43,0xff,0x02);

typedef struct dxva_mode {
    const GUID     *guid;
    enum AVCodecID codec;
} dxva_mode;

static const dxva_mode dxva_modes[] = {
    /* MPEG-2 */
    { &ff_DXVA2_ModeMPEG2_VLD,       AV_CODEC_ID_MPEG2VIDEO },
    { &ff_DXVA2_ModeMPEG2and1_VLD,   AV_CODEC_ID_MPEG2VIDEO },

    /* H.264 */
    { &ff_DXVA2_ModeH264_F,          AV_CODEC_ID_H264 },
    { &ff_DXVA2_ModeH264_E,          AV_CODEC_ID_H264 },
    /* Intel specific H.264 mode */
    { &ff_DXVADDI_Intel_ModeH264_E,  AV_CODEC_ID_H264 },

    /* VC-1 / WMV3 */
    { &ff_DXVA2_ModeVC1_D2010,       AV_CODEC_ID_VC1 },
    { &ff_DXVA2_ModeVC1_D2010,       AV_CODEC_ID_WMV3 },
    { &ff_DXVA2_ModeVC1_D,           AV_CODEC_ID_VC1 },
    { &ff_DXVA2_ModeVC1_D,           AV_CODEC_ID_WMV3 },

    /* HEVC/H.265 */
    { &ff_DXVA2_ModeHEVC_VLD_Main,   AV_CODEC_ID_HEVC },
    { &ff_DXVA2_ModeHEVC_VLD_Main10, AV_CODEC_ID_HEVC },

    { NULL,                          0 },
};

static int dxva_get_decoder_configuration(AVCodecContext *avctx,
                                          const void *cfg_list,
                                          unsigned cfg_count)
{
    FFDXVASharedContext *sctx = DXVA_SHARED_CONTEXT(avctx);
    unsigned i, best_score = 0;
    int best_cfg = -1;

    for (i = 0; i < cfg_count; i++) {
        unsigned score;
        UINT ConfigBitstreamRaw;
        GUID guidConfigBitstreamEncryption;

#if CONFIG_D3D11VA
        if (sctx->pix_fmt == AV_PIX_FMT_D3D11) {
            D3D11_VIDEO_DECODER_CONFIG *cfg = &((D3D11_VIDEO_DECODER_CONFIG *)cfg_list)[i];
            ConfigBitstreamRaw = cfg->ConfigBitstreamRaw;
            guidConfigBitstreamEncryption = cfg->guidConfigBitstreamEncryption;
        }
#endif
#if CONFIG_DXVA2
        if (sctx->pix_fmt == AV_PIX_FMT_DXVA2_VLD) {
            DXVA2_ConfigPictureDecode *cfg = &((DXVA2_ConfigPictureDecode *)cfg_list)[i];
            ConfigBitstreamRaw = cfg->ConfigBitstreamRaw;
            guidConfigBitstreamEncryption = cfg->guidConfigBitstreamEncryption;
        }
#endif

        if (ConfigBitstreamRaw == 1)
            score = 1;
        else if (avctx->codec_id == AV_CODEC_ID_H264 && ConfigBitstreamRaw == 2)
            score = 2;
        else
            continue;
        if (IsEqualGUID(&guidConfigBitstreamEncryption, &ff_DXVA2_NoEncrypt))
            score += 16;
        if (score > best_score) {
            best_score = score;
            best_cfg = i;
        }
    }

    if (!best_score) {
        av_log(avctx, AV_LOG_VERBOSE, "No valid decoder configuration available\n");
        return AVERROR(EINVAL);
    }

    return best_cfg;
}

#if CONFIG_D3D11VA
static int d3d11va_validate_output(void *service, GUID guid, void *surface_format)
{
    HRESULT hr;
    BOOL is_supported = FALSE;
    hr = ID3D11VideoDevice_CheckVideoDecoderFormat((ID3D11VideoDevice *)service,
                                                   &guid,
                                                   *(DXGI_FORMAT *)surface_format,
                                                   &is_supported);
    return SUCCEEDED(hr) && is_supported;
}
#endif

#if CONFIG_DXVA2
static int dxva2_validate_output(void *decoder_service, GUID guid, void *surface_format)
{
    HRESULT hr;
    int ret = 0;
    unsigned j, target_count;
    D3DFORMAT *target_list;
    hr = IDirectXVideoDecoderService_GetDecoderRenderTargets((IDirectXVideoDecoderService *)decoder_service, &guid, &target_count, &target_list);
    if (SUCCEEDED(hr)) {
        for (j = 0; j < target_count; j++) {
            const D3DFORMAT format = target_list[j];
            if (format == *(D3DFORMAT *)surface_format) {
                ret = 1;
                break;
            }
        }
        CoTaskMemFree(target_list);
    }
    return ret;
}
#endif

static int dxva_get_decoder_guid(AVCodecContext *avctx, void *service, void *surface_format,
                                 unsigned guid_count, const GUID *guid_list, GUID *decoder_guid)
{
    FFDXVASharedContext *sctx = DXVA_SHARED_CONTEXT(avctx);
    unsigned i, j;

    *decoder_guid = ff_GUID_NULL;
    for (i = 0; dxva_modes[i].guid; i++) {
        const dxva_mode *mode = &dxva_modes[i];
        int validate;
        if (mode->codec != avctx->codec_id)
            continue;

        for (j = 0; j < guid_count; j++) {
            if (IsEqualGUID(mode->guid, &guid_list[j]))
                break;
        }
        if (j == guid_count)
            continue;

#if CONFIG_D3D11VA
        if (sctx->pix_fmt == AV_PIX_FMT_D3D11)
            validate = d3d11va_validate_output(service, *mode->guid, surface_format);
#endif
#if CONFIG_DXVA2
        if (sctx->pix_fmt == AV_PIX_FMT_DXVA2_VLD)
            validate = dxva2_validate_output(service, *mode->guid, surface_format);
#endif
        if (validate) {
            *decoder_guid = *mode->guid;
            break;
        }
    }

    if (IsEqualGUID(decoder_guid, &ff_GUID_NULL)) {
        av_log(avctx, AV_LOG_VERBOSE, "No decoder device for codec found\n");
        return AVERROR(EINVAL);
    }

    if (IsEqualGUID(decoder_guid, &ff_DXVADDI_Intel_ModeH264_E))
        sctx->workaround |= FF_DXVA2_WORKAROUND_INTEL_CLEARVIDEO;

    return 0;
}

static void bufref_free_interface(void *opaque, uint8_t *data)
{
    IUnknown_Release((IUnknown *)opaque);
}

static AVBufferRef *bufref_wrap_interface(IUnknown *iface)
{
    return av_buffer_create((uint8_t*)iface, 1, bufref_free_interface, iface, 0);
}

#if CONFIG_DXVA2

static int dxva2_get_decoder_configuration(AVCodecContext *avctx, const GUID *device_guid,
                                           const DXVA2_VideoDesc *desc,
                                           DXVA2_ConfigPictureDecode *config)
{
    FFDXVASharedContext *sctx = DXVA_SHARED_CONTEXT(avctx);
    unsigned cfg_count;
    DXVA2_ConfigPictureDecode *cfg_list;
    HRESULT hr;
    int ret;

    hr = IDirectXVideoDecoderService_GetDecoderConfigurations(sctx->dxva2_service, device_guid, desc, NULL, &cfg_count, &cfg_list);
    if (FAILED(hr)) {
        av_log(avctx, AV_LOG_ERROR, "Unable to retrieve decoder configurations\n");
        return AVERROR(EINVAL);
    }

    ret = dxva_get_decoder_configuration(avctx, cfg_list, cfg_count);
    if (ret >= 0)
        *config = cfg_list[ret];
    CoTaskMemFree(cfg_list);
    return ret;
}

static int dxva2_create_decoder(AVCodecContext *avctx)
{
    FFDXVASharedContext *sctx = DXVA_SHARED_CONTEXT(avctx);
    GUID *guid_list;
    unsigned guid_count;
    GUID device_guid;
    D3DFORMAT surface_format = avctx->sw_pix_fmt == AV_PIX_FMT_YUV420P10 ?
                               MKTAG('P', '0', '1', '0') : MKTAG('N', 'V', '1', '2');
    DXVA2_VideoDesc desc = { 0 };
    DXVA2_ConfigPictureDecode config;
    HRESULT hr;
    int ret;
    HANDLE device_handle;
    AVHWFramesContext *frames_ctx = (AVHWFramesContext*)avctx->hw_frames_ctx->data;
    AVDXVA2FramesContext *frames_hwctx = frames_ctx->hwctx;
    AVDXVA2DeviceContext *device_hwctx = frames_ctx->device_ctx->hwctx;

    hr = IDirect3DDeviceManager9_OpenDeviceHandle(device_hwctx->devmgr,
                                                  &device_handle);
    if (FAILED(hr)) {
        av_log(avctx, AV_LOG_ERROR, "Failed to open a device handle\n");
        goto fail;
    }

    hr = IDirect3DDeviceManager9_GetVideoService(device_hwctx->devmgr, device_handle,
                                                 &ff_IID_IDirectXVideoDecoderService,
                                                 (void **)&sctx->dxva2_service);
    IDirect3DDeviceManager9_CloseDeviceHandle(device_hwctx->devmgr, device_handle);
    if (FAILED(hr)) {
        av_log(avctx, AV_LOG_ERROR, "Failed to create IDirectXVideoDecoderService\n");
        goto fail;
    }

    hr = IDirectXVideoDecoderService_GetDecoderDeviceGuids(sctx->dxva2_service, &guid_count, &guid_list);
    if (FAILED(hr)) {
        av_log(avctx, AV_LOG_ERROR, "Failed to retrieve decoder device GUIDs\n");
        goto fail;
    }

    ret = dxva_get_decoder_guid(avctx, sctx->dxva2_service, &surface_format,
                                guid_count, guid_list, &device_guid);
    CoTaskMemFree(guid_list);
    if (ret < 0) {
        goto fail;
    }

    desc.SampleWidth  = avctx->coded_width;
    desc.SampleHeight = avctx->coded_height;
    desc.Format       = surface_format;

    ret = dxva2_get_decoder_configuration(avctx, &device_guid, &desc, &config);
    if (ret < 0) {
        goto fail;
    }

    hr = IDirectXVideoDecoderService_CreateVideoDecoder(sctx->dxva2_service, &device_guid,
                                                        &desc, &config, frames_hwctx->surfaces,
                                                        frames_hwctx->nb_surfaces, &sctx->dxva2_decoder);
    if (FAILED(hr)) {
        av_log(avctx, AV_LOG_ERROR, "Failed to create DXVA2 video decoder\n");
        goto fail;
    }

    sctx->dxva2_config = config;

    sctx->decoder_ref = bufref_wrap_interface((IUnknown *)sctx->dxva2_decoder);
    if (!sctx->decoder_ref)
        return AVERROR(ENOMEM);

    return 0;
fail:
    return AVERROR(EINVAL);
}

#endif

#if CONFIG_D3D11VA

static int d3d11va_get_decoder_configuration(AVCodecContext *avctx,
                                             ID3D11VideoDevice *video_device,
                                             const D3D11_VIDEO_DECODER_DESC *desc,
                                             D3D11_VIDEO_DECODER_CONFIG *config)
{
    FFDXVASharedContext *sctx = DXVA_SHARED_CONTEXT(avctx);
    unsigned cfg_count = 0;
    D3D11_VIDEO_DECODER_CONFIG *cfg_list = NULL;
    HRESULT hr;
    int i, ret;

    hr = ID3D11VideoDevice_GetVideoDecoderConfigCount(video_device, desc, &cfg_count);
    if (FAILED(hr)) {
        av_log(avctx, AV_LOG_ERROR, "Unable to retrieve decoder configurations\n");
        return AVERROR(EINVAL);
    }

    cfg_list = av_malloc_array(cfg_count, sizeof(D3D11_VIDEO_DECODER_CONFIG));
    if (cfg_list == NULL)
        return AVERROR(ENOMEM);
    for (i = 0; i < cfg_count; i++) {
        hr = ID3D11VideoDevice_GetVideoDecoderConfig(video_device, desc, i, &cfg_list[i]);
        if (FAILED(hr)) {
            av_log(avctx, AV_LOG_ERROR, "Unable to retrieve decoder configurations. (hr=0x%lX)\n", hr);
            av_free(cfg_list);
            return AVERROR(EINVAL);
        }
    }

    ret = dxva_get_decoder_configuration(avctx, cfg_list, cfg_count);
    if (ret >= 0)
        *config = cfg_list[ret];
    av_free(cfg_list);
    return ret;
}

static int d3d11va_create_decoder(AVCodecContext *avctx)
{
    FFDXVASharedContext *sctx = DXVA_SHARED_CONTEXT(avctx);
    GUID *guid_list;
    unsigned guid_count, i;
    GUID decoder_guid;
    DXGI_FORMAT surface_format = avctx->sw_pix_fmt == AV_PIX_FMT_YUV420P10 ?
                                 DXGI_FORMAT_P010 : DXGI_FORMAT_NV12;
    D3D11_VIDEO_DECODER_DESC desc = { 0 };
    D3D11_VIDEO_DECODER_CONFIG config;
    AVHWFramesContext *frames_ctx = (AVHWFramesContext *)avctx->hw_frames_ctx->data;
    AVD3D11VADeviceContext *device_hwctx = frames_ctx->device_ctx->hwctx;
    AVD3D11VAFramesContext *frames_hwctx = frames_ctx->hwctx;
    D3D11_TEXTURE2D_DESC texdesc;
    HRESULT hr;
    int ret;

    if (!frames_hwctx->texture) {
        av_log(avctx, AV_LOG_ERROR, "AVD3D11VAFramesContext.texture not set.\n");
        return AVERROR(EINVAL);
    }
    ID3D11Texture2D_GetDesc(frames_hwctx->texture, &texdesc);

    guid_count = ID3D11VideoDevice_GetVideoDecoderProfileCount(device_hwctx->video_device);
    guid_list = av_malloc_array(guid_count, sizeof(*guid_list));
    if (guid_list == NULL || guid_count == 0) {
        av_log(avctx, AV_LOG_ERROR, "Failed to get the decoder GUIDs\n");
        av_free(guid_list);
        return AVERROR(EINVAL);
    }
    for (i = 0; i < guid_count; i++) {
        hr = ID3D11VideoDevice_GetVideoDecoderProfile(device_hwctx->video_device, i, &guid_list[i]);
        if (FAILED(hr)) {
            av_log(avctx, AV_LOG_ERROR, "Failed to retrieve decoder GUID %d\n", i);
            av_free(guid_list);
            return AVERROR(EINVAL);
        }
    }

    ret = dxva_get_decoder_guid(avctx, device_hwctx->video_device, &surface_format,
                                guid_count, guid_list, &decoder_guid);
    av_free(guid_list);
    if (ret < 0)
        return AVERROR(EINVAL);

    desc.SampleWidth  = avctx->coded_width;
    desc.SampleHeight = avctx->coded_height;
    desc.OutputFormat = surface_format;
    desc.Guid         = decoder_guid;

    ret = d3d11va_get_decoder_configuration(avctx, device_hwctx->video_device, &desc, &config);
    if (ret < 0)
        return AVERROR(EINVAL);

    sctx->d3d11_views = av_mallocz_array(texdesc.ArraySize, sizeof(sctx->d3d11_views[0]));
    if (!sctx->d3d11_views)
        return AVERROR(ENOMEM);
    sctx->nb_d3d11_views = texdesc.ArraySize;

    for (i = 0; i < sctx->nb_d3d11_views; i++) {
        D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC viewDesc = {
            .DecodeProfile = decoder_guid,
            .ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D,
            .Texture2D = {
                .ArraySlice = i,
            }
        };
        hr = ID3D11VideoDevice_CreateVideoDecoderOutputView(device_hwctx->video_device,
                                                            (ID3D11Resource*) frames_hwctx->texture,
                                                            &viewDesc,
                                                            (ID3D11VideoDecoderOutputView**) &sctx->d3d11_views[i]);
        if (FAILED(hr)) {
            av_log(avctx, AV_LOG_ERROR, "Could not create the decoder output view %d\n", i);
            return AVERROR_UNKNOWN;
        }
    }

    hr = ID3D11VideoDevice_CreateVideoDecoder(device_hwctx->video_device, &desc,
                                              &config, &sctx->d3d11_decoder);
    if (FAILED(hr)) {
        av_log(avctx, AV_LOG_ERROR, "Failed to create D3D11VA video decoder\n");
        return AVERROR(EINVAL);
    }

    sctx->d3d11_config = config;
    sctx->d3d11_texture = frames_hwctx->texture;

    sctx->decoder_ref = bufref_wrap_interface((IUnknown *)sctx->d3d11_decoder);
    if (!sctx->decoder_ref)
        return AVERROR(ENOMEM);

    return 0;
}

#endif

static void ff_dxva2_lock(AVCodecContext *avctx)
{
#if CONFIG_D3D11VA
    if (ff_dxva2_is_d3d11(avctx)) {
        FFDXVASharedContext *sctx = DXVA_SHARED_CONTEXT(avctx);
        AVDXVAContext *ctx = DXVA_CONTEXT(avctx);
        if (D3D11VA_CONTEXT(ctx)->context_mutex != INVALID_HANDLE_VALUE)
            WaitForSingleObjectEx(D3D11VA_CONTEXT(ctx)->context_mutex, INFINITE, FALSE);
        if (sctx->device_ctx) {
            AVD3D11VADeviceContext *hwctx = sctx->device_ctx->hwctx;
            hwctx->lock(hwctx->lock_ctx);
        }
    }
#endif
}

static void ff_dxva2_unlock(AVCodecContext *avctx)
{
#if CONFIG_D3D11VA
    if (ff_dxva2_is_d3d11(avctx)) {
        FFDXVASharedContext *sctx = DXVA_SHARED_CONTEXT(avctx);
        AVDXVAContext *ctx = DXVA_CONTEXT(avctx);
        if (D3D11VA_CONTEXT(ctx)->context_mutex != INVALID_HANDLE_VALUE)
            ReleaseMutex(D3D11VA_CONTEXT(ctx)->context_mutex);
        if (sctx->device_ctx) {
            AVD3D11VADeviceContext *hwctx = sctx->device_ctx->hwctx;
            hwctx->unlock(hwctx->lock_ctx);
        }
    }
#endif
}

// This must work before the decoder is created.
// This somehow needs to be exported to the user.
static void dxva_adjust_hwframes(AVCodecContext *avctx, AVHWFramesContext *frames_ctx)
{
    FFDXVASharedContext *sctx = DXVA_SHARED_CONTEXT(avctx);
    int surface_alignment, num_surfaces;

    frames_ctx->format = sctx->pix_fmt;

    /* decoding MPEG-2 requires additional alignment on some Intel GPUs,
    but it causes issues for H.264 on certain AMD GPUs..... */
    if (avctx->codec_id == AV_CODEC_ID_MPEG2VIDEO)
        surface_alignment = 32;
    /* the HEVC DXVA2 spec asks for 128 pixel aligned surfaces to ensure
    all coding features have enough room to work with */
    else if (avctx->codec_id == AV_CODEC_ID_HEVC)
        surface_alignment = 128;
    else
        surface_alignment = 16;

    /* 4 base work surfaces */
    num_surfaces = 4;

    /* add surfaces based on number of possible refs */
    if (avctx->codec_id == AV_CODEC_ID_H264 || avctx->codec_id == AV_CODEC_ID_HEVC)
        num_surfaces += 16;
    else
        num_surfaces += 2;

    /* add extra surfaces for frame threading */
    if (avctx->active_thread_type & FF_THREAD_FRAME)
        num_surfaces += avctx->thread_count;

    frames_ctx->sw_format = avctx->sw_pix_fmt == AV_PIX_FMT_YUV420P10 ?
                            AV_PIX_FMT_P010 : AV_PIX_FMT_NV12;
    frames_ctx->width = FFALIGN(avctx->coded_width, surface_alignment);
    frames_ctx->height = FFALIGN(avctx->coded_height, surface_alignment);
    frames_ctx->initial_pool_size = num_surfaces;


#if CONFIG_DXVA2
    if (frames_ctx->format == AV_PIX_FMT_DXVA2_VLD) {
        AVDXVA2FramesContext *frames_hwctx = frames_ctx->hwctx;

        frames_hwctx->surface_type = DXVA2_VideoDecoderRenderTarget;
    }
#endif

#if CONFIG_D3D11VA
    if (frames_ctx->format == AV_PIX_FMT_D3D11) {
        AVD3D11VAFramesContext *frames_hwctx = frames_ctx->hwctx;

        frames_hwctx->BindFlags |= D3D11_BIND_DECODER;
    }
#endif
}

int ff_dxva2_decode_init(AVCodecContext *avctx)
{
    FFDXVASharedContext *sctx = DXVA_SHARED_CONTEXT(avctx);
    AVHWFramesContext *frames_ctx = NULL;
    int ret = 0;

    // Old API.
    if (avctx->hwaccel_context)
        return 0;

    // (avctx->pix_fmt is not updated yet at this point)
    sctx->pix_fmt = avctx->hwaccel->pix_fmt;

    if (avctx->codec_id == AV_CODEC_ID_H264 &&
        (avctx->profile & ~FF_PROFILE_H264_CONSTRAINED) > FF_PROFILE_H264_HIGH) {
        av_log(avctx, AV_LOG_VERBOSE, "Unsupported H.264 profile for DXVA HWAccel: %d\n",avctx->profile);
        return AVERROR(ENOTSUP);
    }

    if (avctx->codec_id == AV_CODEC_ID_HEVC &&
        avctx->profile != FF_PROFILE_HEVC_MAIN && avctx->profile != FF_PROFILE_HEVC_MAIN_10) {
        av_log(avctx, AV_LOG_VERBOSE, "Unsupported HEVC profile for DXVA HWAccel: %d\n", avctx->profile);
        return AVERROR(ENOTSUP);
    }

    if (!avctx->hw_frames_ctx && !avctx->hw_device_ctx) {
        av_log(avctx, AV_LOG_ERROR, "Either a hw_frames_ctx or a hw_device_ctx needs to be set for hardware decoding.\n");
        return AVERROR(EINVAL);
    }

    if (avctx->hw_frames_ctx) {
        frames_ctx = (AVHWFramesContext*)avctx->hw_frames_ctx->data;
    } else {
        avctx->hw_frames_ctx = av_hwframe_ctx_alloc(avctx->hw_device_ctx);
        if (!avctx->hw_frames_ctx)
            return AVERROR(ENOMEM);

        frames_ctx = (AVHWFramesContext*)avctx->hw_frames_ctx->data;

        dxva_adjust_hwframes(avctx, frames_ctx);

        ret = av_hwframe_ctx_init(avctx->hw_frames_ctx);
        if (ret < 0)
            goto fail;
    }

    sctx->device_ctx = frames_ctx->device_ctx;

    if (frames_ctx->format != sctx->pix_fmt ||
        !((sctx->pix_fmt == AV_PIX_FMT_D3D11 && CONFIG_D3D11VA) ||
          (sctx->pix_fmt == AV_PIX_FMT_DXVA2_VLD && CONFIG_DXVA2))) {
        av_log(avctx, AV_LOG_ERROR, "Invalid pixfmt for hwaccel!\n");
        ret = AVERROR(EINVAL);
        goto fail;
    }

#if CONFIG_D3D11VA
    if (sctx->pix_fmt == AV_PIX_FMT_D3D11) {
        AVD3D11VADeviceContext *device_hwctx = frames_ctx->device_ctx->hwctx;
        AVD3D11VAContext *d3d11_ctx = &sctx->ctx.d3d11va;
        HRESULT hr;

        ff_dxva2_lock(avctx);
        ret = d3d11va_create_decoder(avctx);
        ff_dxva2_unlock(avctx);
        if (ret < 0)
            goto fail;

        d3d11_ctx->decoder       = sctx->d3d11_decoder;
        d3d11_ctx->video_context = device_hwctx->video_context;
        d3d11_ctx->cfg           = &sctx->d3d11_config;
        d3d11_ctx->surface_count = sctx->nb_d3d11_views;
        d3d11_ctx->surface       = sctx->d3d11_views;
        d3d11_ctx->workaround    = sctx->workaround;
        d3d11_ctx->context_mutex = INVALID_HANDLE_VALUE;
    }
#endif

#if CONFIG_DXVA2
    if (sctx->pix_fmt == AV_PIX_FMT_DXVA2_VLD) {
        AVDXVA2FramesContext *frames_hwctx = frames_ctx->hwctx;
        struct dxva_context *dxva_ctx = &sctx->ctx.dxva2;

        ff_dxva2_lock(avctx);
        ret = dxva2_create_decoder(avctx);
        ff_dxva2_unlock(avctx);
        if (ret < 0)
            goto fail;

        dxva_ctx->decoder       = sctx->dxva2_decoder;
        dxva_ctx->cfg           = &sctx->dxva2_config;
        dxva_ctx->surface       = frames_hwctx->surfaces;
        dxva_ctx->surface_count = frames_hwctx->nb_surfaces;
        dxva_ctx->workaround    = sctx->workaround;
    }
#endif

    return 0;

fail:
    ff_dxva2_decode_uninit(avctx);
    return ret;
}

int ff_dxva2_decode_uninit(AVCodecContext *avctx)
{
    FFDXVASharedContext *sctx = DXVA_SHARED_CONTEXT(avctx);
    int i;

    av_buffer_unref(&sctx->decoder_ref);

#if CONFIG_D3D11VA
    for (i = 0; i < sctx->nb_d3d11_views; i++) {
        if (sctx->d3d11_views[i])
            ID3D11VideoDecoderOutputView_Release(sctx->d3d11_views[i]);
    }
    av_freep(&sctx->d3d11_views);
#endif

#if CONFIG_DXVA2
    if (sctx->dxva2_service)
        IDirectXVideoDecoderService_Release(sctx->dxva2_service);
#endif

    return 0;
}

static void *get_surface(AVCodecContext *avctx, const AVFrame *frame)
{
#if CONFIG_D3D11VA
    if (frame->format == AV_PIX_FMT_D3D11) {
        FFDXVASharedContext *sctx = DXVA_SHARED_CONTEXT(avctx);
        intptr_t index = (intptr_t)frame->data[1];
        if (index < 0 || index >= sctx->nb_d3d11_views ||
            sctx->d3d11_texture != (ID3D11Texture2D *)frame->data[0]) {
            av_log(avctx, AV_LOG_ERROR, "get_buffer frame is invalid!\n");
            return NULL;
        }
        return sctx->d3d11_views[index];
    }
#endif
    return frame->data[3];
}

unsigned ff_dxva2_get_surface_index(const AVCodecContext *avctx,
                                    const AVDXVAContext *ctx,
                                    const AVFrame *frame)
{
    void *surface = get_surface(avctx, frame);
    unsigned i;

#if CONFIG_D3D11VA
    if (avctx->pix_fmt == AV_PIX_FMT_D3D11)
        return (intptr_t)frame->data[1];
    if (avctx->pix_fmt == AV_PIX_FMT_D3D11VA_VLD) {
        D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC viewDesc;
        ID3D11VideoDecoderOutputView_GetDesc((ID3D11VideoDecoderOutputView*) surface, &viewDesc);
        return viewDesc.Texture2D.ArraySlice;
    }
#endif
#if CONFIG_DXVA2
    for (i = 0; i < DXVA_CONTEXT_COUNT(avctx, ctx); i++) {
        if (avctx->pix_fmt == AV_PIX_FMT_DXVA2_VLD && ctx->dxva2.surface[i] == surface)
            return i;
    }
#endif

    assert(0);
    return 0;
}

int ff_dxva2_commit_buffer(AVCodecContext *avctx,
                           AVDXVAContext *ctx,
                           DECODER_BUFFER_DESC *dsc,
                           unsigned type, const void *data, unsigned size,
                           unsigned mb_count)
{
    void     *dxva_data;
    unsigned dxva_size;
    int      result;
    HRESULT hr;

#if CONFIG_D3D11VA
    if (ff_dxva2_is_d3d11(avctx))
        hr = ID3D11VideoContext_GetDecoderBuffer(D3D11VA_CONTEXT(ctx)->video_context,
                                                 D3D11VA_CONTEXT(ctx)->decoder,
                                                 type,
                                                 &dxva_size, &dxva_data);
#endif
#if CONFIG_DXVA2
    if (avctx->pix_fmt == AV_PIX_FMT_DXVA2_VLD)
        hr = IDirectXVideoDecoder_GetBuffer(DXVA2_CONTEXT(ctx)->decoder, type,
                                            &dxva_data, &dxva_size);
#endif
    if (FAILED(hr)) {
        av_log(avctx, AV_LOG_ERROR, "Failed to get a buffer for %u: 0x%x\n",
               type, hr);
        return -1;
    }
    if (size <= dxva_size) {
        memcpy(dxva_data, data, size);

#if CONFIG_D3D11VA
        if (ff_dxva2_is_d3d11(avctx)) {
            D3D11_VIDEO_DECODER_BUFFER_DESC *dsc11 = dsc;
            memset(dsc11, 0, sizeof(*dsc11));
            dsc11->BufferType           = type;
            dsc11->DataSize             = size;
            dsc11->NumMBsInBuffer       = mb_count;
        }
#endif
#if CONFIG_DXVA2
        if (avctx->pix_fmt == AV_PIX_FMT_DXVA2_VLD) {
            DXVA2_DecodeBufferDesc *dsc2 = dsc;
            memset(dsc2, 0, sizeof(*dsc2));
            dsc2->CompressedBufferType = type;
            dsc2->DataSize             = size;
            dsc2->NumMBsInBuffer       = mb_count;
        }
#endif

        result = 0;
    } else {
        av_log(avctx, AV_LOG_ERROR, "Buffer for type %u was too small\n", type);
        result = -1;
    }

#if CONFIG_D3D11VA
    if (ff_dxva2_is_d3d11(avctx))
        hr = ID3D11VideoContext_ReleaseDecoderBuffer(D3D11VA_CONTEXT(ctx)->video_context, D3D11VA_CONTEXT(ctx)->decoder, type);
#endif
#if CONFIG_DXVA2
    if (avctx->pix_fmt == AV_PIX_FMT_DXVA2_VLD)
        hr = IDirectXVideoDecoder_ReleaseBuffer(DXVA2_CONTEXT(ctx)->decoder, type);
#endif
    if (FAILED(hr)) {
        av_log(avctx, AV_LOG_ERROR,
               "Failed to release buffer type %u: 0x%x\n",
               type, hr);
        result = -1;
    }
    return result;
}

static int frame_add_buf(AVFrame *frame, AVBufferRef *ref)
{
    int i;

    for (i = 0; i < AV_NUM_DATA_POINTERS; i++) {
        if (!frame->buf[i]) {
            frame->buf[i] = av_buffer_ref(ref);
            return frame->buf[i] ? 0 : AVERROR(ENOMEM);
        }
    }

    // For now we expect that the caller does not use more than
    // AV_NUM_DATA_POINTERS-1 buffers if the user uses a custom pool.
    return AVERROR(EINVAL);
}

int ff_dxva2_common_end_frame(AVCodecContext *avctx, AVFrame *frame,
                              const void *pp, unsigned pp_size,
                              const void *qm, unsigned qm_size,
                              int (*commit_bs_si)(AVCodecContext *,
                                                  DECODER_BUFFER_DESC *bs,
                                                  DECODER_BUFFER_DESC *slice))
{
    AVDXVAContext *ctx = DXVA_CONTEXT(avctx);
    unsigned               buffer_count = 0;
#if CONFIG_D3D11VA
    D3D11_VIDEO_DECODER_BUFFER_DESC buffer11[4];
#endif
#if CONFIG_DXVA2
    DXVA2_DecodeBufferDesc          buffer2[4];
#endif
    DECODER_BUFFER_DESC             *buffer,*buffer_slice;
    int result, runs = 0;
    HRESULT hr;
    unsigned type;
    FFDXVASharedContext *sctx = DXVA_SHARED_CONTEXT(avctx);

    if (sctx->decoder_ref) {
        result = frame_add_buf(frame, sctx->decoder_ref);
        if (result < 0)
            return result;
    }

    do {
        ff_dxva2_lock(avctx);
#if CONFIG_D3D11VA
        if (ff_dxva2_is_d3d11(avctx))
            hr = ID3D11VideoContext_DecoderBeginFrame(D3D11VA_CONTEXT(ctx)->video_context, D3D11VA_CONTEXT(ctx)->decoder,
                                                      get_surface(avctx, frame),
                                                      0, NULL);
#endif
#if CONFIG_DXVA2
        if (avctx->pix_fmt == AV_PIX_FMT_DXVA2_VLD)
            hr = IDirectXVideoDecoder_BeginFrame(DXVA2_CONTEXT(ctx)->decoder,
                                                 get_surface(avctx, frame),
                                                 NULL);
#endif
        if (hr != E_PENDING || ++runs > 50)
            break;
        ff_dxva2_unlock(avctx);
        av_usleep(2000);
    } while(1);

    if (FAILED(hr)) {
        av_log(avctx, AV_LOG_ERROR, "Failed to begin frame: 0x%x\n", hr);
        ff_dxva2_unlock(avctx);
        return -1;
    }

#if CONFIG_D3D11VA
    if (ff_dxva2_is_d3d11(avctx)) {
        buffer = &buffer11[buffer_count];
        type = D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS;
    }
#endif
#if CONFIG_DXVA2
    if (avctx->pix_fmt == AV_PIX_FMT_DXVA2_VLD) {
        buffer = &buffer2[buffer_count];
        type = DXVA2_PictureParametersBufferType;
    }
#endif
    result = ff_dxva2_commit_buffer(avctx, ctx, buffer,
                                    type,
                                    pp, pp_size, 0);
    if (result) {
        av_log(avctx, AV_LOG_ERROR,
               "Failed to add picture parameter buffer\n");
        goto end;
    }
    buffer_count++;

    if (qm_size > 0) {
#if CONFIG_D3D11VA
        if (ff_dxva2_is_d3d11(avctx)) {
            buffer = &buffer11[buffer_count];
            type = D3D11_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX;
        }
#endif
#if CONFIG_DXVA2
        if (avctx->pix_fmt == AV_PIX_FMT_DXVA2_VLD) {
            buffer = &buffer2[buffer_count];
            type = DXVA2_InverseQuantizationMatrixBufferType;
        }
#endif
        result = ff_dxva2_commit_buffer(avctx, ctx, buffer,
                                        type,
                                        qm, qm_size, 0);
        if (result) {
            av_log(avctx, AV_LOG_ERROR,
                   "Failed to add inverse quantization matrix buffer\n");
            goto end;
        }
        buffer_count++;
    }

#if CONFIG_D3D11VA
    if (ff_dxva2_is_d3d11(avctx)) {
        buffer       = &buffer11[buffer_count + 0];
        buffer_slice = &buffer11[buffer_count + 1];
    }
#endif
#if CONFIG_DXVA2
    if (avctx->pix_fmt == AV_PIX_FMT_DXVA2_VLD) {
        buffer       = &buffer2[buffer_count + 0];
        buffer_slice = &buffer2[buffer_count + 1];
    }
#endif

    result = commit_bs_si(avctx,
                          buffer,
                          buffer_slice);
    if (result) {
        av_log(avctx, AV_LOG_ERROR,
               "Failed to add bitstream or slice control buffer\n");
        goto end;
    }
    buffer_count += 2;

    /* TODO Film Grain when possible */

    assert(buffer_count == 1 + (qm_size > 0) + 2);

#if CONFIG_D3D11VA
    if (ff_dxva2_is_d3d11(avctx))
        hr = ID3D11VideoContext_SubmitDecoderBuffers(D3D11VA_CONTEXT(ctx)->video_context,
                                                     D3D11VA_CONTEXT(ctx)->decoder,
                                                     buffer_count, buffer11);
#endif
#if CONFIG_DXVA2
    if (avctx->pix_fmt == AV_PIX_FMT_DXVA2_VLD) {
        DXVA2_DecodeExecuteParams exec = {
            .NumCompBuffers     = buffer_count,
            .pCompressedBuffers = buffer2,
            .pExtensionData     = NULL,
        };
        hr = IDirectXVideoDecoder_Execute(DXVA2_CONTEXT(ctx)->decoder, &exec);
    }
#endif
    if (FAILED(hr)) {
        av_log(avctx, AV_LOG_ERROR, "Failed to execute: 0x%x\n", hr);
        result = -1;
    }

end:
#if CONFIG_D3D11VA
    if (ff_dxva2_is_d3d11(avctx))
        hr = ID3D11VideoContext_DecoderEndFrame(D3D11VA_CONTEXT(ctx)->video_context, D3D11VA_CONTEXT(ctx)->decoder);
#endif
#if CONFIG_DXVA2
    if (avctx->pix_fmt == AV_PIX_FMT_DXVA2_VLD)
        hr = IDirectXVideoDecoder_EndFrame(DXVA2_CONTEXT(ctx)->decoder, NULL);
#endif
    ff_dxva2_unlock(avctx);
    if (FAILED(hr)) {
        av_log(avctx, AV_LOG_ERROR, "Failed to end frame: 0x%x\n", hr);
        result = -1;
    }

    return result;
}

int ff_dxva2_is_d3d11(const AVCodecContext *avctx)
{
    if (CONFIG_D3D11VA)
        return avctx->pix_fmt == AV_PIX_FMT_D3D11VA_VLD ||
               avctx->pix_fmt == AV_PIX_FMT_D3D11;
    else
        return 0;
}
