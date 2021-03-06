/* bitmap_scale.c - Bitmap scaling. */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2006,2007,2008,2009  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/video.h>
#include <grub/bitmap.h>
#include <grub/bitmap_scale.h>
#include <grub/types.h>
#include <grub/dl.h>

GRUB_MOD_LICENSE ("GPLv3+");

/* Prototypes for module-local functions.  */
static grub_err_t scale_nn (struct grub_video_bitmap *dst,
                            struct grub_video_bitmap *src);
static grub_err_t scale_bilinear (struct grub_video_bitmap *dst,
                                  struct grub_video_bitmap *src);

static grub_err_t
verify_source_bitmap (struct grub_video_bitmap *src)
{
  /* Verify the simplifying assumptions. */
  if (src == 0)
    return grub_error (GRUB_ERR_BUG,
                       "null src bitmap in grub_video_bitmap_create_scaled");
  if (src->mode_info.red_field_pos % 8 != 0
      || src->mode_info.green_field_pos % 8 != 0
      || src->mode_info.blue_field_pos % 8 != 0
      || src->mode_info.reserved_field_pos % 8 != 0)
    return grub_error (GRUB_ERR_BUG,
                       "src format not supported for scale");
  if (src->mode_info.width == 0 || src->mode_info.height == 0)
    return grub_error (GRUB_ERR_BUG,
                       "source bitmap has a zero dimension");
  if (src->mode_info.bytes_per_pixel * 8 != src->mode_info.bpp)
    return grub_error (GRUB_ERR_BUG,
                       "bitmap to scale has inconsistent Bpp and bpp");
  return GRUB_ERR_NONE;
}

static grub_err_t
grub_video_bitmap_scale (struct grub_video_bitmap *dst,
                         struct grub_video_bitmap *src,
                         enum grub_video_bitmap_scale_method scale_method)
{
  switch (scale_method)
    {
    case GRUB_VIDEO_BITMAP_SCALE_METHOD_FASTEST:
    case GRUB_VIDEO_BITMAP_SCALE_METHOD_NEAREST:
      return scale_nn (dst, src);
    case GRUB_VIDEO_BITMAP_SCALE_METHOD_BEST:
    case GRUB_VIDEO_BITMAP_SCALE_METHOD_BILINEAR:
      return scale_bilinear (dst, src);
    default:
      return grub_error (GRUB_ERR_BUG, "Invalid scale_method value");
    }
}

/* This function creates a new scaled version of the bitmap SRC.  The new
   bitmap has dimensions DST_WIDTH by DST_HEIGHT.  The scaling algorithm
   is given by SCALE_METHOD.  If an error is encountered, the return code is
   not equal to GRUB_ERR_NONE, and the bitmap DST is either not created, or
   it is destroyed before this function returns.

   Supports only direct color modes which have components separated
   into bytes (e.g., RGBA 8:8:8:8 or BGR 8:8:8 true color).
   But because of this simplifying assumption, the implementation is
   greatly simplified.  */
grub_err_t
grub_video_bitmap_create_scaled (struct grub_video_bitmap **dst,
                                 int dst_width, int dst_height,
                                 struct grub_video_bitmap *src,
                                 enum grub_video_bitmap_scale_method
                                 scale_method)
{
  *dst = 0;

  grub_err_t err = verify_source_bitmap(src);
  if (err != GRUB_ERR_NONE)
    return err;
  if (dst_width <= 0 || dst_height <= 0)
    return grub_error (GRUB_ERR_BUG,
                       "requested to scale to a size w/ a zero dimension");

  /* Create the new bitmap. */
  grub_err_t ret;
  ret = grub_video_bitmap_create (dst, dst_width, dst_height,
                                  src->mode_info.blit_format);
  if (ret != GRUB_ERR_NONE)
    return ret;                 /* Error. */

  ret = grub_video_bitmap_scale (*dst, src, scale_method);

  if (ret == GRUB_ERR_NONE)
    {
      /* Success:  *dst is now a pointer to the scaled bitmap. */
      return GRUB_ERR_NONE;
    }
  else
    {
      /* Destroy the bitmap and return the error code. */
      grub_video_bitmap_destroy (*dst);
      *dst = 0;
      return ret;
    }
}

