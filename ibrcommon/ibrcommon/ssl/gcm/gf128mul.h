/*
 ---------------------------------------------------------------------------
 Copyright (c) 1998-2006, Brian Gladman, Worcester, UK. All rights reserved.

 LICENSE TERMS

 The free distribution and use of this software in both source and binary
 form is allowed (with or without changes) provided that:

   1. distributions of this source code include the above copyright
      notice, this list of conditions and the following disclaimer;

   2. distributions in binary form include the above copyright
      notice, this list of conditions and the following disclaimer
      in the documentation and/or other associated materials;

   3. the copyright holder's name is not used to endorse products
      built using this software without specific written permission.

 ALTERNATIVELY, provided that this notice is retained in full, this product
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.

 DISCLAIMER

 This software is provided 'as is' with no explicit or implied warranties
 in respect of its properties, including, but not limited to, correctness
 and/or fitness for purpose.
 ---------------------------------------------------------------------------
 Issue Date: 13/10/2006

 An implementation of field multiplication in Galois Field GF(128)
*/

#ifndef GF128MUL_H
#define GF128MUL_H

#include <stdlib.h>
#include <string.h>

#include "mode_hdr.h"

/*  Table sizes for GF(128) Multiply.  Normally larger tables give 
    higher speed but cache loading might change this. Normally only 
    one table size (or none at all) will be specified here
*/

#if 0
#  define TABLES_64K
#endif
#if 1
#  define TABLES_8K
#endif
#if 0
#  define TABLES_4K
#endif
#if 0
#  define TABLES_256
#endif

/*  Use of inlines is preferred but code blocks can also be expanded inline
    using 'defines'.  But the latter approach will typically generate a LOT
    of code and is not recommended. 
*/
#if 0
#  define USE_INLINES
#endif

/*  Speed critical loops can be unrolled to gain speed but consume more
    memory
*/
#if 0
#  define UNROLL_LOOPS
#endif

/*  Multiply a GF128 field element by x. Field elements are held in arrays
    of bytes in which field bits 8n..8n + 7 are held in byte[n], with lower
    indexed bits placed in the more numerically significant bit positions
    within bytes.

    On little endian machines the bit indexes translate into the bit
    positions within four 32-bit words in the following way

    MS            x[0]           LS  MS            x[1]           LS
    ms   ls ms   ls ms   ls ms   ls  ms   ls ms   ls ms   ls ms   ls
    24...31 16...23 08...15 00...07  56...63 48...55 40...47 32...39

    MS            x[2]           LS  MS            x[3]           LS
    ms   ls ms   ls ms   ls ms   ls  ms   ls ms   ls ms   ls ms   ls
    88...95 80...87 72...79 64...71  120.127 112.119 104.111 96..103

    On big endian machines the bit indexes translate into the bit
    positions within four 32-bit words in the following way

    MS            x[0]           LS  MS            x[1]           LS
    ms   ls ms   ls ms   ls ms   ls  ms   ls ms   ls ms   ls ms   ls
    00...07 08...15 16...23 24...31  32...39 40...47 48...55 56...63

    MS            x[2]           LS  MS            x[3]           LS
    ms   ls ms   ls ms   ls ms   ls  ms   ls ms   ls ms   ls ms   ls
    64...71 72...79 80...87 88...95  96..103 104.111 112.119 120.127
*/

#define GF_BYTE_LEN 16

#if defined( USE_INLINES )
#  if defined( _MSC_VER )
#    define gf_inline __inline
#  elif defined( __GNUC__ ) || defined( __GNU_LIBRARY__ )
#    define gf_inline static inline
#  else
#    define gf_inline static
#  endif
#endif

