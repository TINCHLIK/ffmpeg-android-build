/*
 * Interface to xvidcore for MPEG-4 encoding
 * Copyright (c) 2004 Adam Thayer <krevnik@comcast.net>
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

/**
 * @file
 * Interface to xvidcore for MPEG-4 compliant encoding.
 * @author Adam Thayer (krevnik@comcast.net)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xvid.h>

#include "libavutil/cpu.h"
#include "libavutil/internal.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/mathematics.h"
#include "libavutil/mem.h"
#include "libavutil/opt.h"

#include "avcodec.h"
#include "internal.h"
#include "mpegutils.h"

/**
 * Buffer management macros.
 */
#define BUFFER_SIZE         1024
#define BUFFER_REMAINING(x) (BUFFER_SIZE - strlen(x))
#define BUFFER_CAT(x)       (&((x)[strlen(x)]))

/**
 * Structure for the private Xvid context.
 * This stores all the private context for the codec.
 */
struct xvid_context {
    AVClass *class;                /**< Handle for Xvid encoder */
    void *encoder_handle;          /**< Handle for Xvid encoder */
    int xsize;                     /**< Frame x size */
    int ysize;                     /**< Frame y size */
    int vop_flags;                 /**< VOP flags for Xvid encoder */
    int vol_flags;                 /**< VOL flags for Xvid encoder */
    int me_flags;                  /**< Motion Estimation flags */
    int qscale;                    /**< Do we use constant scale? */
    int quicktime_format;          /**< Are we in a QT-based format? */
    char *twopassbuffer;           /**< Character buffer for two-pass */
    char *old_twopassbuffer;       /**< Old character buffer (two-pass) */
    char *twopassfile;             /**< second pass temp file name */
    unsigned char *intra_matrix;   /**< P-Frame Quant Matrix */
    unsigned char *inter_matrix;   /**< I-Frame Quant Matrix */
    int lumi_aq;                   /**< Lumi masking as an aq method */
    int variance_aq;               /**< Variance adaptive quantization */
    int ssim;                      /**< SSIM information display mode */
    int ssim_acc;                  /**< SSIM accuracy. 0: accurate. 4: fast. */
    int gmc;
    int me_quality;                /**< Motion estimation quality. 0: fast 6: best. */
    int mpeg_quant;                /**< Quantization type. 0: H.263, 1: MPEG */
};

/**
 * Structure for the private first-pass plugin.
 */
struct xvid_ff_pass1 {
    int version;                    /**< Xvid version */
    struct xvid_context *context;   /**< Pointer to private context */
};

static int xvid_encode_frame(AVCodecContext *avctx, AVPacket *pkt,
                             const AVFrame *picture, int *got_packet);


/*
 * Xvid 2-Pass Kludge Section
 *
 * Xvid's default 2-pass doesn't allow us to create data as we need to, so
 * this section spends time replacing the first pass plugin so we can write
 * statistic information as libavcodec requests in. We have another kludge
 * that allows us to pass data to the second pass in Xvid without a custom
 * rate-control plugin.
 */

/**
 * Initialize the two-pass plugin and context.
 *
 * @param param Input construction parameter structure
 * @param handle Private context handle
 * @return Returns XVID_ERR_xxxx on failure, or 0 on success.
 */
static int xvid_ff_2pass_create(xvid_plg_create_t *param, void **handle)
{
    struct xvid_ff_pass1 *x = (struct xvid_ff_pass1 *) param->param;
    char *log = x->context->twopassbuffer;

    /* Do a quick bounds check */
    if (!log)
        return XVID_ERR_FAIL;

    /* We use snprintf() */
    /* This is because we can safely prevent a buffer overflow */
    log[0] = 0;
    snprintf(log, BUFFER_REMAINING(log),
             "# avconv 2-pass log file, using xvid codec\n");
    snprintf(BUFFER_CAT(log), BUFFER_REMAINING(log),
             "# Do not modify. libxvidcore version: %d.%d.%d\n\n",
             XVID_VERSION_MAJOR(XVID_VERSION),
             XVID_VERSION_MINOR(XVID_VERSION),
             XVID_VERSION_PATCH(XVID_VERSION));

    *handle = x->context;
    return 0;
}

/**
 * Destroy the two-pass plugin context.
 *
 * @param ref Context pointer for the plugin
 * @param param Destroy context
 * @return Returns 0, success guaranteed
 */
static int xvid_ff_2pass_destroy(struct xvid_context *ref,
                                 xvid_plg_destroy_t *param)
{
    /* Currently cannot think of anything to do on destruction */
    /* Still, the framework should be here for reference/use */
    if (ref->twopassbuffer)
        ref->twopassbuffer[0] = 0;
    return 0;
}

