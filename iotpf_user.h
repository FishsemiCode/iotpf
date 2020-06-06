/****************************************************************************
 * external/services/iotpf/iotpf_user.h
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

#ifndef _IOTPF_USER_H_
#define _IOTPF_USER_H_

#include <nuttx/fs/fs.h>

#if CIS_ONE_MCU && CIS_OPERATOR_CTCC

typedef struct user_data_info_s
{
  void *data;
  uint32_t data_len;
} user_data_info_t;

typedef struct user_thread_context_s
{
  void *context;
  int  send_pipe_fd[2];
  int  recv_pipe_fd[2];
  struct file send_pipe_file;
  struct file recv_pipe_file;
} user_thread_context_t;

void cisapi_send_data_to_server(user_thread_context_t *utc);
void cisapi_recv_data_from_server(user_thread_context_t *utc,
                                  const void *data, uint32_t data_len);
void *cisapi_user_send_thread(void *obj);
void *cisapi_user_recv_thread(void *obj);

#endif

#endif /* _IOTPF_USER_H_ */
