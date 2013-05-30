/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */


#ifndef FF_YUV_RGB_H_

#define FF_YUV_RGB_H_


/* Planes must start on a 16-bytes boundary. Pitches must be multiples of 16
 * bytes even for subsampled components. */

/* Planar picture buffer.
 * Pitch corresponds to luminance component in bytes. Chrominance pitches are
 * inferred from the color subsampling ratio. */
struct yuv_planes
{
    void *y, *u, *v;
    size_t pitch;
};

/* Packed picture buffer. Pitch is in bytes (_not_ pixels). */
struct yuv_pack
{
    void *yuv;
    size_t pitch;
};


/* I420 to RGBA conversion. */
void i420_rgb_neon (struct yuv_pack *const out,
                    const struct yuv_planes *const in,
                    int width, int height) asm("i420_rgb_neon");

/* NV21 to RGBA conversion. */
void nv21_rgb_neon (struct yuv_pack *const out,
                    const struct yuv_planes *const in,
                    int width, int height) asm("nv21_rgb_neon");

/* NV12 to RGBA conversion. */
void nv12_rgb_neon (struct yuv_pack *const out,
                    const struct yuv_planes *const in,
                    int width, int height) asm("nv12_rgb_neon");


#endif

