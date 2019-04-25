/****************************************************************************
 * external/services/iotpf/dtls/cis_aes.h
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

#ifndef NBIOT_SOURCE_DTLS_AES_H_
#define NBIOT_SOURCE_DTLS_AES_H_
#include "stdint.h"
#ifdef __cplusplus
extern "C" {
#endif

#define AES_MAXKEYBITS  (256)
#define AES_MAXKEYBYTES (AES_MAXKEYBITS/8)
    /* for 256-bit keys we need 14 rounds for a 128 we only need 10 round */
#define AES_MAXROUNDS   10

/* bergmann: to avoid conflicts with typedefs from certain Contiki platforms,
 * the following type names have been prefixed with "aes_":
*/
typedef unsigned char   u_char;
typedef uint8_t         aes_u8;
typedef uint16_t        aes_u16;
typedef uint32_t        aes_u32;

/* The structure for key information */
typedef struct
{
  int Nr;                          /* key-length-dependent number of rounds */
  aes_u32 ek[4 * (AES_MAXROUNDS + 1)]; /* encrypt key schedule */
} rijndael_ctx;

int rijndael_set_key_enc_only(rijndael_ctx *ctx,
                               const u_char *key,
                               int bits);
void rijndael_encrypt(rijndael_ctx *ctx,
                       const u_char *src,
                       u_char *dst);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* NBIOT_SOURCE_DTLS_AES_H_ */