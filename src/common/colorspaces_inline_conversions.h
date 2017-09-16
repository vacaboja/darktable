/*
 *    This file is part of darktable,
 *    copyright (c) 2009--2017 johannes hanika.
 *    copyright (c) 2011--2017 tobias ellinghaus.
 *    copyright (c) 2015 Bruce Guenter
 *
 *    darktable is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    darktable is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with darktable.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#ifdef __SSE2__
#include "common/sse.h"
#include <xmmintrin.h>

static inline __m128 lab_f_inv_m(const __m128 x)
{
  const __m128 epsilon = _mm_set1_ps(0.20689655172413796f); // cbrtf(216.0f/24389.0f);
  const __m128 kappa_rcp_x16 = _mm_set1_ps(16.0f * 27.0f / 24389.0f);
  const __m128 kappa_rcp_x116 = _mm_set1_ps(116.0f * 27.0f / 24389.0f);

  // x > epsilon
  const __m128 res_big = _mm_mul_ps(_mm_mul_ps(x, x), x);
  // x <= epsilon
  const __m128 res_small = _mm_sub_ps(_mm_mul_ps(kappa_rcp_x116, x), kappa_rcp_x16);

  // blend results according to whether each component is > epsilon or not
  const __m128 mask = _mm_cmpgt_ps(x, epsilon);
  return _mm_or_ps(_mm_and_ps(mask, res_big), _mm_andnot_ps(mask, res_small));
}

/** uses D50 white point. */
static inline __m128 dt_Lab_to_XYZ_sse2(const __m128 Lab)
{
  const __m128 d50 = _mm_set_ps(0.0f, 0.8249f, 1.0f, 0.9642f);
  const __m128 coef = _mm_set_ps(0.0f, -1.0f / 200.0f, 1.0f / 116.0f, 1.0f / 500.0f);
  const __m128 offset = _mm_set1_ps(0.137931034f);

  // last component ins shuffle taken from 1st component of Lab to make sure it is not nan, so it will become
  // 0.0f in f
  const __m128 f = _mm_mul_ps(_mm_shuffle_ps(Lab, Lab, _MM_SHUFFLE(0, 2, 0, 1)), coef);

  return _mm_mul_ps(d50,
                    lab_f_inv_m(_mm_add_ps(_mm_add_ps(f, _mm_shuffle_ps(f, f, _MM_SHUFFLE(1, 1, 3, 1))), offset)));
}

static inline __m128 lab_f_m_sse2(const __m128 x)
{
  const __m128 epsilon = _mm_set1_ps(216.0f / 24389.0f);
  const __m128 kappa = _mm_set1_ps(24389.0f / 27.0f);

  // calculate as if x > epsilon : result = cbrtf(x)
  // approximate cbrtf(x):
  const __m128 a = _mm_castsi128_ps(
      _mm_add_epi32(_mm_cvtps_epi32(_mm_div_ps(_mm_cvtepi32_ps(_mm_castps_si128(x)), _mm_set1_ps(3.0f))),
                    _mm_set1_epi32(709921077)));
  const __m128 a3 = _mm_mul_ps(_mm_mul_ps(a, a), a);
  const __m128 res_big
      = _mm_div_ps(_mm_mul_ps(a, _mm_add_ps(a3, _mm_add_ps(x, x))), _mm_add_ps(_mm_add_ps(a3, a3), x));

  // calculate as if x <= epsilon : result = (kappa*x+16)/116
  const __m128 res_small = _mm_div_ps(_mm_add_ps(_mm_mul_ps(kappa, x), _mm_set1_ps(16.0f)), _mm_set1_ps(116.0f));

  // blend results according to whether each component is > epsilon or not
  const __m128 mask = _mm_cmpgt_ps(x, epsilon);
  return _mm_or_ps(_mm_and_ps(mask, res_big), _mm_andnot_ps(mask, res_small));
}

/** uses D50 white point. */
static inline __m128 dt_XYZ_to_Lab_sse2(const __m128 XYZ)
{
  const __m128 d50_inv = _mm_set_ps(0.0f, 1.0f / 0.8249f, 1.0f, 1.0f / 0.9642f);
  const __m128 coef = _mm_set_ps(0.0f, 200.0f, 500.0f, 116.0f);
  const __m128 f = lab_f_m_sse2(_mm_mul_ps(XYZ, d50_inv));
  // because d50_inv.z is 0.0f, lab_f(0) == 16/116, so Lab[0] = 116*f[0] - 16 equal to 116*(f[0]-f[3])
  return _mm_mul_ps(coef, _mm_sub_ps(_mm_shuffle_ps(f, f, _MM_SHUFFLE(3, 1, 0, 1)),
                                     _mm_shuffle_ps(f, f, _MM_SHUFFLE(3, 2, 1, 3))));
}

