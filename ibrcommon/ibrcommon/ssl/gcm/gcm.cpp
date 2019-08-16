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
 Issue Date: 13/06/2006

 My thanks to John Viega and David McGrew for their support in developing
 this code and to David for testing it on a big-endain system.
*/

//#ifdef HAVE_CONFIG_H
//#  include <dtn-config.h>
//#endif
#define BSP_ENABLED true

#ifdef BSP_ENABLED

#include "gcm.h"
#include "mode_hdr.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define BLOCK_SIZE      GCM_BLOCK_SIZE      /* block length                 */
#define BLK_ADR_MASK    (BLOCK_SIZE - 1)    /* mask for 'in block' address  */
#define CTR_POS         12

#define inc_ctr(x)  \
    {   int i = BLOCK_SIZE; while(i-- > CTR_POS && !++(ui8_ptr(x)[i])) ; }

ret_type gcm_init_and_key(                      /* initialise mode and set key  */
            const unsigned char key[],          /* the key value                */
            unsigned long key_len,              /* and its length in bytes      */
            gcm_ctx ctx[1])                     /* the mode context             */
{
    memset(ctx->ghash_h, 0, sizeof(ctx->ghash_h));

    /* set the AES key                          */
    aes_encrypt_key(key, key_len, ctx->aes);

    /* compute E(0) (for the hash function)     */
    aes_encrypt(ui8_ptr(ctx->ghash_h), ui8_ptr(ctx->ghash_h), ctx->aes);

#if defined( TABLES_64K )
    init_64k_table(ui8_ptr(ctx->ghash_h), ctx->gf_t64k);
#elif defined( TABLES_8K )
    init_8k_table(ui8_ptr(ctx->ghash_h), ctx->gf_t8k);
#elif defined( TABLES_4K )
    init_4k_table(ui8_ptr(ctx->ghash_h), ctx->gf_t4k);
#elif defined( TABLES_256 )
    init_256_table(ui8_ptr(ctx->ghash_h), ctx->gf_t256);
#endif
    return RETURN_OK;
}

#if defined( TABLES_64K )
#define gf_mul_hh(a, ctx, scr)  gf_mul_64k(a, ctx->gf_t64k, scr)
#elif defined( TABLES_8K )
#define gf_mul_hh(a, ctx, scr)  gf_mul_8k(a, ctx->gf_t8k, scr)
#elif defined( TABLES_4K )
#define gf_mul_hh(a, ctx, scr)  gf_mul_4k(a, ctx->gf_t4k, scr)
#elif defined( TABLES_256 )
#define gf_mul_hh(a, ctx, scr)  gf_mul_256(a, ctx->gf_t256, scr)
#else
#define gf_mul_hh(a, ctx, scr)  ibrdtn_gf_mul(a, ui8_ptr(ctx->ghash_h))
#endif

ret_type gcm_init_message(                      /* initialise a new message     */
            const unsigned char iv[],           /* the initialisation vector    */
            unsigned long iv_len,               /* and its length in bytes      */
            gcm_ctx ctx[1])                     /* the mode context             */
{   uint_32t i, n_pos = 0, scratch[GF_BYTE_LEN >> 2];
    uint_8t *p;

    memset(ctx->ctr_val, 0, BLOCK_SIZE);
    if(iv_len == CTR_POS)
    {
        memcpy(ctx->ctr_val, iv, CTR_POS); ui8_ptr(ctx->ctr_val)[15] = 0x01;
    }
    else
    {   n_pos = iv_len;
        while(n_pos >= BLOCK_SIZE)
        {
            xor_block_aligned(ctx->ctr_val, iv);
            n_pos -= BLOCK_SIZE;
            iv += BLOCK_SIZE;
            gf_mul_hh(ui8_ptr(ctx->ctr_val), ctx, scratch);
        }

        if(n_pos)
        {
            p = ui8_ptr(ctx->ctr_val);
            while(n_pos-- > 0)
                *p++ ^= *iv++;
            gf_mul_hh(ui8_ptr(ctx->ctr_val), ctx, scratch);
        }
        n_pos = (iv_len << 3);
        for(i = BLOCK_SIZE - 1; n_pos; --i, n_pos >>= 8)
            ui8_ptr(ctx->ctr_val)[i] ^= (unsigned char)n_pos;
        gf_mul_hh(ui8_ptr(ctx->ctr_val), ctx, scratch);
    }

    ctx->y0_val = *ui32_ptr(ui8_ptr(ctx->ctr_val) + CTR_POS);
    inc_ctr(ctx->ctr_val);
    memset(ctx->hdr_ghv, 0, BLOCK_SIZE);
    memset(ctx->txt_ghv, 0, BLOCK_SIZE);
    ctx->hdr_cnt = 0;
    ctx->txt_ccnt = ctx->txt_acnt = 0;
    return RETURN_OK;
}

