/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2006 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

/*
 * RLE encoding for software colorkey and alpha-channel acceleration
 *
 * Original version by Sam Lantinga
 *
 * Mattias Engdeg�rd (Yorick): Rewrite. New encoding format, encoder and
 * decoder. Added per-surface alpha blitter. Added per-pixel alpha
 * format, encoder and blitter.
 *
 * Many thanks to Xark and johns for hints, benchmarks and useful comments
 * leading to this code.
 *
 * Welcome to Macro Mayhem.
 */

/*
 * The encoding translates the image data to a stream of segments of the form
 *
 * <skip> <run> <data>
 *
 * where <skip> is the number of transparent pixels to skip,
 *       <run>  is the number of opaque pixels to blit,
 * and   <data> are the pixels themselves.
 *
 * This basic structure is used both for colorkeyed surfaces, used for simple
 * binary transparency and for per-surface alpha blending, and for surfaces
 * with per-pixel alpha. The details differ, however:
 *
 * Encoding of colorkeyed surfaces:
 *
 *   Encoded pixels always have the same format as the target surface.
 *   <skip> and <run> are unsigned 8 bit integers, except for 32 bit depth
 *   where they are 16 bit. This makes the pixel data aligned at all times.
 *   Segments never wrap around from one scan line to the next.
 *
 *   The end of the sequence is marked by a zero <skip>,<run> pair at the *
 *   beginning of a line.
 *
 * Encoding of surfaces with per-pixel alpha:
 *
 *   The sequence begins with a struct RLEDestFormat describing the target
 *   pixel format, to provide reliable un-encoding.
 *
 *   Each scan line is encoded twice: First all completely opaque pixels,
 *   encoded in the target format as described above, and then all
 *   partially transparent (translucent) pixels (where 1 <= alpha <= 254),
 *   in the following 32-bit format:
 *
 *   For 32-bit targets, each pixel has the target RGB format but with
 *   the alpha value occupying the highest 8 bits. The <skip> and <run>
 *   counts are 16 bit.
 * 
 *   For 16-bit targets, each pixel has the target RGB format, but with
 *   the middle component (usually green) shifted 16 steps to the left,
 *   and the hole filled with the 5 most significant bits of the alpha value.
 *   i.e. if the target has the format         rrrrrggggggbbbbb,
 *   the encoded pixel will be 00000gggggg00000rrrrr0aaaaabbbbb.
 *   The <skip> and <run> counts are 8 bit for the opaque lines, 16 bit
 *   for the translucent lines. Two padding bytes may be inserted
 *   before each translucent line to keep them 32-bit aligned.
 *
 *   The end of the sequence is marked by a zero <skip>,<run> pair at the
 *   beginning of an opaque line.
 */

#include "SDL_video.h"
#include "SDL_sysvideo.h"
#include "SDL_blit.h"
#include "SDL_RLEaccel_c.h"

#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__)) && SDL_ASSEMBLY_ROUTINES
#define MMX_ASMBLIT
#endif

#ifdef MMX_ASMBLIT
#include "mmx.h"
#include "SDL_cpuinfo.h"
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define PIXEL_COPY(to, from, len, bpp)			\
do {							\
    if(bpp == 4) {					\
	SDL_memcpy4(to, from, (size_t)(len));		\
    } else {						\
	SDL_memcpy(to, from, (size_t)(len) * (bpp));	\
    }							\
} while(0)

/*
 * Various colorkey blit methods, for opaque and per-surface alpha
 */

#define OPAQUE_BLIT(to, from, length, bpp, alpha)	\
    PIXEL_COPY(to, from, length, bpp)

#ifdef MMX_ASMBLIT

#define ALPHA_BLIT32_888MMX(to, from, length, bpp, alpha)	\
    do {							\
	Uint32 *srcp = (Uint32 *)(from);			\
	Uint32 *dstp = (Uint32 *)(to);				\
        int i = 0x00FF00FF;					\
        movd_m2r(*(&i), mm3);					\
        punpckldq_r2r(mm3, mm3);				\
        i = 0xFF000000;						\
        movd_m2r(*(&i), mm7);					\
        punpckldq_r2r(mm7, mm7);				\
        i = alpha | alpha << 16;				\
        movd_m2r(*(&i), mm4);					\
        punpckldq_r2r(mm4, mm4);				\
	pcmpeqd_r2r(mm5,mm5); /* set mm5 to "1" */		\
	pxor_r2r(mm7, mm5); /* make clear alpha mask */		\
        i = length;						\
	if(i & 1) {						\
          movd_m2r((*srcp), mm1); /* src -> mm1 */		\
          punpcklbw_r2r(mm1, mm1);				\
          pand_r2r(mm3, mm1);					\
	  movd_m2r((*dstp), mm2); /* dst -> mm2 */		\
          punpcklbw_r2r(mm2, mm2);				\
          pand_r2r(mm3, mm2);					\
	  psubw_r2r(mm2, mm1);					\
	  pmullw_r2r(mm4, mm1);					\
	  psrlw_i2r(8, mm1);					\
	  paddw_r2r(mm1, mm2);					\
	  pand_r2r(mm3, mm2);					\
	  packuswb_r2r(mm2, mm2);				\
	  pand_r2r(mm5, mm2); /* 00000RGB -> mm2 */		\
	  movd_r2m(mm2, *dstp);					\
	  ++srcp;						\
	  ++dstp;						\
	  i--;							\
	}							\
	for(; i > 0; --i) {					\
          movq_m2r((*srcp), mm0);				\
	  movq_r2r(mm0, mm1);					\
          punpcklbw_r2r(mm0, mm0);				\
	  movq_m2r((*dstp), mm2);				\
	  punpckhbw_r2r(mm1, mm1);				\
	  movq_r2r(mm2, mm6);					\
          pand_r2r(mm3, mm0);					\
          punpcklbw_r2r(mm2, mm2);				\
	  pand_r2r(mm3, mm1);					\
	  punpckhbw_r2r(mm6, mm6);				\
          pand_r2r(mm3, mm2);					\
	  psubw_r2r(mm2, mm0);					\
	  pmullw_r2r(mm4, mm0);					\
	  pand_r2r(mm3, mm6);					\
	  psubw_r2r(mm6, mm1);					\
	  pmullw_r2r(mm4, mm1);					\
	  psrlw_i2r(8, mm0);					\
	  paddw_r2r(mm0, mm2);					\
	  psrlw_i2r(8, mm1);					\
	  paddw_r2r(mm1, mm6);					\
	  pand_r2r(mm3, mm2);					\
	  pand_r2r(mm3, mm6);					\
	  packuswb_r2r(mm2, mm2);				\
	  packuswb_r2r(mm6, mm6);				\
	  psrlq_i2r(32, mm2);					\
	  psllq_i2r(32, mm6);					\
	  por_r2r(mm6, mm2);					\
	  pand_r2r(mm5, mm2); /* 00000RGB -> mm2 */		\
         movq_r2m(mm2, *dstp);					\
	  srcp += 2;						\
	  dstp += 2;						\
	  i--;							\
	}							\
	emms();							\
    } while(0)

