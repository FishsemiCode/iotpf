/****************************************************************************
 * external/services/iotpf/dtls/numeric.h
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

#ifndef NBIOT_SOURCE_DTLS_NUMERIC_H_
#define NBIOT_SOURCE_DTLS_NUMERIC_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef min
#define min(A,B) ((A) <= (B) ? (A) : (B))
#endif

#ifndef max
#define max(A,B) ((A) < (B) ? (B) : (A))
#endif

/* this one is for consistency... */
static inline int dtls_int_to_uint8(unsigned char *field, uint8_t value)
{
  field[0] = value & 0xff;
  return 1;
}

static inline int dtls_int_to_uint16(unsigned char *field, uint16_t value)
{
  field[0] = (value >> 8) & 0xff;
  field[1] = value & 0xff;
  return 2;
}

static inline int dtls_int_to_uint24(unsigned char *field, uint32_t value)
{
  field[0] = (value >> 16) & 0xff;
  field[1] = (value >> 8) & 0xff;
  field[2] = value & 0xff;
  return 3;
}

static inline int dtls_int_to_uint32(unsigned char *field, uint32_t value)
{
  field[0] = (value >> 24) & 0xff;
  field[1] = (value >> 16) & 0xff;
  field[2] = (value >> 8) & 0xff;
  field[3] = value & 0xff;
  return 4;
}

static inline int dtls_int_to_uint48(unsigned char *field, uint64_t value)
{
  field[0] = (value >> 40) & 0xff;
  field[1] = (value >> 32) & 0xff;
  field[2] = (value >> 24) & 0xff;
  field[3] = (value >> 16) & 0xff;
  field[4] = (value >> 8) & 0xff;
  field[5] = value & 0xff;
  return 6;
}

static inline int dtls_int_to_uint64(unsigned char *field, uint64_t value)
{
  field[0] = (value >> 56) & 0xff;
  field[1] = (value >> 48) & 0xff;
  field[2] = (value >> 40) & 0xff;
  field[3] = (value >> 32) & 0xff;
  field[4] = (value >> 24) & 0xff;
  field[5] = (value >> 16) & 0xff;
  field[6] = (value >> 8) & 0xff;
  field[7] = value & 0xff;
  return 8;
}

static inline uint8_t dtls_uint8_to_int(const unsigned char *field)
{
  return (uint8_t)field[0];
}

static inline uint16_t dtls_uint16_to_int(const unsigned char *field)
{
  return ((uint16_t)field[0] << 8) | (uint16_t)field[1];
}

static inline uint32_t dtls_uint24_to_int(const unsigned char *field)
{
  return ((uint32_t)field[0] << 16)
    | ((uint32_t)field[1] << 8)
    | (uint32_t)field[2];
}

static inline uint32_t dtls_uint32_to_int(const unsigned char *field)
{
  return ((uint32_t)field[0] << 24)
    | ((uint32_t)field[1] << 16)
    | ((uint32_t)field[2] << 8)
    | (uint32_t)field[3];
}

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* NBIOT_SOURCE_DTLS_NUMERIC_H_ */