static grub_err_t
make_h_align (unsigned *x, unsigned *w, unsigned new_w,
              grub_video_bitmap_h_align_t h_align)
{
  grub_err_t ret = GRUB_ERR_NONE;
  if (new_w >= *w)
    {
      *x = 0;
      *w = new_w;
      return GRUB_ERR_NONE;
    }
  switch (h_align)
    {
    case GRUB_VIDEO_BITMAP_H_ALIGN_LEFT:
      *x = 0;
      break;
    case GRUB_VIDEO_BITMAP_H_ALIGN_CENTER:
      *x = (*w - new_w) / 2;
      break;
    case GRUB_VIDEO_BITMAP_H_ALIGN_RIGHT:
      *x = *w - new_w;
      break;
    default:
      ret = grub_error (GRUB_ERR_BUG, "Invalid h_align value");
      break;
    }
  *w = new_w;
  return ret;
}

static grub_err_t
make_v_align (unsigned *y, unsigned *h, unsigned new_h,
              grub_video_bitmap_v_align_t v_align)
{
  grub_err_t ret = GRUB_ERR_NONE;
  if (new_h >= *h)
    {
      *y = 0;
      *h = new_h;
      return GRUB_ERR_NONE;
    }
  switch (v_align)
    {
    case GRUB_VIDEO_BITMAP_V_ALIGN_TOP:
      *y = 0;
      break;
    case GRUB_VIDEO_BITMAP_V_ALIGN_CENTER:
      *y = (*h - new_h) / 2;
      break;
    case GRUB_VIDEO_BITMAP_V_ALIGN_BOTTOM:
      *y = *h - new_h;
      break;
    default:
      ret = grub_error (GRUB_ERR_BUG, "Invalid v_align value");
      break;
    }
  *h = new_h;
  return ret;
}

grub_err_t
grub_video_bitmap_scale_proportional (struct grub_video_bitmap **dst,
                                      int dst_width, int dst_height,
                                      struct grub_video_bitmap *src,
                                      enum grub_video_bitmap_scale_method
                                      scale_method,
                                      grub_video_bitmap_selection_method_t
                                      selection_method,
                                      grub_video_bitmap_v_align_t v_align,
                                      grub_video_bitmap_h_align_t h_align)
{
  *dst = 0;
  grub_err_t ret = verify_source_bitmap(src);
  if (ret != GRUB_ERR_NONE)
    return ret;
  if (dst_width <= 0 || dst_height <= 0)
    return grub_error (GRUB_ERR_BUG,
                       "requested to scale to a size w/ a zero dimension");

  ret = grub_video_bitmap_create (dst, dst_width, dst_height,
                                  src->mode_info.blit_format);
  if (ret != GRUB_ERR_NONE)
    return ret;                 /* Error. */

  unsigned dx0 = 0;
  unsigned dy0 = 0;
  unsigned dw = dst_width;
  unsigned dh = dst_height;
  unsigned sx0 = 0;
  unsigned sy0 = 0;
  unsigned sw = src->mode_info.width;
  unsigned sh = src->mode_info.height;

  switch (selection_method)
    {
    case GRUB_VIDEO_BITMAP_SELECTION_METHOD_CROP:
      /* Comparing sw/sh VS dw/dh. */
      if (sw * dh < dw * sh)
        ret = make_v_align (&sy0, &sh, sw * dh / dw, v_align);
      else
        ret = make_h_align (&sx0, &sw, sh * dw / dh, h_align);
      break;
    case GRUB_VIDEO_BITMAP_SELECTION_METHOD_PADDING:
      if (sw * dh < dw * sh)
        ret = make_h_align (&dx0, &dw, sw * dh / sh, h_align);
      else
        ret = make_v_align (&dy0, &dh, sh * dw / sw, v_align);
      break;
    case GRUB_VIDEO_BITMAP_SELECTION_METHOD_FITWIDTH:
      if (sw * dh < dw * sh)
        ret = make_v_align (&sy0, &sh, sw * dh / dw, v_align);
      else
        ret = make_v_align (&dy0, &dh, sh * dw / sw, v_align);
      break;
    case GRUB_VIDEO_BITMAP_SELECTION_METHOD_FITHEIGHT:
      if (sw * dh < dw * sh)
        ret = make_h_align (&dx0, &dw, sw * dh / sh, h_align);
      else
        ret = make_h_align (&sx0, &sw, sh * dw / dh, h_align);
      break;
    default:
      ret = grub_error (GRUB_ERR_BUG, "Invalid selection_method value");
      break;
    }

  if (ret == GRUB_ERR_NONE)
    {
      /* Backup original data. */
      int src_width_orig = src->mode_info.width;
      int src_height_orig = src->mode_info.height;
      grub_uint8_t *src_data_orig = src->data;
      int dst_width_orig = (*dst)->mode_info.width;
      int dst_height_orig = (*dst)->mode_info.height;
      grub_uint8_t *dst_data_orig = (*dst)->data;

      int dstride = (*dst)->mode_info.pitch;
      int sstride = src->mode_info.pitch;
      /* bytes_per_pixel is the same for both src and dst. */
      int bytes_per_pixel = src->mode_info.bytes_per_pixel;

      /* Crop src and dst. */
      src->mode_info.width = sw;
      src->mode_info.height = sh;
      src->data = (grub_uint8_t *) src->data + sx0 * bytes_per_pixel
                  + sy0 * sstride;
      (*dst)->mode_info.width = dw;
      (*dst)->mode_info.height = dh;
      (*dst)->data = (grub_uint8_t *) (*dst)->data + dx0 * bytes_per_pixel
                     + dy0 * dstride;

      /* Scale our image. */
      ret = grub_video_bitmap_scale (*dst, src, scale_method);

      /* Restore original data. */
      src->mode_info.width = src_width_orig;
      src->mode_info.height = src_height_orig;
      src->data = src_data_orig;
      (*dst)->mode_info.width = dst_width_orig;
      (*dst)->mode_info.height = dst_height_orig;
      (*dst)->data = dst_data_orig;
    }

  if (ret == GRUB_ERR_NONE)
    {
      /* Success:  *dst is now a pointer to the scaled bitmap. */
      return GRUB_ERR_NONE;
    }
  else
    {
      /* Destroy the bitmap and return the error code. */
      grub_video_bitmap_destroy (*dst);
      *dst = 0;
      return ret;
    }
}