#define ALPHA_BLIT16_565MMX(to, from, length, bpp, alpha)	\
    do {						\
        int i, n = 0;					\
	Uint16 *srcp = (Uint16 *)(from);		\
	Uint16 *dstp = (Uint16 *)(to);			\
        Uint32 ALPHA = 0xF800;				\
	movd_m2r(*(&ALPHA), mm1);			\
        punpcklwd_r2r(mm1, mm1);			\
        punpcklwd_r2r(mm1, mm1);			\
	ALPHA = 0x07E0;					\
	movd_m2r(*(&ALPHA), mm4);			\
        punpcklwd_r2r(mm4, mm4);			\
        punpcklwd_r2r(mm4, mm4);			\
	ALPHA = 0x001F;					\
	movd_m2r(*(&ALPHA), mm7);			\
        punpcklwd_r2r(mm7, mm7);			\
        punpcklwd_r2r(mm7, mm7);			\
	alpha &= ~(1+2+4);				\
        i = (Uint32)alpha | (Uint32)alpha << 16;	\
        movd_m2r(*(&i), mm0);				\
        punpckldq_r2r(mm0, mm0);			\
        ALPHA = alpha >> 3;				\
        i = ((int)(length) & 3);			\
	for(; i > 0; --i) {				\
	    Uint32 s = *srcp++;				\
	    Uint32 d = *dstp;				\
	    s = (s | s << 16) & 0x07e0f81f;		\
	    d = (d | d << 16) & 0x07e0f81f;		\
	    d += (s - d) * ALPHA >> 5;			\
	    d &= 0x07e0f81f;				\
	    *dstp++ = d | d >> 16;			\
	    n++;					\
	}						\
	i = (int)(length) - n;				\
	for(; i > 0; --i) {				\
	  movq_m2r((*dstp), mm3);			\
	  movq_m2r((*srcp), mm2);			\
	  movq_r2r(mm2, mm5);				\
	  pand_r2r(mm1 , mm5);				\
	  psrlq_i2r(11, mm5);				\
	  movq_r2r(mm3, mm6);				\
	  pand_r2r(mm1 , mm6);				\
	  psrlq_i2r(11, mm6);				\
	  psubw_r2r(mm6, mm5);				\
	  pmullw_r2r(mm0, mm5);				\
	  psrlw_i2r(8, mm5);				\
	  paddw_r2r(mm5, mm6);				\
	  psllq_i2r(11, mm6);				\
	  pand_r2r(mm1, mm6);				\
	  movq_r2r(mm4, mm5);				\
	  por_r2r(mm7, mm5);				\
	  pand_r2r(mm5, mm3);				\
	  por_r2r(mm6, mm3);				\
	  movq_r2r(mm2, mm5);				\
	  pand_r2r(mm4 , mm5);				\
	  psrlq_i2r(5, mm5);				\
	  movq_r2r(mm3, mm6);				\
	  pand_r2r(mm4 , mm6);				\
	  psrlq_i2r(5, mm6);				\
	  psubw_r2r(mm6, mm5);				\
	  pmullw_r2r(mm0, mm5);				\
	  psrlw_i2r(8, mm5);				\
	  paddw_r2r(mm5, mm6);				\
	  psllq_i2r(5, mm6);				\
	  pand_r2r(mm4, mm6);				\
	  movq_r2r(mm1, mm5);				\
	  por_r2r(mm7, mm5);				\
	  pand_r2r(mm5, mm3);				\
	  por_r2r(mm6, mm3);				\
	  movq_r2r(mm2, mm5);				\
	  pand_r2r(mm7 , mm5);				\
          movq_r2r(mm3, mm6);				\
	  pand_r2r(mm7 , mm6);				\
	  psubw_r2r(mm6, mm5);				\
	  pmullw_r2r(mm0, mm5);				\
	  psrlw_i2r(8, mm5);				\
	  paddw_r2r(mm5, mm6);				\
	  pand_r2r(mm7, mm6);				\
	  movq_r2r(mm1, mm5);				\
	  por_r2r(mm4, mm5);				\
	  pand_r2r(mm5, mm3);				\
	  por_r2r(mm6, mm3);				\
	  movq_r2m(mm3, *dstp);				\
	  srcp += 4;					\
	  dstp += 4;					\
	  i -= 3;					\
	}						\
	emms();						\
    } while(0)

#define ALPHA_BLIT16_555MMX(to, from, length, bpp, alpha)	\
    do {						\
        int i, n = 0;					\
	Uint16 *srcp = (Uint16 *)(from);		\
	Uint16 *dstp = (Uint16 *)(to);			\
        Uint32 ALPHA = 0x7C00;				\
	movd_m2r(*(&ALPHA), mm1);			\
        punpcklwd_r2r(mm1, mm1);			\
        punpcklwd_r2r(mm1, mm1);			\
	ALPHA = 0x03E0;					\
        movd_m2r(*(&ALPHA), mm4);			\
        punpcklwd_r2r(mm4, mm4);			\
        punpcklwd_r2r(mm4, mm4);			\
	ALPHA = 0x001F;					\
	movd_m2r(*(&ALPHA), mm7);			\
        punpcklwd_r2r(mm7, mm7);			\
        punpcklwd_r2r(mm7, mm7);			\
	alpha &= ~(1+2+4);				\
        i = (Uint32)alpha | (Uint32)alpha << 16;	\
        movd_m2r(*(&i), mm0);				\
        punpckldq_r2r(mm0, mm0);			\
        i = ((int)(length) & 3);				\
        ALPHA = alpha >> 3;				\
	for(; i > 0; --i) {				\
	    Uint32 s = *srcp++;				\
	    Uint32 d = *dstp;				\
	    s = (s | s << 16) & 0x03e07c1f;		\
	    d = (d | d << 16) & 0x03e07c1f;		\
	    d += (s - d) * ALPHA >> 5;			\
	    d &= 0x03e07c1f;				\
	    *dstp++ = d | d >> 16;			\
	    n++;					\
	}						\
	i = (int)(length) - n;				\
	for(; i > 0; --i) {				\
	  movq_m2r((*dstp), mm3);			\
	  movq_m2r((*srcp), mm2);			\
	  movq_r2r(mm2, mm5);				\
	  pand_r2r(mm1 , mm5);				\
	  psrlq_i2r(10, mm5);				\
	  movq_r2r(mm3, mm6);				\
	  pand_r2r(mm1 , mm6);				\
	  psrlq_i2r(10, mm6);				\
	  psubw_r2r(mm6, mm5);				\
	  pmullw_r2r(mm0, mm5);				\
	  psrlw_i2r(8, mm5);				\
	  paddw_r2r(mm5, mm6);				\
	  psllq_i2r(10, mm6);				\
	  pand_r2r(mm1, mm6);				\
	  movq_r2r(mm4, mm5);				\
	  por_r2r(mm7, mm5);				\
	  pand_r2r(mm5, mm3);				\
	  por_r2r(mm6, mm3);				\
	  movq_r2r(mm2, mm5);				\
	  pand_r2r(mm4 , mm5);				\
	  psrlq_i2r(5, mm5);				\
	  movq_r2r(mm3, mm6);				\
	  pand_r2r(mm4 , mm6);				\
	  psrlq_i2r(5, mm6);				\
	  psubw_r2r(mm6, mm5);				\
	  pmullw_r2r(mm0, mm5);				\
	  psrlw_i2r(8, mm5);				\
	  paddw_r2r(mm5, mm6);				\
	  psllq_i2r(5, mm6);				\
	  pand_r2r(mm4, mm6);				\
	  movq_r2r(mm1, mm5);				\
	  por_r2r(mm7, mm5);				\
	  pand_r2r(mm5, mm3);				\
	  por_r2r(mm6, mm3);				\
	  movq_r2r(mm2, mm5);				\
	  pand_r2r(mm7 , mm5);				\
          movq_r2r(mm3, mm6);				\
	  pand_r2r(mm7 , mm6);				\
	  psubw_r2r(mm6, mm5);				\
	  pmullw_r2r(mm0, mm5);				\
	  psrlw_i2r(8, mm5);				\
	  paddw_r2r(mm5, mm6);				\
	  pand_r2r(mm7, mm6);				\
	  movq_r2r(mm1, mm5);				\
	  por_r2r(mm4, mm5);				\
	  pand_r2r(mm5, mm3);				\
	  por_r2r(mm6, mm3);				\
	  movq_r2m(mm3, *dstp);				\
	  srcp += 4;					\
	  dstp += 4;					\
	  i -= 3;					\
	}						\
	emms();						\
    } while(0)

#endif

/*
 * For 32bpp pixels on the form 0x00rrggbb:
 * If we treat the middle component separately, we can process the two
 * remaining in parallel. This is safe to do because of the gap to the left
 * of each component, so the bits from the multiplication don't collide.
 * This can be used for any RGB permutation of course.
 */
#define ALPHA_BLIT32_888(to, from, length, bpp, alpha)		\
    do {							\
        int i;							\
	Uint32 *src = (Uint32 *)(from);				\
	Uint32 *dst = (Uint32 *)(to);				\
	for(i = 0; i < (int)(length); i++) {			\
	    Uint32 s = *src++;					\
	    Uint32 d = *dst;					\
	    Uint32 s1 = s & 0xff00ff;				\
	    Uint32 d1 = d & 0xff00ff;				\
	    d1 = (d1 + ((s1 - d1) * alpha >> 8)) & 0xff00ff;	\
	    s &= 0xff00;					\
	    d &= 0xff00;					\
	    d = (d + ((s - d) * alpha >> 8)) & 0xff00;		\
	    *dst++ = d1 | d;					\
	}							\
    } while(0)

/*
 * For 16bpp pixels we can go a step further: put the middle component
 * in the high 16 bits of a 32 bit word, and process all three RGB
 * components at the same time. Since the smallest gap is here just
 * 5 bits, we have to scale alpha down to 5 bits as well.
 */
#define ALPHA_BLIT16_565(to, from, length, bpp, alpha)	\
    do {						\
        int i;						\
	Uint16 *src = (Uint16 *)(from);			\
	Uint16 *dst = (Uint16 *)(to);			\
	Uint32 ALPHA = alpha >> 3;			\
	for(i = 0; i < (int)(length); i++) {		\
	    Uint32 s = *src++;				\
	    Uint32 d = *dst;				\
	    s = (s | s << 16) & 0x07e0f81f;		\
	    d = (d | d << 16) & 0x07e0f81f;		\
	    d += (s - d) * ALPHA >> 5;			\
	    d &= 0x07e0f81f;				\
	    *dst++ = (Uint16)(d | d >> 16);			\
	}						\
    } while(0)

