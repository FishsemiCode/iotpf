/****************************************************************************
 * external/services/iotpf/dtls/dtls_debug.h
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

#ifndef NBIOT_SOURCE_DTLS_DEBUG_H_
#define NBIOT_SOURCE_DTLS_DEBUG_H_

#include "global.h"
#include "cis_log.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CIS_DTLS_LOG
#define dsrv_log(...) LOG_PRINT(__VA_ARGS__)
void dsrv_hexdump_log(const char *name,
                       const unsigned char *buf,
                       size_t length,
                       int extend);
#else
#define dsrv_log(...)
#define dsrv_hexdump_log(...)
#endif

#define dtls_emerg(...)                       dsrv_log(__VA_ARGS__)
#define dtls_alert(...)                       dsrv_log(__VA_ARGS__)
#define dtls_crit(...)                        dsrv_log(__VA_ARGS__)
#define dtls_warn(...)                        dsrv_log(__VA_ARGS__)
#define dtls_notice(...)                      dsrv_log(__VA_ARGS__)
#define dtls_info(...)                        dsrv_log(__VA_ARGS__)
#define dtls_debug(...)                       dsrv_log(__VA_ARGS__)
#define dtls_debug_hexdump(name, buf, length) dsrv_hexdump_log(name, buf, length, 1)
#define dtls_debug_dump(name, buf, length)    dsrv_hexdump_log(name, buf, length, 0)

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* NBIOT_SOURCE_DTLS_DEBUG_H_ */