static grub_err_t
verify_bitmaps (struct grub_video_bitmap *dst, struct grub_video_bitmap *src)
{
  /* Verify the simplifying assumptions. */
  if (dst == 0 || src == 0)
    return grub_error (GRUB_ERR_BUG, "null bitmap in scale function");
  if (dst->mode_info.red_field_pos % 8 != 0
      || dst->mode_info.green_field_pos % 8 != 0
      || dst->mode_info.blue_field_pos % 8 != 0
      || dst->mode_info.reserved_field_pos % 8 != 0)
    return grub_error (GRUB_ERR_BUG,
		       "dst format not supported");
  if (src->mode_info.red_field_pos % 8 != 0
      || src->mode_info.green_field_pos % 8 != 0
      || src->mode_info.blue_field_pos % 8 != 0
      || src->mode_info.reserved_field_pos % 8 != 0)
    return grub_error (GRUB_ERR_BUG,
		       "src format not supported");
  if (dst->mode_info.red_field_pos != src->mode_info.red_field_pos
      || dst->mode_info.red_mask_size != src->mode_info.red_mask_size
      || dst->mode_info.green_field_pos != src->mode_info.green_field_pos
      || dst->mode_info.green_mask_size != src->mode_info.green_mask_size
      || dst->mode_info.blue_field_pos != src->mode_info.blue_field_pos
      || dst->mode_info.blue_mask_size != src->mode_info.blue_mask_size
      || dst->mode_info.reserved_field_pos !=
      src->mode_info.reserved_field_pos
      || dst->mode_info.reserved_mask_size !=
      src->mode_info.reserved_mask_size)
    return grub_error (GRUB_ERR_BUG,
		       "dst and src not compatible");
  if (dst->mode_info.bytes_per_pixel != src->mode_info.bytes_per_pixel)
    return grub_error (GRUB_ERR_NOT_IMPLEMENTED_YET,
		       "dst and src not compatible");
  if (dst->mode_info.width == 0 || dst->mode_info.height == 0
      || src->mode_info.width == 0 || src->mode_info.height == 0)
    return grub_error (GRUB_ERR_BUG, "bitmap has a zero dimension");

  return GRUB_ERR_NONE;
}

/* Nearest neighbor bitmap scaling algorithm.

   Copy the bitmap SRC to the bitmap DST, scaling the bitmap to fit the
   dimensions of DST.  This function uses the nearest neighbor algorithm to
   interpolate the pixels.

   Supports only direct color modes which have components separated
   into bytes (e.g., RGBA 8:8:8:8 or BGR 8:8:8 true color).
   But because of this simplifying assumption, the implementation is
   greatly simplified.  */