#define ALPHA_BLIT16_555(to, from, length, bpp, alpha)	\
    do {						\
        int i;						\
	Uint16 *src = (Uint16 *)(from);			\
	Uint16 *dst = (Uint16 *)(to);			\
	Uint32 ALPHA = alpha >> 3;			\
	for(i = 0; i < (int)(length); i++) {		\
	    Uint32 s = *src++;				\
	    Uint32 d = *dst;				\
	    s = (s | s << 16) & 0x03e07c1f;		\
	    d = (d | d << 16) & 0x03e07c1f;		\
	    d += (s - d) * ALPHA >> 5;			\
	    d &= 0x03e07c1f;				\
	    *dst++ = (Uint16)(d | d >> 16);			\
	}						\
    } while(0)

/*
 * The general slow catch-all function, for remaining depths and formats
 */
#define ALPHA_BLIT_ANY(to, from, length, bpp, alpha)			\
    do {								\
        int i;								\
	Uint8 *src = from;						\
	Uint8 *dst = to;						\
	for(i = 0; i < (int)(length); i++) {				\
	    Uint32 s, d;						\
	    unsigned rs, gs, bs, rd, gd, bd;				\
	    switch(bpp) {						\
	    case 2:							\
		s = *(Uint16 *)src;					\
		d = *(Uint16 *)dst;					\
		break;							\
	    case 3:							\
		if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {			\
		    s = (src[0] << 16) | (src[1] << 8) | src[2];	\
		    d = (dst[0] << 16) | (dst[1] << 8) | dst[2];	\
		} else {						\
		    s = (src[2] << 16) | (src[1] << 8) | src[0];	\
		    d = (dst[2] << 16) | (dst[1] << 8) | dst[0];	\
		}							\
		break;							\
	    case 4:							\
		s = *(Uint32 *)src;					\
		d = *(Uint32 *)dst;					\
		break;							\
	    }								\
	    RGB_FROM_PIXEL(s, fmt, rs, gs, bs);				\
	    RGB_FROM_PIXEL(d, fmt, rd, gd, bd);				\
	    rd += (rs - rd) * alpha >> 8;				\
	    gd += (gs - gd) * alpha >> 8;				\
	    bd += (bs - bd) * alpha >> 8;				\
	    PIXEL_FROM_RGB(d, fmt, rd, gd, bd);				\
	    switch(bpp) {						\
	    case 2:							\
		*(Uint16 *)dst = (Uint16)d;					\
		break;							\
	    case 3:							\
		if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {			\
		    dst[0] = (Uint8)(d >> 16);					\
		    dst[1] = (Uint8)(d >> 8);					\
		    dst[2] = (Uint8)(d);						\
		} else {						\
		    dst[0] = (Uint8)d;						\
		    dst[1] = (Uint8)(d >> 8);					\
		    dst[2] = (Uint8)(d >> 16);					\
		}							\
		break;							\
	    case 4:							\
		*(Uint32 *)dst = d;					\
		break;							\
	    }								\
	    src += bpp;							\
	    dst += bpp;							\
	}								\
    } while(0)

#ifdef MMX_ASMBLIT

#define ALPHA_BLIT32_888_50MMX(to, from, length, bpp, alpha)		\
    do {								\
	Uint32 *srcp = (Uint32 *)(from);				\
	Uint32 *dstp = (Uint32 *)(to);					\
        int i = 0x00fefefe;						\
        movd_m2r(*(&i), mm4);						\
        punpckldq_r2r(mm4, mm4);					\
        i = 0x00010101;							\
        movd_m2r(*(&i), mm3);						\
        punpckldq_r2r(mm3, mm3);					\
        i = (int)(length);						\
        if( i & 1 ) {							\
	  Uint32 s = *srcp++;						\
	  Uint32 d = *dstp;						\
	  *dstp++ = (((s & 0x00fefefe) + (d & 0x00fefefe)) >> 1)	\
		     + (s & d & 0x00010101);				\
	  i--;								\
	}								\
	for(; i > 0; --i) {						\
	    movq_m2r((*dstp), mm2); /* dst -> mm2 */			\
	    movq_r2r(mm2, mm6);	/* dst -> mm6 */			\
	    movq_m2r((*srcp), mm1); /* src -> mm1 */			\
	    movq_r2r(mm1, mm5);	/* src -> mm5 */			\
	    pand_r2r(mm4, mm6);	/* dst & 0x00fefefe -> mm6 */		\
	    pand_r2r(mm4, mm5); /* src & 0x00fefefe -> mm5 */		\
	    paddd_r2r(mm6, mm5); /* (dst & 0x00fefefe) + (dst & 0x00fefefe) -> mm5 */	\
	    psrld_i2r(1, mm5);						\
	    pand_r2r(mm1, mm2);	/* s & d -> mm2 */			\
	    pand_r2r(mm3, mm2);	/* s & d & 0x00010101 -> mm2 */		\
	    paddd_r2r(mm5, mm2);					\
	    movq_r2m(mm2, (*dstp));					\
	    dstp += 2;							\
	    srcp += 2;							\
	    i--;							\
	}								\
	emms();								\
    } while(0)

#endif
    
/*
 * Special case: 50% alpha (alpha=128)
 * This is treated specially because it can be optimized very well, and
 * since it is good for many cases of semi-translucency.
 * The theory is to do all three components at the same time:
 * First zero the lowest bit of each component, which gives us room to
 * add them. Then shift right and add the sum of the lowest bits.
 */
#define ALPHA_BLIT32_888_50(to, from, length, bpp, alpha)		\
    do {								\
        int i;								\
	Uint32 *src = (Uint32 *)(from);					\
	Uint32 *dst = (Uint32 *)(to);					\
	for(i = 0; i < (int)(length); i++) {				\
	    Uint32 s = *src++;						\
	    Uint32 d = *dst;						\
	    *dst++ = (((s & 0x00fefefe) + (d & 0x00fefefe)) >> 1)	\
		     + (s & d & 0x00010101);				\
	}								\
    } while(0)

/*
 * For 16bpp, we can actually blend two pixels in parallel, if we take
 * care to shift before we add, not after.
 */

/* helper: blend a single 16 bit pixel at 50% */
#define BLEND16_50(dst, src, mask)			\
    do {						\
	Uint32 s = *src++;				\
	Uint32 d = *dst;				\
	*dst++ = (Uint16)((((s & mask) + (d & mask)) >> 1) +	\
	                  (s & d & (~mask & 0xffff)));		\
    } while(0)

/* basic 16bpp blender. mask is the pixels to keep when adding. */
#define ALPHA_BLIT16_50(to, from, length, bpp, alpha, mask)		\
    do {								\
	unsigned n = (length);						\
	Uint16 *src = (Uint16 *)(from);					\
	Uint16 *dst = (Uint16 *)(to);					\
	if(((uintptr_t)src ^ (uintptr_t)dst) & 3) {			\
	    /* source and destination not in phase, blit one by one */	\
	    while(n--)							\
		BLEND16_50(dst, src, mask);				\
	} else {							\
	    if((uintptr_t)src & 3) {					\
		/* first odd pixel */					\
		BLEND16_50(dst, src, mask);				\
		n--;							\
	    }								\
	    for(; n > 1; n -= 2) {					\
		Uint32 s = *(Uint32 *)src;				\
		Uint32 d = *(Uint32 *)dst;				\
		*(Uint32 *)dst = ((s & (mask | mask << 16)) >> 1)	\
		               + ((d & (mask | mask << 16)) >> 1)	\
		               + (s & d & (~(mask | mask << 16)));	\
		src += 2;						\
		dst += 2;						\
	    }								\
	    if(n)							\
		BLEND16_50(dst, src, mask); /* last odd pixel */	\
	}								\
    } while(0)

#define ALPHA_BLIT16_565_50(to, from, length, bpp, alpha)	\
    ALPHA_BLIT16_50(to, from, length, bpp, alpha, 0xf7de)

#define ALPHA_BLIT16_555_50(to, from, length, bpp, alpha)	\
    ALPHA_BLIT16_50(to, from, length, bpp, alpha, 0xfbde)

#ifdef MMX_ASMBLIT