#if defined(__cplusplus)
extern "C"
{
#endif

/*  These functions multiply a field element x, by x^4 and by x^8 in the 
    polynomial field representation. It uses 32-bit word operations to
    gain speed but compensates for machine endianess and hence works 
    correctly on both styles of machine.
*/
extern const unsigned short gf_tab[256];

#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN

/*  This section is not needed as GF(128) multiplication is now implemented
    but is left in place as it provides a template for an alternative little
    endian implementation approach based on conversion to and from big endian
    format
*/
#if 0

/*  This is a template for mul_x.  The mul_x4 and mul_x8 little endian
    alternative implementations (and their defined versions) follow the 
    big endian functions below in the same way.
*/

gf_inline void mul_x(void *r, const void *x)
{   uint_32t _tt;
    bswap32_block(r, x, 4); 
    _tt = gf_tab[(ui32_ptr(r)[3] << 7) & 0xff];
    ui32_ptr(r)[3] = (ui32_ptr(r)[3] >> 1) | (ui32_ptr(r)[2] << 31);
    ui32_ptr(r)[2] = (ui32_ptr(r)[2] >> 1) | (ui32_ptr(r)[1] << 31);
    ui32_ptr(r)[1] = (ui32_ptr(r)[1] >> 1) | (ui32_ptr(r)[0] << 31);
    ui32_ptr(r)[0] = (ui32_ptr(r)[0] >> 1) ^ bswap_32(_tt);
    bswap32_block(r, r, 4);
}

#endif

#define VERSION_1

#define MSK_80   (0x80 * (unit_cast(BFR_UNIT,-1) / 0xff))
#define MSK_F0   (0xf0 * (unit_cast(BFR_UNIT,-1) / 0xff))

#if defined( USE_INLINES )

#if BFR_UNIT == 64

    gf_inline void mul_x(void *r, const void *x)
    {   uint_64t  _tt = gf_tab[(ui64_ptr(x)[1] >> 49) & MSK_80];

        ui64_ptr(r)[1] =  (ui64_ptr(x)[1] >> 1) & ~MSK_80 | ((ui64_ptr(x)[1] << 15) | (ui64_ptr(x)[0] >> 49)) & MSK_80;
        ui64_ptr(r)[0] = ((ui64_ptr(x)[0] >> 1) & ~MSK_80 |  (ui64_ptr(x)[0] << 15) & MSK_80) ^ _tt;
    }

  #if defined( VERSION_1 )

    gf_inline void mul_x4(void *x)
    {   uint_64t   _tt = gf_tab[(ui64_ptr(x)[1] >> 52) & MSK_F0];

        ui64_ptr(x)[1] =  (ui64_ptr(x)[1] >> 4) & ~MSK_F0 | ((ui64_ptr(x)[1] << 12) | (ui64_ptr(x)[0] >> 52)) & MSK_F0;
        ui64_ptr(x)[0] = ((ui64_ptr(x)[0] >> 4) & ~MSK_F0 |  (ui64_ptr(x)[0] << 12) & MSK_F0) ^ _tt;
    }

  #else

    gf_inline void mul_x4(void *x)
    {   uint_64t _tt = gf_tab[(ui64_ptr(x)[1] >> 52) & 0xf0];
        bswap64_block(x, x, 2);
        ui64_ptr(x)[1] = bswap_64((ui64_ptr(x)[1] >> 4) | (ui64_ptr(x)[0] << 60));
        ui64_ptr(x)[0] = bswap_64((ui64_ptr(x)[0] >> 4)) ^ _tt;
    }

  #endif

    gf_inline void mul_x8(void *x)
    {   uint_64t _tt = gf_tab[ui64_ptr(x)[1] >> 56];
        ui64_ptr(x)[1] = (ui64_ptr(x)[1] << 8) | (ui64_ptr(x)[0] >> 56); 
        ui64_ptr(x)[0] = (ui64_ptr(x)[0] << 8) ^ _tt;
    }

#elif BFR_UNIT == 32

    gf_inline void mul_x(void *r, const void *x)
    {   uint_32t  _tt = gf_tab[(ui32_ptr(x)[3] >> 17) & MSK_80];

        ui32_ptr(r)[3] =  (ui32_ptr(x)[3] >> 1) & ~MSK_80 | ((ui32_ptr(x)[3] << 15) | (ui32_ptr(x)[2] >> 17)) & MSK_80;
        ui32_ptr(r)[2] =  (ui32_ptr(x)[2] >> 1) & ~MSK_80 | ((ui32_ptr(x)[2] << 15) | (ui32_ptr(x)[1] >> 17)) & MSK_80;
        ui32_ptr(r)[1] =  (ui32_ptr(x)[1] >> 1) & ~MSK_80 | ((ui32_ptr(x)[1] << 15) | (ui32_ptr(x)[0] >> 17)) & MSK_80;
        ui32_ptr(r)[0] = ((ui32_ptr(x)[0] >> 1) & ~MSK_80 |  (ui32_ptr(x)[0] << 15) & MSK_80) ^ _tt;
    }

  #if defined( VERSION_1 )

    gf_inline void mul_x4(void *x)
    {   uint_32t   _tt = gf_tab[(ui32_ptr(x)[3] >> 20) & MSK_F0];

        ui32_ptr(x)[3] =  (ui32_ptr(x)[3] >> 4) & ~MSK_F0 | ((ui32_ptr(x)[3] << 12) | (ui32_ptr(x)[2] >> 20)) & MSK_F0;
        ui32_ptr(x)[2] =  (ui32_ptr(x)[2] >> 4) & ~MSK_F0 | ((ui32_ptr(x)[2] << 12) | (ui32_ptr(x)[1] >> 20)) & MSK_F0;
        ui32_ptr(x)[1] =  (ui32_ptr(x)[1] >> 4) & ~MSK_F0 | ((ui32_ptr(x)[1] << 12) | (ui32_ptr(x)[0] >> 20)) & MSK_F0;
        ui32_ptr(x)[0] = ((ui32_ptr(x)[0] >> 4) & ~MSK_F0 |  (ui32_ptr(x)[0] << 12) & MSK_F0) ^ _tt;
    }

  #else

    gf_inline void mul_x4(void *x)
    {   uint_32t _tt = gf_tab[(ui32_ptr(x)[3] >> 20) & 0xf0];
        bswap32_block(x, x, 4);
        ui32_ptr(x)[3] = bswap_32((ui32_ptr(x)[3] >> 4) | (ui32_ptr(x)[2] << 28));
        ui32_ptr(x)[2] = bswap_32((ui32_ptr(x)[2] >> 4) | (ui32_ptr(x)[1] << 28));
        ui32_ptr(x)[1] = bswap_32((ui32_ptr(x)[1] >> 4) | (ui32_ptr(x)[0] << 28));
        ui32_ptr(x)[0] = bswap_32((ui32_ptr(x)[0] >> 4)) ^ _tt;
    }

  #endif

    gf_inline void mul_x8(void *x)
    {   uint_32t   _tt = gf_tab[ui32_ptr(x)[3] >> 24];

        ui32_ptr(x)[3] = (ui32_ptr(x)[3] << 8) | (ui32_ptr(x)[2] >> 24);
        ui32_ptr(x)[2] = (ui32_ptr(x)[2] << 8) | (ui32_ptr(x)[1] >> 24);
        ui32_ptr(x)[1] = (ui32_ptr(x)[1] << 8) | (ui32_ptr(x)[0] >> 24);
        ui32_ptr(x)[0] = (ui32_ptr(x)[0] << 8) ^ _tt;
    }

#else

    gf_inline void mul_x(void *r, const void *x)
    {   uint_8t _tt = ui8_ptr(x)[15] & 1;
        ui8_ptr(r)[15] = (ui8_ptr(x)[15] >> 1) | (ui8_ptr(x)[14] << 7);
        ui8_ptr(r)[14] = (ui8_ptr(x)[14] >> 1) | (ui8_ptr(x)[13] << 7);
        ui8_ptr(r)[13] = (ui8_ptr(x)[13] >> 1) | (ui8_ptr(x)[12] << 7);
        ui8_ptr(r)[12] = (ui8_ptr(x)[12] >> 1) | (ui8_ptr(x)[11] << 7);
        ui8_ptr(r)[11] = (ui8_ptr(x)[11] >> 1) | (ui8_ptr(x)[10] << 7);
        ui8_ptr(r)[10] = (ui8_ptr(x)[10] >> 1) | (ui8_ptr(x)[ 9] << 7);
        ui8_ptr(r)[ 9] = (ui8_ptr(x)[ 9] >> 1) | (ui8_ptr(x)[ 8] << 7);
        ui8_ptr(r)[ 8] = (ui8_ptr(x)[ 8] >> 1) | (ui8_ptr(x)[ 7] << 7);
        ui8_ptr(r)[ 7] = (ui8_ptr(x)[ 7] >> 1) | (ui8_ptr(x)[ 6] << 7);
        ui8_ptr(r)[ 6] = (ui8_ptr(x)[ 6] >> 1) | (ui8_ptr(x)[ 5] << 7);
        ui8_ptr(r)[ 5] = (ui8_ptr(x)[ 5] >> 1) | (ui8_ptr(x)[ 4] << 7);
        ui8_ptr(r)[ 4] = (ui8_ptr(x)[ 4] >> 1) | (ui8_ptr(x)[ 3] << 7);
        ui8_ptr(r)[ 3] = (ui8_ptr(x)[ 3] >> 1) | (ui8_ptr(x)[ 2] << 7);
        ui8_ptr(r)[ 2] = (ui8_ptr(x)[ 2] >> 1) | (ui8_ptr(x)[ 1] << 7);
        ui8_ptr(r)[ 1] = (ui8_ptr(x)[ 1] >> 1) | (ui8_ptr(x)[ 0] << 7);
        ui8_ptr(r)[ 0] = (ui8_ptr(x)[ 0] >> 1) ^ (_tt ? 0xe1 : 0x00);
    }

    gf_inline void mul_x4(void *x)
    {   uint_16t _tt = gf_tab[(ui8_ptr(x)[15] << 4) & 0xff];
        ui8_ptr(x)[15] =  (ui8_ptr(x)[15] >> 4) | (ui8_ptr(x)[14] << 4);
        ui8_ptr(x)[14] =  (ui8_ptr(x)[14] >> 4) | (ui8_ptr(x)[13] << 4);
        ui8_ptr(x)[13] =  (ui8_ptr(x)[13] >> 4) | (ui8_ptr(x)[12] << 4);
        ui8_ptr(x)[12] =  (ui8_ptr(x)[12] >> 4) | (ui8_ptr(x)[11] << 4);
        ui8_ptr(x)[11] =  (ui8_ptr(x)[11] >> 4) | (ui8_ptr(x)[10] << 4);
        ui8_ptr(x)[10] =  (ui8_ptr(x)[10] >> 4) | (ui8_ptr(x)[ 9] << 4);
        ui8_ptr(x)[ 9] =  (ui8_ptr(x)[ 9] >> 4) | (ui8_ptr(x)[ 8] << 4);
        ui8_ptr(x)[ 8] =  (ui8_ptr(x)[ 8] >> 4) | (ui8_ptr(x)[ 7] << 4);
        ui8_ptr(x)[ 7] =  (ui8_ptr(x)[ 7] >> 4) | (ui8_ptr(x)[ 6] << 4);
        ui8_ptr(x)[ 6] =  (ui8_ptr(x)[ 6] >> 4) | (ui8_ptr(x)[ 5] << 4);
        ui8_ptr(x)[ 5] =  (ui8_ptr(x)[ 5] >> 4) | (ui8_ptr(x)[ 4] << 4);
        ui8_ptr(x)[ 4] =  (ui8_ptr(x)[ 4] >> 4) | (ui8_ptr(x)[ 3] << 4);
        ui8_ptr(x)[ 3] =  (ui8_ptr(x)[ 3] >> 4) | (ui8_ptr(x)[ 2] << 4);
        ui8_ptr(x)[ 2] =  (ui8_ptr(x)[ 2] >> 4) | (ui8_ptr(x)[ 1] << 4);
        ui8_ptr(x)[ 1] = ((ui8_ptr(x)[ 1] >> 4) | (ui8_ptr(x)[ 0] << 4)) ^ (_tt >> 8);
        ui8_ptr(x)[ 0] =  (ui8_ptr(x)[ 0] >> 4) ^ (_tt & 0xff);
    }

    gf_inline void mul_x8(void *x)
    {   uint_16t _tt = gf_tab[ui8_ptr(x)[15]];
        memmove(ui8_ptr(x) + 1, ui8_ptr(x), 15);
        ui8_ptr(x)[1] ^= (_tt >> 8);
        ui8_ptr(x)[0] = (_tt & 0xff);
    }

#endif

#else   /* DEFINES */

#if BFR_UNIT == 64

    #define mul_x(r, x) do { uint_64t  _tt = gf_tab[(ui64_ptr(x)[1] >> 49) & MSK_80]; \
        ui64_ptr(r)[1] =  (ui64_ptr(x)[1] >> 1) & ~MSK_80                             \
            | ((ui64_ptr(x)[1] << 15) | (ui64_ptr(x)[0] >> 49)) & MSK_80;             \
        ui64_ptr(r)[0] = ((ui64_ptr(x)[0] >> 1) & ~MSK_80                             \
            |  (ui64_ptr(x)[0] << 15) & MSK_80) ^ _tt;                                \
    } while(0)

  #if defined( VERSION_1 )

    #define mul_x4(x) do { uint_64t   _tt = gf_tab[(ui64_ptr(x)[1] >> 52) & MSK_F0];  \
        ui64_ptr(x)[1] =  (ui64_ptr(x)[1] >> 4) & ~MSK_F0 | ((ui64_ptr(x)[1] << 12)   \
            | (ui64_ptr(x)[0] >> 52)) & MSK_F0;                                       \
        ui64_ptr(x)[0] = ((ui64_ptr(x)[0] >> 4) & ~MSK_F0                             \
            |  (ui64_ptr(x)[0] << 12) & MSK_F0) ^ _tt;                                \
    } while(0)

  #else

    #define mul_x4(x) do { uint_64t _tt = gf_tab[(ui64_ptr(x)[1] >> 52) & 0xf0];        \
        bswap64_block(x, x, 2);                                                         \
        ui64_ptr(x)[1] = bswap_64((ui64_ptr(x)[1] >> 4) | (ui64_ptr(x)[0] << 60));      \
        ui64_ptr(x)[0] = bswap_64((ui64_ptr(x)[0] >> 4)) ^ _tt;                         \
    } while(0)

  #endif

    #define mul_x8(x) do { uint_64t _tt = gf_tab[ui64_ptr(x)[1] >> 56];     \
        ui64_ptr(x)[1] = (ui64_ptr(x)[1] << 8) | (ui64_ptr(x)[0] >> 56);    \
        ui64_ptr(x)[0] = (ui64_ptr(x)[0] << 8) ^ _tt;                       \
    } while(0)

