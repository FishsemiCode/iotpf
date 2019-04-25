/****************************************************************************
 * external/services/iotpf/dtls/crypto.c
 *
 *     Copyright (C) 2019 FishSemi Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/


#include "netq.h"
#include "prng.h"
#include "dtls_debug.h"
#include "crypto.h"
#include "numeric.h"
#include "cis_internals.h"

#define HMAC_UPDATE_SEED(Context,Seed,Length) \
if ( Seed ) \
    dtls_hmac_update( Context, (Seed), (Length) )

static dtls_cipher_context_t cipher_context;

static inline dtls_cipher_context_t *dtls_cipher_context_get(void)
{
  return &cipher_context;
}

static inline void dtls_cipher_context_release(void)
{
  /* nothing */
}

static inline dtls_handshake_parameters_t *dtls_handshake_malloc(void)
{
  return (dtls_handshake_parameters_t*)cis_malloc( sizeof(dtls_handshake_parameters_t));
}

static inline void dtls_handshake_dealloc(dtls_handshake_parameters_t *handshake)
{
  cis_free(handshake);
}

static inline dtls_security_parameters_t *dtls_security_malloc(void)
{
  return (dtls_security_parameters_t*)cis_malloc(sizeof(dtls_security_parameters_t));
}

static inline void dtls_security_dealloc(dtls_security_parameters_t *security)
{
  cis_free(security);
}

dtls_handshake_parameters_t *dtls_handshake_new(void)
{
  dtls_handshake_parameters_t *handshake;

  handshake = dtls_handshake_malloc( );
  if (!handshake)
    {
      dtls_crit("can not allocate a handshake struct\n");
      return NULL;
    }

  cis_memset(handshake, 0, sizeof(dtls_handshake_parameters_t));//dtls_handshake_parameters_t

  if (handshake)
    {
      /* initialize the handshake hash wrt. the hard-coded DTLS version */
      dtls_debug("DTLSv12: initialize HASH_SHA256\n");
      /* TLS 1.2:  PRF(secret, label, seed) = P_<hash>(secret, label + seed) */
      /* FIXME: we use the default SHA256 here, might need to support other
      hash functions as well */
      dtls_hash_init(&handshake->hs_state.hs_hash);
    }
  return handshake;
}

void dtls_handshake_free(dtls_handshake_parameters_t *handshake)
{
  if (!handshake)
    {
      return;
    }

  netq_delete_all(handshake->reorder_queue);
  dtls_handshake_dealloc(handshake);
}

dtls_security_parameters_t *dtls_security_new(void)
{
  dtls_security_parameters_t *security;

  security = dtls_security_malloc();
  if (!security)
    {
      dtls_crit("can not allocate a security struct\n");
      return NULL;
    }

  cis_memset(security, 0, sizeof(*security));

  if (security)
    {
      security->cipher = TLS_NULL_WITH_NULL_NULL;
      security->compression = TLS_COMPRESSION_NULL;
    }
  return security;
}

void dtls_security_free(dtls_security_parameters_t *security)
{
  if (!security)
    {
      return;
    }

  dtls_security_dealloc(security);
}

size_t dtls_p_hash(dtls_hashfunc_t h,
                    const unsigned char *key,
                    size_t keylen,
                    const unsigned char *label,
                    size_t labellen,
                    const unsigned char *random1,
                    size_t random1len,
                    const unsigned char *random2,
                    size_t random2len,
                    unsigned char *buf,
                    size_t buflen)
{
  dtls_hmac_context_t *hmac_a, *hmac_p;

  unsigned char A[DTLS_HMAC_DIGEST_SIZE];
  unsigned char tmp[DTLS_HMAC_DIGEST_SIZE];
  size_t dlen;			/* digest length */
  size_t len = 0;			/* result length */

  hmac_a = dtls_hmac_new(key, keylen);
  if (!hmac_a)
    {
      return 0;
    }

  /* calculate A(1) from A(0) == seed */
  HMAC_UPDATE_SEED(hmac_a, label, labellen);
  HMAC_UPDATE_SEED(hmac_a, random1, random1len);
  HMAC_UPDATE_SEED(hmac_a, random2, random2len);

  dlen = dtls_hmac_finalize(hmac_a, A);

  hmac_p = dtls_hmac_new(key, keylen);
  if (!hmac_p)
    {
      goto error;
    }

  while (len + dlen < buflen)
    {
      /* FIXME: rewrite loop to avoid superflous call to dtls_hmac_init() */
      dtls_hmac_init(hmac_p, key, keylen);
      dtls_hmac_update(hmac_p, A, dlen);

       HMAC_UPDATE_SEED(hmac_p, label, labellen);
       HMAC_UPDATE_SEED(hmac_p, random1, random1len);
       HMAC_UPDATE_SEED(hmac_p, random2, random2len);

       len += dtls_hmac_finalize(hmac_p, tmp);
       cis_memmove(buf, tmp, dlen);
       buf += dlen;

       /* calculate A(i+1) */
       dtls_hmac_init(hmac_a, key, keylen);
       dtls_hmac_update(hmac_a, A, dlen);
       dtls_hmac_finalize(hmac_a, A);
     }

  dtls_hmac_init(hmac_p, key, keylen);
  dtls_hmac_update(hmac_p, A, dlen);

  HMAC_UPDATE_SEED(hmac_p, label, labellen);
  HMAC_UPDATE_SEED(hmac_p, random1, random1len);
  HMAC_UPDATE_SEED(hmac_p, random2, random2len);

  dtls_hmac_finalize(hmac_p, tmp);
  cis_memmove(buf, tmp, buflen - len);

error:
  dtls_hmac_free(hmac_a);
  dtls_hmac_free(hmac_p);

return buflen;
}