#define CHOOSE_BLIT(blitter, alpha, fmt)				\
    do {								\
        if(alpha == 255) {						\
	    switch(fmt->BytesPerPixel) {				\
	    case 1: blitter(1, Uint8, OPAQUE_BLIT); break;		\
	    case 2: blitter(2, Uint8, OPAQUE_BLIT); break;		\
	    case 3: blitter(3, Uint8, OPAQUE_BLIT); break;		\
	    case 4: blitter(4, Uint16, OPAQUE_BLIT); break;		\
	    }								\
	} else {							\
	    switch(fmt->BytesPerPixel) {				\
	    case 1:							\
		/* No 8bpp alpha blitting */				\
		break;							\
									\
	    case 2:							\
		switch(fmt->Rmask | fmt->Gmask | fmt->Bmask) {		\
		case 0xffff:						\
		    if(fmt->Gmask == 0x07e0				\
		       || fmt->Rmask == 0x07e0				\
		       || fmt->Bmask == 0x07e0) {			\
			if(alpha == 128)				\
			    blitter(2, Uint8, ALPHA_BLIT16_565_50);	\
			else {						\
			    if(SDL_HasMMX())				\
				blitter(2, Uint8, ALPHA_BLIT16_565MMX);	\
			    else					\
				blitter(2, Uint8, ALPHA_BLIT16_565);	\
			}						\
		    } else						\
			goto general16;					\
		    break;						\
									\
		case 0x7fff:						\
		    if(fmt->Gmask == 0x03e0				\
		       || fmt->Rmask == 0x03e0				\
		       || fmt->Bmask == 0x03e0) {			\
			if(alpha == 128)				\
			    blitter(2, Uint8, ALPHA_BLIT16_555_50);	\
			else {						\
			    if(SDL_HasMMX())				\
				blitter(2, Uint8, ALPHA_BLIT16_555MMX);	\
			    else					\
				blitter(2, Uint8, ALPHA_BLIT16_555);	\
			}						\
			break;						\
		    }							\
		    /* fallthrough */					\
									\
		default:						\
		general16:						\
		    blitter(2, Uint8, ALPHA_BLIT_ANY);			\
		}							\
		break;							\
									\
	    case 3:							\
		blitter(3, Uint8, ALPHA_BLIT_ANY);			\
		break;							\
									\
	    case 4:							\
		if((fmt->Rmask | fmt->Gmask | fmt->Bmask) == 0x00ffffff	\
		   && (fmt->Gmask == 0xff00 || fmt->Rmask == 0xff00	\
		       || fmt->Bmask == 0xff00)) {			\
		    if(alpha == 128)					\
		    {							\
			if(SDL_HasMMX())				\
				blitter(4, Uint16, ALPHA_BLIT32_888_50MMX);\
			else						\
				blitter(4, Uint16, ALPHA_BLIT32_888_50);\
		    }							\
		    else						\
		    {							\
			if(SDL_HasMMX())				\
				blitter(4, Uint16, ALPHA_BLIT32_888MMX);\
			else						\
				blitter(4, Uint16, ALPHA_BLIT32_888);	\
		    }							\
		} else							\
		    blitter(4, Uint16, ALPHA_BLIT_ANY);			\
		break;							\
	    }								\
	}								\
    } while(0)

#else
	
#define CHOOSE_BLIT(blitter, alpha, fmt)				\
    do {								\
        if(alpha == 255) {						\
	    switch(fmt->BytesPerPixel) {				\
	    case 1: blitter(1, Uint8, OPAQUE_BLIT); break;		\
	    case 2: blitter(2, Uint8, OPAQUE_BLIT); break;		\
	    case 3: blitter(3, Uint8, OPAQUE_BLIT); break;		\
	    case 4: blitter(4, Uint16, OPAQUE_BLIT); break;		\
	    }								\
	} else {							\
	    switch(fmt->BytesPerPixel) {				\
	    case 1:							\
		/* No 8bpp alpha blitting */				\
		break;							\
									\
	    case 2:							\
		switch(fmt->Rmask | fmt->Gmask | fmt->Bmask) {		\
		case 0xffff:						\
		    if(fmt->Gmask == 0x07e0				\
		       || fmt->Rmask == 0x07e0				\
		       || fmt->Bmask == 0x07e0) {			\
			if(alpha == 128)				\
			    blitter(2, Uint8, ALPHA_BLIT16_565_50);	\
			else {						\
			    blitter(2, Uint8, ALPHA_BLIT16_565);	\
			}						\
		    } else						\
			goto general16;					\
		    break;						\
									\
		case 0x7fff:						\
		    if(fmt->Gmask == 0x03e0				\
		       || fmt->Rmask == 0x03e0				\
		       || fmt->Bmask == 0x03e0) {			\
			if(alpha == 128)				\
			    blitter(2, Uint8, ALPHA_BLIT16_555_50);	\
			else {						\
			    blitter(2, Uint8, ALPHA_BLIT16_555);	\
			}						\
			break;						\
		    }							\
		    /* fallthrough */					\
									\
		default:						\
		general16:						\
		    blitter(2, Uint8, ALPHA_BLIT_ANY);			\
		}							\
		break;							\
									\
	    case 3:							\
		blitter(3, Uint8, ALPHA_BLIT_ANY);			\
		break;							\
									\
	    case 4:							\
		if((fmt->Rmask | fmt->Gmask | fmt->Bmask) == 0x00ffffff	\
		   && (fmt->Gmask == 0xff00 || fmt->Rmask == 0xff00	\
		       || fmt->Bmask == 0xff00)) {			\
		    if(alpha == 128)					\
			blitter(4, Uint16, ALPHA_BLIT32_888_50);	\
		    else						\
			blitter(4, Uint16, ALPHA_BLIT32_888);		\
		} else							\
		    blitter(4, Uint16, ALPHA_BLIT_ANY);			\
		break;							\
	    }								\
	}								\
    } while(0)

#endif

/*
 * This takes care of the case when the surface is clipped on the left and/or
 * right. Top clipping has already been taken care of.
 */
static void RLEClipBlit(int w, Uint8 *srcbuf, SDL_Surface *dst,
			Uint8 *dstbuf, SDL_Rect *srcrect, unsigned alpha)
{
    SDL_PixelFormat *fmt = dst->format;

#define RLECLIPBLIT(bpp, Type, do_blit)					   \
    do {								   \
	int linecount = srcrect->h;					   \
	int ofs = 0;							   \
	int left = srcrect->x;						   \
	int right = left + srcrect->w;					   \
	dstbuf -= left * bpp;						   \
	for(;;) {							   \
	    int run;							   \
	    ofs += *(Type *)srcbuf;					   \
	    run = ((Type *)srcbuf)[1];					   \
	    srcbuf += 2 * sizeof(Type);					   \
	    if(run) {							   \
		/* clip to left and right borders */			   \
		if(ofs < right) {					   \
		    int start = 0;					   \
		    int len = run;					   \
		    int startcol;					   \
		    if(left - ofs > 0) {				   \
			start = left - ofs;				   \
			len -= start;					   \
			if(len <= 0)					   \
			    goto nocopy ## bpp ## do_blit;		   \
		    }							   \
		    startcol = ofs + start;				   \
		    if(len > right - startcol)				   \
			len = right - startcol;				   \
		    do_blit(dstbuf + startcol * bpp, srcbuf + start * bpp, \
			    len, bpp, alpha);				   \
		}							   \
	    nocopy ## bpp ## do_blit:					   \
		srcbuf += run * bpp;					   \
		ofs += run;						   \
	    } else if(!ofs)						   \
		break;							   \
	    if(ofs == w) {						   \
		ofs = 0;						   \
		dstbuf += dst->pitch;					   \
		if(!--linecount)					   \
		    break;						   \
	    }								   \
	}								   \
    } while(0)

    CHOOSE_BLIT(RLECLIPBLIT, alpha, fmt);

#undef RLECLIPBLIT

}


/* blit a colorkeyed RLE surface */
int SDL_RLEBlit(SDL_Surface *src, SDL_Rect *srcrect,
		SDL_Surface *dst, SDL_Rect *dstrect)
{
	Uint8 *dstbuf;
	Uint8 *srcbuf;
	int x, y;
	int w = src->w;
	unsigned alpha;

	/* Lock the destination if necessary */
	if ( SDL_MUSTLOCK(dst) ) {
		if ( SDL_LockSurface(dst) < 0 ) {
			return(-1);
		}
	}

	/* Set up the source and destination pointers */
	x = dstrect->x;
	y = dstrect->y;
	dstbuf = (Uint8 *)dst->pixels
	         + y * dst->pitch + x * src->format->BytesPerPixel;
	srcbuf = (Uint8 *)src->map->sw_data->aux_data;

	{
	    /* skip lines at the top if neccessary */
	    int vskip = srcrect->y;
	    int ofs = 0;
	    if(vskip) {

#define RLESKIP(bpp, Type)			\
		for(;;) {			\
		    int run;			\
		    ofs += *(Type *)srcbuf;	\
		    run = ((Type *)srcbuf)[1];	\
		    srcbuf += sizeof(Type) * 2;	\
		    if(run) {			\
			srcbuf += run * bpp;	\
			ofs += run;		\
		    } else if(!ofs)		\
			goto done;		\
		    if(ofs == w) {		\
			ofs = 0;		\
			if(!--vskip)		\
			    break;		\
		    }				\
		}

		switch(src->format->BytesPerPixel) {
		case 1: RLESKIP(1, Uint8); break;
		case 2: RLESKIP(2, Uint8); break;
		case 3: RLESKIP(3, Uint8); break;
		case 4: RLESKIP(4, Uint16); break;
		}

#undef RLESKIP

	    }
	}

	alpha = (src->flags & SDL_SRCALPHA) == SDL_SRCALPHA
	        ? src->format->alpha : 255;
	/* if left or right edge clipping needed, call clip blit */
	if ( srcrect->x || srcrect->w != src->w ) {
	    RLEClipBlit(w, srcbuf, dst, dstbuf, srcrect, alpha);
	} else {
	    SDL_PixelFormat *fmt = src->format;

#define RLEBLIT(bpp, Type, do_blit)					      \
	    do {							      \
		int linecount = srcrect->h;				      \
		int ofs = 0;						      \
		for(;;) {						      \
		    unsigned run;					      \
		    ofs += *(Type *)srcbuf;				      \
		    run = ((Type *)srcbuf)[1];				      \
		    srcbuf += 2 * sizeof(Type);				      \
		    if(run) {						      \
			do_blit(dstbuf + ofs * bpp, srcbuf, run, bpp, alpha); \
			srcbuf += run * bpp;				      \
			ofs += run;					      \
		    } else if(!ofs)					      \
			break;						      \
		    if(ofs == w) {					      \
			ofs = 0;					      \
			dstbuf += dst->pitch;				      \
			if(!--linecount)				      \
			    break;					      \
		    }							      \
		}							      \
	    } while(0)

	    CHOOSE_BLIT(RLEBLIT, alpha, fmt);

#undef RLEBLIT
	}

done:
	/* Unlock the destination if necessary */
	if ( SDL_MUSTLOCK(dst) ) {
		SDL_UnlockSurface(dst);
	}
	return(0);
}

