/*
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

#ifndef AVUTIL_IMGUTILS_H
#define AVUTIL_IMGUTILS_H

/**
 * @file
 * misc image utilities
 *
 * @addtogroup lavu_picture
 * @{
 */

#include "avutil.h"
#include "pixdesc.h"
#include "rational.h"

/**
 * Compute the max pixel step for each plane of an image with a
 * format described by pixdesc.
 *
 * The pixel step is the distance in bytes between the first byte of
 * the group of bytes which describe a pixel component and the first
 * byte of the successive group in the same plane for the same
 * component.
 *
 * @param max_pixsteps an array which is filled with the max pixel step
 * for each plane. Since a plane may contain different pixel
 * components, the computed max_pixsteps[plane] is relative to the
 * component in the plane with the max pixel step.
 * @param max_pixstep_comps an array which is filled with the component
 * for each plane which has the max pixel step. May be NULL.
 */
void av_image_fill_max_pixsteps(int max_pixsteps[4], int max_pixstep_comps[4],
                                const AVPixFmtDescriptor *pixdesc);

/**
 * Compute the size of an image line with format pix_fmt and width
 * width for the plane plane.
 *
 * @return the computed size in bytes
 */
int av_image_get_linesize(enum AVPixelFormat pix_fmt, int width, int plane);

/**
 * Fill plane linesizes for an image with pixel format pix_fmt and
 * width width.
 *
 * @param linesizes array to be filled with the linesize for each plane
 * @return >= 0 in case of success, a negative error code otherwise
 */
int av_image_fill_linesizes(int linesizes[4], enum AVPixelFormat pix_fmt, int width);

/**
 * Fill plane data pointers for an image with pixel format pix_fmt and
 * height height.
 *
 * @param data pointers array to be filled with the pointer for each image plane
 * @param ptr the pointer to a buffer which will contain the image
 * @param linesizes the array containing the linesize for each
 * plane, should be filled by av_image_fill_linesizes()
 * @return the size in bytes required for the image buffer, a negative
 * error code in case of failure
 */
int av_image_fill_pointers(uint8_t *data[4], enum AVPixelFormat pix_fmt, int height,
                           uint8_t *ptr, const int linesizes[4]);

/**
 * Allocate an image with size w and h and pixel format pix_fmt, and
 * fill pointers and linesizes accordingly.
 * The allocated image buffer has to be freed by using
 * av_freep(&pointers[0]).
 *
 * @param align the value to use for buffer size alignment
 * @return the size in bytes required for the image buffer, a negative
 * error code in case of failure
 */
int av_image_alloc(uint8_t *pointers[4], int linesizes[4],
                   int w, int h, enum AVPixelFormat pix_fmt, int align);

/**
 * Copy image plane from src to dst.
 * That is, copy "height" number of lines of "bytewidth" bytes each.
 * The first byte of each successive line is separated by *_linesize
 * bytes.
 *
 * @param dst_linesize linesize for the image plane in dst
 * @param src_linesize linesize for the image plane in src
 */
void av_image_copy_plane(uint8_t       *dst, int dst_linesize,
                         const uint8_t *src, int src_linesize,
                         int bytewidth, int height);

/**
 * Copy image in src_data to dst_data.
 *
 * @param dst_linesizes linesizes for the image in dst_data
 * @param src_linesizes linesizes for the image in src_data
 */
void av_image_copy(uint8_t *dst_data[4], int dst_linesizes[4],
                   const uint8_t *src_data[4], const int src_linesizes[4],
                   enum AVPixelFormat pix_fmt, int width, int height);

/**
 * Copy image data located in uncacheable (e.g. GPU mapped) memory. Where
 * available, this function will use special functionality for reading from such
 * memory, which may result in greatly improved performance compared to plain
 * av_image_copy().
 *
 * The data pointers and the linesizes must be aligned to the maximum required
 * by the CPU architecture.
 *
 * @note The linesize parameters have the type ptrdiff_t here, while they are
 *       int for av_image_copy().
 * @note On x86, the linesizes currently need to be aligned to the cacheline
 *       size (i.e. 64) to get improved performance.
 */
