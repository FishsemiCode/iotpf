/****************************************************************************
 * external/services/iotpf/cis_if_com.c
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

#include <nuttx/config.h>

#include <nuttx/serial/pty.h>
#include <sys/select.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <poll.h>
#include <sched.h>
#include <string.h>
#include <strings.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netutils/base64.h>
#include <cis_def.h>
#include <cis_api.h>
#include "at_api.h"

static int g_regStaus = -1;

pthread_mutex_t gRegmutex;
pthread_cond_t gRegcond;

extern void *g_context;



static int cis_g_modem_fd;


bool ciscom_isRegistered(int regStatus)
{
  return regStatus == 1 || regStatus == 5;
}

int ciscom_getRegisteredStaus(void)
{
  return g_regStaus;
}


static bool ciscom_enable_reg_indication(int fd)
{
  int ret = set_ceregindicationstatus(fd, 1);
  if (ret < 0)
    {
      LOGE("%s: set_ceregindicationstatus fail\n", __func__);
      return false;
    }
  return true;
}

void ciscom_handle_cereg(const char *s)
{
  int ret;
  int value;
  int prev_regStatus;
  char *line = (char *)s;
  LOGD("%s: %s\n", __func__, s);
  ret = at_tok_start(&line);
  if (ret < 0)
    {
      return;
    }
  ret = at_tok_nextint(&line, &value);
  if (ret < 0)
    {
      return;
    }
  prev_regStatus = g_regStaus;
  g_regStaus = value;
  if (g_context)
    {
      if (ciscom_isRegistered(value))
        {
          ((struct st_cis_context *)g_context)->netAttached = 1;
        #if CIS_TWO_MCU
          cisat_wakeup_pump();
        #else
          cisapi_wakeup_pump();
        #endif
        }
      else
        {
          ((struct st_cis_context *)g_context)->netAttached = 0;
          if (ciscom_isRegistered(prev_regStatus))
            {
              core_updatePumpState(g_context, PUMP_STATE_DISCONNECTED);
            #if CIS_TWO_MCU
              cisat_wakeup_pump();
            #else
              cisapi_wakeup_pump();
            #endif
            }
        }
    }
}

int ciscom_getImei(char *buffer)
{
  at_spi_imei imei;
  int ret;
  ret = get_imei(cis_g_modem_fd, &imei);
  if (ret < 0)
    {
      LOGE("%s: getImei fail\n", __func__);
      return ret;
    }
  strcpy(buffer, imei.imei);
  return 0;
}

int ciscom_getImsi(char *buffer)
{
  at_api_imsi imsi;
  int ret;
  ret = get_imsi(cis_g_modem_fd, &imsi);
  if (ret < 0)
    {
      LOGE("%s: get_imsi fail\n", __func__);
      return ret;
    }
  strcpy(buffer, imsi.imsi);
  return 0;
}

int ciscom_getCellId(void)
{
  at_api_cellinfo cellinfo;
  int ret;
  ret = get_cellinfo(cis_g_modem_fd, &cellinfo);
  if (ret < 0)
    {
      LOGE("%s: get_imsi fail\n", __func__);
      return ret;
    }
  return cellinfo.cellId;
}


int ciscom_initialize(void)
{
  int ret;
  int fd;

  fd = at_client_open();
  if(fd < 0)
    {
      LOGE("%s: Open client error\n", __func__);
      return -1;
    }

  ret = register_indication(fd, "+CEREG", ciscom_handle_cereg);
  if(ret < 0)
    {
      LOGE("%s: RegisterIndication fail\n", __func__);
      goto clean;
    }
  if (!ciscom_enable_reg_indication(fd))
    {
      LOGE("%s: enable reg indication fail\n", __func__);
      goto clean;
    }

  cis_g_modem_fd = fd;
  return 0;

clean:
  at_client_close(fd);
  return -1;
}

void ciscom_destory(void)
{
  at_client_close(cis_g_modem_fd);
  cis_g_modem_fd = -1;
}