#elif BFR_UNIT == 32

    #define mul_x(r, x) do { uint_32t  _tt = gf_tab[(ui32_ptr(x)[3] >> 17) & MSK_80]; \
        ui32_ptr(r)[3] =  (ui32_ptr(x)[3] >> 1) & ~MSK_80 | ((ui32_ptr(x)[3] << 15)   \
            | (ui32_ptr(x)[2] >> 17)) & MSK_80;                                       \
        ui32_ptr(r)[2] =  (ui32_ptr(x)[2] >> 1) & ~MSK_80 | ((ui32_ptr(x)[2] << 15)   \
            | (ui32_ptr(x)[1] >> 17)) & MSK_80;                                       \
        ui32_ptr(r)[1] =  (ui32_ptr(x)[1] >> 1) & ~MSK_80 | ((ui32_ptr(x)[1] << 15)   \
            | (ui32_ptr(x)[0] >> 17)) & MSK_80;                                       \
        ui32_ptr(r)[0] = ((ui32_ptr(x)[0] >> 1) & ~MSK_80                             \
            | (ui32_ptr(x)[0] << 15) & MSK_80) ^ _tt;                                 \
    } while(0)

  #if defined( VERSION_1 )

    #define mul_x4(x) do { uint_32t   _tt = gf_tab[(ui32_ptr(x)[3] >> 20) & MSK_F0];  \
        ui32_ptr(x)[3] =  (ui32_ptr(x)[3] >> 4) & ~MSK_F0 | ((ui32_ptr(x)[3] << 12)   \
            | (ui32_ptr(x)[2] >> 20)) & MSK_F0;                                       \
        ui32_ptr(x)[2] =  (ui32_ptr(x)[2] >> 4) & ~MSK_F0 | ((ui32_ptr(x)[2] << 12)   \
            | (ui32_ptr(x)[1] >> 20)) & MSK_F0;                                       \
        ui32_ptr(x)[1] =  (ui32_ptr(x)[1] >> 4) & ~MSK_F0 | ((ui32_ptr(x)[1] << 12)   \
            | (ui32_ptr(x)[0] >> 20)) & MSK_F0;                                       \
        ui32_ptr(x)[0] = ((ui32_ptr(x)[0] >> 4) & ~MSK_F0                             \
            |  (ui32_ptr(x)[0] << 12) & MSK_F0) ^ _tt;                                \
    } while(0)

  #else

    #define mul_x4(x) do { uint_32t _tt = gf_tab[(ui32_ptr(x)[3] >> 20) & 0xf0];    \
        bswap32_block(x, x, 4);                                                     \
        ui32_ptr(x)[3] = bswap_32((ui32_ptr(x)[3] >> 4) | (ui32_ptr(x)[2] << 28));  \
        ui32_ptr(x)[2] = bswap_32((ui32_ptr(x)[2] >> 4) | (ui32_ptr(x)[1] << 28));  \
        ui32_ptr(x)[1] = bswap_32((ui32_ptr(x)[1] >> 4) | (ui32_ptr(x)[0] << 28));  \
        ui32_ptr(x)[0] = bswap_32((ui32_ptr(x)[0] >> 4)) ^ _tt;                     \
    } while(0)

  #endif

#define mul_x8(x) do { uint_32t   _tt = gf_tab[ui32_ptr(x)[3] >> 24];       \
        ui32_ptr(x)[3] = (ui32_ptr(x)[3] << 8) | (ui32_ptr(x)[2] >> 24);    \
        ui32_ptr(x)[2] = (ui32_ptr(x)[2] << 8) | (ui32_ptr(x)[1] >> 24);    \
        ui32_ptr(x)[1] = (ui32_ptr(x)[1] << 8) | (ui32_ptr(x)[0] >> 24);    \
        ui32_ptr(x)[0] = (ui32_ptr(x)[0] << 8) ^ _tt;                       \
    } while(0)