/** uses D50 white point. */
// see http://www.brucelindbloom.com/Eqn_RGB_XYZ_Matrix.html for the transformation matrices
static inline __m128 dt_XYZ_to_sRGB_sse2(__m128 XYZ)
{
  // XYZ -> sRGB matrix, D65
  const __m128 xyz_to_srgb_0 = _mm_setr_ps(3.1338561f, -0.9787684f, 0.0719453f, 0.0f);
  const __m128 xyz_to_srgb_1 = _mm_setr_ps(-1.6168667f, 1.9161415f, -0.2289914f, 0.0f);
  const __m128 xyz_to_srgb_2 = _mm_setr_ps(-0.4906146f, 0.0334540f, 1.4052427f, 0.0f);

  __m128 rgb
      = _mm_add_ps(_mm_mul_ps(xyz_to_srgb_0, _mm_shuffle_ps(XYZ, XYZ, _MM_SHUFFLE(0, 0, 0, 0))),
                   _mm_add_ps(_mm_mul_ps(xyz_to_srgb_1, _mm_shuffle_ps(XYZ, XYZ, _MM_SHUFFLE(1, 1, 1, 1))),
                              _mm_mul_ps(xyz_to_srgb_2, _mm_shuffle_ps(XYZ, XYZ, _MM_SHUFFLE(2, 2, 2, 2)))));

  // linear sRGB -> gamma corrected sRGB
  __m128 mask = _mm_cmple_ps(rgb, _mm_set1_ps(0.0031308));
  __m128 rgb0 = _mm_mul_ps(_mm_set1_ps(12.92), rgb);
  __m128 rgb1 = _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(1.0 + 0.055), _mm_pow_ps1(rgb, 1.0 / 2.4)), _mm_set1_ps(0.055));
  return _mm_or_ps(_mm_and_ps(mask, rgb0), _mm_andnot_ps(mask, rgb1));
}

static inline __m128 dt_sRGB_to_XYZ_sse2(__m128 rgb)
{
  // sRGB -> XYZ matrix, D65
  const __m128 srgb_to_xyz_0 = _mm_setr_ps(0.4360747f, 0.2225045f, 0.0139322f, 0.0f);
  const __m128 srgb_to_xyz_1 = _mm_setr_ps(0.3850649f, 0.7168786f, 0.0971045f, 0.0f);
  const __m128 srgb_to_xyz_2 = _mm_setr_ps(0.1430804f, 0.0606169f, 0.7141733f, 0.0f);

  // gamma corrected sRGB -> linear sRGB
  __m128 mask = _mm_cmple_ps(rgb, _mm_set1_ps(0.04045));
  __m128 rgb0 = _mm_div_ps(rgb, _mm_set1_ps(12.92));
  __m128 rgb1 = _mm_pow_ps1(_mm_div_ps(_mm_add_ps(rgb, _mm_set1_ps(0.055)), _mm_set1_ps(1 + 0.055)), 2.4);
  rgb = _mm_or_ps(_mm_and_ps(mask, rgb0), _mm_andnot_ps(mask, rgb1));

  __m128 XYZ
      = _mm_add_ps(_mm_mul_ps(srgb_to_xyz_0, _mm_shuffle_ps(rgb, rgb, _MM_SHUFFLE(0, 0, 0, 0))),
                   _mm_add_ps(_mm_mul_ps(srgb_to_xyz_1, _mm_shuffle_ps(rgb, rgb, _MM_SHUFFLE(1, 1, 1, 1))),
                              _mm_mul_ps(srgb_to_xyz_2, _mm_shuffle_ps(rgb, rgb, _MM_SHUFFLE(2, 2, 2, 2)))));
  return XYZ;
}
#endif

static inline float cbrt_5f(float f)
{
  uint32_t *p = (uint32_t *)&f;
  *p = *p / 3 + 709921077;
  return f;
}

static inline float cbrta_halleyf(const float a, const float R)
{
  const float a3 = a * a * a;
  const float b = a * (a3 + R + R) / (a3 + a3 + R);
  return b;
}

static inline float lab_f(const float x)
{
  const float epsilon = 216.0f / 24389.0f;
  const float kappa = 24389.0f / 27.0f;
  if(x > epsilon)
  {
    // approximate cbrtf(x):
    const float a = cbrt_5f(x);
    return cbrta_halleyf(a, x);
  }
  else
    return (kappa * x + 16.0f) / 116.0f;
}

/** uses D50 white point. */
static inline void dt_XYZ_to_Lab(const float *XYZ, float *Lab)
{
  const float d50[3] = { 0.9642, 1.0, 0.8249 };
  const float f[3] = { lab_f(XYZ[0] / d50[0]), lab_f(XYZ[1] / d50[1]), lab_f(XYZ[2] / d50[2]) };
  Lab[0] = 116.0f * f[1] - 16.0f;
  Lab[1] = 500.0f * (f[0] - f[1]);
  Lab[2] = 200.0f * (f[1] - f[2]);
}