#undef OPAQUE_BLIT

/*
 * Per-pixel blitting macros for translucent pixels:
 * These use the same techniques as the per-surface blitting macros
 */

/*
 * For 32bpp pixels, we have made sure the alpha is stored in the top
 * 8 bits, so proceed as usual
 */
#define BLIT_TRANSL_888(src, dst)				\
    do {							\
        Uint32 s = src;						\
	Uint32 d = dst;						\
	unsigned alpha = s >> 24;				\
	Uint32 s1 = s & 0xff00ff;				\
	Uint32 d1 = d & 0xff00ff;				\
	d1 = (d1 + ((s1 - d1) * alpha >> 8)) & 0xff00ff;	\
	s &= 0xff00;						\
	d &= 0xff00;						\
	d = (d + ((s - d) * alpha >> 8)) & 0xff00;		\
	dst = d1 | d;						\
    } while(0)

/*
 * For 16bpp pixels, we have stored the 5 most significant alpha bits in
 * bits 5-10. As before, we can process all 3 RGB components at the same time.
 */
#define BLIT_TRANSL_565(src, dst)		\
    do {					\
	Uint32 s = src;				\
	Uint32 d = dst;				\
	unsigned alpha = (s & 0x3e0) >> 5;	\
	s &= 0x07e0f81f;			\
	d = (d | d << 16) & 0x07e0f81f;		\
	d += (s - d) * alpha >> 5;		\
	d &= 0x07e0f81f;			\
	dst = (Uint16)(d | d >> 16);			\
    } while(0)

#define BLIT_TRANSL_555(src, dst)		\
    do {					\
	Uint32 s = src;				\
	Uint32 d = dst;				\
	unsigned alpha = (s & 0x3e0) >> 5;	\
	s &= 0x03e07c1f;			\
	d = (d | d << 16) & 0x03e07c1f;		\
	d += (s - d) * alpha >> 5;		\
	d &= 0x03e07c1f;			\
	dst = (Uint16)(d | d >> 16);			\
    } while(0)

/* used to save the destination format in the encoding. Designed to be
   macro-compatible with SDL_PixelFormat but without the unneeded fields */
typedef struct {
	Uint8  BytesPerPixel;
	Uint8  Rloss;
	Uint8  Gloss;
	Uint8  Bloss;
	Uint8  Rshift;
	Uint8  Gshift;
	Uint8  Bshift;
	Uint8  Ashift;
	Uint32 Rmask;
	Uint32 Gmask;
	Uint32 Bmask;
	Uint32 Amask;
} RLEDestFormat;

/* blit a pixel-alpha RLE surface clipped at the right and/or left edges */
static void RLEAlphaClipBlit(int w, Uint8 *srcbuf, SDL_Surface *dst,
			     Uint8 *dstbuf, SDL_Rect *srcrect)
{
    SDL_PixelFormat *df = dst->format;
    /*
     * clipped blitter: Ptype is the destination pixel type,
     * Ctype the translucent count type, and do_blend the macro
     * to blend one pixel.
     */
#define RLEALPHACLIPBLIT(Ptype, Ctype, do_blend)			  \
    do {								  \
	int linecount = srcrect->h;					  \
	int left = srcrect->x;						  \
	int right = left + srcrect->w;					  \
	dstbuf -= left * sizeof(Ptype);					  \
	do {								  \
	    int ofs = 0;						  \
	    /* blit opaque pixels on one line */			  \
	    do {							  \
		unsigned run;						  \
		ofs += ((Ctype *)srcbuf)[0];				  \
		run = ((Ctype *)srcbuf)[1];				  \
		srcbuf += 2 * sizeof(Ctype);				  \
		if(run) {						  \
		    /* clip to left and right borders */		  \
		    int cofs = ofs;					  \
		    int crun = run;					  \
		    if(left - cofs > 0) {				  \
			crun -= left - cofs;				  \
			cofs = left;					  \
		    }							  \
		    if(crun > right - cofs)				  \
			crun = right - cofs;				  \
		    if(crun > 0)					  \
			PIXEL_COPY(dstbuf + cofs * sizeof(Ptype),	  \
				   srcbuf + (cofs - ofs) * sizeof(Ptype), \
				   (unsigned)crun, sizeof(Ptype));	  \
		    srcbuf += run * sizeof(Ptype);			  \
		    ofs += run;						  \
		} else if(!ofs)						  \
		    return;						  \
	    } while(ofs < w);						  \
	    /* skip padding if necessary */				  \
	    if(sizeof(Ptype) == 2)					  \
		srcbuf += (uintptr_t)srcbuf & 2;			  \
	    /* blit translucent pixels on the same line */		  \
	    ofs = 0;							  \
	    do {							  \
		unsigned run;						  \
		ofs += ((Uint16 *)srcbuf)[0];				  \
		run = ((Uint16 *)srcbuf)[1];				  \
		srcbuf += 4;						  \
		if(run) {						  \
		    /* clip to left and right borders */		  \
		    int cofs = ofs;					  \
		    int crun = run;					  \
		    if(left - cofs > 0) {				  \
			crun -= left - cofs;				  \
			cofs = left;					  \
		    }							  \
		    if(crun > right - cofs)				  \
			crun = right - cofs;				  \
		    if(crun > 0) {					  \
			Ptype *dst = (Ptype *)dstbuf + cofs;		  \
			Uint32 *src = (Uint32 *)srcbuf + (cofs - ofs);	  \
			int i;						  \
			for(i = 0; i < crun; i++)			  \
			    do_blend(src[i], dst[i]);			  \
		    }							  \
		    srcbuf += run * 4;					  \
		    ofs += run;						  \
		}							  \
	    } while(ofs < w);						  \
	    dstbuf += dst->pitch;					  \
	} while(--linecount);						  \
    } while(0)

    switch(df->BytesPerPixel) {
    case 2:
	if(df->Gmask == 0x07e0 || df->Rmask == 0x07e0
	   || df->Bmask == 0x07e0)
	    RLEALPHACLIPBLIT(Uint16, Uint8, BLIT_TRANSL_565);
	else
	    RLEALPHACLIPBLIT(Uint16, Uint8, BLIT_TRANSL_555);
	break;
    case 4:
	RLEALPHACLIPBLIT(Uint32, Uint16, BLIT_TRANSL_888);
	break;
    }
}