ret_type gcm_auth_header(                       /* authenticate the header      */
            const unsigned char hdr[],          /* the header buffer            */
            unsigned long hdr_len,              /* and its length in bytes      */
            gcm_ctx ctx[1])                     /* the mode context             */
{   uint_32t cnt = 0, b_pos = (uint_32t)ctx->hdr_cnt & BLK_ADR_MASK;
    uint_32t scratch[GF_BYTE_LEN >> 2];

    if(!hdr_len)
        return RETURN_OK;

    if(ctx->hdr_cnt && b_pos == 0)
        gf_mul_hh(ui8_ptr(ctx->hdr_ghv), ctx, scratch);

    while(cnt < hdr_len && (b_pos & BUF_ADRMASK))
        ui8_ptr(ctx->hdr_ghv)[b_pos++] ^= hdr[cnt++];
    
    if(!(b_pos & BUF_ADRMASK) && !((hdr + cnt - ui8_ptr(ctx->hdr_ghv)) & BUF_ADRMASK))
    {
        while(cnt + BUF_INC <= hdr_len && b_pos <= BLOCK_SIZE - BUF_INC)
        {
            *unit_ptr(ui8_ptr(ctx->hdr_ghv) + b_pos) ^= *unit_ptr(hdr + cnt);
            cnt += BUF_INC; b_pos += BUF_INC;
        }
        
        while(cnt + BLOCK_SIZE <= hdr_len)
        {
            gf_mul_hh(ui8_ptr(ctx->hdr_ghv), ctx, scratch);
            xor_block_aligned(ctx->hdr_ghv, hdr + cnt);
            cnt += BLOCK_SIZE;
        }
    }
    else
    {
        while(cnt < hdr_len && b_pos < BLOCK_SIZE)
            ui8_ptr(ctx->hdr_ghv)[b_pos++] ^= hdr[cnt++];

        while(cnt + BLOCK_SIZE <= hdr_len)
        {
            gf_mul_hh(ui8_ptr(ctx->hdr_ghv), ctx, scratch);
            xor_block(ctx->hdr_ghv, hdr + cnt);
            cnt += BLOCK_SIZE;
        }
    }

    while(cnt < hdr_len)
    {
        if(b_pos == BLOCK_SIZE)
        {
            gf_mul_hh(ui8_ptr(ctx->hdr_ghv), ctx, scratch);
            b_pos = 0;
        }
        ui8_ptr(ctx->hdr_ghv)[b_pos++] ^= hdr[cnt++];
    }

    ctx->hdr_cnt += cnt;
    return RETURN_OK;
}

ret_type gcm_auth_data(                         /* authenticate ciphertext data */
            const unsigned char data[],         /* the data buffer              */
            unsigned long data_len,             /* and its length in bytes      */
            gcm_ctx ctx[1])                     /* the mode context             */
{   uint_32t cnt = 0, b_pos = (uint_32t)(ctx->txt_acnt & BLK_ADR_MASK);
    uint_32t scratch[GF_BYTE_LEN >> 2];

    if(!data_len)
        return RETURN_OK;

    if(ctx->txt_acnt && b_pos == 0)
        gf_mul_hh(ui8_ptr(ctx->txt_ghv), ctx, scratch);

    while(cnt < data_len && (b_pos & BUF_ADRMASK))
        ui8_ptr(ctx->txt_ghv)[b_pos++] ^= data[cnt++];

    if(!(b_pos & BUF_ADRMASK) && !((data + cnt - ui8_ptr(ctx->txt_ghv)) & BUF_ADRMASK))
    {
        while(cnt + BUF_INC <= data_len && b_pos <= BLOCK_SIZE - BUF_INC)
        {
            *unit_ptr(ui8_ptr(ctx->txt_ghv) + b_pos) ^= *unit_ptr(data + cnt);
            cnt += BUF_INC; b_pos += BUF_INC;
        }

        while(cnt + BLOCK_SIZE <= data_len)
        {
            gf_mul_hh(ui8_ptr(ctx->txt_ghv), ctx, scratch);
            xor_block_aligned(ctx->txt_ghv, data + cnt);
            cnt += BLOCK_SIZE;
        }
    }
    else
    {
        while(cnt < data_len && b_pos < BLOCK_SIZE)
            ui8_ptr(ctx->txt_ghv)[b_pos++] ^= data[cnt++];

        while(cnt + BLOCK_SIZE <= data_len)
        {
            gf_mul_hh(ui8_ptr(ctx->txt_ghv), ctx, scratch);
            xor_block(ctx->txt_ghv, data + cnt);
            cnt += BLOCK_SIZE;
        }
    }

    while(cnt < data_len)
    {
        if(b_pos == BLOCK_SIZE)
        {
            gf_mul_hh(ui8_ptr(ctx->txt_ghv), ctx, scratch);
            b_pos = 0;
        }
        ui8_ptr(ctx->txt_ghv)[b_pos++] ^= data[cnt++];
    }

    ctx->txt_acnt += cnt;
    return RETURN_OK;
}

