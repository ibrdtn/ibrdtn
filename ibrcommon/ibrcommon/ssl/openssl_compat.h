#ifndef OPENSSL_COMPAT_H
#define OPENSSL_COMPAT_H

#include <openssl/crypto.h>
#if OPENSSL_VERSION_NUMBER < 0x10100000L

#include <openssl/evp.h>
#include <openssl/hmac.h>

static inline EVP_MD_CTX * EVP_MD_CTX_new()
{
	EVP_MD_CTX *ctx;

	ctx = (EVP_MD_CTX *) OPENSSL_malloc(sizeof(EVP_MD_CTX));
	EVP_MD_CTX_init(ctx);
        return ctx;
}
#define EVP_MD_CTX_free(c) if (c != NULL) OPENSSL_free(c)

static inline HMAC_CTX * HMAC_CTX_new()
{
        HMAC_CTX *ctx;

        ctx = (HMAC_CTX *) OPENSSL_malloc(sizeof(HMAC_CTX));
        HMAC_CTX_init(ctx);
        return ctx;
}
#define HMAC_CTX_free(c) if (c != NULL) OPENSSL_free(c)

#define BIO_get_data(b) b->ptr
#define BIO_set_data(b, v) b->ptr=v
#define BIO_set_shutdown(b, v) b->shutdown=v
#define BIO_set_init(b, v) b->init=v

#endif /* OPENSSL_VERSION_NUMBER */

#endif /* OPENSSL_COMPAT_H */