/* blit a pixel-alpha RLE surface */
int SDL_RLEAlphaBlit(SDL_Surface *src, SDL_Rect *srcrect,
		     SDL_Surface *dst, SDL_Rect *dstrect)
{
    int x, y;
    int w = src->w;
    Uint8 *srcbuf, *dstbuf;
    SDL_PixelFormat *df = dst->format;

    /* Lock the destination if necessary */
    if ( SDL_MUSTLOCK(dst) ) {
	if ( SDL_LockSurface(dst) < 0 ) {
	    return -1;
	}
    }

    x = dstrect->x;
    y = dstrect->y;
    dstbuf = (Uint8 *)dst->pixels
	     + y * dst->pitch + x * df->BytesPerPixel;
    srcbuf = (Uint8 *)src->map->sw_data->aux_data + sizeof(RLEDestFormat);

    {
	/* skip lines at the top if necessary */
	int vskip = srcrect->y;
	if(vskip) {
	    int ofs;
	    if(df->BytesPerPixel == 2) {
		/* the 16/32 interleaved format */
		do {
		    /* skip opaque line */
		    ofs = 0;
		    do {
			int run;
			ofs += srcbuf[0];
			run = srcbuf[1];
			srcbuf += 2;
			if(run) {
			    srcbuf += 2 * run;
			    ofs += run;
			} else if(!ofs)
			    goto done;
		    } while(ofs < w);

		    /* skip padding */
		    srcbuf += (uintptr_t)srcbuf & 2;

		    /* skip translucent line */
		    ofs = 0;
		    do {
			int run;
			ofs += ((Uint16 *)srcbuf)[0];
			run = ((Uint16 *)srcbuf)[1];
			srcbuf += 4 * (run + 1);
			ofs += run;
		    } while(ofs < w);
		} while(--vskip);
	    } else {
		/* the 32/32 interleaved format */
		vskip <<= 1;	/* opaque and translucent have same format */
		do {
		    ofs = 0;
		    do {
			int run;
			ofs += ((Uint16 *)srcbuf)[0];
			run = ((Uint16 *)srcbuf)[1];
			srcbuf += 4;
			if(run) {
			    srcbuf += 4 * run;
			    ofs += run;
			} else if(!ofs)
			    goto done;
		    } while(ofs < w);
		} while(--vskip);
	    }
	}
    }

    /* if left or right edge clipping needed, call clip blit */
    if(srcrect->x || srcrect->w != src->w) {
	RLEAlphaClipBlit(w, srcbuf, dst, dstbuf, srcrect);
    } else {

	/*
	 * non-clipped blitter. Ptype is the destination pixel type,
	 * Ctype the translucent count type, and do_blend the
	 * macro to blend one pixel.
	 */
#define RLEALPHABLIT(Ptype, Ctype, do_blend)				 \
	do {								 \
	    int linecount = srcrect->h;					 \
	    do {							 \
		int ofs = 0;						 \
		/* blit opaque pixels on one line */			 \
		do {							 \
		    unsigned run;					 \
		    ofs += ((Ctype *)srcbuf)[0];			 \
		    run = ((Ctype *)srcbuf)[1];				 \
		    srcbuf += 2 * sizeof(Ctype);			 \
		    if(run) {						 \
			PIXEL_COPY(dstbuf + ofs * sizeof(Ptype), srcbuf, \
				   run, sizeof(Ptype));			 \
			srcbuf += run * sizeof(Ptype);			 \
			ofs += run;					 \
		    } else if(!ofs)					 \
			goto done;					 \
		} while(ofs < w);					 \
		/* skip padding if necessary */				 \
		if(sizeof(Ptype) == 2)					 \
		    srcbuf += (uintptr_t)srcbuf & 2;		 	 \
		/* blit translucent pixels on the same line */		 \
		ofs = 0;						 \
		do {							 \
		    unsigned run;					 \
		    ofs += ((Uint16 *)srcbuf)[0];			 \
		    run = ((Uint16 *)srcbuf)[1];			 \
		    srcbuf += 4;					 \
		    if(run) {						 \
			Ptype *dst = (Ptype *)dstbuf + ofs;		 \
			unsigned i;					 \
			for(i = 0; i < run; i++) {			 \
			    Uint32 src = *(Uint32 *)srcbuf;		 \
			    do_blend(src, *dst);			 \
			    srcbuf += 4;				 \
			    dst++;					 \
			}						 \
			ofs += run;					 \
		    }							 \
		} while(ofs < w);					 \
		dstbuf += dst->pitch;					 \
	    } while(--linecount);					 \
	} while(0)

	switch(df->BytesPerPixel) {
	case 2:
	    if(df->Gmask == 0x07e0 || df->Rmask == 0x07e0
	       || df->Bmask == 0x07e0)
		RLEALPHABLIT(Uint16, Uint8, BLIT_TRANSL_565);
	    else
		RLEALPHABLIT(Uint16, Uint8, BLIT_TRANSL_555);
	    break;
	case 4:
	    RLEALPHABLIT(Uint32, Uint16, BLIT_TRANSL_888);
	    break;
	}
    }

 done:
    /* Unlock the destination if necessary */
    if ( SDL_MUSTLOCK(dst) ) {
	SDL_UnlockSurface(dst);
    }
    return 0;
}

/*
 * Auxiliary functions:
 * The encoding functions take 32bpp rgb + a, and
 * return the number of bytes copied to the destination.
 * The decoding functions copy to 32bpp rgb + a, and
 * return the number of bytes copied from the source.
 * These are only used in the encoder and un-RLE code and are therefore not
 * highly optimised.
 */

/* encode 32bpp rgb + a into 16bpp rgb, losing alpha */
static int copy_opaque_16(void *dst, Uint32 *src, int n,
			  SDL_PixelFormat *sfmt, SDL_PixelFormat *dfmt)
{
    int i;
    Uint16 *d = dst;
    for(i = 0; i < n; i++) {
	unsigned r, g, b;
	RGB_FROM_PIXEL(*src, sfmt, r, g, b);
	PIXEL_FROM_RGB(*d, dfmt, r, g, b);
	src++;
	d++;
    }
    return n * 2;
}

/* decode opaque pixels from 16bpp to 32bpp rgb + a */
static int uncopy_opaque_16(Uint32 *dst, void *src, int n,
			    RLEDestFormat *sfmt, SDL_PixelFormat *dfmt)
{
    int i;
    Uint16 *s = src;
    unsigned alpha = dfmt->Amask ? 255 : 0;
    for(i = 0; i < n; i++) {
	unsigned r, g, b;
	RGB_FROM_PIXEL(*s, sfmt, r, g, b);
	PIXEL_FROM_RGBA(*dst, dfmt, r, g, b, alpha);
	s++;
	dst++;
    }
    return n * 2;
}



/* encode 32bpp rgb + a into 32bpp G0RAB format for blitting into 565 */
static int copy_transl_565(void *dst, Uint32 *src, int n,
			   SDL_PixelFormat *sfmt, SDL_PixelFormat *dfmt)
{
    int i;
    Uint32 *d = dst;
    for(i = 0; i < n; i++) {
	unsigned r, g, b, a;
	Uint16 pix;
	RGBA_FROM_8888(*src, sfmt, r, g, b, a);
	PIXEL_FROM_RGB(pix, dfmt, r, g, b);
	*d = ((pix & 0x7e0) << 16) | (pix & 0xf81f) | ((a << 2) & 0x7e0);
	src++;
	d++;
    }
    return n * 4;
}

/* encode 32bpp rgb + a into 32bpp G0RAB format for blitting into 555 */
static int copy_transl_555(void *dst, Uint32 *src, int n,
			   SDL_PixelFormat *sfmt, SDL_PixelFormat *dfmt)
{
    int i;
    Uint32 *d = dst;
    for(i = 0; i < n; i++) {
	unsigned r, g, b, a;
	Uint16 pix;
	RGBA_FROM_8888(*src, sfmt, r, g, b, a);
	PIXEL_FROM_RGB(pix, dfmt, r, g, b);
	*d = ((pix & 0x3e0) << 16) | (pix & 0xfc1f) | ((a << 2) & 0x3e0);
	src++;
	d++;
    }
    return n * 4;
}

/* decode translucent pixels from 32bpp GORAB to 32bpp rgb + a */
static int uncopy_transl_16(Uint32 *dst, void *src, int n,
			    RLEDestFormat *sfmt, SDL_PixelFormat *dfmt)
{
    int i;
    Uint32 *s = src;
    for(i = 0; i < n; i++) {
	unsigned r, g, b, a;
	Uint32 pix = *s++;
	a = (pix & 0x3e0) >> 2;
	pix = (pix & ~0x3e0) | pix >> 16;
	RGB_FROM_PIXEL(pix, sfmt, r, g, b);
	PIXEL_FROM_RGBA(*dst, dfmt, r, g, b, a);
	dst++;
    }
    return n * 4;
}

/* encode 32bpp rgba into 32bpp rgba, keeping alpha (dual purpose) */
static int copy_32(void *dst, Uint32 *src, int n,
		   SDL_PixelFormat *sfmt, SDL_PixelFormat *dfmt)
{
    int i;
    Uint32 *d = dst;
    for(i = 0; i < n; i++) {
	unsigned r, g, b, a;
	Uint32 pixel;
	RGBA_FROM_8888(*src, sfmt, r, g, b, a);
	PIXEL_FROM_RGB(pixel, dfmt, r, g, b);
	*d++ = pixel | a << 24;
	src++;
    }
    return n * 4;
}

/* decode 32bpp rgba into 32bpp rgba, keeping alpha (dual purpose) */
static int uncopy_32(Uint32 *dst, void *src, int n,
		     RLEDestFormat *sfmt, SDL_PixelFormat *dfmt)
{
    int i;
    Uint32 *s = src;
    for(i = 0; i < n; i++) {
	unsigned r, g, b, a;
	Uint32 pixel = *s++;
	RGB_FROM_PIXEL(pixel, sfmt, r, g, b);
	a = pixel >> 24;
	PIXEL_FROM_RGBA(*dst, dfmt, r, g, b, a);
	dst++;
    }
    return n * 4;
}

#define ISOPAQUE(pixel, fmt) ((((pixel) & fmt->Amask) >> fmt->Ashift) == 255)

#define ISTRANSL(pixel, fmt)	\
    ((unsigned)((((pixel) & fmt->Amask) >> fmt->Ashift) - 1U) < 254U)