ret_type gcm_crypt_data(                        /* encrypt or decrypt data      */
            unsigned char data[],               /* the data buffer              */
            unsigned long data_len,             /* and its length in bytes      */
            gcm_ctx ctx[1])                     /* the mode context             */
{   uint_32t cnt = 0, b_pos = (uint_32t)(ctx->txt_ccnt & BLK_ADR_MASK);

    if(!data_len)
        return RETURN_OK;

    if(b_pos == 0)
    {
        aes_encrypt(ui8_ptr(ctx->ctr_val), ui8_ptr(ctx->enc_ctr), ctx->aes);
        inc_ctr(ctx->ctr_val);
    }

    while(cnt < data_len && (b_pos & BUF_ADRMASK))
        data[cnt++] ^= ui8_ptr(ctx->enc_ctr)[b_pos++];

    if(!(b_pos & BUF_ADRMASK) && !((data + cnt - ui8_ptr(ctx->enc_ctr)) & BUF_ADRMASK))
    {
        while(cnt + BUF_INC <= data_len && b_pos <= BLOCK_SIZE - BUF_INC)
        {
            *unit_ptr(data + cnt) ^= *unit_ptr(ui8_ptr(ctx->enc_ctr) + b_pos);
            cnt += BUF_INC; b_pos += BUF_INC;
        }

        while(cnt + BLOCK_SIZE <= data_len)
        {
            aes_encrypt(ui8_ptr(ctx->ctr_val), ui8_ptr(ctx->enc_ctr), ctx->aes);
            inc_ctr(ctx->ctr_val);
            xor_block_aligned(data + cnt, ctx->enc_ctr);
            cnt += BLOCK_SIZE;
        }
    }
    else
    {
        while(cnt < data_len && b_pos < BLOCK_SIZE)
            data[cnt++] ^= ui8_ptr(ctx->enc_ctr)[b_pos++];

        while(cnt + BLOCK_SIZE <= data_len)
        {
            aes_encrypt(ui8_ptr(ctx->ctr_val), ui8_ptr(ctx->enc_ctr), ctx->aes);
            inc_ctr(ctx->ctr_val);
            xor_block(data + cnt, ctx->enc_ctr);
            cnt += BLOCK_SIZE;
        }
    }

    while(cnt < data_len)
    {
        if(b_pos == BLOCK_SIZE)
        {
            aes_encrypt(ui8_ptr(ctx->ctr_val), ui8_ptr(ctx->enc_ctr), ctx->aes);
            inc_ctr(ctx->ctr_val);
            b_pos = 0;
        }
        data[cnt++] ^= ui8_ptr(ctx->enc_ctr)[b_pos++];
    }

    ctx->txt_ccnt += cnt;
    return RETURN_OK;
}