#else

    #define mul_x(r, x) do { uint_8t _tt = ui8_ptr(x)[15] & 1;          \
        ui8_ptr(r)[15] = (ui8_ptr(x)[15] >> 1) | (ui8_ptr(x)[14] << 7); \
        ui8_ptr(r)[14] = (ui8_ptr(x)[14] >> 1) | (ui8_ptr(x)[13] << 7); \
        ui8_ptr(r)[13] = (ui8_ptr(x)[13] >> 1) | (ui8_ptr(x)[12] << 7); \
        ui8_ptr(r)[12] = (ui8_ptr(x)[12] >> 1) | (ui8_ptr(x)[11] << 7); \
        ui8_ptr(r)[11] = (ui8_ptr(x)[11] >> 1) | (ui8_ptr(x)[10] << 7); \
        ui8_ptr(r)[10] = (ui8_ptr(x)[10] >> 1) | (ui8_ptr(x)[ 9] << 7); \
        ui8_ptr(r)[ 9] = (ui8_ptr(x)[ 9] >> 1) | (ui8_ptr(x)[ 8] << 7); \
        ui8_ptr(r)[ 8] = (ui8_ptr(x)[ 8] >> 1) | (ui8_ptr(x)[ 7] << 7); \
        ui8_ptr(r)[ 7] = (ui8_ptr(x)[ 7] >> 1) | (ui8_ptr(x)[ 6] << 7); \
        ui8_ptr(r)[ 6] = (ui8_ptr(x)[ 6] >> 1) | (ui8_ptr(x)[ 5] << 7); \
        ui8_ptr(r)[ 5] = (ui8_ptr(x)[ 5] >> 1) | (ui8_ptr(x)[ 4] << 7); \
        ui8_ptr(r)[ 4] = (ui8_ptr(x)[ 4] >> 1) | (ui8_ptr(x)[ 3] << 7); \
        ui8_ptr(r)[ 3] = (ui8_ptr(x)[ 3] >> 1) | (ui8_ptr(x)[ 2] << 7); \
        ui8_ptr(r)[ 2] = (ui8_ptr(x)[ 2] >> 1) | (ui8_ptr(x)[ 1] << 7); \
        ui8_ptr(r)[ 1] = (ui8_ptr(x)[ 1] >> 1) | (ui8_ptr(x)[ 0] << 7); \
        ui8_ptr(r)[ 0] = (ui8_ptr(x)[ 0] >> 1) ^ (_tt ? 0xe1 : 0x00);   \
    } while(0)

    #define mul_x4(x) do { uint_16t _tt = gf_tab[(ui8_ptr(x)[15] << 4) & 0xff];         \
        ui8_ptr(x)[15] =  (ui8_ptr(x)[15] >> 4) | (ui8_ptr(x)[14] << 4);                \
        ui8_ptr(x)[14] =  (ui8_ptr(x)[14] >> 4) | (ui8_ptr(x)[13] << 4);                \
        ui8_ptr(x)[13] =  (ui8_ptr(x)[13] >> 4) | (ui8_ptr(x)[12] << 4);                \
        ui8_ptr(x)[12] =  (ui8_ptr(x)[12] >> 4) | (ui8_ptr(x)[11] << 4);                \
        ui8_ptr(x)[11] =  (ui8_ptr(x)[11] >> 4) | (ui8_ptr(x)[10] << 4);                \
        ui8_ptr(x)[10] =  (ui8_ptr(x)[10] >> 4) | (ui8_ptr(x)[ 9] << 4);                \
        ui8_ptr(x)[ 9] =  (ui8_ptr(x)[ 9] >> 4) | (ui8_ptr(x)[ 8] << 4);                \
        ui8_ptr(x)[ 8] =  (ui8_ptr(x)[ 8] >> 4) | (ui8_ptr(x)[ 7] << 4);                \
        ui8_ptr(x)[ 7] =  (ui8_ptr(x)[ 7] >> 4) | (ui8_ptr(x)[ 6] << 4);                \
        ui8_ptr(x)[ 6] =  (ui8_ptr(x)[ 6] >> 4) | (ui8_ptr(x)[ 5] << 4);                \
        ui8_ptr(x)[ 5] =  (ui8_ptr(x)[ 5] >> 4) | (ui8_ptr(x)[ 4] << 4);                \
        ui8_ptr(x)[ 4] =  (ui8_ptr(x)[ 4] >> 4) | (ui8_ptr(x)[ 3] << 4);                \
        ui8_ptr(x)[ 3] =  (ui8_ptr(x)[ 3] >> 4) | (ui8_ptr(x)[ 2] << 4);                \
        ui8_ptr(x)[ 2] =  (ui8_ptr(x)[ 2] >> 4) | (ui8_ptr(x)[ 1] << 4);                \
        ui8_ptr(x)[ 1] = ((ui8_ptr(x)[ 1] >> 4) | (ui8_ptr(x)[ 0] << 4)) ^ (_tt >> 8);  \
        ui8_ptr(x)[ 0] =  (ui8_ptr(x)[ 0] >> 4) ^ (_tt & 0xff);                         \
    } while(0)

    #define mul_x8(x) do { uint_16t _tt = gf_tab[ui8_ptr(x)[15]];   \
        memmove(ui8_ptr(x) + 1, ui8_ptr(x), 15);                    \
        ui8_ptr(x)[1] ^= (_tt >> 8);                                \
        ui8_ptr(x)[0] = (_tt & 0xff);                               \
    } while(0)

#endif 

#endif

#elif PLATFORM_BYTE_ORDER == IS_BIG_ENDIAN

#if defined( USE_INLINES )

#if BFR_UNIT == 64

    gf_inline void mul_x(void *r, const void *x)
    {   uint_64t _tt = gf_tab[(ui64_ptr(x)[1] << 7) & 0xff];
        ui64_ptr(r)[1] = (ui64_ptr(x)[1] >> 1) | (ui64_ptr(x)[0] << 63);
        ui64_ptr(r)[0] = (ui64_ptr(x)[0] >> 1) ^ (_tt << 48);
    }

    gf_inline void mul_x4(void *x)
    {   uint_64t _tt = gf_tab[(ui64_ptr(x)[1] << 4) & 0xff];
        ui64_ptr(x)[1] = (ui64_ptr(x)[1] >> 4) | (ui64_ptr(x)[0] << 60);
        ui64_ptr(x)[0] = (ui64_ptr(x)[0] >> 4) ^ (_tt << 48);
    }

    gf_inline void mul_x8(void *x)
    {   uint_64t _tt = gf_tab[ui64_ptr(x)[1] & 0xff];
        ui64_ptr(x)[1] = (ui64_ptr(x)[1] >> 8) | (ui64_ptr(x)[0] << 56);
        ui64_ptr(x)[0] = (ui64_ptr(x)[0] >> 8) ^ (_tt << 48);
    }