/* convert surface to be quickly alpha-blittable onto dest, if possible */
static int RLEAlphaSurface(SDL_Surface *surface)
{
    SDL_Surface *dest;
    SDL_PixelFormat *df;
    int maxsize = 0;
    int max_opaque_run;
    int max_transl_run = 65535;
    unsigned masksum;
    Uint8 *rlebuf, *dst;
    int (*copy_opaque)(void *, Uint32 *, int,
		       SDL_PixelFormat *, SDL_PixelFormat *);
    int (*copy_transl)(void *, Uint32 *, int,
		       SDL_PixelFormat *, SDL_PixelFormat *);

    dest = surface->map->dst;
    if(!dest)
	return -1;
    df = dest->format;
    if(surface->format->BitsPerPixel != 32)
	return -1;		/* only 32bpp source supported */

    /* find out whether the destination is one we support,
       and determine the max size of the encoded result */
    masksum = df->Rmask | df->Gmask | df->Bmask;
    switch(df->BytesPerPixel) {
    case 2:
	/* 16bpp: only support 565 and 555 formats */
	switch(masksum) {
	case 0xffff:
	    if(df->Gmask == 0x07e0
	       || df->Rmask == 0x07e0 || df->Bmask == 0x07e0) {
		copy_opaque = copy_opaque_16;
		copy_transl = copy_transl_565;
	    } else
		return -1;
	    break;
	case 0x7fff:
	    if(df->Gmask == 0x03e0
	       || df->Rmask == 0x03e0 || df->Bmask == 0x03e0) {
		copy_opaque = copy_opaque_16;
		copy_transl = copy_transl_555;
	    } else
		return -1;
	    break;
	default:
	    return -1;
	}
	max_opaque_run = 255;	/* runs stored as bytes */

	/* worst case is alternating opaque and translucent pixels,
	   with room for alignment padding between lines */
	maxsize = surface->h * (2 + (4 + 2) * (surface->w + 1)) + 2;
	break;
    case 4:
	if(masksum != 0x00ffffff)
	    return -1;		/* requires unused high byte */
	copy_opaque = copy_32;
	copy_transl = copy_32;
	max_opaque_run = 255;	/* runs stored as short ints */

	/* worst case is alternating opaque and translucent pixels */
	maxsize = surface->h * 2 * 4 * (surface->w + 1) + 4;
	break;
    default:
	return -1;		/* anything else unsupported right now */
    }

    maxsize += sizeof(RLEDestFormat);
    rlebuf = (Uint8 *)SDL_malloc(maxsize);
    if(!rlebuf) {
	SDL_OutOfMemory();
	return -1;
    }
    {
	/* save the destination format so we can undo the encoding later */
	RLEDestFormat *r = (RLEDestFormat *)rlebuf;
	r->BytesPerPixel = df->BytesPerPixel;
	r->Rloss = df->Rloss;
	r->Gloss = df->Gloss;
	r->Bloss = df->Bloss;
	r->Rshift = df->Rshift;
	r->Gshift = df->Gshift;
	r->Bshift = df->Bshift;
	r->Ashift = df->Ashift;
	r->Rmask = df->Rmask;
	r->Gmask = df->Gmask;
	r->Bmask = df->Bmask;
	r->Amask = df->Amask;
    }
    dst = rlebuf + sizeof(RLEDestFormat);

    /* Do the actual encoding */
    {
	int x, y;
	int h = surface->h, w = surface->w;
	SDL_PixelFormat *sf = surface->format;
	Uint32 *src = (Uint32 *)surface->pixels;
	Uint8 *lastline = dst;	/* end of last non-blank line */

	/* opaque counts are 8 or 16 bits, depending on target depth */
#define ADD_OPAQUE_COUNTS(n, m)			\
	if(df->BytesPerPixel == 4) {		\
	    ((Uint16 *)dst)[0] = n;		\
	    ((Uint16 *)dst)[1] = m;		\
	    dst += 4;				\
	} else {				\
	    dst[0] = n;				\
	    dst[1] = m;				\
	    dst += 2;				\
	}

	/* translucent counts are always 16 bit */
#define ADD_TRANSL_COUNTS(n, m)		\
	(((Uint16 *)dst)[0] = n, ((Uint16 *)dst)[1] = m, dst += 4)

	for(y = 0; y < h; y++) {
	    int runstart, skipstart;
	    int blankline = 0;
	    /* First encode all opaque pixels of a scan line */
	    x = 0;
	    do {
		int run, skip, len;
		skipstart = x;
		while(x < w && !ISOPAQUE(src[x], sf))
		    x++;
		runstart = x;
		while(x < w && ISOPAQUE(src[x], sf))
		    x++;
		skip = runstart - skipstart;
		if(skip == w)
		    blankline = 1;
		run = x - runstart;
		while(skip > max_opaque_run) {
		    ADD_OPAQUE_COUNTS(max_opaque_run, 0);
		    skip -= max_opaque_run;
		}
		len = MIN(run, max_opaque_run);
		ADD_OPAQUE_COUNTS(skip, len);
		dst += copy_opaque(dst, src + runstart, len, sf, df);
		runstart += len;
		run -= len;
		while(run) {
		    len = MIN(run, max_opaque_run);
		    ADD_OPAQUE_COUNTS(0, len);
		    dst += copy_opaque(dst, src + runstart, len, sf, df);
		    runstart += len;
		    run -= len;
		}
	    } while(x < w);

	    /* Make sure the next output address is 32-bit aligned */
	    dst += (uintptr_t)dst & 2;

	    /* Next, encode all translucent pixels of the same scan line */
	    x = 0;
	    do {
		int run, skip, len;
		skipstart = x;
		while(x < w && !ISTRANSL(src[x], sf))
		    x++;
		runstart = x;
		while(x < w && ISTRANSL(src[x], sf))
		    x++;
		skip = runstart - skipstart;
		blankline &= (skip == w);
		run = x - runstart;
		while(skip > max_transl_run) {
		    ADD_TRANSL_COUNTS(max_transl_run, 0);
		    skip -= max_transl_run;
		}
		len = MIN(run, max_transl_run);
		ADD_TRANSL_COUNTS(skip, len);
		dst += copy_transl(dst, src + runstart, len, sf, df);
		runstart += len;
		run -= len;
		while(run) {
		    len = MIN(run, max_transl_run);
		    ADD_TRANSL_COUNTS(0, len);
		    dst += copy_transl(dst, src + runstart, len, sf, df);
		    runstart += len;
		    run -= len;
		}
		if(!blankline)
		    lastline = dst;
	    } while(x < w);

	    src += surface->pitch >> 2;
	}
	dst = lastline;		/* back up past trailing blank lines */
	ADD_OPAQUE_COUNTS(0, 0);
    }

#undef ADD_OPAQUE_COUNTS
#undef ADD_TRANSL_COUNTS

    /* Now that we have it encoded, release the original pixels */
    if((surface->flags & SDL_PREALLOC) != SDL_PREALLOC
       && (surface->flags & SDL_HWSURFACE) != SDL_HWSURFACE) {
	SDL_free( surface->pixels );
	surface->pixels = NULL;
    }

    /* realloc the buffer to release unused memory */
    {
	Uint8 *p = SDL_realloc(rlebuf, dst - rlebuf);
	if(!p)
	    p = rlebuf;
	surface->map->sw_data->aux_data = p;
    }

    return 0;
}

static Uint32 getpix_8(Uint8 *srcbuf)
{
    return *srcbuf;
}

static Uint32 getpix_16(Uint8 *srcbuf)
{
    return *(Uint16 *)srcbuf;
}

static Uint32 getpix_24(Uint8 *srcbuf)
{
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    return srcbuf[0] + (srcbuf[1] << 8) + (srcbuf[2] << 16);
#else
    return (srcbuf[0] << 16) + (srcbuf[1] << 8) + srcbuf[2];
#endif
}

static Uint32 getpix_32(Uint8 *srcbuf)
{
    return *(Uint32 *)srcbuf;
}

typedef Uint32 (*getpix_func)(Uint8 *);

static getpix_func getpixes[4] = {
    getpix_8, getpix_16, getpix_24, getpix_32
};

