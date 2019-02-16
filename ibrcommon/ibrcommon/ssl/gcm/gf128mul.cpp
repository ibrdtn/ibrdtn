
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
 Issue 31/01/2006

 This file provides fast multiplication in GF(128) as required by several
 cryptographic authentication modes
*/

//#ifdef HAVE_CONFIG_H
//#  include <dtn-config.h>
//#endif
#define BSP_ENABLED true

#ifdef BSP_ENABLED

#include "brg_types.h"
#include "brg_endian.h"
#include "gf128mul.h"

#define gf_dat(q) {\
    q(0x00), q(0x01), q(0x02), q(0x03), q(0x04), q(0x05), q(0x06), q(0x07),\
    q(0x08), q(0x09), q(0x0a), q(0x0b), q(0x0c), q(0x0d), q(0x0e), q(0x0f),\
    q(0x10), q(0x11), q(0x12), q(0x13), q(0x14), q(0x15), q(0x16), q(0x17),\
    q(0x18), q(0x19), q(0x1a), q(0x1b), q(0x1c), q(0x1d), q(0x1e), q(0x1f),\
    q(0x20), q(0x21), q(0x22), q(0x23), q(0x24), q(0x25), q(0x26), q(0x27),\
    q(0x28), q(0x29), q(0x2a), q(0x2b), q(0x2c), q(0x2d), q(0x2e), q(0x2f),\
    q(0x30), q(0x31), q(0x32), q(0x33), q(0x34), q(0x35), q(0x36), q(0x37),\
    q(0x38), q(0x39), q(0x3a), q(0x3b), q(0x3c), q(0x3d), q(0x3e), q(0x3f),\
    q(0x40), q(0x41), q(0x42), q(0x43), q(0x44), q(0x45), q(0x46), q(0x47),\
    q(0x48), q(0x49), q(0x4a), q(0x4b), q(0x4c), q(0x4d), q(0x4e), q(0x4f),\
    q(0x50), q(0x51), q(0x52), q(0x53), q(0x54), q(0x55), q(0x56), q(0x57),\
    q(0x58), q(0x59), q(0x5a), q(0x5b), q(0x5c), q(0x5d), q(0x5e), q(0x5f),\
    q(0x60), q(0x61), q(0x62), q(0x63), q(0x64), q(0x65), q(0x66), q(0x67),\
    q(0x68), q(0x69), q(0x6a), q(0x6b), q(0x6c), q(0x6d), q(0x6e), q(0x6f),\
    q(0x70), q(0x71), q(0x72), q(0x73), q(0x74), q(0x75), q(0x76), q(0x77),\
    q(0x78), q(0x79), q(0x7a), q(0x7b), q(0x7c), q(0x7d), q(0x7e), q(0x7f),\
    q(0x80), q(0x81), q(0x82), q(0x83), q(0x84), q(0x85), q(0x86), q(0x87),\
    q(0x88), q(0x89), q(0x8a), q(0x8b), q(0x8c), q(0x8d), q(0x8e), q(0x8f),\
    q(0x90), q(0x91), q(0x92), q(0x93), q(0x94), q(0x95), q(0x96), q(0x97),\
    q(0x98), q(0x99), q(0x9a), q(0x9b), q(0x9c), q(0x9d), q(0x9e), q(0x9f),\
    q(0xa0), q(0xa1), q(0xa2), q(0xa3), q(0xa4), q(0xa5), q(0xa6), q(0xa7),\
    q(0xa8), q(0xa9), q(0xaa), q(0xab), q(0xac), q(0xad), q(0xae), q(0xaf),\
    q(0xb0), q(0xb1), q(0xb2), q(0xb3), q(0xb4), q(0xb5), q(0xb6), q(0xb7),\
    q(0xb8), q(0xb9), q(0xba), q(0xbb), q(0xbc), q(0xbd), q(0xbe), q(0xbf),\
    q(0xc0), q(0xc1), q(0xc2), q(0xc3), q(0xc4), q(0xc5), q(0xc6), q(0xc7),\
    q(0xc8), q(0xc9), q(0xca), q(0xcb), q(0xcc), q(0xcd), q(0xce), q(0xcf),\
    q(0xd0), q(0xd1), q(0xd2), q(0xd3), q(0xd4), q(0xd5), q(0xd6), q(0xd7),\
    q(0xd8), q(0xd9), q(0xda), q(0xdb), q(0xdc), q(0xdd), q(0xde), q(0xdf),\
    q(0xe0), q(0xe1), q(0xe2), q(0xe3), q(0xe4), q(0xe5), q(0xe6), q(0xe7),\
    q(0xe8), q(0xe9), q(0xea), q(0xeb), q(0xec), q(0xed), q(0xee), q(0xef),\
    q(0xf0), q(0xf1), q(0xf2), q(0xf3), q(0xf4), q(0xf5), q(0xf6), q(0xf7),\
    q(0xf8), q(0xf9), q(0xfa), q(0xfb), q(0xfc), q(0xfd), q(0xfe), q(0xff) }