#elif BFR_UNIT == 32

    gf_inline void mul_x(void *r, const void *x)
    {   uint_32t _tt = gf_tab[(ui32_ptr(x)[3] << 7) & 0xff];
        ui32_ptr(r)[3] = (ui32_ptr(x)[3] >> 1) | (ui32_ptr(x)[2] << 31);
        ui32_ptr(r)[2] = (ui32_ptr(x)[2] >> 1) | (ui32_ptr(x)[1] << 31);
        ui32_ptr(r)[1] = (ui32_ptr(x)[1] >> 1) | (ui32_ptr(x)[0] << 31);
        ui32_ptr(r)[0] = (ui32_ptr(x)[0] >> 1) ^ (_tt << 16);
    }

    gf_inline void mul_x4(void *x)
    {   uint_32t _tt = gf_tab[(ui32_ptr(x)[3] << 4) & 0xff];
        ui32_ptr(x)[3] = (ui32_ptr(x)[3] >> 4) | (ui32_ptr(x)[2] << 28);
        ui32_ptr(x)[2] = (ui32_ptr(x)[2] >> 4) | (ui32_ptr(x)[1] << 28);
        ui32_ptr(x)[1] = (ui32_ptr(x)[1] >> 4) | (ui32_ptr(x)[0] << 28);
        ui32_ptr(x)[0] = (ui32_ptr(x)[0] >> 4) ^ (_tt << 16);
    }

    gf_inline void mul_x8(void *x)
    {   uint_32t _tt = gf_tab[ui32_ptr(x)[3] & 0xff];
        ui32_ptr(x)[3] = (ui32_ptr(x)[3] >> 8) | (ui32_ptr(x)[2] << 24);
        ui32_ptr(x)[2] = (ui32_ptr(x)[2] >> 8) | (ui32_ptr(x)[1] << 24);
        ui32_ptr(x)[1] = (ui32_ptr(x)[1] >> 8) | (ui32_ptr(x)[0] << 24);
        ui32_ptr(x)[0] = (ui32_ptr(x)[0] >> 8) ^ (_tt << 16);
    }

#else

    gf_inline void mul_x(void *r, const void *x)
    {   uint_8t _tt = ui8_ptr(x)[15] & 1;
        ui8_ptr(r)[15] = (ui8_ptr(x)[15] >> 1) | (ui8_ptr(x)[14] << 7);
        ui8_ptr(r)[14] = (ui8_ptr(x)[14] >> 1) | (ui8_ptr(x)[13] << 7);
        ui8_ptr(r)[13] = (ui8_ptr(x)[13] >> 1) | (ui8_ptr(x)[12] << 7);
        ui8_ptr(r)[12] = (ui8_ptr(x)[12] >> 1) | (ui8_ptr(x)[11] << 7);
        ui8_ptr(r)[11] = (ui8_ptr(x)[11] >> 1) | (ui8_ptr(x)[10] << 7);
        ui8_ptr(r)[10] = (ui8_ptr(x)[10] >> 1) | (ui8_ptr(x)[ 9] << 7);
        ui8_ptr(r)[ 9] = (ui8_ptr(x)[ 9] >> 1) | (ui8_ptr(x)[ 8] << 7);
        ui8_ptr(r)[ 8] = (ui8_ptr(x)[ 8] >> 1) | (ui8_ptr(x)[ 7] << 7);
        ui8_ptr(r)[ 7] = (ui8_ptr(x)[ 7] >> 1) | (ui8_ptr(x)[ 6] << 7);
        ui8_ptr(r)[ 6] = (ui8_ptr(x)[ 6] >> 1) | (ui8_ptr(x)[ 5] << 7);
        ui8_ptr(r)[ 5] = (ui8_ptr(x)[ 5] >> 1) | (ui8_ptr(x)[ 4] << 7);
        ui8_ptr(r)[ 4] = (ui8_ptr(x)[ 4] >> 1) | (ui8_ptr(x)[ 3] << 7);
        ui8_ptr(r)[ 3] = (ui8_ptr(x)[ 3] >> 1) | (ui8_ptr(x)[ 2] << 7);
        ui8_ptr(r)[ 2] = (ui8_ptr(x)[ 2] >> 1) | (ui8_ptr(x)[ 1] << 7);
        ui8_ptr(r)[ 1] = (ui8_ptr(x)[ 1] >> 1) | (ui8_ptr(x)[ 0] << 7);
        ui8_ptr(r)[ 0] = (ui8_ptr(x)[ 0] >> 1) ^ (_tt ? 0xe1 : 0x00);
    }

    gf_inline void mul_x4(void *x)
    {
        uint_16t _tt = gf_tab[(ui8_ptr(x)[15] << 4) & 0xff];
        ui8_ptr(x)[15] =  (ui8_ptr(x)[15] >> 4) | (ui8_ptr(x)[14] << 4);
        ui8_ptr(x)[14] =  (ui8_ptr(x)[14] >> 4) | (ui8_ptr(x)[13] << 4);
        ui8_ptr(x)[13] =  (ui8_ptr(x)[13] >> 4) | (ui8_ptr(x)[12] << 4);
        ui8_ptr(x)[12] =  (ui8_ptr(x)[12] >> 4) | (ui8_ptr(x)[11] << 4);
        ui8_ptr(x)[11] =  (ui8_ptr(x)[11] >> 4) | (ui8_ptr(x)[10] << 4);
        ui8_ptr(x)[10] =  (ui8_ptr(x)[10] >> 4) | (ui8_ptr(x)[ 9] << 4);
        ui8_ptr(x)[ 9] =  (ui8_ptr(x)[ 9] >> 4) | (ui8_ptr(x)[ 8] << 4);
        ui8_ptr(x)[ 8] =  (ui8_ptr(x)[ 8] >> 4) | (ui8_ptr(x)[ 7] << 4);
        ui8_ptr(x)[ 7] =  (ui8_ptr(x)[ 7] >> 4) | (ui8_ptr(x)[ 6] << 4);
        ui8_ptr(x)[ 6] =  (ui8_ptr(x)[ 6] >> 4) | (ui8_ptr(x)[ 5] << 4);
        ui8_ptr(x)[ 5] =  (ui8_ptr(x)[ 5] >> 4) | (ui8_ptr(x)[ 4] << 4);
        ui8_ptr(x)[ 4] =  (ui8_ptr(x)[ 4] >> 4) | (ui8_ptr(x)[ 3] << 4);
        ui8_ptr(x)[ 3] =  (ui8_ptr(x)[ 3] >> 4) | (ui8_ptr(x)[ 2] << 4);
        ui8_ptr(x)[ 2] =  (ui8_ptr(x)[ 2] >> 4) | (ui8_ptr(x)[ 1] << 4);
        ui8_ptr(x)[ 1] = ((ui8_ptr(x)[ 1] >> 4) | (ui8_ptr(x)[ 0] << 4)) ^ (_tt & 0xff);
        ui8_ptr(x)[ 0] =  (ui8_ptr(x)[ 0] >> 4) ^ (_tt >> 8);
    }

    gf_inline void mul_x8(void *x)
    {   uint_16t _tt = gf_tab[ui8_ptr(x)[15]];
        memmove(ui8_ptr(x) + 1, ui8_ptr(x), 15);
        ui8_ptr(x)[1] ^= (_tt & 0xff);
        ui8_ptr(x)[0] = (_tt >> 8);
    }

#endif

#else   /* DEFINES */

#if BFR_UNIT == 64

    #define mul_x(r, x) do { uint_64t _tt = gf_tab[(ui64_ptr(x)[1] << 7) & 0xff];   \
        ui64_ptr(r)[1] = (ui64_ptr(x)[1] >> 1) | (ui64_ptr(x)[0] << 63);            \
        ui64_ptr(r)[0] = (ui64_ptr(x)[0] >> 1) ^ (_tt << 48);                       \
    } while(0)

    #define mul_x4(x) do { uint_64t _tt = gf_tab[(ui64_ptr(x)[1] << 4) & 0xff]; \
        ui64_ptr(x)[1] = (ui64_ptr(x)[1] >> 4) | (ui64_ptr(x)[0] << 60);        \
        ui64_ptr(x)[0] = (ui64_ptr(x)[0] >> 4) ^ (_tt << 48);                   \
    } while(0)

    #define mul_x8(x) do { uint_64t _tt = gf_tab[ui64_ptr(x)[1] & 0xff];    \
        ui64_ptr(x)[1] = (ui64_ptr(x)[1] >> 8) | (ui64_ptr(x)[0] << 56);    \
        ui64_ptr(x)[0] = (ui64_ptr(x)[0] >> 8) ^ (_tt << 48);               \
    } while(0)