/**
 * Enable fast encode mode during the first pass.
 *
 * @param ref Context pointer for the plugin
 * @param param Frame data
 * @return Returns 0, success guaranteed
 */
static int xvid_ff_2pass_before(struct xvid_context *ref,
                                xvid_plg_data_t *param)
{
    int motion_remove;
    int motion_replacements;
    int vop_remove;

    /* Nothing to do here, result is changed too much */
    if (param->zone && param->zone->mode == XVID_ZONE_QUANT)
        return 0;

    /* We can implement a 'turbo' first pass mode here */
    param->quant = 2;

    /* Init values */
    motion_remove       = ~XVID_ME_CHROMA_PVOP &
                          ~XVID_ME_CHROMA_BVOP &
                          ~XVID_ME_EXTSEARCH16 &
                          ~XVID_ME_ADVANCEDDIAMOND16;
    motion_replacements = XVID_ME_FAST_MODEINTERPOLATE |
                          XVID_ME_SKIP_DELTASEARCH     |
                          XVID_ME_FASTREFINE16         |
                          XVID_ME_BFRAME_EARLYSTOP;
    vop_remove          = ~XVID_VOP_MODEDECISION_RD      &
                          ~XVID_VOP_FAST_MODEDECISION_RD &
                          ~XVID_VOP_TRELLISQUANT         &
                          ~XVID_VOP_INTER4V              &
                          ~XVID_VOP_HQACPRED;

    param->vol_flags    &= ~XVID_VOL_GMC;
    param->vop_flags    &= vop_remove;
    param->motion_flags &= motion_remove;
    param->motion_flags |= motion_replacements;

    return 0;
}

/**
 * Capture statistic data and write it during first pass.
 *
 * @param ref Context pointer for the plugin
 * @param param Statistic data
 * @return Returns XVID_ERR_xxxx on failure, or 0 on success
 */
static int xvid_ff_2pass_after(struct xvid_context *ref,
                               xvid_plg_data_t *param)
{
    char *log = ref->twopassbuffer;
    const char *frame_types = " ipbs";
    char frame_type;

    /* Quick bounds check */
    if (!log)
        return XVID_ERR_FAIL;

    /* Convert the type given to us into a character */
    if (param->type < 5 && param->type > 0)
        frame_type = frame_types[param->type];
    else
        return XVID_ERR_FAIL;

    snprintf(BUFFER_CAT(log), BUFFER_REMAINING(log),
             "%c %d %d %d %d %d %d\n",
             frame_type, param->stats.quant, param->stats.kblks,
             param->stats.mblks, param->stats.ublks,
             param->stats.length, param->stats.hlength);

    return 0;
}

/**
 * Dispatch function for our custom plugin.
 * This handles the dispatch for the Xvid plugin. It passes data
 * on to other functions for actual processing.
 *
 * @param ref Context pointer for the plugin
 * @param cmd The task given for us to complete
 * @param p1 First parameter (varies)
 * @param p2 Second parameter (varies)
 * @return Returns XVID_ERR_xxxx on failure, or 0 on success
 */
static int xvid_ff_2pass(void *ref, int cmd, void *p1, void *p2)
{
    switch (cmd) {
    case XVID_PLG_INFO:
    case XVID_PLG_FRAME:
        return 0;
    case XVID_PLG_BEFORE:
        return xvid_ff_2pass_before(ref, p1);
    case XVID_PLG_CREATE:
        return xvid_ff_2pass_create(p1, p2);
    case XVID_PLG_AFTER:
        return xvid_ff_2pass_after(ref, p1);
    case XVID_PLG_DESTROY:
        return xvid_ff_2pass_destroy(ref, p1);
    default:
        return XVID_ERR_FAIL;
    }
}

/**
 * Routine to create a global VO/VOL header for MP4 container.
 * What we do here is extract the header from the Xvid bitstream
 * as it is encoded. We also strip the repeated headers from the
 * bitstream when a global header is requested for MPEG-4 ISO
 * compliance.
 *
 * @param avctx AVCodecContext pointer to context
 * @param frame Pointer to encoded frame data
 * @param header_len Length of header to search
 * @param frame_len Length of encoded frame data
 * @return Returns new length of frame data
 */