/*  Given the value i in 0..255 as the byte overflow when a field element
    in GHASH is multipled by x^8, this function will return the values that
    are generated in the lo 16-bit word of the field value by applying the
    modular polynomial. The values lo_byte and hi_byte are returned via the
    macro xp_fun(lo_byte, hi_byte) so that the values can be assembled into
    memory as required by a suitable definition of this macro operating on
    the table above
*/

#if (PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN)
#  define xx(p,q)   0x##q##p    /* assemble in little endian order */
#else
#  define xx(p,q)   0x##p##q    /* assemble in big endian order    */
#endif

#define xda(i) (                                              \
    (i & 0x80 ? xx(e1,00) : 0) ^ (i & 0x40 ? xx(70,80) : 0) ^ \
    (i & 0x20 ? xx(38,40) : 0) ^ (i & 0x10 ? xx(1c,20) : 0) ^ \
    (i & 0x08 ? xx(0e,10) : 0) ^ (i & 0x04 ? xx(07,08) : 0) ^ \
    (i & 0x02 ? xx(03,84) : 0) ^ (i & 0x01 ? xx(01,c2) : 0) )

const unsigned short gf_tab[256] = gf_dat(xda);

void ibrdtn_gf_mul(void *a, const void* b)
{   uint_32t r[GF_BYTE_LEN >> 2], p[8][GF_BYTE_LEN >> 2];
    int i;

    move_block_aligned(p[0], b);
    for(i = 0; i < 7; ++i)
        mul_x(p[i + 1], p[i]);

    memset(r, 0, GF_BYTE_LEN);
    for(i = 0; i < 16; ++i)
    {   unsigned char ch = ((unsigned char*)a)[15 - i];
        if(i) mul_x8(r);

        if(ch & 0x80)
            xor_block_aligned(r, p[0]);
        if(ch & 0x40)
            xor_block_aligned(r, p[1]);
        if(ch & 0x20)
            xor_block_aligned(r, p[2]);
        if(ch & 0x10)
            xor_block_aligned(r, p[3]);
        if(ch & 0x08)
            xor_block_aligned(r, p[4]);
        if(ch & 0x04)
            xor_block_aligned(r, p[5]);
        if(ch & 0x02)
            xor_block_aligned(r, p[6]);
        if(ch & 0x01)
            xor_block_aligned(r, p[7]);
    }
    move_block_aligned(a, r);
}

#if defined( TABLES_64K )

void init_64k_table(unsigned char g[], void *t)
{   int i, j, k;

    memset(t, 0, 16 * 256 * 16);
    for(i = 0; i < GF_BYTE_LEN; ++i)
    {
        if(!i)
        {
            memcpy(tab64k(t)[0][128], g, GF_BYTE_LEN);
            for(j = 64; j > 0; j >>= 1)
                mul_x(tab64k(t)[0][j], tab64k(t)[0][j + j]);
        }
        else
            for(j = 128; j > 0; j >>= 1)
            {
                memcpy(tab64k(t)[i][j], tab64k(t)[i - 1][j], GF_BYTE_LEN);
                mul_x8(tab64k(t)[i][j]);
            }

        for(j = 2; j < 256; j += j)
            for(k = 1; k < j; ++k)
            {
                tab64k(t)[i][j + k][0] = tab64k(t)[i][j][0] ^ tab64k(t)[i][k][0];
                tab64k(t)[i][j + k][1] = tab64k(t)[i][j][1] ^ tab64k(t)[i][k][1];
                tab64k(t)[i][j + k][2] = tab64k(t)[i][j][2] ^ tab64k(t)[i][k][2];
                tab64k(t)[i][j + k][3] = tab64k(t)[i][j][3] ^ tab64k(t)[i][k][3];
            }
    }
}