ret_type gcm_compute_tag(                       /* compute authentication tag   */
            unsigned char tag[],                /* the buffer for the tag       */
            unsigned long tag_len,              /* and its length in bytes      */
            gcm_ctx ctx[1])                     /* the mode context             */
{   uint_32t i, ln, scratch[GF_BYTE_LEN >> 2];
    uint_8t tbuf[BLOCK_SIZE];

    if(ctx->txt_acnt != ctx->txt_ccnt && ctx->txt_ccnt > 0)
        return RETURN_ERROR;

    gf_mul_hh(ui8_ptr(ctx->hdr_ghv), ctx, scratch);
    gf_mul_hh(ui8_ptr(ctx->txt_ghv), ctx, scratch);

#if 1   /* alternative versions of the exponentiation operation */
    if(ctx->hdr_cnt && (ln = (uint_32t)((ctx->txt_acnt + BLOCK_SIZE - 1) / BLOCK_SIZE)))
    {
        memcpy(tbuf, ctx->ghash_h, BLOCK_SIZE);
        for( ; ; )
        {
            if(ln & 1) ibrdtn_gf_mul(ui8_ptr(ctx->hdr_ghv), tbuf);
            if(!(ln >>= 1)) break;
            ibrdtn_gf_mul(tbuf, tbuf);
        }
    }
#else   /* this one seems slower on x86 and x86_64 :-( */
    if(ctx->hdr_cnt && (ln = (uint_32t)((ctx->txt_acnt + BLOCK_SIZE - 1) / BLOCK_SIZE)))
    {
        i = ln | ln >> 1; i |= i >> 2; i |= i >> 4;
        i |= i >> 8; i |= i >> 16; i = i & ~(i >> 1);
        memset(tbuf, 0, BLOCK_SIZE);
        tbuf[0] = 0x80;
        while(i)
        {
            ibrdtn_gf_mul(tbuf, tbuf);
            if(i & ln)
                gf_mul_hh(tbuf, ctx, scratch);
            i >>= 1;
        }
        ibrdtn_gf_mul(ui8_ptr(ctx->hdr_ghv), tbuf);
    }
#endif
    i = BLOCK_SIZE; ln = (uint_32t)(ctx->txt_acnt << 3);
    while(i-- > 0)
    {
        ui8_ptr(ctx->hdr_ghv)[i] ^= ui8_ptr(ctx->txt_ghv)[i] ^ (unsigned char)ln;
        ln = (i == 8 ? (uint_32t)(ctx->hdr_cnt << 3) : ln >> 8);
    }

    gf_mul_hh(ui8_ptr(ctx->hdr_ghv), ctx, scratch);

    *ui32_ptr(ui8_ptr(ctx->ctr_val) + CTR_POS) = ctx->y0_val;
    aes_encrypt(ui8_ptr(ctx->ctr_val), ui8_ptr(ctx->enc_ctr), ctx->aes);
    for(i = 0; i < (unsigned int)tag_len; ++i)
        tag[i] = ui8_ptr(ctx->hdr_ghv)[i] ^ ui8_ptr(ctx->enc_ctr)[i];

    return (ctx->txt_ccnt == ctx->txt_acnt ? RETURN_OK : RETURN_WARN);
}

ret_type gcm_end(                               /* clean up and end operation   */
            gcm_ctx ctx[1])                     /* the mode context             */
{
    memset(ctx, 0, sizeof(gcm_ctx));
    return RETURN_OK;
}

ret_type gcm_encrypt(                           /* encrypt & authenticate data  */
            unsigned char data[],               /* the data buffer              */
            unsigned long data_len,             /* and its length in bytes      */
            gcm_ctx ctx[1])                     /* the mode context             */
{

    gcm_crypt_data(data, data_len, ctx);
    gcm_auth_data(data, data_len, ctx);
    return RETURN_OK;
}

ret_type gcm_decrypt(                           /* authenticate & decrypt data  */
            unsigned char data[],               /* the data buffer              */
            unsigned long data_len,             /* and its length in bytes      */
            gcm_ctx ctx[1])                     /* the mode context             */
{
    gcm_auth_data(data, data_len, ctx);
    gcm_crypt_data(data, data_len, ctx);
    return RETURN_OK;
}

ret_type gcm_encrypt_message(                   /* encrypt an entire message    */
            const unsigned char iv[],           /* the initialisation vector    */
            unsigned long iv_len,               /* and its length in bytes      */
            const unsigned char hdr[],          /* the header buffer            */
            unsigned long hdr_len,              /* and its length in bytes      */
            unsigned char msg[],                /* the message buffer           */
            unsigned long msg_len,              /* and its length in bytes      */
            unsigned char tag[],                /* the buffer for the tag       */
            unsigned long tag_len,              /* and its length in bytes      */
            gcm_ctx ctx[1])                     /* the mode context             */
{
    gcm_init_message(iv, iv_len, ctx);
    gcm_auth_header(hdr, hdr_len, ctx);
    gcm_encrypt(msg, msg_len, ctx);
    return gcm_compute_tag(tag, tag_len, ctx) ? RETURN_ERROR : RETURN_OK;
}

ret_type gcm_decrypt_message(                   /* decrypt an entire message    */
            const unsigned char iv[],           /* the initialisation vector    */
            unsigned long iv_len,               /* and its length in bytes      */
            const unsigned char hdr[],          /* the header buffer            */
            unsigned long hdr_len,              /* and its length in bytes      */
            unsigned char msg[],                /* the message buffer           */
            unsigned long msg_len,              /* and its length in bytes      */
            const unsigned char tag[],          /* the buffer for the tag       */
            unsigned long tag_len,              /* and its length in bytes      */
            gcm_ctx ctx[1])                     /* the mode context             */
{   uint_8t local_tag[BLOCK_SIZE];
    ret_type rr;

    gcm_init_message(iv, iv_len, ctx);
    gcm_auth_header(hdr, hdr_len, ctx);
    gcm_decrypt(msg, msg_len, ctx);
    rr = gcm_compute_tag(local_tag, tag_len, ctx);
    return (rr != RETURN_OK || memcmp(tag, local_tag, tag_len)) ? RETURN_ERROR : RETURN_OK;
}

#if defined(__cplusplus)
}
#endif

#endif /* BSP_ENABLED */
