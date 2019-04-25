/****************************************************************************
 * external/services/iotpf/dtls/ccm.h
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

#ifndef NBIOT_SOURCE_DTLS_CCM_H_
#define NBIOT_SOURCE_DTLS_CCM_H_
#include <sys/types.h>
#include "cis_aes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* implementation of Counter Mode CBC-MAC, RFC 3610 */
#define DTLS_CCM_BLOCKSIZE  16 /**< size of hmac blocks */
#define DTLS_CCM_MAX        16 /**< max number of bytes in digest */
#define DTLS_CCM_NONCE_SIZE 12 /**< size of nonce */

/**
 * Authenticates and encrypts a message using AES in CCM mode. Please
 * see also RFC 3610 for the meaning of \p M, \p L, \p lm and \p la.
 *
 * \param ctx The initialized rijndael_ctx object to be used for AES operations.
 * \param M   The number of authentication octets.
 * \param L   The number of bytes used to encode the message length.
 * \param N   The nonce value to use. You must provide \c DTLS_CCM_BLOCKSIZE
 *            nonce octets, although only the first \c 16 - \p L are used.
 * \param msg The message to encrypt. The first \p la octets are additional
 *            authentication data that will be cleartext. Note that the
 *            encryption operation modifies the contents of \p msg and adds
 *            \p M bytes MAC. Therefore, the buffer must be at least
 *            \p lm + \p M bytes large.
 * \param lm  The actual length of \p msg.
 * \param aad A pointer to the additional authentication data (can be \c NULL if
 *            \p la is zero).
 * \param la  The number of additional authentication octets (may be zero).
 * \return FIXME
 */
long int dtls_ccm_encrypt_message(rijndael_ctx *ctx,
                                   size_t M,
                                   size_t L,
                                   unsigned char nonce[DTLS_CCM_BLOCKSIZE],
                                   unsigned char *msg,
                                   size_t lm,
                                   const unsigned char *aad,
                                   size_t la);

long int dtls_ccm_decrypt_message(rijndael_ctx *ctx,
                                   size_t M,
                                   size_t L,
                                   unsigned char nonce[DTLS_CCM_BLOCKSIZE],
                                   unsigned char *msg,
                                   size_t lm,
                                   const unsigned char *aad,
                                   size_t la);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* NBIOT_SOURCE_DTLS_CCM_H_ */