static grub_err_t
scale_nn (struct grub_video_bitmap *dst, struct grub_video_bitmap *src)
{
  grub_err_t err = verify_bitmaps(dst, src);
  if (err != GRUB_ERR_NONE)
    return err;

  grub_uint8_t *ddata = dst->data;
  grub_uint8_t *sdata = src->data;
  unsigned dw = dst->mode_info.width;
  unsigned dh = dst->mode_info.height;
  unsigned sw = src->mode_info.width;
  unsigned sh = src->mode_info.height;
  unsigned dstride = dst->mode_info.pitch;
  unsigned sstride = src->mode_info.pitch;
  /* bytes_per_pixel is the same for both src and dst. */
  unsigned bytes_per_pixel = dst->mode_info.bytes_per_pixel;

  unsigned dy;
  for (dy = 0; dy < dh; dy++)
    {
      unsigned dx;
      for (dx = 0; dx < dw; dx++)
        {
          grub_uint8_t *dptr;
          grub_uint8_t *sptr;
          unsigned sx;
          unsigned sy;
          unsigned comp;

          /* Compute the source coordinate that the destination coordinate
             maps to.  Note: sx/sw = dx/dw  =>  sx = sw*dx/dw. */
          sx = sw * dx / dw;
          sy = sh * dy / dh;

          /* Get the address of the pixels in src and dst. */
          dptr = ddata + dy * dstride + dx * bytes_per_pixel;
          sptr = sdata + sy * sstride + sx * bytes_per_pixel;

          /* Copy the pixel color value. */
          for (comp = 0; comp < bytes_per_pixel; comp++)
            dptr[comp] = sptr[comp];
        }
    }
  return GRUB_ERR_NONE;
}

/* Bilinear interpolation image scaling algorithm.

   Copy the bitmap SRC to the bitmap DST, scaling the bitmap to fit the
   dimensions of DST.  This function uses the bilinear interpolation algorithm
   to interpolate the pixels.

   Supports only direct color modes which have components separated
   into bytes (e.g., RGBA 8:8:8:8 or BGR 8:8:8 true color).
   But because of this simplifying assumption, the implementation is
   greatly simplified.  */
static grub_err_t
scale_bilinear (struct grub_video_bitmap *dst, struct grub_video_bitmap *src)
{
  grub_err_t err = verify_bitmaps(dst, src);
  if (err != GRUB_ERR_NONE)
    return err;

  grub_uint8_t *ddata = dst->data;
  grub_uint8_t *sdata = src->data;
  unsigned dw = dst->mode_info.width;
  unsigned dh = dst->mode_info.height;
  unsigned sw = src->mode_info.width;
  unsigned sh = src->mode_info.height;
  int dstride = dst->mode_info.pitch;
  int sstride = src->mode_info.pitch;
  /* bytes_per_pixel is the same for both src and dst. */
  int bytes_per_pixel = dst->mode_info.bytes_per_pixel;

  unsigned dy;
  for (dy = 0; dy < dh; dy++)
    {
      unsigned dx;
      for (dx = 0; dx < dw; dx++)
        {
          grub_uint8_t *dptr;
          grub_uint8_t *sptr;
          unsigned sx;
          unsigned sy;
          int comp;

          /* Compute the source coordinate that the destination coordinate
             maps to.  Note: sx/sw = dx/dw  =>  sx = sw*dx/dw. */
          sx = sw * dx / dw;
          sy = sh * dy / dh;

          /* Get the address of the pixels in src and dst. */
          dptr = ddata + dy * dstride + dx * bytes_per_pixel;
          sptr = sdata + sy * sstride + sx * bytes_per_pixel;

          /* If we have enough space to do so, use bilinear interpolation.
             Otherwise, fall back to nearest neighbor for this pixel. */
          if (sx < sw - 1 && sy < sh - 1)
            {
              /* Do bilinear interpolation. */

              /* Fixed-point .8 numbers representing the fraction of the
                 distance in the x (u) and y (v) direction within the
                 box of 4 pixels in the source. */
              int u = (256 * sw * dx / dw) - (sx * 256);
              int v = (256 * sh * dy / dh) - (sy * 256);

              for (comp = 0; comp < bytes_per_pixel; comp++)
                {
                  /* Get the component's values for the
                     four source corner pixels. */
                  int f00 = sptr[comp];
                  int f10 = sptr[comp + bytes_per_pixel];
                  int f01 = sptr[comp + sstride];
                  int f11 = sptr[comp + sstride + bytes_per_pixel];

                  /* Count coeffecients. */
                  int c00 = (256 - u) * (256 - v);
                  int c10 = u * (256 - v);
                  int c01 = (256 - u) * v;
                  int c11 = u * v;

                  /* Interpolate. */
                  int fxy = c00 * f00 + c01 * f01 + c10 * f10 + c11 * f11;
                  fxy = fxy / (256 * 256);

                  dptr[comp] = fxy;
                }
            }
          else
            {
              /* Fall back to nearest neighbor interpolation. */
              /* Copy the pixel color value. */
              for (comp = 0; comp < bytes_per_pixel; comp++)
                dptr[comp] = sptr[comp];
            }
        }
    }
  return GRUB_ERR_NONE;
}
