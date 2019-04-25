/****************************************************************************
 * external/services/iotpf/dtls/hmac.c
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

#include "hmac.h"
#include "cis_internals.h"

static inline dtls_hmac_context_t *dtls_hmac_context_new(void)
{
  return (dtls_hmac_context_t*)cis_malloc(sizeof(dtls_hmac_context_t));
}

static inline void dtls_hmac_context_free(dtls_hmac_context_t *ctx)
{
  cis_free(ctx);
}

void dtls_hmac_storage_init(void)
{
  /* add your code here */
}

void dtls_hmac_update(dtls_hmac_context_t *ctx,
                       const unsigned char *input,
                       size_t ilen)
{
  if (NULL == ctx || NULL == input)
    {
      return;
    }

  dtls_hash_update(&ctx->data, input, ilen);
}

dtls_hmac_context_t* dtls_hmac_new(const unsigned char *key,
                                    size_t klen)
{
  dtls_hmac_context_t *ctx;

  ctx = dtls_hmac_context_new();
  if (ctx)
    {
      dtls_hmac_init(ctx, key, klen);
    }

  return ctx;
}

void dtls_hmac_init(dtls_hmac_context_t *ctx,
                     const unsigned char *key,
                     size_t klen)
{
  int i;

  if (NULL == ctx || NULL == key)
    {
      return;
    }

  cis_memset(ctx, 0, sizeof(dtls_hmac_context_t));

  if (klen > DTLS_HMAC_BLOCKSIZE)
    {
      dtls_hash_init(&ctx->data);
      dtls_hash_update(&ctx->data, key, klen);
      dtls_hash_finalize(ctx->pad, &ctx->data);
    }
  else
    {
      cis_memmove(ctx->pad, key, klen);
    }

  /* create ipad: */
  for (i = 0; i < DTLS_HMAC_BLOCKSIZE; ++i)
    {
      ctx->pad[i] ^= 0x36;
    }

  dtls_hash_init(&ctx->data);
  dtls_hmac_update(ctx, ctx->pad, DTLS_HMAC_BLOCKSIZE);

  /* create opad by xor-ing pad[i] with 0x36 ^ 0x5C: */
  for (i = 0; i < DTLS_HMAC_BLOCKSIZE; ++i)
    {
      ctx->pad[i] ^= 0x6A;
    }
}

void dtls_hmac_free(dtls_hmac_context_t *ctx)
{
  if (ctx)
    {
      dtls_hmac_context_free(ctx);
    }
}

int dtls_hmac_finalize(dtls_hmac_context_t *ctx,
                        unsigned char *result)
{
  unsigned char buf[DTLS_HMAC_DIGEST_SIZE];
  size_t len;

  if (NULL == ctx || NULL == result)
    {
      return 0;
    }

  len = dtls_hash_finalize(buf, &ctx->data);

  dtls_hash_init(&ctx->data);
  dtls_hash_update(&ctx->data, ctx->pad, DTLS_HMAC_BLOCKSIZE);
  dtls_hash_update(&ctx->data, buf, len);

  len = dtls_hash_finalize(result, &ctx->data);

  return len;
}