#elif BFR_UNIT == 32

    #define mul_x(r, x) do { uint_32t _tt = gf_tab[(ui32_ptr(x)[3] << 7) & 0xff];   \
        ui32_ptr(r)[3] = (ui32_ptr(x)[3] >> 1) | (ui32_ptr(x)[2] << 31);            \
        ui32_ptr(r)[2] = (ui32_ptr(x)[2] >> 1) | (ui32_ptr(x)[1] << 31);            \
        ui32_ptr(r)[1] = (ui32_ptr(x)[1] >> 1) | (ui32_ptr(x)[0] << 31);            \
        ui32_ptr(r)[0] = (ui32_ptr(x)[0] >> 1) ^ (_tt << 16);                       \
    } while(0)

    #define mul_x4(x) do { uint_32t _tt = gf_tab[(ui32_ptr(x)[3] << 4) & 0xff]; \
        ui32_ptr(x)[3] = (ui32_ptr(x)[3] >> 4) | (ui32_ptr(x)[2] << 28);        \
        ui32_ptr(x)[2] = (ui32_ptr(x)[2] >> 4) | (ui32_ptr(x)[1] << 28);        \
        ui32_ptr(x)[1] = (ui32_ptr(x)[1] >> 4) | (ui32_ptr(x)[0] << 28);        \
        ui32_ptr(x)[0] = (ui32_ptr(x)[0] >> 4) ^ (_tt << 16);                   \
    } while(0)

    #define mul_x8(x) do { uint_32t _tt = gf_tab[ui32_ptr(x)[3] & 0xff];    \
        ui32_ptr(x)[3] = (ui32_ptr(x)[3] >> 8) | (ui32_ptr(x)[2] << 24);    \
        ui32_ptr(x)[2] = (ui32_ptr(x)[2] >> 8) | (ui32_ptr(x)[1] << 24);    \
        ui32_ptr(x)[1] = (ui32_ptr(x)[1] >> 8) | (ui32_ptr(x)[0] << 24);    \
        ui32_ptr(x)[0] = (ui32_ptr(x)[0] >> 8) ^ (_tt << 16);               \
    } while(0)

#else

    #define mul_x(r, x) do { uint_8t _tt = ui8_ptr(x)[15] & 1;          \
        ui8_ptr(r)[15] = (ui8_ptr(x)[15] >> 1) | (ui8_ptr(x)[14] << 7); \
        ui8_ptr(r)[14] = (ui8_ptr(x)[14] >> 1) | (ui8_ptr(x)[13] << 7); \
        ui8_ptr(r)[13] = (ui8_ptr(x)[13] >> 1) | (ui8_ptr(x)[12] << 7); \
        ui8_ptr(r)[12] = (ui8_ptr(x)[12] >> 1) | (ui8_ptr(x)[11] << 7); \
        ui8_ptr(r)[11] = (ui8_ptr(x)[11] >> 1) | (ui8_ptr(x)[10] << 7); \
        ui8_ptr(r)[10] = (ui8_ptr(x)[10] >> 1) | (ui8_ptr(x)[ 9] << 7); \
        ui8_ptr(r)[ 9] = (ui8_ptr(x)[ 9] >> 1) | (ui8_ptr(x)[ 8] << 7); \
        ui8_ptr(r)[ 8] = (ui8_ptr(x)[ 8] >> 1) | (ui8_ptr(x)[ 7] << 7); \
        ui8_ptr(r)[ 7] = (ui8_ptr(x)[ 7] >> 1) | (ui8_ptr(x)[ 6] << 7); \
        ui8_ptr(r)[ 6] = (ui8_ptr(x)[ 6] >> 1) | (ui8_ptr(x)[ 5] << 7); \
        ui8_ptr(r)[ 5] = (ui8_ptr(x)[ 5] >> 1) | (ui8_ptr(x)[ 4] << 7); \
        ui8_ptr(r)[ 4] = (ui8_ptr(x)[ 4] >> 1) | (ui8_ptr(x)[ 3] << 7); \
        ui8_ptr(r)[ 3] = (ui8_ptr(x)[ 3] >> 1) | (ui8_ptr(x)[ 2] << 7); \
        ui8_ptr(r)[ 2] = (ui8_ptr(x)[ 2] >> 1) | (ui8_ptr(x)[ 1] << 7); \
        ui8_ptr(r)[ 1] = (ui8_ptr(x)[ 1] >> 1) | (ui8_ptr(x)[ 0] << 7); \
        ui8_ptr(r)[ 0] = (ui8_ptr(x)[ 0] >> 1) ^ (_tt ? 0xe1 : 0x00);   \
    } while(0)

    #define mul_x4(x) do { uint_16t _tt = gf_tab[(ui8_ptr(x)[15] << 4) & 0xff]; \
        ui8_ptr(x)[15] =  (ui8_ptr(x)[15] >> 4) | (ui8_ptr(x)[14] << 4);        \
        ui8_ptr(x)[14] =  (ui8_ptr(x)[14] >> 4) | (ui8_ptr(x)[13] << 4);        \
        ui8_ptr(x)[13] =  (ui8_ptr(x)[13] >> 4) | (ui8_ptr(x)[12] << 4);        \
        ui8_ptr(x)[12] =  (ui8_ptr(x)[12] >> 4) | (ui8_ptr(x)[11] << 4);        \
        ui8_ptr(x)[11] =  (ui8_ptr(x)[11] >> 4) | (ui8_ptr(x)[10] << 4);        \
        ui8_ptr(x)[10] =  (ui8_ptr(x)[10] >> 4) | (ui8_ptr(x)[ 9] << 4);        \
        ui8_ptr(x)[ 9] =  (ui8_ptr(x)[ 9] >> 4) | (ui8_ptr(x)[ 8] << 4);        \
        ui8_ptr(x)[ 8] =  (ui8_ptr(x)[ 8] >> 4) | (ui8_ptr(x)[ 7] << 4);        \
        ui8_ptr(x)[ 7] =  (ui8_ptr(x)[ 7] >> 4) | (ui8_ptr(x)[ 6] << 4);        \
        ui8_ptr(x)[ 6] =  (ui8_ptr(x)[ 6] >> 4) | (ui8_ptr(x)[ 5] << 4);        \
        ui8_ptr(x)[ 5] =  (ui8_ptr(x)[ 5] >> 4) | (ui8_ptr(x)[ 4] << 4);        \
        ui8_ptr(x)[ 4] =  (ui8_ptr(x)[ 4] >> 4) | (ui8_ptr(x)[ 3] << 4);        \
        ui8_ptr(x)[ 3] =  (ui8_ptr(x)[ 3] >> 4) | (ui8_ptr(x)[ 2] << 4);        \
        ui8_ptr(x)[ 2] =  (ui8_ptr(x)[ 2] >> 4) | (ui8_ptr(x)[ 1] << 4);        \
        ui8_ptr(x)[ 1] = ((ui8_ptr(x)[ 1] >> 4) | (ui8_ptr(x)[ 0] << 4)) ^ (_tt & 0xff);    \
        ui8_ptr(x)[ 0] =  (ui8_ptr(x)[ 0] >> 4) ^ (_tt >> 8);                   \
    } while(0)

    #define mul_x8(x) do { uint_16t _tt = gf_tab[ui8_ptr(x)[15]];   \
        memmove(ui8_ptr(x) + 1, ui8_ptr(x), 15);                    \
        ui8_ptr(x)[1] ^= (_tt & 0xff);                              \
        ui8_ptr(x)[0] = (_tt >> 8);                                 \
    } while(0)

