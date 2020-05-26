/****************************************************************************
 * external/services/iotpf/iotpf_user.c
 *
 *     Copyright (C) 2020 FishSemi Inc. All rights reserved.
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

#if CIS_ONE_MCU && CIS_OPERATOR_CTCC

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/select.h>
#include <string.h>

#include "cis_internals.h"
#include "cis_log.h"
#include "cis_api.h"
#include "cis_if_api_ctcc.h"

#include "iotpf_user.h"

#define REPORT_INTERVAL 5  // 5 seconds one report

static pthread_mutex_t g_exit_mutex;
static bool g_exit;

static void stop_user_thread(void)
{
  pthread_mutex_lock(&g_exit_mutex);
  g_exit = true;
  pthread_mutex_unlock(&g_exit_mutex);
}

static void send_data_to_server(user_thread_context_t *utc,
                                void *data, uint32_t data_len)
{
  user_data_info_t udi;

  udi.data = cis_malloc(data_len);
  if (udi.data == NULL)
    {
      return;
    }
  udi.data_len = data_len;
  memcpy(udi.data, data, data_len);
  write(utc->send_pipe_fd[1], &udi, sizeof(user_data_info_t));
}

void cisapi_send_data_to_server(user_thread_context_t *utc)
{
  user_data_info_t udi;

  read(utc->send_pipe_fd[0], &udi, sizeof(user_data_info_t));

  LOGI("\n\nuser_thread: send data %d bytes\n\n", udi.data_len);
  cis_notify_raw(utc->context, udi.data, udi.data_len);
  free(udi.data);
}

void cisapi_recv_data_from_server(user_thread_context_t *utc,
                                  const void *data, uint32_t data_len)
{
  user_data_info_t udi;

  udi.data = cis_malloc(data_len);
  if (udi.data == NULL)
    {
      return;
    }
  udi.data_len = data_len;
  memcpy(udi.data, data, data_len);
  write(utc->recv_pipe_fd[1], &udi, sizeof(user_data_info_t));
}

static void recv_data_from_server(user_thread_context_t *utc)
{
  user_data_info_t udi;
  char *buf = NULL;
  int bufLen = 0;
  int i;

  read(utc->recv_pipe_fd[0], &udi, sizeof(user_data_info_t));

  buf = (char *)malloc(2 * udi.data_len + 1);
  memset(buf, 0x0, 2 * udi.data_len + 1);
  for (i = 0; i < udi.data_len; i++)
    {
      bufLen += snprintf(buf + bufLen, 2 * udi.data_len + 1 - bufLen, "%02X", ((char *)(udi.data))[i]);
    }
  LOGI("\n\nuser_thread: recv data: %d,%s.\n\n", udi.data_len, buf);

  free(buf);
  free(udi.data);
}

void *cisapi_user_send_thread(void *obj)
{
  user_thread_context_t *utc = (user_thread_context_t *)obj;
  uint8_t data[6] = {0x05, 0x31, 0x32, 0x33, 0x34, 0x35};

  pthread_mutex_init(&g_exit_mutex, NULL);
  pthread_setname_np(pthread_self(), "cisapi_user_send_thread");
  pthread_detach(pthread_self());

  LOGI("\n\nentering into user send thread\n\n");

  while (1)
    {
      pthread_mutex_lock(&g_exit_mutex);
      if (g_exit)
        {
          pthread_mutex_unlock(&g_exit_mutex);
          break;
        }
      pthread_mutex_unlock(&g_exit_mutex);
      send_data_to_server(utc, data, sizeof(data));
      sleep(REPORT_INTERVAL);
    }

  return NULL;
}

void *cisapi_user_recv_thread(void *obj)
{
  user_thread_context_t *utc = (user_thread_context_t *)obj;

  pthread_mutex_init(&g_exit_mutex, NULL);
  pthread_setname_np(pthread_self(), "cisapi_user_send_thread");
  pthread_detach(pthread_self());

  LOGI("\n\nentering into user recv thread\n\n");

  while (1)
    {
      pthread_mutex_lock(&g_exit_mutex);
      if (g_exit)
        {
          pthread_mutex_unlock(&g_exit_mutex);
          break;
        }
      pthread_mutex_unlock(&g_exit_mutex);
      recv_data_from_server(utc);
    }

  return NULL;
}

#endif /* CIS_ONE_MCU && CIS_OPERATIOR_CTCC */