void av_image_copy_uc_from(uint8_t *dst_data[4],       const ptrdiff_t dst_linesizes[4],
                           const uint8_t *src_data[4], const ptrdiff_t src_linesizes[4],
                           enum AVPixelFormat pix_fmt, int width, int height);

/**
 * Setup the data pointers and linesizes based on the specified image
 * parameters and the provided array.
 *
 * The fields of the given image are filled in by using the src
 * address which points to the image data buffer. Depending on the
 * specified pixel format, one or multiple image data pointers and
 * line sizes will be set.  If a planar format is specified, several
 * pointers will be set pointing to the different picture planes and
 * the line sizes of the different planes will be stored in the
 * lines_sizes array. Call with src == NULL to get the required
 * size for the src buffer.
 *
 * To allocate the buffer and fill in the dst_data and dst_linesize in
 * one call, use av_image_alloc().
 *
 * @param dst_data      data pointers to be filled in
 * @param dst_linesizes linesizes for the image in dst_data to be filled in
 * @param src           buffer which will contain or contains the actual image data, can be NULL
 * @param pix_fmt       the pixel format of the image
 * @param width         the width of the image in pixels
 * @param height        the height of the image in pixels
 * @param align         the value used in src for linesize alignment
 * @return the size in bytes required for src, a negative error code
 * in case of failure
 */
int av_image_fill_arrays(uint8_t *dst_data[4], int dst_linesize[4],
                         const uint8_t *src,
                         enum AVPixelFormat pix_fmt, int width, int height, int align);

/**
 * Return the size in bytes of the amount of data required to store an
 * image with the given parameters.
 *
 * @param[in] align the assumed linesize alignment
 */
int av_image_get_buffer_size(enum AVPixelFormat pix_fmt, int width, int height, int align);

/**
 * Copy image data from an image into a buffer.
 *
 * av_image_get_buffer_size() can be used to compute the required size
 * for the buffer to fill.
 *
 * @param dst           a buffer into which picture data will be copied
 * @param dst_size      the size in bytes of dst
 * @param src_data      pointers containing the source image data
 * @param src_linesizes linesizes for the image in src_data
 * @param pix_fmt       the pixel format of the source image
 * @param width         the width of the source image in pixels
 * @param height        the height of the source image in pixels
 * @param align         the assumed linesize alignment for dst
 * @return the number of bytes written to dst, or a negative value
 * (error code) on error
 */
int av_image_copy_to_buffer(uint8_t *dst, int dst_size,
                            const uint8_t * const src_data[4], const int src_linesize[4],
                            enum AVPixelFormat pix_fmt, int width, int height, int align);

/**
 * Check if the given dimension of an image is valid, meaning that all
 * bytes of the image can be addressed with a signed int.
 *
 * @param w the width of the picture
 * @param h the height of the picture
 * @param log_offset the offset to sum to the log level for logging with log_ctx
 * @param log_ctx the parent logging context, it may be NULL
 * @return >= 0 if valid, a negative error code otherwise
 */
int av_image_check_size(unsigned int w, unsigned int h, int log_offset, void *log_ctx);

/**
 * Check if the given sample aspect ratio of an image is valid.
 *
 * It is considered invalid if the denominator is 0 or if applying the ratio
 * to the image size would make the smaller dimension less than 1. If the
 * sar numerator is 0, it is considered unknown and will return as valid.
 *
 * @param w width of the image
 * @param h height of the image
 * @param sar sample aspect ratio of the image
 * @return 0 if valid, a negative AVERROR code otherwise
 */
int av_image_check_sar(unsigned int w, unsigned int h, AVRational sar);

/**
 * @}
 */


#endif /* AVUTIL_IMGUTILS_H */