static inline float lab_f_inv(const float x)
{
  const float epsilon = 0.20689655172413796; // cbrtf(216.0f/24389.0f);
  const float kappa = 24389.0f / 27.0f;
  if(x > epsilon)
    return x * x * x;
  else
    return (116.0f * x - 16.0f) / kappa;
}

/** uses D50 white point. */
static inline void dt_Lab_to_XYZ(const float *Lab, float *XYZ)
{
  const float d50[3] = { 0.9642, 1.0, 0.8249 };
  const float fy = (Lab[0] + 16.0f) / 116.0f;
  const float fx = Lab[1] / 500.0f + fy;
  const float fz = fy - Lab[2] / 200.0f;
  XYZ[0] = d50[0] * lab_f_inv(fx);
  XYZ[1] = d50[1] * lab_f_inv(fy);
  XYZ[2] = d50[2] * lab_f_inv(fz);
}

/** uses D50 white point. */
static inline void dt_XYZ_to_sRGB(const float *const XYZ, float *sRGB)
{
  const float xyz_to_srgb_matrix[3][3] = { { 3.1338561, -1.6168667, -0.4906146 },
                                           { -0.9787684, 1.9161415, 0.0334540 },
                                           { 0.0719453, -0.2289914, 1.4052427 } };

  // XYZ -> sRGB
  float rgb[3] = { 0, 0, 0 };
  for(int r = 0; r < 3; r++)
    for(int c = 0; c < 3; c++) rgb[r] += xyz_to_srgb_matrix[r][c] * XYZ[c];
  // linear sRGB -> gamma corrected sRGB
  for(int c = 0; c < 3; c++)
    sRGB[c] = rgb[c] <= 0.0031308 ? 12.92 * rgb[c] : (1.0 + 0.055) * powf(rgb[c], 1.0 / 2.4) - 0.055;
}

/** uses D50 white point and clips the output to [0..1]. */
static inline void dt_XYZ_to_sRGB_clipped(const float *const XYZ, float *sRGB)
{
  dt_XYZ_to_sRGB(XYZ, sRGB);

#define CLIP(a) ((a) < 0 ? 0 : (a) > 1 ? 1 : (a))

  for(int i = 0; i < 3; i++) sRGB[i] = CLIP(sRGB[i]);

#undef CLIP
}

static inline void dt_sRGB_to_XYZ(const float *const sRGB, float *XYZ)
{
  const float srgb_to_xyz[3][3] = { { 0.4360747, 0.3850649, 0.1430804 },
                                    { 0.2225045, 0.7168786, 0.0606169 },
                                    { 0.0139322, 0.0971045, 0.7141733 } };

  // sRGB -> XYZ
  XYZ[0] = XYZ[1] = XYZ[2] = 0.0;
  float rgb[3] = { 0 };
  // gamma corrected sRGB -> linear sRGB
  for(int c = 0; c < 3; c++)
    rgb[c] = sRGB[c] <= 0.04045 ? sRGB[c] / 12.92 : powf((sRGB[c] + 0.055) / (1 + 0.055), 2.4);
  for(int r = 0; r < 3; r++)
    for(int c = 0; c < 3; c++) XYZ[r] += srgb_to_xyz[r][c] * rgb[c];
}

static inline void dt_Lab_to_prophotorgb(const float *const Lab, float *rgb)
{
  const float xyz_to_rgb[3][3] = {
    // prophoto rgb d50
    { 1.3459433, -0.2556075, -0.0511118 },
    { -0.5445989, 1.5081673, 0.0205351 },
    { 0.0000000, 0.0000000, 1.2118128 },
  };

  float XYZ[3];
  dt_Lab_to_XYZ(Lab, XYZ);
  rgb[0] = rgb[1] = rgb[2] = 0.0f;
  for(int r = 0; r < 3; r++)
    for(int c = 0; c < 3; c++) rgb[r] += xyz_to_rgb[r][c] * XYZ[c];
}

static inline void dt_prophotorgb_to_Lab(const float *const rgb, float *Lab)
{
  const float rgb_to_xyz[3][3] = {
    // prophoto rgb
    { 0.7976749, 0.1351917, 0.0313534 },
    { 0.2880402, 0.7118741, 0.0000857 },
    { 0.0000000, 0.0000000, 0.8252100 },
  };
  float XYZ[3] = { 0.0f };
  for(int r = 0; r < 3; r++)
    for(int c = 0; c < 3; c++) XYZ[r] += rgb_to_xyz[r][c] * rgb[c];
  dt_XYZ_to_Lab(XYZ, Lab);
}

// modelines: These editor modelines have been set for all relevant files by tools/update_modelines.sh
// vim: shiftwidth=2 expandtab tabstop=2 cindent
// kate: tab-indents: off; indent-width 2; replace-tabs on; indent-mode cstyle; remove-trailing-spaces modified;