static int xvid_strip_vol_header(AVCodecContext *avctx, AVPacket *pkt,
                                 unsigned int header_len,
                                 unsigned int frame_len)
{
    int vo_len = 0, i;

    for (i = 0; i < header_len - 3; i++) {
        if (pkt->data[i]     == 0x00 &&
            pkt->data[i + 1] == 0x00 &&
            pkt->data[i + 2] == 0x01 &&
            pkt->data[i + 3] == 0xB6) {
            vo_len = i;
            break;
        }
    }

    if (vo_len > 0) {
        /* We need to store the header, so extract it */
        if (!avctx->extradata) {
            avctx->extradata = av_malloc(vo_len);
            if (!avctx->extradata)
                return AVERROR(ENOMEM);
            memcpy(avctx->extradata, pkt->data, vo_len);
            avctx->extradata_size = vo_len;
        }
        /* Less dangerous now, memmove properly copies the two
         * chunks of overlapping data */
        memmove(pkt->data, &pkt->data[vo_len], frame_len - vo_len);
        pkt->size = frame_len - vo_len;
    }
    return 0;
}

/**
 * Routine to correct a possibly erroneous framerate being fed to us.
 * Xvid currently chokes on framerates where the ticks per frame is
 * extremely large. This function works to correct problems in this area
 * by estimating a new framerate and taking the simpler fraction of
 * the two presented.
 *
 * @param avctx Context that contains the framerate to correct.
 */
static void xvid_correct_framerate(AVCodecContext *avctx)
{
    int frate, fbase;
    int est_frate, est_fbase;
    int gcd;
    float est_fps, fps;

    frate = avctx->time_base.den;
    fbase = avctx->time_base.num;

    gcd = av_gcd(frate, fbase);
    if (gcd > 1) {
        frate /= gcd;
        fbase /= gcd;
    }

    if (frate <= 65000 && fbase <= 65000) {
        avctx->time_base.den = frate;
        avctx->time_base.num = fbase;
        return;
    }

    fps     = (float) frate / (float) fbase;
    est_fps = roundf(fps * 1000.0) / 1000.0;

    est_frate = (int) est_fps;
    if (est_fps > (int) est_fps) {
        est_frate = (est_frate + 1) * 1000;
        est_fbase = (int) roundf((float) est_frate / est_fps);
    } else
        est_fbase = 1;

    gcd = av_gcd(est_frate, est_fbase);
    if (gcd > 1) {
        est_frate /= gcd;
        est_fbase /= gcd;
    }

    if (fbase > est_fbase) {
        avctx->time_base.den = est_frate;
        avctx->time_base.num = est_fbase;
        av_log(avctx, AV_LOG_DEBUG,
               "Xvid: framerate re-estimated: %.2f, %.3f%% correction\n",
               est_fps, (((est_fps - fps) / fps) * 100.0));
    } else {
        avctx->time_base.den = frate;
        avctx->time_base.num = fbase;
    }
}

/* Create temporary file using mkstemp(), tries /tmp first, if possible.
 * *prefix can be a character constant; *filename will be allocated internally.
 * Return file descriptor of opened file (or error code on error)
 * and opened file name in **filename. */
static int xvid_tempfile(AVCodecContext *avctx, const char *prefix,
                         char **filename)
{
    int fd = -1;
    size_t len = strlen(prefix) + 12; /* room for "/tmp/" and "XXXXXX\0" */
    *filename  = av_malloc(len);
    if (!(*filename)) {
        av_log(avctx, AV_LOG_ERROR, "xvid_tempfile: Cannot allocate file name\n");
        return AVERROR(ENOMEM);
    }
    snprintf(*filename, len, "/tmp/%sXXXXXX", prefix);
    fd = mkstemp(*filename);
    if (fd < 0) {
        snprintf(*filename, len, "./%sXXXXXX", prefix);
        fd = mkstemp(*filename);
    }
    if (fd < 0) {
        av_log(avctx, AV_LOG_ERROR, "xvid_tempfile: Cannot open temporary file %s\n", *filename);
        return AVERROR(EIO);
    }
    return fd; /* success */
}