#endif

#if defined( TABLES_8K )

void init_8k_table(unsigned char g[], void *t)
{   int i, j, k;

    memset(tab8k(t), 0, 32 * 16 * 16);
    for(i = 0; i < 2 * GF_BYTE_LEN; ++i)
    {
        if(i == 0)
        {
            memcpy(tab8k(t)[1][8], g, GF_BYTE_LEN);
            for(j = 4; j > 0; j >>= 1)
                mul_x(tab8k(t)[1][j], tab8k(t)[1][j + j]);

            mul_x(tab8k(t)[0][8], tab8k(t)[1][1]);

            for(j = 4; j > 0; j >>= 1)
                mul_x(tab8k(t)[0][j], tab8k(t)[0][j + j]);
        }
        else if(i > 1)
            for(j = 8; j > 0; j >>= 1)
            {
                memcpy(tab8k(t)[i][j], tab8k(t)[i - 2][j], GF_BYTE_LEN);
                mul_x8(tab8k(t)[i][j]);
            }

        for(j = 2; j < 16; j += j)
            for(k = 1; k < j; ++k)
            {
                tab8k(t)[i][j + k][0] = tab8k(t)[i][j][0] ^ tab8k(t)[i][k][0];
                tab8k(t)[i][j + k][1] = tab8k(t)[i][j][1] ^ tab8k(t)[i][k][1];
                tab8k(t)[i][j + k][2] = tab8k(t)[i][j][2] ^ tab8k(t)[i][k][2];
                tab8k(t)[i][j + k][3] = tab8k(t)[i][j][3] ^ tab8k(t)[i][k][3];
            }
    }
}

#endif

#if defined( TABLES_4K )

void init_4k_table(unsigned char g[], void *t)
{   int j, k;

    memset(tab4k(t), 0, 256 * 16);
    memcpy(tab4k(t)[128], g, GF_BYTE_LEN);
    for(j = 64; j > 0; j >>= 1)
    {
        mul_x(tab4k(t)[j], tab4k(t)[j + j]);
    }

    for(j = 2; j < 256; j += j)
        for(k = 1; k < j; ++k)
        {
            tab4k(t)[j + k][0] = tab4k(t)[j][0] ^ tab4k(t)[k][0];
            tab4k(t)[j + k][1] = tab4k(t)[j][1] ^ tab4k(t)[k][1];
            tab4k(t)[j + k][2] = tab4k(t)[j][2] ^ tab4k(t)[k][2];
            tab4k(t)[j + k][3] = tab4k(t)[j][3] ^ tab4k(t)[k][3];
        }
}

#endif

#if defined( TABLES_256 )

void init_256_table(unsigned char g[], void *t)
{   int j, k;

    memset(tab256(t), 0, 16 * 16);
    memcpy(tab256(t)[8], g, GF_BYTE_LEN);
    for(j = 4; j > 0; j >>= 1)
    {
        mul_x(tab256(t)[j], tab256(t)[j + j]);
    }

    for(j = 2; j < 16; j += j)
        for(k = 1; k < j; ++k)
        {
            tab256(t)[j + k][0] = tab256(t)[j][0] ^ tab256(t)[k][0];
            tab256(t)[j + k][1] = tab256(t)[j][1] ^ tab256(t)[k][1];
            tab256(t)[j + k][2] = tab256(t)[j][2] ^ tab256(t)[k][2];
            tab256(t)[j + k][3] = tab256(t)[j][3] ^ tab256(t)[k][3];
        }
}

#endif

#endif /* BSP_ENABLED */
