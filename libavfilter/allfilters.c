/*
 * filter registration
 * Copyright (c) 2008 Vitor Sessak
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "avfilter.h"
#include "filters.h"

extern const FFFilter ff_af_aap;
extern const FFFilter ff_af_abench;
extern const FFFilter ff_af_acompressor;
extern const FFFilter ff_af_acontrast;
extern const FFFilter ff_af_acopy;
extern const FFFilter ff_af_acue;
extern const FFFilter ff_af_acrossfade;
extern const FFFilter ff_af_acrossover;
extern const FFFilter ff_af_acrusher;
extern const FFFilter ff_af_adeclick;
extern const FFFilter ff_af_adeclip;
extern const FFFilter ff_af_adecorrelate;
extern const FFFilter ff_af_adelay;
extern const FFFilter ff_af_adenorm;
extern const FFFilter ff_af_aderivative;
extern const FFFilter ff_af_adrc;
extern const FFFilter ff_af_adynamicequalizer;
extern const FFFilter ff_af_adynamicsmooth;
extern const FFFilter ff_af_aecho;
extern const FFFilter ff_af_aemphasis;
extern const FFFilter ff_af_aeval;
extern const FFFilter ff_af_aexciter;
extern const FFFilter ff_af_afade;
extern const FFFilter ff_af_afftdn;
extern const FFFilter ff_af_afftfilt;
extern const FFFilter ff_af_afir;
extern const FFFilter ff_af_aformat;
extern const FFFilter ff_af_afreqshift;
extern const FFFilter ff_af_afwtdn;
extern const FFFilter ff_af_agate;
extern const FFFilter ff_af_aiir;
extern const FFFilter ff_af_aintegral;
extern const FFFilter ff_af_ainterleave;
extern const FFFilter ff_af_alatency;
extern const FFFilter ff_af_alimiter;
extern const FFFilter ff_af_allpass;
extern const FFFilter ff_af_aloop;
extern const FFFilter ff_af_amerge;
extern const FFFilter ff_af_ametadata;
extern const FFFilter ff_af_amix;
extern const FFFilter ff_af_amultiply;
extern const FFFilter ff_af_anequalizer;
extern const FFFilter ff_af_anlmdn;
extern const FFFilter ff_af_anlmf;
extern const FFFilter ff_af_anlms;
extern const FFFilter ff_af_anull;
extern const FFFilter ff_af_apad;
extern const FFFilter ff_af_aperms;
extern const FFFilter ff_af_aphaser;
extern const FFFilter ff_af_aphaseshift;
extern const FFFilter ff_af_apsnr;
extern const FFFilter ff_af_apsyclip;
extern const FFFilter ff_af_apulsator;
extern const FFFilter ff_af_arealtime;
extern const FFFilter ff_af_aresample;
extern const FFFilter ff_af_areverse;
extern const FFFilter ff_af_arls;
extern const FFFilter ff_af_arnndn;
extern const FFFilter ff_af_asdr;
extern const FFFilter ff_af_asegment;
extern const FFFilter ff_af_aselect;
extern const FFFilter ff_af_asendcmd;
extern const FFFilter ff_af_asetnsamples;
extern const FFFilter ff_af_asetpts;
extern const FFFilter ff_af_asetrate;
extern const FFFilter ff_af_asettb;
extern const FFFilter ff_af_ashowinfo;
extern const FFFilter ff_af_asidedata;
extern const FFFilter ff_af_asisdr;
extern const FFFilter ff_af_asoftclip;
extern const FFFilter ff_af_aspectralstats;
extern const FFFilter ff_af_asplit;
extern const FFFilter ff_af_asr;
extern const FFFilter ff_af_astats;
extern const FFFilter ff_af_astreamselect;
extern const FFFilter ff_af_asubboost;
extern const FFFilter ff_af_asubcut;
extern const FFFilter ff_af_asupercut;
extern const FFFilter ff_af_asuperpass;
extern const FFFilter ff_af_asuperstop;
extern const FFFilter ff_af_atempo;
extern const FFFilter ff_af_atilt;
extern const FFFilter ff_af_atrim;
extern const FFFilter ff_af_axcorrelate;
extern const FFFilter ff_af_azmq;
extern const FFFilter ff_af_bandpass;
extern const FFFilter ff_af_bandreject;
extern const FFFilter ff_af_bass;
extern const FFFilter ff_af_biquad;
extern const FFFilter ff_af_bs2b;
extern const FFFilter ff_af_channelmap;
extern const FFFilter ff_af_channelsplit;
extern const FFFilter ff_af_chorus;
extern const FFFilter ff_af_compand;
extern const FFFilter ff_af_compensationdelay;
extern const FFFilter ff_af_crossfeed;
extern const FFFilter ff_af_crystalizer;
extern const FFFilter ff_af_dcshift;
extern const FFFilter ff_af_deesser;
extern const FFFilter ff_af_dialoguenhance;
extern const FFFilter ff_af_drmeter;
extern const FFFilter ff_af_dynaudnorm;
extern const FFFilter ff_af_earwax;
extern const FFFilter ff_af_ebur128;
extern const FFFilter ff_af_equalizer;
extern const FFFilter ff_af_extrastereo;
extern const FFFilter ff_af_firequalizer;
extern const FFFilter ff_af_flanger;
extern const FFFilter ff_af_haas;
extern const FFFilter ff_af_hdcd;
extern const FFFilter ff_af_headphone;
extern const FFFilter ff_af_highpass;
extern const FFFilter ff_af_highshelf;
extern const FFFilter ff_af_join;
extern const FFFilter ff_af_ladspa;
extern const FFFilter ff_af_loudnorm;
extern const FFFilter ff_af_lowpass;
extern const FFFilter ff_af_lowshelf;
extern const FFFilter ff_af_lv2;
extern const FFFilter ff_af_mcompand;
extern const FFFilter ff_af_pan;
extern const FFFilter ff_af_replaygain;
extern const FFFilter ff_af_rubberband;
extern const FFFilter ff_af_sidechaincompress;
extern const FFFilter ff_af_sidechaingate;
extern const FFFilter ff_af_silencedetect;
extern const FFFilter ff_af_silenceremove;
extern const FFFilter ff_af_sofalizer;
extern const FFFilter ff_af_speechnorm;
extern const FFFilter ff_af_stereotools;
extern const FFFilter ff_af_stereowiden;
extern const FFFilter ff_af_superequalizer;
extern const FFFilter ff_af_surround;
extern const FFFilter ff_af_tiltshelf;
extern const FFFilter ff_af_treble;
extern const FFFilter ff_af_tremolo;
extern const FFFilter ff_af_vibrato;
extern const FFFilter ff_af_virtualbass;
extern const FFFilter ff_af_volume;
extern const FFFilter ff_af_volumedetect;

extern const FFFilter ff_asrc_aevalsrc;
extern const FFFilter ff_asrc_afdelaysrc;
extern const FFFilter ff_asrc_afireqsrc;
extern const FFFilter ff_asrc_afirsrc;
extern const FFFilter ff_asrc_anoisesrc;
extern const FFFilter ff_asrc_anullsrc;
extern const FFFilter ff_asrc_flite;
extern const FFFilter ff_asrc_hilbert;
extern const FFFilter ff_asrc_sinc;
extern const FFFilter ff_asrc_sine;

extern const FFFilter ff_asink_anullsink;

extern const FFFilter ff_vf_addroi;
extern const FFFilter ff_vf_alphaextract;
extern const FFFilter ff_vf_alphamerge;
extern const FFFilter ff_vf_amplify;
extern const FFFilter ff_vf_ass;
extern const FFFilter ff_vf_atadenoise;
extern const FFFilter ff_vf_avgblur;
extern const FFFilter ff_vf_avgblur_opencl;
extern const FFFilter ff_vf_avgblur_vulkan;
extern const FFFilter ff_vf_backgroundkey;
extern const FFFilter ff_vf_bbox;
extern const FFFilter ff_vf_bench;
extern const FFFilter ff_vf_bilateral;
extern const FFFilter ff_vf_bilateral_cuda;
extern const FFFilter ff_vf_bitplanenoise;
extern const FFFilter ff_vf_blackdetect;
extern const FFFilter ff_vf_blackframe;
extern const FFFilter ff_vf_blend;
extern const FFFilter ff_vf_blend_vulkan;
extern const FFFilter ff_vf_blockdetect;
extern const FFFilter ff_vf_blurdetect;
extern const FFFilter ff_vf_bm3d;
extern const FFFilter ff_vf_boxblur;
extern const FFFilter ff_vf_boxblur_opencl;
extern const FFFilter ff_vf_bwdif;
extern const FFFilter ff_vf_bwdif_cuda;
extern const FFFilter ff_vf_bwdif_vulkan;
extern const FFFilter ff_vf_cas;
extern const FFFilter ff_vf_ccrepack;
extern const FFFilter ff_vf_chromaber_vulkan;
extern const FFFilter ff_vf_chromahold;
extern const FFFilter ff_vf_chromakey;
extern const FFFilter ff_vf_chromakey_cuda;
extern const FFFilter ff_vf_chromanr;
extern const FFFilter ff_vf_chromashift;
extern const FFFilter ff_vf_ciescope;
extern const FFFilter ff_vf_codecview;
extern const FFFilter ff_vf_colorbalance;
extern const FFFilter ff_vf_colorchannelmixer;
extern const FFFilter ff_vf_colorcontrast;
extern const FFFilter ff_vf_colorcorrect;
extern const FFFilter ff_vf_colorize;
extern const FFFilter ff_vf_colorkey;
extern const FFFilter ff_vf_colorkey_opencl;
extern const FFFilter ff_vf_colorhold;
extern const FFFilter ff_vf_colorlevels;
extern const FFFilter ff_vf_colormap;
extern const FFFilter ff_vf_colormatrix;
extern const FFFilter ff_vf_colorspace;
extern const FFFilter ff_vf_colorspace_cuda;
extern const FFFilter ff_vf_colortemperature;
extern const FFFilter ff_vf_convolution;
extern const FFFilter ff_vf_convolution_opencl;
extern const FFFilter ff_vf_convolve;
extern const FFFilter ff_vf_copy;
extern const FFFilter ff_vf_coreimage;
extern const FFFilter ff_vf_corr;
extern const FFFilter ff_vf_cover_rect;
extern const FFFilter ff_vf_crop;
extern const FFFilter ff_vf_cropdetect;
extern const FFFilter ff_vf_cue;
extern const FFFilter ff_vf_curves;
extern const FFFilter ff_vf_datascope;
extern const FFFilter ff_vf_dblur;
extern const FFFilter ff_vf_dctdnoiz;
extern const FFFilter ff_vf_deband;
extern const FFFilter ff_vf_deblock;
extern const FFFilter ff_vf_decimate;
extern const FFFilter ff_vf_deconvolve;
extern const FFFilter ff_vf_dedot;
extern const FFFilter ff_vf_deflate;
extern const FFFilter ff_vf_deflicker;
extern const FFFilter ff_vf_deinterlace_qsv;
extern const FFFilter ff_vf_deinterlace_vaapi;
extern const FFFilter ff_vf_dejudder;
extern const FFFilter ff_vf_delogo;
extern const FFFilter ff_vf_denoise_vaapi;
extern const FFFilter ff_vf_derain;
extern const FFFilter ff_vf_deshake;
extern const FFFilter ff_vf_deshake_opencl;
extern const FFFilter ff_vf_despill;
extern const FFFilter ff_vf_detelecine;
extern const FFFilter ff_vf_dilation;
extern const FFFilter ff_vf_dilation_opencl;
extern const FFFilter ff_vf_displace;
extern const FFFilter ff_vf_dnn_classify;
extern const FFFilter ff_vf_dnn_detect;
extern const FFFilter ff_vf_dnn_processing;
extern const FFFilter ff_vf_doubleweave;
extern const FFFilter ff_vf_drawbox;
extern const FFFilter ff_vf_drawgraph;
extern const FFFilter ff_vf_drawgrid;
extern const FFFilter ff_vf_drawtext;
extern const FFFilter ff_vf_edgedetect;
extern const FFFilter ff_vf_elbg;
extern const FFFilter ff_vf_entropy;
extern const FFFilter ff_vf_epx;
extern const FFFilter ff_vf_eq;
extern const FFFilter ff_vf_erosion;
extern const FFFilter ff_vf_erosion_opencl;
extern const FFFilter ff_vf_estdif;
extern const FFFilter ff_vf_exposure;
extern const FFFilter ff_vf_extractplanes;
extern const FFFilter ff_vf_fade;
extern const FFFilter ff_vf_feedback;
extern const FFFilter ff_vf_fftdnoiz;
extern const FFFilter ff_vf_fftfilt;
extern const FFFilter ff_vf_field;
extern const FFFilter ff_vf_fieldhint;
extern const FFFilter ff_vf_fieldmatch;
extern const FFFilter ff_vf_fieldorder;
extern const FFFilter ff_vf_fillborders;
extern const FFFilter ff_vf_find_rect;
extern const FFFilter ff_vf_flip_vulkan;
extern const FFFilter ff_vf_floodfill;
extern const FFFilter ff_vf_format;
extern const FFFilter ff_vf_fps;
extern const FFFilter ff_vf_framepack;
extern const FFFilter ff_vf_framerate;
extern const FFFilter ff_vf_framestep;
extern const FFFilter ff_vf_freezedetect;
extern const FFFilter ff_vf_freezeframes;
extern const FFFilter ff_vf_frei0r;
extern const FFFilter ff_vf_fspp;
extern const FFFilter ff_vf_fsync;
extern const FFFilter ff_vf_gblur;
extern const FFFilter ff_vf_gblur_vulkan;
extern const FFFilter ff_vf_geq;
extern const FFFilter ff_vf_gradfun;
extern const FFFilter ff_vf_graphmonitor;
extern const FFFilter ff_vf_grayworld;
extern const FFFilter ff_vf_greyedge;
extern const FFFilter ff_vf_guided;
extern const FFFilter ff_vf_haldclut;
extern const FFFilter ff_vf_hflip;
extern const FFFilter ff_vf_hflip_vulkan;
extern const FFFilter ff_vf_histeq;
extern const FFFilter ff_vf_histogram;
extern const FFFilter ff_vf_hqdn3d;
extern const FFFilter ff_vf_hqx;
extern const FFFilter ff_vf_hstack;
extern const FFFilter ff_vf_hsvhold;
extern const FFFilter ff_vf_hsvkey;
extern const FFFilter ff_vf_hue;
extern const FFFilter ff_vf_huesaturation;
extern const FFFilter ff_vf_hwdownload;
extern const FFFilter ff_vf_hwmap;
extern const FFFilter ff_vf_hwupload;
extern const FFFilter ff_vf_hwupload_cuda;
extern const FFFilter ff_vf_hysteresis;
extern const FFFilter ff_vf_iccdetect;
extern const FFFilter ff_vf_iccgen;
extern const FFFilter ff_vf_identity;
extern const FFFilter ff_vf_idet;
extern const FFFilter ff_vf_il;
extern const FFFilter ff_vf_inflate;
extern const FFFilter ff_vf_interlace;
extern const FFFilter ff_vf_interleave;
extern const FFFilter ff_vf_kerndeint;
extern const FFFilter ff_vf_kirsch;
extern const FFFilter ff_vf_lagfun;
extern const FFFilter ff_vf_latency;
extern const FFFilter ff_vf_lcevc;
extern const FFFilter ff_vf_lenscorrection;
extern const FFFilter ff_vf_lensfun;
extern const FFFilter ff_vf_libplacebo;
extern const FFFilter ff_vf_libvmaf;
extern const FFFilter ff_vf_libvmaf_cuda;
extern const FFFilter ff_vf_limitdiff;
extern const FFFilter ff_vf_limiter;
extern const FFFilter ff_vf_loop;
extern const FFFilter ff_vf_lumakey;
extern const FFFilter ff_vf_lut;
extern const FFFilter ff_vf_lut1d;
extern const FFFilter ff_vf_lut2;
extern const FFFilter ff_vf_lut3d;
extern const FFFilter ff_vf_lutrgb;
extern const FFFilter ff_vf_lutyuv;
extern const FFFilter ff_vf_maskedclamp;
extern const FFFilter ff_vf_maskedmax;
extern const FFFilter ff_vf_maskedmerge;
extern const FFFilter ff_vf_maskedmin;
extern const FFFilter ff_vf_maskedthreshold;
extern const FFFilter ff_vf_maskfun;
extern const FFFilter ff_vf_mcdeint;
extern const FFFilter ff_vf_median;
extern const FFFilter ff_vf_mergeplanes;
extern const FFFilter ff_vf_mestimate;
extern const FFFilter ff_vf_metadata;
extern const FFFilter ff_vf_midequalizer;
extern const FFFilter ff_vf_minterpolate;
extern const FFFilter ff_vf_mix;
extern const FFFilter ff_vf_monochrome;
extern const FFFilter ff_vf_morpho;
extern const FFFilter ff_vf_mpdecimate;
extern const FFFilter ff_vf_msad;
extern const FFFilter ff_vf_multiply;
extern const FFFilter ff_vf_negate;
extern const FFFilter ff_vf_nlmeans;
extern const FFFilter ff_vf_nlmeans_opencl;
extern const FFFilter ff_vf_nlmeans_vulkan;
extern const FFFilter ff_vf_nnedi;
extern const FFFilter ff_vf_noformat;
extern const FFFilter ff_vf_noise;
extern const FFFilter ff_vf_normalize;
extern const FFFilter ff_vf_null;
extern const FFFilter ff_vf_ocr;
extern const FFFilter ff_vf_ocv;
extern const FFFilter ff_vf_oscilloscope;
extern const FFFilter ff_vf_overlay;
extern const FFFilter ff_vf_overlay_opencl;
extern const FFFilter ff_vf_overlay_qsv;
extern const FFFilter ff_vf_overlay_vaapi;
extern const FFFilter ff_vf_overlay_vulkan;
extern const FFFilter ff_vf_overlay_cuda;
extern const FFFilter ff_vf_owdenoise;
extern const FFFilter ff_vf_pad;
extern const FFFilter ff_vf_pad_opencl;
extern const FFFilter ff_vf_palettegen;
extern const FFFilter ff_vf_paletteuse;
extern const FFFilter ff_vf_perms;
extern const FFFilter ff_vf_perspective;
extern const FFFilter ff_vf_phase;
extern const FFFilter ff_vf_photosensitivity;
extern const FFFilter ff_vf_pixdesctest;
extern const FFFilter ff_vf_pixelize;
extern const FFFilter ff_vf_pixscope;
extern const FFFilter ff_vf_pp;
extern const FFFilter ff_vf_pp7;
extern const FFFilter ff_vf_premultiply;
extern const FFFilter ff_vf_prewitt;
extern const FFFilter ff_vf_prewitt_opencl;
extern const FFFilter ff_vf_procamp_vaapi;
extern const FFFilter ff_vf_program_opencl;
extern const FFFilter ff_vf_pseudocolor;
extern const FFFilter ff_vf_psnr;
extern const FFFilter ff_vf_pullup;
extern const FFFilter ff_vf_qp;
extern const FFFilter ff_vf_qrencode;
extern const FFFilter ff_vf_quirc;
extern const FFFilter ff_vf_random;
extern const FFFilter ff_vf_readeia608;
extern const FFFilter ff_vf_readvitc;
extern const FFFilter ff_vf_realtime;
extern const FFFilter ff_vf_remap;
extern const FFFilter ff_vf_remap_opencl;
extern const FFFilter ff_vf_removegrain;
extern const FFFilter ff_vf_removelogo;
extern const FFFilter ff_vf_repeatfields;
extern const FFFilter ff_vf_reverse;
extern const FFFilter ff_vf_rgbashift;
extern const FFFilter ff_vf_roberts;
extern const FFFilter ff_vf_roberts_opencl;
extern const FFFilter ff_vf_rotate;
extern const FFFilter ff_vf_sab;
extern const FFFilter ff_vf_scale;
extern const FFFilter ff_vf_vpp_amf;
extern const FFFilter ff_vf_sr_amf;
extern const FFFilter ff_vf_scale_cuda;
extern const FFFilter ff_vf_scale_npp;
extern const FFFilter ff_vf_scale_qsv;
extern const FFFilter ff_vf_scale_vaapi;
extern const FFFilter ff_vf_scale_vt;
extern const FFFilter ff_vf_scale_vulkan;
extern const FFFilter ff_vf_scale2ref;
extern const FFFilter ff_vf_scale2ref_npp;
extern const FFFilter ff_vf_scdet;
extern const FFFilter ff_vf_scharr;
extern const FFFilter ff_vf_scroll;
extern const FFFilter ff_vf_segment;
extern const FFFilter ff_vf_select;
extern const FFFilter ff_vf_selectivecolor;
extern const FFFilter ff_vf_sendcmd;
extern const FFFilter ff_vf_separatefields;
extern const FFFilter ff_vf_setdar;
extern const FFFilter ff_vf_setfield;
extern const FFFilter ff_vf_setparams;
extern const FFFilter ff_vf_setpts;
extern const FFFilter ff_vf_setrange;
extern const FFFilter ff_vf_setsar;
extern const FFFilter ff_vf_settb;
extern const FFFilter ff_vf_sharpen_npp;
extern const FFFilter ff_vf_sharpness_vaapi;
extern const FFFilter ff_vf_shear;
extern const FFFilter ff_vf_showinfo;
extern const FFFilter ff_vf_showpalette;
extern const FFFilter ff_vf_shuffleframes;
extern const FFFilter ff_vf_shufflepixels;
extern const FFFilter ff_vf_shuffleplanes;
extern const FFFilter ff_vf_sidedata;
extern const FFFilter ff_vf_signalstats;
extern const FFFilter ff_vf_signature;
extern const FFFilter ff_vf_siti;
extern const FFFilter ff_vf_smartblur;
extern const FFFilter ff_vf_sobel;
extern const FFFilter ff_vf_sobel_opencl;
extern const FFFilter ff_vf_split;
extern const FFFilter ff_vf_spp;
extern const FFFilter ff_vf_sr;
extern const FFFilter ff_vf_ssim;
extern const FFFilter ff_vf_ssim360;
extern const FFFilter ff_vf_stereo3d;
extern const FFFilter ff_vf_streamselect;
extern const FFFilter ff_vf_subtitles;
extern const FFFilter ff_vf_super2xsai;
extern const FFFilter ff_vf_swaprect;
extern const FFFilter ff_vf_swapuv;
extern const FFFilter ff_vf_tblend;
extern const FFFilter ff_vf_telecine;
extern const FFFilter ff_vf_thistogram;
extern const FFFilter ff_vf_threshold;
extern const FFFilter ff_vf_thumbnail;
extern const FFFilter ff_vf_thumbnail_cuda;
extern const FFFilter ff_vf_tile;
extern const FFFilter ff_vf_tiltandshift;
extern const FFFilter ff_vf_tinterlace;
extern const FFFilter ff_vf_tlut2;
extern const FFFilter ff_vf_tmedian;
extern const FFFilter ff_vf_tmidequalizer;
extern const FFFilter ff_vf_tmix;
extern const FFFilter ff_vf_tonemap;
extern const FFFilter ff_vf_tonemap_opencl;
extern const FFFilter ff_vf_tonemap_vaapi;
extern const FFFilter ff_vf_tpad;
extern const FFFilter ff_vf_transpose;
extern const FFFilter ff_vf_transpose_npp;
extern const FFFilter ff_vf_transpose_opencl;
extern const FFFilter ff_vf_transpose_vaapi;
extern const FFFilter ff_vf_transpose_vt;
extern const FFFilter ff_vf_transpose_vulkan;
extern const FFFilter ff_vf_trim;
extern const FFFilter ff_vf_unpremultiply;
extern const FFFilter ff_vf_unsharp;
extern const FFFilter ff_vf_unsharp_opencl;
extern const FFFilter ff_vf_untile;
extern const FFFilter ff_vf_uspp;
extern const FFFilter ff_vf_v360;
extern const FFFilter ff_vf_vaguedenoiser;
extern const FFFilter ff_vf_varblur;
extern const FFFilter ff_vf_vectorscope;
extern const FFFilter ff_vf_vflip;
extern const FFFilter ff_vf_vflip_vulkan;
extern const FFFilter ff_vf_vfrdet;
extern const FFFilter ff_vf_vibrance;
extern const FFFilter ff_vf_vidstabdetect;
extern const FFFilter ff_vf_vidstabtransform;
extern const FFFilter ff_vf_vif;
extern const FFFilter ff_vf_vignette;
extern const FFFilter ff_vf_vmafmotion;
extern const FFFilter ff_vf_vpp_qsv;
extern const FFFilter ff_vf_vstack;
extern const FFFilter ff_vf_w3fdif;
extern const FFFilter ff_vf_waveform;
extern const FFFilter ff_vf_weave;
extern const FFFilter ff_vf_xbr;
extern const FFFilter ff_vf_xcorrelate;
extern const FFFilter ff_vf_xfade;
extern const FFFilter ff_vf_xfade_opencl;
extern const FFFilter ff_vf_xfade_vulkan;
extern const FFFilter ff_vf_xmedian;
extern const FFFilter ff_vf_xpsnr;
extern const FFFilter ff_vf_xstack;
extern const FFFilter ff_vf_yadif;
extern const FFFilter ff_vf_yadif_cuda;
extern const FFFilter ff_vf_yadif_videotoolbox;
extern const FFFilter ff_vf_yaepblur;
extern const FFFilter ff_vf_zmq;
extern const FFFilter ff_vf_zoompan;
extern const FFFilter ff_vf_zscale;
extern const FFFilter ff_vf_hstack_vaapi;
extern const FFFilter ff_vf_vstack_vaapi;
extern const FFFilter ff_vf_xstack_vaapi;
extern const FFFilter ff_vf_hstack_qsv;
extern const FFFilter ff_vf_vstack_qsv;
extern const FFFilter ff_vf_xstack_qsv;
extern const FFFilter ff_vf_pad_vaapi;
extern const FFFilter ff_vf_drawbox_vaapi;

extern const FFFilter ff_vsrc_allrgb;
extern const FFFilter ff_vsrc_allyuv;
extern const FFFilter ff_vsrc_cellauto;
extern const FFFilter ff_vsrc_color;
extern const FFFilter ff_vsrc_color_vulkan;
extern const FFFilter ff_vsrc_colorchart;
extern const FFFilter ff_vsrc_colorspectrum;
extern const FFFilter ff_vsrc_coreimagesrc;
extern const FFFilter ff_vsrc_ddagrab;
extern const FFFilter ff_vsrc_frei0r_src;
extern const FFFilter ff_vsrc_gradients;
extern const FFFilter ff_vsrc_haldclutsrc;
extern const FFFilter ff_vsrc_life;
extern const FFFilter ff_vsrc_mandelbrot;
extern const FFFilter ff_vsrc_mptestsrc;
extern const FFFilter ff_vsrc_nullsrc;
extern const FFFilter ff_vsrc_openclsrc;
extern const FFFilter ff_vsrc_qrencodesrc;
extern const FFFilter ff_vsrc_pal75bars;
extern const FFFilter ff_vsrc_pal100bars;
extern const FFFilter ff_vsrc_perlin;
extern const FFFilter ff_vsrc_rgbtestsrc;
extern const FFFilter ff_vsrc_sierpinski;
extern const FFFilter ff_vsrc_smptebars;
extern const FFFilter ff_vsrc_smptehdbars;
extern const FFFilter ff_vsrc_testsrc;
extern const FFFilter ff_vsrc_testsrc2;
extern const FFFilter ff_vsrc_yuvtestsrc;
extern const FFFilter ff_vsrc_zoneplate;

extern const FFFilter ff_vsink_nullsink;

/* multimedia filters */
extern const FFFilter ff_avf_a3dscope;
extern const FFFilter ff_avf_abitscope;
extern const FFFilter ff_avf_adrawgraph;
extern const FFFilter ff_avf_agraphmonitor;
extern const FFFilter ff_avf_ahistogram;
extern const FFFilter ff_avf_aphasemeter;
extern const FFFilter ff_avf_avectorscope;
extern const FFFilter ff_avf_concat;
extern const FFFilter ff_avf_showcqt;
extern const FFFilter ff_avf_showcwt;
extern const FFFilter ff_avf_showfreqs;
extern const FFFilter ff_avf_showspatial;
extern const FFFilter ff_avf_showspectrum;
extern const FFFilter ff_avf_showspectrumpic;
extern const FFFilter ff_avf_showvolume;
extern const FFFilter ff_avf_showwaves;
extern const FFFilter ff_avf_showwavespic;
extern const FFFilter ff_vaf_spectrumsynth;

/* multimedia sources */
extern const FFFilter ff_avsrc_avsynctest;
extern const FFFilter ff_avsrc_amovie;
extern const FFFilter ff_avsrc_movie;

/* those filters are part of public or internal API,
 * they are formatted to not be found by the grep
 * as they are manually added again (due to their 'names'
 * being the same while having different 'types'). */
extern  const FFFilter ff_asrc_abuffer;
extern  const FFFilter ff_vsrc_buffer;
extern  const FFFilter ff_asink_abuffer;
extern  const FFFilter ff_vsink_buffer;

#include "libavfilter/filter_list.c"


const AVFilter *av_filter_iterate(void **opaque)
{
    uintptr_t i = (uintptr_t)*opaque;
    const FFFilter *f = filter_list[i];

    if (f) {
        *opaque = (void*)(i + 1);
        return &f->p;
    }

    return NULL;
}

const AVFilter *avfilter_get_by_name(const char *name)
{
    const AVFilter *f = NULL;
    void *opaque = 0;

    if (!name)
        return NULL;

    while ((f = av_filter_iterate(&opaque)))
        if (!strcmp(f->name, name))
            return f;

    return NULL;
}