static av_cold int xvid_encode_init(AVCodecContext *avctx)
{
    int xerr, i;
    int xvid_flags = avctx->flags;
    struct xvid_context *x = avctx->priv_data;
    uint16_t *intra, *inter;
    int fd;

    xvid_plugin_single_t single         = { 0 };
    struct xvid_ff_pass1 rc2pass1       = { 0 };
    xvid_plugin_2pass2_t rc2pass2       = { 0 };
    xvid_plugin_lumimasking_t masking_l = { 0 }; /* For lumi masking */
    xvid_plugin_lumimasking_t masking_v = { 0 }; /* For variance AQ */
    xvid_plugin_ssim_t ssim             = { 0 };
    xvid_gbl_init_t xvid_gbl_init       = { 0 };
    xvid_enc_create_t xvid_enc_create   = { 0 };
    xvid_enc_plugin_t plugins[7];

    /* Bring in VOP flags from avconv command-line */
    x->vop_flags = XVID_VOP_HALFPEL; /* Bare minimum quality */
    if (xvid_flags & AV_CODEC_FLAG_4MV)
        x->vop_flags |= XVID_VOP_INTER4V; /* Level 3 */
    if (avctx->trellis)
        x->vop_flags |= XVID_VOP_TRELLISQUANT; /* Level 5 */
    if (xvid_flags & AV_CODEC_FLAG_AC_PRED)
        x->vop_flags |= XVID_VOP_HQACPRED; /* Level 6 */
    if (xvid_flags & AV_CODEC_FLAG_GRAY)
        x->vop_flags |= XVID_VOP_GREYSCALE;

    /* Decide which ME quality setting to use */
    x->me_flags = 0;
    switch (x->me_quality) {
    case 6:
    case 5:
        x->me_flags |= XVID_ME_EXTSEARCH16 |
                       XVID_ME_EXTSEARCH8;
    case 4:
    case 3:
        x->me_flags |= XVID_ME_ADVANCEDDIAMOND8 |
                       XVID_ME_HALFPELREFINE8   |
                       XVID_ME_CHROMA_PVOP      |
                       XVID_ME_CHROMA_BVOP;
    case 2:
    case 1:
        x->me_flags |= XVID_ME_ADVANCEDDIAMOND16 |
                       XVID_ME_HALFPELREFINE16;
    }

    /* Decide how we should decide blocks */
    switch (avctx->mb_decision) {
    case 2:
        x->vop_flags |=  XVID_VOP_MODEDECISION_RD;
        x->me_flags  |=  XVID_ME_HALFPELREFINE8_RD    |
                         XVID_ME_QUARTERPELREFINE8_RD |
                         XVID_ME_EXTSEARCH_RD         |
                         XVID_ME_CHECKPREDICTION_RD;
    case 1:
        if (!(x->vop_flags & XVID_VOP_MODEDECISION_RD))
            x->vop_flags |= XVID_VOP_FAST_MODEDECISION_RD;
        x->me_flags |= XVID_ME_HALFPELREFINE16_RD |
                       XVID_ME_QUARTERPELREFINE16_RD;
    default:
        break;
    }

    x->vol_flags = 0;
    if (x->gmc) {
        x->vol_flags |= XVID_VOL_GMC;
        x->me_flags  |= XVID_ME_GME_REFINE;
    }
    if (xvid_flags & AV_CODEC_FLAG_QPEL) {
        x->vol_flags |= XVID_VOL_QUARTERPEL;
        x->me_flags  |= XVID_ME_QUARTERPELREFINE16;
        if (x->vop_flags & XVID_VOP_INTER4V)
            x->me_flags |= XVID_ME_QUARTERPELREFINE8;
    }

    xvid_gbl_init.version   = XVID_VERSION;
    xvid_gbl_init.debug     = 0;
    xvid_gbl_init.cpu_flags = 0;

    /* Initialize */
    xvid_global(NULL, XVID_GBL_INIT, &xvid_gbl_init, NULL);

    /* Create the encoder reference */
    xvid_enc_create.version = XVID_VERSION;

    /* Store the desired frame size */
    xvid_enc_create.width  =
    x->xsize               = avctx->width;
    xvid_enc_create.height =
    x->ysize               = avctx->height;

    /* Xvid can determine the proper profile to use */
    /* xvid_enc_create.profile = XVID_PROFILE_S_L3; */

    /* We don't use zones */
    xvid_enc_create.zones     = NULL;
    xvid_enc_create.num_zones = 0;

    xvid_enc_create.num_threads = avctx->thread_count;

    xvid_enc_create.plugins     = plugins;
    xvid_enc_create.num_plugins = 0;

    /* Initialize Buffers */
    x->twopassbuffer     = NULL;
    x->old_twopassbuffer = NULL;
    x->twopassfile       = NULL;

    if (xvid_flags & AV_CODEC_FLAG_PASS1) {
        rc2pass1.version     = XVID_VERSION;
        rc2pass1.context     = x;
        x->twopassbuffer     = av_malloc(BUFFER_SIZE);
        x->old_twopassbuffer = av_malloc(BUFFER_SIZE);
        if (!x->twopassbuffer || !x->old_twopassbuffer) {
            av_log(avctx, AV_LOG_ERROR,
                   "Xvid: Cannot allocate 2-pass log buffers\n");
            return AVERROR(ENOMEM);
        }
        x->twopassbuffer[0]     =
        x->old_twopassbuffer[0] = 0;

        plugins[xvid_enc_create.num_plugins].func  = xvid_ff_2pass;
        plugins[xvid_enc_create.num_plugins].param = &rc2pass1;
        xvid_enc_create.num_plugins++;
    } else if (xvid_flags & AV_CODEC_FLAG_PASS2) {
        rc2pass2.version = XVID_VERSION;
        rc2pass2.bitrate = avctx->bit_rate;

        fd = xvid_tempfile(avctx, "xvidff.", &x->twopassfile);
        if (fd < 0) {
            av_log(avctx, AV_LOG_ERROR, "Xvid: Cannot write 2-pass pipe\n");
            return fd;
        }

        if (!avctx->stats_in) {
            av_log(avctx, AV_LOG_ERROR,
                   "Xvid: No 2-pass information loaded for second pass\n");
            return AVERROR_INVALIDDATA;
        }

        if (strlen(avctx->stats_in) >
            write(fd, avctx->stats_in, strlen(avctx->stats_in))) {
            close(fd);
            av_log(avctx, AV_LOG_ERROR, "Xvid: Cannot write to 2-pass pipe\n");
            return AVERROR(EIO);
        }

        close(fd);
        rc2pass2.filename                          = x->twopassfile;
        plugins[xvid_enc_create.num_plugins].func  = xvid_plugin_2pass2;
        plugins[xvid_enc_create.num_plugins].param = &rc2pass2;
        xvid_enc_create.num_plugins++;
    } else if (!(xvid_flags & AV_CODEC_FLAG_QSCALE)) {
        /* Single Pass Bitrate Control! */
        single.version = XVID_VERSION;
        single.bitrate = avctx->bit_rate;

        plugins[xvid_enc_create.num_plugins].func  = xvid_plugin_single;
        plugins[xvid_enc_create.num_plugins].param = &single;
        xvid_enc_create.num_plugins++;
    }

    if (avctx->lumi_masking != 0.0)
        x->lumi_aq = 1;

    if (x->lumi_aq && x->variance_aq) {
        x->variance_aq = 0;
        av_log(avctx, AV_LOG_WARNING,
               "variance_aq is ignored when lumi_aq is set.\n");
    }

    /* Luminance Masking */
    if (x->lumi_aq) {
        masking_l.method                          = 0;
        plugins[xvid_enc_create.num_plugins].func = xvid_plugin_lumimasking;

        /* The old behavior is that when avctx->lumi_masking is specified,
         * plugins[...].param = NULL. Trying to keep the old behavior here. */
        plugins[xvid_enc_create.num_plugins].param =
            avctx->lumi_masking ? NULL : &masking_l;
        xvid_enc_create.num_plugins++;
    }

    /* Variance AQ */
    if (x->variance_aq) {
        masking_v.method                           = 1;
        plugins[xvid_enc_create.num_plugins].func  = xvid_plugin_lumimasking;
        plugins[xvid_enc_create.num_plugins].param = &masking_v;
        xvid_enc_create.num_plugins++;
    }

    /* SSIM */
    if (x->ssim) {
        plugins[xvid_enc_create.num_plugins].func  = xvid_plugin_ssim;
        ssim.b_printstat                           = x->ssim == 2;
        ssim.acc                                   = x->ssim_acc;
        ssim.cpu_flags                             = xvid_gbl_init.cpu_flags;
        ssim.b_visualize                           = 0;
        plugins[xvid_enc_create.num_plugins].param = &ssim;
        xvid_enc_create.num_plugins++;
    }

    /* Frame Rate and Key Frames */
    xvid_correct_framerate(avctx);
    xvid_enc_create.fincr = avctx->time_base.num;
    xvid_enc_create.fbase = avctx->time_base.den;
    if (avctx->gop_size > 0)
        xvid_enc_create.max_key_interval = avctx->gop_size;
    else
        xvid_enc_create.max_key_interval = 240; /* Xvid's best default */

    /* Quants */
    if (xvid_flags & AV_CODEC_FLAG_QSCALE)
        x->qscale = 1;
    else
        x->qscale = 0;

    xvid_enc_create.min_quant[0] = avctx->qmin;
    xvid_enc_create.min_quant[1] = avctx->qmin;
    xvid_enc_create.min_quant[2] = avctx->qmin;
    xvid_enc_create.max_quant[0] = avctx->qmax;
    xvid_enc_create.max_quant[1] = avctx->qmax;
    xvid_enc_create.max_quant[2] = avctx->qmax;

    /* Quant Matrices */
    x->intra_matrix =
    x->inter_matrix = NULL;

#if FF_API_PRIVATE_OPT
FF_DISABLE_DEPRECATION_WARNINGS
    if (avctx->mpeg_quant)
        x->mpeg_quant = avctx->mpeg_quant;
FF_ENABLE_DEPRECATION_WARNINGS
#endif

    if (x->mpeg_quant)
        x->vol_flags |= XVID_VOL_MPEGQUANT;
    if ((avctx->intra_matrix || avctx->inter_matrix)) {
        x->vol_flags |= XVID_VOL_MPEGQUANT;

        if (avctx->intra_matrix) {
            intra           = avctx->intra_matrix;
            x->intra_matrix = av_malloc(sizeof(unsigned char) * 64);
            if (!x->intra_matrix)
                return AVERROR(ENOMEM);
        } else
            intra = NULL;
        if (avctx->inter_matrix) {
            inter           = avctx->inter_matrix;
            x->inter_matrix = av_malloc(sizeof(unsigned char) * 64);
            if (!x->inter_matrix)
                return AVERROR(ENOMEM);
        } else
            inter = NULL;

        for (i = 0; i < 64; i++) {
            if (intra)
                x->intra_matrix[i] = (unsigned char) intra[i];
            if (inter)
                x->inter_matrix[i] = (unsigned char) inter[i];
        }
    }

    /* Misc Settings */
    xvid_enc_create.frame_drop_ratio = 0;
    xvid_enc_create.global           = 0;
    if (xvid_flags & AV_CODEC_FLAG_CLOSED_GOP)
        xvid_enc_create.global |= XVID_GLOBAL_CLOSED_GOP;

    /* Determines which codec mode we are operating in */
    avctx->extradata      = NULL;
    avctx->extradata_size = 0;
    if (xvid_flags & AV_CODEC_FLAG_GLOBAL_HEADER) {
        /* In this case, we are claiming to be MPEG-4 */
        x->quicktime_format = 1;
        avctx->codec_id     = AV_CODEC_ID_MPEG4;
    } else {
        /* We are claiming to be Xvid */
        x->quicktime_format = 0;
        if (!avctx->codec_tag)
            avctx->codec_tag = AV_RL32("xvid");
    }

    /* Bframes */
    xvid_enc_create.max_bframes   = avctx->max_b_frames;
    xvid_enc_create.bquant_offset = 100 * avctx->b_quant_offset;
    xvid_enc_create.bquant_ratio  = 100 * avctx->b_quant_factor;
    if (avctx->max_b_frames > 0 && !x->quicktime_format)
        xvid_enc_create.global |= XVID_GLOBAL_PACKED;

    /* Encode a dummy frame to get the extradata immediately */
    if (x->quicktime_format) {
        AVFrame *picture;
        AVPacket packet;
        int got_packet, ret;

        av_init_packet(&packet);

        picture = av_frame_alloc();
        if (!picture)
            return AVERROR(ENOMEM);

        xerr = xvid_encore(NULL, XVID_ENC_CREATE, &xvid_enc_create, NULL);
        if (xerr) {
            av_frame_free(&picture);
            av_log(avctx, AV_LOG_ERROR, "Xvid: Could not create encoder reference\n");
            return AVERROR_UNKNOWN;
        }
        x->encoder_handle = xvid_enc_create.handle;

        picture->width  = avctx->width;
        picture->height = avctx->height;
        picture->format = avctx->pix_fmt;

        if ((ret = av_frame_get_buffer(picture, 32)) < 0) {
            xvid_encore(x->encoder_handle, XVID_ENC_DESTROY, NULL, NULL);
            av_frame_free(&picture);
            return ret;
        }

        ret = xvid_encode_frame(avctx, &packet, picture, &got_packet);
        if (!ret && got_packet)
            av_packet_unref(&packet);
        av_frame_free(&picture);
        xvid_encore(x->encoder_handle, XVID_ENC_DESTROY, NULL, NULL);
    }

    /* Create encoder context */
    xerr = xvid_encore(NULL, XVID_ENC_CREATE, &xvid_enc_create, NULL);
    if (xerr) {
        av_log(avctx, AV_LOG_ERROR, "Xvid: Could not create encoder reference\n");
        return -1;
    }

    x->encoder_handle  = xvid_enc_create.handle;

    return 0;
}