size_t dtls_prf(const unsigned char *key,
                 size_t keylen,
                 const unsigned char *label,
                 size_t labellen,
                 const unsigned char *random1,
                 size_t random1len,
                 const unsigned char *random2,
                 size_t random2len,
                 unsigned char *buf,
                 size_t buflen)
{
  /* Clear the result buffer */
  cis_memset(buf,0, buflen);
  return dtls_p_hash(HASH_SHA256,
    key, keylen,
    label, labellen,
    random1, random1len,
    random2, random2len,
    buf, buflen);
}

void dtls_mac(dtls_hmac_context_t *hmac_ctx,
               const unsigned char *record,
               const unsigned char *packet,
               size_t length,
               unsigned char *buf)
{
  uint16 L;
  dtls_int_to_uint16(L, length);

  if (NULL == hmac_ctx ||
    NULL == record ||
    NULL == packet ||
    NULL == buf)
    {
      return;
    }

  dtls_hmac_update(hmac_ctx, record + 3, sizeof(uint16)+sizeof(uint48));
  dtls_hmac_update(hmac_ctx, record, sizeof(uint8)+sizeof(uint16));
  dtls_hmac_update(hmac_ctx, L, sizeof(uint16));
  dtls_hmac_update(hmac_ctx, packet, length);

  dtls_hmac_finalize(hmac_ctx, buf);
}

static size_t dtls_ccm_encrypt(aes128_ccm_t *ccm_ctx,
                                const unsigned char *src,
                                size_t srclen,
                                unsigned char *buf,
                                unsigned char *nounce,
                                const unsigned char *aad,
                                size_t la)
{
  long int len;

  if (NULL == ccm_ctx ||
    NULL == src ||
    NULL == buf ||
    NULL == nounce ||
    NULL == aad)
    {
      return 0;
    }

  len = dtls_ccm_encrypt_message(&ccm_ctx->ctx,
    8 /* M */,
    max(2, 15 - DTLS_CCM_NONCE_SIZE),
    nounce,
    buf,
    srclen,
    aad,
    la);
  return len;
}

static size_t dtls_ccm_decrypt(aes128_ccm_t *ccm_ctx,
                                const unsigned char *src,
                                size_t srclen,
                                unsigned char *buf,
                                unsigned char *nounce,
                                const unsigned char *aad,
                                size_t la)
{
  long int len;

  if (NULL == ccm_ctx ||
    NULL == src ||
    NULL == buf ||
    NULL == nounce ||
    NULL == aad )
    {
      return 0;
    }

  len = dtls_ccm_decrypt_message(&ccm_ctx->ctx, 8 /* M */,
    max(2, 15 - DTLS_CCM_NONCE_SIZE),
    nounce,
    buf, srclen,
    aad, la);
  return len;
}
#if CIS_ENABLE_PSK
int dtls_psk_pre_master_secret(unsigned char *key, size_t keylen,
			   unsigned char *result, size_t result_len)
{
  unsigned char *p = result;

  if (result_len < (2 * (sizeof(uint16) + keylen)))
    {
      return -1;
    }

  dtls_int_to_uint16(p, keylen);
  p += sizeof(uint16);

  cis_memset(p, 0, keylen);
  p += keylen;

  cis_memcpy(p, result, sizeof(uint16));
  p += sizeof(uint16);

  cis_memcpy(p, key, keylen);

  return 2 * (sizeof(uint16) + keylen);
}
#endif /* DTLS_PSK */

int dtls_encrypt(const unsigned char *src,
                  size_t length,
                  unsigned char *buf,
                  unsigned char *nounce,
                  unsigned char *key,
                  size_t keylen,
                  const unsigned char *aad,
                  size_t la)
{
  int ret;
  dtls_cipher_context_t *ctx = dtls_cipher_context_get();

  ret = rijndael_set_key_enc_only(&ctx->data.ctx, key, 8 * keylen);
  if (ret < 0)
    {
      /* cleanup everything in case the key has the wrong size */
      dtls_warn("cannot set rijndael key\n");
      goto error;
    }

  if (src != buf)
    {
      cis_memmove( buf, src, length );
    }
  ret = dtls_ccm_encrypt(&ctx->data, src, length, buf, nounce, aad, la);

error:
  dtls_cipher_context_release();
  return ret;
}

int dtls_decrypt(const unsigned char *src,
                  size_t length,
                  unsigned char *buf,
                  unsigned char *nounce,
                  unsigned char *key,
                  size_t keylen,
                  const unsigned char *aad,
                  size_t la)
{
  int ret;
  dtls_cipher_context_t *ctx = dtls_cipher_context_get();

  ret = rijndael_set_key_enc_only(&ctx->data.ctx, key, 8 * keylen);
  if (ret < 0)
    {
      /* cleanup everything in case the key has the wrong size */
      dtls_warn("cannot set rijndael key\n");
      goto error;
    }

  if (src != buf)
    {
      cis_memmove(buf, src, length);
    }
  ret = dtls_ccm_decrypt(&ctx->data, src, length, buf, nounce, aad, la);

error:
  dtls_cipher_context_release();
  return ret;
}