#endif

#endif

#else
#  error Platform byte order has not been set. 
#endif

/*  A slow generic version of gf_mul (a = a * b) */

void ibrdtn_gf_mul(void *a, const void* b);

/*  This version uses 64k bytes of table space on the stack.
    A 16 byte buffer has to be multiplied by a 16 byte key
    value in GF(128).  If we consider a GF(128) value in
    the buffer's lowest byte, we can construct a table of
    the 256 16 byte values that result from the 256 values
    of this byte.  This requires 4096 bytes. But we also
    need tables for each of the 16 higher bytes in the
    buffer as well, which makes 64 kbytes in total.
*/

void init_64k_table(unsigned char g[], void *t);
typedef uint_32t            (*gf_t64k)[256][GF_BYTE_LEN >> 2];
#define tab64k(x)           ((gf_t64k)x)
#define xor_64k(i,a,t,r)    xor_block_aligned(r, tab64k(t)[i][a[i]])

#if defined( USE_INLINES )

#if defined( UNROLL_LOOPS )

gf_inline void gf_mul_64k(unsigned char a[], void *t, void *r)
{
    move_block_aligned(r, tab64k(t)[0][a[0]]); xor_64k( 1, a, t, r);
    xor_64k( 2, a, t, r); xor_64k( 3, a, t, r);
    xor_64k( 4, a, t, r); xor_64k( 5, a, t, r);
    xor_64k( 6, a, t, r); xor_64k( 7, a, t, r);
    xor_64k( 8, a, t, r); xor_64k( 9, a, t, r);
    xor_64k(10, a, t, r); xor_64k(11, a, t, r);
    xor_64k(12, a, t, r); xor_64k(13, a, t, r);
    xor_64k(14, a, t, r); xor_64k(15, a, t, r);
    move_block_aligned(a, r);
}

#else

gf_inline void gf_mul_64k(unsigned char a[], void *t, void *r)
{   int i;
    move_block_aligned(r, tab64k(t)[0][a[0]]);
    for(i = 1; i < GF_BYTE_LEN; ++i)
        xor_64k(i, a, t, r);
    move_block_aligned(a, r);
}

#endif

#else

#if !defined( UNROLL_LOOPS )

#define gf_mul_64k(a, t, r) do {                \
    move_block_aligned(r, tab64k(t)[0][a[0]]);  \
    xor_64k( 1, a, t, r);                       \
    xor_64k( 2, a, t, r); xor_64k( 3, a, t, r); \
    xor_64k( 4, a, t, r); xor_64k( 5, a, t, r); \
    xor_64k( 6, a, t, r); xor_64k( 7, a, t, r); \
    xor_64k( 8, a, t, r); xor_64k( 9, a, t, r); \
    xor_64k(10, a, t, r); xor_64k(11, a, t, r); \
    xor_64k(12, a, t, r); xor_64k(13, a, t, r); \
    xor_64k(14, a, t, r); xor_64k(15, a, t, r); \
    move_block_aligned(a, r);                   \
} while(0)

#else

#define gf_mul_64k(a, t, r) do { int i;         \
    move_block_aligned(r, tab64k(t)[0][a[0]]);  \
    for(i = 1; i < GF_BYTE_LEN; ++i)            \
    {   xor_64k(i, a, t, r);                    \
    }                                           \
    move_block_aligned(a, r);                   \
} while(0)

#endif

#endif

/*  This version uses 8k bytes of table space on the stack.
    A 16 byte buffer has to be multiplied by a 16 byte key
    value in GF(128).  If we consider a GF(128) value in
    the buffer's lowest 4-bits, we can construct a table of
    the 16 16 byte values that result from the 16 values
    of these 4 bits. This requires 256 bytes. But we also
    need tables for each of the 32 higher 4 bit groups,
    which makes 8 kbytes in total.
*/

void init_8k_table(unsigned char g[], void *t);

typedef uint_32t    (*gf_t8k)[16][GF_BYTE_LEN >> 2];
#define tab8k(x)    ((gf_t8k)x)
#define xor_8k(i,a,t,r)   \
    xor_block_aligned(r, tab8k(t)[i + i][a[i] & 15]); \
    xor_block_aligned(r, tab8k(t)[i + i + 1][a[i] >> 4])

#if defined( USE_INLINES )

#if defined( UNROLL_LOOPS )

gf_inline void gf_mul_8k(unsigned char a[], void *t, void *r)
{
    move_block_aligned(r, tab8k(t)[0][a[0] & 15]);
    xor_block_aligned(r, tab8k(t)[1][a[0] >> 4]);
                xor_8k( 1, a, t, r); xor_8k( 2, a, t, r); xor_8k( 3, a, t, r);
    xor_8k( 4, a, t, r); xor_8k( 5, a, t, r); xor_8k( 6, a, t, r); xor_8k( 7, a, t, r);
    xor_8k( 8, a, t, r); xor_8k( 9, a, t, r); xor_8k(10, a, t, r); xor_8k(11, a, t, r);
    xor_8k(12, a, t, r); xor_8k(13, a, t, r); xor_8k(14, a, t, r); xor_8k(15, a, t, r);
    move_block_aligned(a, r);
}

#else

gf_inline void gf_mul_8k(unsigned char a[], void *t, void *r)
{   int i;
    memcpy(r, tab8k(t)[0][a[0] & 15], GF_BYTE_LEN);
    xor_block_aligned(r, tab8k(t)[1][a[0] >> 4]);
    for(i = 1; i < GF_BYTE_LEN; ++i)
    {   xor_8k(i, a, t, r);
    }
    memcpy(a, r, GF_BYTE_LEN);
}

#endif

#else

#if defined( UNROLL_LOOPS )

#define gf_mul_8k(a, t, r) do {                     \
    move_block_aligned(r, tab8k(t)[0][a[0] & 15]);  \
    xor_block_aligned(r, tab8k(t)[1][a[0] >> 4]);   \
    xor_8k( 1, a, t, r); xor_8k( 2, a, t, r);       \
    xor_8k( 3, a, t, r); xor_8k( 4, a, t, r);       \
    xor_8k( 5, a, t, r); xor_8k( 6, a, t, r);       \
    xor_8k( 7, a, t, r); xor_8k( 8, a, t, r);       \
    xor_8k( 9, a, t, r); xor_8k(10, a, t, r);       \
    xor_8k(11, a, t, r); xor_8k(12, a, t, r);       \
    xor_8k(13, a, t, r); xor_8k(14, a, t, r);       \
    xor_8k(15, a, t, r); move_block_aligned(a, r);  \
} while(0)

#else

#define gf_mul_8k(a, t, r) do { int i;              \
    memcpy(r, tab8k(t)[0][a[0] & 15], GF_BYTE_LEN); \
    xor_block_aligned(r, tab8k(t)[1][a[0] >> 4]);   \
    for(i = 1; i < GF_BYTE_LEN; ++i)                \
    {   xor_8k(i, a, t, r);                         \
    }                                               \
    memcpy(a, r, GF_BYTE_LEN);                      \
} while(0)