static int xvid_encode_frame(AVCodecContext *avctx, AVPacket *pkt,
                             const AVFrame *picture, int *got_packet)
{
    int xerr, i, ret, user_packet = !!pkt->data;
    struct xvid_context *x = avctx->priv_data;
    int mb_width  = (avctx->width  + 15) / 16;
    int mb_height = (avctx->height + 15) / 16;
    char *tmp;

    xvid_enc_frame_t xvid_enc_frame = { 0 };
    xvid_enc_stats_t xvid_enc_stats = { 0 };

    if (!user_packet &&
        (ret = av_new_packet(pkt, mb_width * mb_height * MAX_MB_BYTES + AV_INPUT_BUFFER_MIN_SIZE)) < 0) {
        av_log(avctx, AV_LOG_ERROR, "Error getting output packet.\n");
        return ret;
    }

    /* Start setting up the frame */
    xvid_enc_frame.version = XVID_VERSION;
    xvid_enc_stats.version = XVID_VERSION;

    /* Let Xvid know where to put the frame. */
    xvid_enc_frame.bitstream = pkt->data;
    xvid_enc_frame.length    = pkt->size;

    /* Initialize input image fields */
    if (avctx->pix_fmt != AV_PIX_FMT_YUV420P) {
        av_log(avctx, AV_LOG_ERROR,
               "Xvid: Color spaces other than 420P not supported\n");
        return -1;
    }

    xvid_enc_frame.input.csp = XVID_CSP_PLANAR; /* YUV420P */

    for (i = 0; i < 4; i++) {
        xvid_enc_frame.input.plane[i]  = picture->data[i];
        xvid_enc_frame.input.stride[i] = picture->linesize[i];
    }

    /* Encoder Flags */
    xvid_enc_frame.vop_flags = x->vop_flags;
    xvid_enc_frame.vol_flags = x->vol_flags;
    xvid_enc_frame.motion    = x->me_flags;
    xvid_enc_frame.type      =
        picture->pict_type == AV_PICTURE_TYPE_I ? XVID_TYPE_IVOP :
        picture->pict_type == AV_PICTURE_TYPE_P ? XVID_TYPE_PVOP :
        picture->pict_type == AV_PICTURE_TYPE_B ? XVID_TYPE_BVOP :
                                                  XVID_TYPE_AUTO;

    /* Pixel aspect ratio setting */
    if (avctx->sample_aspect_ratio.num < 1 || avctx->sample_aspect_ratio.num > 255 ||
        avctx->sample_aspect_ratio.den < 1 || avctx->sample_aspect_ratio.den > 255) {
        av_log(avctx, AV_LOG_ERROR, "Invalid pixel aspect ratio %i/%i\n",
               avctx->sample_aspect_ratio.num, avctx->sample_aspect_ratio.den);
        return -1;
    }
    xvid_enc_frame.par        = XVID_PAR_EXT;
    xvid_enc_frame.par_width  = avctx->sample_aspect_ratio.num;
    xvid_enc_frame.par_height = avctx->sample_aspect_ratio.den;

    /* Quant Setting */
    if (x->qscale)
        xvid_enc_frame.quant = picture->quality / FF_QP2LAMBDA;
    else
        xvid_enc_frame.quant = 0;

    /* Matrices */
    xvid_enc_frame.quant_intra_matrix = x->intra_matrix;
    xvid_enc_frame.quant_inter_matrix = x->inter_matrix;

    /* Encode */
    xerr = xvid_encore(x->encoder_handle, XVID_ENC_ENCODE,
                       &xvid_enc_frame, &xvid_enc_stats);

    /* Two-pass log buffer swapping */
    avctx->stats_out = NULL;
    if (x->twopassbuffer) {
        tmp                  = x->old_twopassbuffer;
        x->old_twopassbuffer = x->twopassbuffer;
        x->twopassbuffer     = tmp;
        x->twopassbuffer[0]  = 0;
        if (x->old_twopassbuffer[0] != 0) {
            avctx->stats_out = x->old_twopassbuffer;
        }
    }

    if (xerr > 0) {
        uint8_t *sd = av_packet_new_side_data(pkt, AV_PKT_DATA_QUALITY_FACTOR,
                                              sizeof(int));
        if (!sd)
            return AVERROR(ENOMEM);
        *(int *)sd = xvid_enc_stats.quant * FF_QP2LAMBDA;

        *got_packet = 1;

#if FF_API_CODED_FRAME
FF_DISABLE_DEPRECATION_WARNINGS
        avctx->coded_frame->quality = xvid_enc_stats.quant * FF_QP2LAMBDA;
        if (xvid_enc_stats.type == XVID_TYPE_PVOP)
            avctx->coded_frame->pict_type = AV_PICTURE_TYPE_P;
        else if (xvid_enc_stats.type == XVID_TYPE_BVOP)
            avctx->coded_frame->pict_type = AV_PICTURE_TYPE_B;
        else if (xvid_enc_stats.type == XVID_TYPE_SVOP)
            avctx->coded_frame->pict_type = AV_PICTURE_TYPE_S;
        else
            avctx->coded_frame->pict_type = AV_PICTURE_TYPE_I;
FF_ENABLE_DEPRECATION_WARNINGS
#endif
        if (xvid_enc_frame.out_flags & XVID_KEYFRAME) {
#if FF_API_CODED_FRAME
FF_DISABLE_DEPRECATION_WARNINGS
            avctx->coded_frame->key_frame = 1;
FF_ENABLE_DEPRECATION_WARNINGS
#endif
            pkt->flags  |= AV_PKT_FLAG_KEY;
            if (x->quicktime_format)
                return xvid_strip_vol_header(avctx, pkt,
                                             xvid_enc_stats.hlength, xerr);
        } else {
#if FF_API_CODED_FRAME
FF_DISABLE_DEPRECATION_WARNINGS
            avctx->coded_frame->key_frame = 0;
FF_ENABLE_DEPRECATION_WARNINGS
#endif
        }

        pkt->size = xerr;

        return 0;
    } else {
        if (!user_packet)
            av_packet_unref(pkt);
        if (!xerr)
            return 0;
        av_log(avctx, AV_LOG_ERROR,
               "Xvid: Encoding Error Occurred: %i\n", xerr);
        return xerr;
    }
}