static int RLEColorkeySurface(SDL_Surface *surface)
{
        Uint8 *rlebuf, *dst;
	int maxn;
	int y;
	Uint8 *srcbuf, *lastline;
	int maxsize = 0;
	int bpp = surface->format->BytesPerPixel;
	getpix_func getpix;
	Uint32 ckey, rgbmask;
	int w, h;

	/* calculate the worst case size for the compressed surface */
	switch(bpp) {
	case 1:
	    /* worst case is alternating opaque and transparent pixels,
	       starting with an opaque pixel */
	    maxsize = surface->h * 3 * (surface->w / 2 + 1) + 2;
	    break;
	case 2:
	case 3:
	    /* worst case is solid runs, at most 255 pixels wide */
	    maxsize = surface->h * (2 * (surface->w / 255 + 1)
				    + surface->w * bpp) + 2;
	    break;
	case 4:
	    /* worst case is solid runs, at most 65535 pixels wide */
	    maxsize = surface->h * (4 * (surface->w / 65535 + 1)
				    + surface->w * 4) + 4;
	    break;
	}

	rlebuf = (Uint8 *)SDL_malloc(maxsize);
	if ( rlebuf == NULL ) {
		SDL_OutOfMemory();
		return(-1);
	}

	/* Set up the conversion */
	srcbuf = (Uint8 *)surface->pixels;
	maxn = bpp == 4 ? 65535 : 255;
	dst = rlebuf;
	rgbmask = ~surface->format->Amask;
	ckey = surface->format->colorkey & rgbmask;
	lastline = dst;
	getpix = getpixes[bpp - 1];
	w = surface->w;
	h = surface->h;

#define ADD_COUNTS(n, m)			\
	if(bpp == 4) {				\
	    ((Uint16 *)dst)[0] = n;		\
	    ((Uint16 *)dst)[1] = m;		\
	    dst += 4;				\
	} else {				\
	    dst[0] = n;				\
	    dst[1] = m;				\
	    dst += 2;				\
	}

	for(y = 0; y < h; y++) {
	    int x = 0;
	    int blankline = 0;
	    do {
		int run, skip, len;
		int runstart;
		int skipstart = x;

		/* find run of transparent, then opaque pixels */
		while(x < w && (getpix(srcbuf + x * bpp) & rgbmask) == ckey)
		    x++;
		runstart = x;
		while(x < w && (getpix(srcbuf + x * bpp) & rgbmask) != ckey)
		    x++;
		skip = runstart - skipstart;
		if(skip == w)
		    blankline = 1;
		run = x - runstart;

		/* encode segment */
		while(skip > maxn) {
		    ADD_COUNTS(maxn, 0);
		    skip -= maxn;
		}
		len = MIN(run, maxn);
		ADD_COUNTS(skip, len);
		SDL_memcpy(dst, srcbuf + runstart * bpp, len * bpp);
		dst += len * bpp;
		run -= len;
		runstart += len;
		while(run) {
		    len = MIN(run, maxn);
		    ADD_COUNTS(0, len);
		    SDL_memcpy(dst, srcbuf + runstart * bpp, len * bpp);
		    dst += len * bpp;
		    runstart += len;
		    run -= len;
		}
		if(!blankline)
		    lastline = dst;
	    } while(x < w);

	    srcbuf += surface->pitch;
	}
	dst = lastline;		/* back up bast trailing blank lines */
	ADD_COUNTS(0, 0);

#undef ADD_COUNTS

	/* Now that we have it encoded, release the original pixels */
	if((surface->flags & SDL_PREALLOC) != SDL_PREALLOC
	   && (surface->flags & SDL_HWSURFACE) != SDL_HWSURFACE) {
	    SDL_free( surface->pixels );
	    surface->pixels = NULL;
	}

	/* realloc the buffer to release unused memory */
	{
	    /* If realloc returns NULL, the original block is left intact */
	    Uint8 *p = SDL_realloc(rlebuf, dst - rlebuf);
	    if(!p)
		p = rlebuf;
	    surface->map->sw_data->aux_data = p;
	}

	return(0);
}

int SDL_RLESurface(SDL_Surface *surface)
{
	int retcode;

	/* Clear any previous RLE conversion */
	if ( (surface->flags & SDL_RLEACCEL) == SDL_RLEACCEL ) {
		SDL_UnRLESurface(surface, 1);
	}

	/* We don't support RLE encoding of bitmaps */
	if ( surface->format->BitsPerPixel < 8 ) {
		return(-1);
	}

	/* Lock the surface if it's in hardware */
	if ( SDL_MUSTLOCK(surface) ) {
		if ( SDL_LockSurface(surface) < 0 ) {
			return(-1);
		}
	}

	/* Encode */
	if((surface->flags & SDL_SRCCOLORKEY) == SDL_SRCCOLORKEY) {
	    retcode = RLEColorkeySurface(surface);
	} else {
	    if((surface->flags & SDL_SRCALPHA) == SDL_SRCALPHA
	       && surface->format->Amask != 0)
		retcode = RLEAlphaSurface(surface);
	    else
		retcode = -1;	/* no RLE for per-surface alpha sans ckey */
	}

	/* Unlock the surface if it's in hardware */
	if ( SDL_MUSTLOCK(surface) ) {
		SDL_UnlockSurface(surface);
	}

	if(retcode < 0)
	    return -1;

	/* The surface is now accelerated */
	surface->flags |= SDL_RLEACCEL;

	return(0);
}

/*
 * Un-RLE a surface with pixel alpha
 * This may not give back exactly the image before RLE-encoding; all
 * completely transparent pixels will be lost, and colour and alpha depth
 * may have been reduced (when encoding for 16bpp targets).
 */
static SDL_bool UnRLEAlpha(SDL_Surface *surface)
{
    Uint8 *srcbuf;
    Uint32 *dst;
    SDL_PixelFormat *sf = surface->format;
    RLEDestFormat *df = surface->map->sw_data->aux_data;
    int (*uncopy_opaque)(Uint32 *, void *, int,
			 RLEDestFormat *, SDL_PixelFormat *);
    int (*uncopy_transl)(Uint32 *, void *, int,
			 RLEDestFormat *, SDL_PixelFormat *);
    int w = surface->w;
    int bpp = df->BytesPerPixel;

    if(bpp == 2) {
	uncopy_opaque = uncopy_opaque_16;
	uncopy_transl = uncopy_transl_16;
    } else {
	uncopy_opaque = uncopy_transl = uncopy_32;
    }

    surface->pixels = SDL_malloc(surface->h * surface->pitch);
    if ( !surface->pixels ) {
        return(SDL_FALSE);
    }
    /* fill background with transparent pixels */
    SDL_memset(surface->pixels, 0, surface->h * surface->pitch);

    dst = surface->pixels;
    srcbuf = (Uint8 *)(df + 1);
    for(;;) {
	/* copy opaque pixels */
	int ofs = 0;
	do {
	    unsigned run;
	    if(bpp == 2) {
		ofs += srcbuf[0];
		run = srcbuf[1];
		srcbuf += 2;
	    } else {
		ofs += ((Uint16 *)srcbuf)[0];
		run = ((Uint16 *)srcbuf)[1];
		srcbuf += 4;
	    }
	    if(run) {
		srcbuf += uncopy_opaque(dst + ofs, srcbuf, run, df, sf);
		ofs += run;
	    } else if(!ofs)
		return(SDL_TRUE);
	} while(ofs < w);

	/* skip padding if needed */
	if(bpp == 2)
	    srcbuf += (uintptr_t)srcbuf & 2;
	
	/* copy translucent pixels */
	ofs = 0;
	do {
	    unsigned run;
	    ofs += ((Uint16 *)srcbuf)[0];
	    run = ((Uint16 *)srcbuf)[1];
	    srcbuf += 4;
	    if(run) {
		srcbuf += uncopy_transl(dst + ofs, srcbuf, run, df, sf);
		ofs += run;
	    }
	} while(ofs < w);
	dst += surface->pitch >> 2;
    }
    /* Make the compiler happy */
    return(SDL_TRUE);
}

void SDL_UnRLESurface(SDL_Surface *surface, int recode)
{
    if ( (surface->flags & SDL_RLEACCEL) == SDL_RLEACCEL ) {
	surface->flags &= ~SDL_RLEACCEL;

	if(recode && (surface->flags & SDL_PREALLOC) != SDL_PREALLOC
	   && (surface->flags & SDL_HWSURFACE) != SDL_HWSURFACE) {
	    if((surface->flags & SDL_SRCCOLORKEY) == SDL_SRCCOLORKEY) {
		SDL_Rect full;
		unsigned alpha_flag;

		/* re-create the original surface */
		surface->pixels = SDL_malloc(surface->h * surface->pitch);
		if ( !surface->pixels ) {
			/* Oh crap... */
			surface->flags |= SDL_RLEACCEL;
			return;
		}

		/* fill it with the background colour */
		SDL_FillRect(surface, NULL, surface->format->colorkey);

		/* now render the encoded surface */
		full.x = full.y = 0;
		full.w = surface->w;
		full.h = surface->h;
		alpha_flag = surface->flags & SDL_SRCALPHA;
		surface->flags &= ~SDL_SRCALPHA; /* opaque blit */
		SDL_RLEBlit(surface, &full, surface, &full);
		surface->flags |= alpha_flag;
	    } else {
		if ( !UnRLEAlpha(surface) ) {
		    /* Oh crap... */
		    surface->flags |= SDL_RLEACCEL;
		    return;
		}
	    }
	}

	if ( surface->map && surface->map->sw_data->aux_data ) {
	    SDL_free(surface->map->sw_data->aux_data);
	    surface->map->sw_data->aux_data = NULL;
	}
    }
}