#endif

#endif

/*  This version uses 4k bytes of table space on the stack.
    A 16 byte buffer has to be multiplied by a 16 byte key
    value in GF(128).  If we consider a GF(128) value in a
    single byte, we can construct a table of the 256 16 byte
    values that result from the 256 values of this byte.
    This requires 4096 bytes. If we take the highest byte in
    the buffer and use this table to get the result, we then
    have to multiply by x^120 to get the final value. For the
    next highest byte the result has to be multiplied by x^112
    and so on. But we can do this by accumulating the result
    in an accumulator starting with the result for the top
    byte.  We repeatedly multiply the accumulator value by
    x^8 and then add in (i.e. xor) the 16 bytes of the next
    lower byte in the buffer, stopping when we reach the
    lowest byte. This requires a 4096 byte table.
*/

void init_4k_table(unsigned char g[], void *t);

typedef uint_32t        (*gf_t4k)[GF_BYTE_LEN >> 2];
#define tab4k(x)        ((gf_t4k)x)
#define xor_4k(i,a,t,r) mul_x8(r); xor_block_aligned(r, tab4k(t)[a[i]])

#if defined( USE_INLINES )

#if defined( UNROLL_LOOPS )

gf_inline void gf_mul_4k(unsigned char a[], void *t, void *r)
{
    move_block_aligned(r,tab4k(t)[a[15]]);
    xor_4k(14, a, t, r); xor_4k(13, a, t, r); xor_4k(12, a, t, r);
    xor_4k(11, a, t, r); xor_4k(10, a, t, r); xor_4k( 9, a, t, r);
    xor_4k( 8, a, t, r); xor_4k( 7, a, t, r); xor_4k( 6, a, t, r);
    xor_4k( 5, a, t, r); xor_4k( 4, a, t, r); xor_4k( 3, a, t, r);
    xor_4k( 2, a, t, r); xor_4k( 1, a, t, r); xor_4k( 0, a, t, r);
    move_block_aligned(a, r);
}

#else

gf_inline void gf_mul_4k(unsigned char a[], void *t, void *r)
{   int i = 15;
    move_block_aligned(r,tab4k(t)[a[15]]);
    while(i--)
    {
        xor_4k(i, a, t, r);
    }
    move_block_aligned(a, r);
}

#endif

#else

#if defined( UNROLL_LOOPS )

#define gf_mul_4k(a, t, r) do {                                     \
    move_block_aligned(r,tab4k(t)[a[15]]);                          \
    xor_4k(14, a, t, r); xor_4k(13, a, t, r); xor_4k(12, a, t, r);  \
    xor_4k(11, a, t, r); xor_4k(10, a, t, r); xor_4k( 9, a, t, r);  \
    xor_4k( 8, a, t, r); xor_4k( 7, a, t, r); xor_4k( 6, a, t, r);  \
    xor_4k( 5, a, t, r); xor_4k( 4, a, t, r); xor_4k( 3, a, t, r);  \
    xor_4k( 2, a, t, r); xor_4k( 1, a, t, r); xor_4k( 0, a, t, r);  \
    move_block_aligned(a, r);                                       \
} while(0)

#else

#define gf_mul_4k(a, t, r) do { int i = 15; \
    move_block_aligned(r,tab4k(t)[a[15]]);  \
    while(i--)                              \
    {   xor_4k(i, a, t, r);                 \
    }                                       \
    move_block_aligned(a, r);               \
} while(0)

#endif

#endif

/*  This version uses 256 bytes of table space on the stack.
    A 16 byte buffer has to be multiplied by a 16 byte key
    value in GF(128).  If we consider a GF(128) value in a
    single 4-bit nibble, we can construct a table of the 16
    16 byte  values that result from the 16 values of this
    byte.  This requires 256 bytes. If we take the highest
    4-bit nibble in the buffer and use this table to get the
    result, we then have to multiply by x^124 to get the
    final value. For the next highest byte the result has to
    be multiplied by x^120 and so on. But we can do this by
    accumulating the result in an accumulator starting with
    the result for the top nibble.  We repeatedly multiply
    the accumulator value by x^4 and then add in (i.e. xor)
    the 16 bytes of the next lower nibble in the buffer,
    stopping when we reach the lowest nibblebyte. This uses
    a 256 byte table.
*/

void init_256_table(unsigned char g[], void *t);

typedef uint_32t    (*gf_t256)[GF_BYTE_LEN >> 2];
#define tab256(t)   ((gf_t256)t)
#define xor_256(i,a,t,r)    \
    mul_x4(r); xor_block_aligned(r, tab256(t)[a[i] & 15]);  \
    mul_x4(r); xor_block_aligned(r, tab256(t)[a[i] >> 4])

#if defined( USE_INLINES )

#if defined( UNROLL_LOOPS )

gf_inline void gf_mul_256(unsigned char a[], void *t, void *r)
{
    move_block_aligned(r,tab256(t)[a[15] & 15]); mul_x4(r);
    xor_block_aligned(r, tab256(t)[a[15] >> 4]);
    xor_256(14, a, t, r); xor_256(13, a, t, r);
    xor_256(12, a, t, r); xor_256(11, a, t, r);
    xor_256(10, a, t, r); xor_256( 9, a, t, r);
    xor_256( 8, a, t, r); xor_256( 7, a, t, r);
    xor_256( 6, a, t, r); xor_256( 5, a, t, r);
    xor_256( 4, a, t, r); xor_256( 3, a, t, r);
    xor_256( 2, a, t, r); xor_256( 1, a, t, r);
    xor_256( 0, a, t, r); move_block_aligned(a, r);
}

#else

gf_inline void gf_mul_256(unsigned char a[], void *t, void *r)
{   int i = 15;
    move_block_aligned(r,tab256(t)[a[15] & 15]); mul_x4(r);
    xor_block_aligned(r, tab256(t)[a[15] >> 4]);
    while(i--)
    {   xor_256(i, a, t, r);
    }
    move_block_aligned(a, r);
}

#endif

#else

#if defined( UNROLL_LOOPS )

#define gf_mul_256(a, t, r) do {                            \
    move_block_aligned(r,tab256(t)[a[15] & 15]); mul_x4(r); \
    xor_block_aligned(r, tab256(t)[a[15] >> 4]);            \
    xor_256(14, a, t, r); xor_256(13, a, t, r);             \
    xor_256(12, a, t, r); xor_256(11, a, t, r);             \
    xor_256(10, a, t, r); xor_256( 9, a, t, r);             \
    xor_256( 8, a, t, r); xor_256( 7, a, t, r);             \
    xor_256( 6, a, t, r); xor_256( 5, a, t, r);             \
    xor_256( 4, a, t, r); xor_256( 3, a, t, r);             \
    xor_256( 2, a, t, r); xor_256( 1, a, t, r);             \
    xor_256( 0, a, t, r); move_block_aligned(a, r);         \
} while(0)

#else

#define gf_mul_256(a, t, r) do { int i = 15;                \
    move_block_aligned(r,tab256(t)[a[15] & 15]); mul_x4(r); \
    xor_block_aligned(r, tab256(t)[a[15] >> 4]);            \
    while(i--)                                              \
    {   xor_256(i, a, t, r);                                \
    }                                                       \
    move_block_aligned(a, r);                               \
} while(0)

#endif

#endif

#if defined(__cplusplus)
}
#endif

#endif
