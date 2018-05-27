#ifndef LIBCRYPTO_COMPAT_H
#define LIBCRYPTO_COMPAT_H

#if OPENSSL_VERSION_NUMBER < 0x10100000L

#include <openssl/dh.h>

void DH_get0_pqg(const DH *dh, const BIGNUM **p, const BIGNUM **q, const BIGNUM **g);
int DH_set0_pqg(DH *dh, BIGNUM *p, BIGNUM *q, BIGNUM *g);
void DH_get0_key(const DH *dh, const BIGNUM **pub_key, const BIGNUM **priv_key);

#endif /* OPENSSL_VERSION_NUMBER */
#endif /* LIBCRYPTO_COMPAT_H */