static av_cold int xvid_encode_close(AVCodecContext *avctx)
{
    struct xvid_context *x = avctx->priv_data;

    if (x->encoder_handle) {
        xvid_encore(x->encoder_handle, XVID_ENC_DESTROY, NULL, NULL);
        x->encoder_handle = NULL;
    }

    av_freep(&avctx->extradata);
    if (x->twopassbuffer) {
        av_free(x->twopassbuffer);
        av_free(x->old_twopassbuffer);
    }
    av_free(x->twopassfile);
    av_free(x->intra_matrix);
    av_free(x->inter_matrix);

    return 0;
}

#define OFFSET(x) offsetof(struct xvid_context, x)
#define VE AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_ENCODING_PARAM
static const AVOption options[] = {
    { "lumi_aq",     "Luminance masking AQ",            OFFSET(lumi_aq),     AV_OPT_TYPE_INT,   { .i64 = 0 },       0,       1, VE         },
    { "variance_aq", "Variance AQ",                     OFFSET(variance_aq), AV_OPT_TYPE_INT,   { .i64 = 0 },       0,       1, VE         },
    { "ssim",        "Show SSIM information to stdout", OFFSET(ssim),        AV_OPT_TYPE_INT,   { .i64 = 0 },       0,       2, VE, "ssim" },
    { "off",         NULL,                                                0, AV_OPT_TYPE_CONST, { .i64 = 0 }, INT_MIN, INT_MAX, VE, "ssim" },
    { "avg",         NULL,                                                0, AV_OPT_TYPE_CONST, { .i64 = 1 }, INT_MIN, INT_MAX, VE, "ssim" },
    { "frame",       NULL,                                                0, AV_OPT_TYPE_CONST, { .i64 = 2 }, INT_MIN, INT_MAX, VE, "ssim" },
    { "ssim_acc",    "SSIM accuracy",                   OFFSET(ssim_acc),    AV_OPT_TYPE_INT,   { .i64 = 2 },       0,       4, VE         },
    { "gmc",         "use GMC",                         OFFSET(gmc),         AV_OPT_TYPE_INT,   { .i64 = 0 },       0,       1, VE         },
    { "me_quality",  "Motion estimation quality",       OFFSET(me_quality),  AV_OPT_TYPE_INT,   { .i64 = 0 },       0,       6, VE         },
    { "mpeg_quant",  "Use MPEG quantizers instead of H.263", OFFSET(mpeg_quant), AV_OPT_TYPE_INT, { .i64 = 0 },     0,       1, VE         },
    { NULL },
};

static const AVClass xvid_class = {
    .class_name = "libxvid",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVCodec ff_libxvid_encoder = {
    .name           = "libxvid",
    .long_name      = NULL_IF_CONFIG_SMALL("libxvidcore MPEG-4 part 2"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_MPEG4,
    .priv_data_size = sizeof(struct xvid_context),
    .init           = xvid_encode_init,
    .encode2        = xvid_encode_frame,
    .close          = xvid_encode_close,
    .pix_fmts       = (const enum AVPixelFormat[]) { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE },
    .priv_class     = &xvid_class,
    .caps_internal  = FF_CODEC_CAP_INIT_THREADSAFE |
                      FF_CODEC_CAP_INIT_CLEANUP,
    .wrapper_name   = "libxvid",
};
