/****************************************************************************
 * external/services/iotpf/cis_if_api_ctcc.c
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/select.h>
#include <string.h>

#include "cis_log.h"
#include "cis_api.h"
#include "cis_if_api_ctcc.h"
#include "object_control.h"


#if CIS_ONE_MCU && CIS_OPERATOR_CTCC

static uint8_t config_hex[] =
{
  0x13, 0x00, 0x2E,
  0xf1, 0x00, 0x03,
  0xf2, 0x00, 0x20,
  0x00, 0x00, 0x00, 0x00,

#ifdef CIS_CTWING
  0x00, 0x15 /*host length*/, 0x32, 0x32, 0x31, 0x2e, 0x32, 0x32, 0x39, 0x2e,
  0x32, 0x31, 0x34, 0x2e, 0x32, 0x30, 0x32, 0x3a, 0x35, 0x36, 0x38, 0x33, 0x00 /*host: 221.229.214.202:5683*/,
#else
  0x00, 0x15 /*host length*/, 0x31, 0x38, 0x30, 0x2e, 0x31, 0x30, 0x31, 0x2e,
  0x31, 0x34, 0x37, 0x2e, 0x31, 0x31, 0x35, 0x3a, 0x35, 0x36, 0x38, 0x33, 0x00 /*host: 180.101.147.115:5683*/,
#endif
  0x00, 0x00 /*userdata length*/ /*userdata*/,
  0xf3, 0x00, 0x08,
  0x83 /*log config*/, 0x00, 0xc8 /*LogBufferSize: 200*/,
  0x00, 0x00 /*userdata length*//*userdata*/
};

static pthread_mutex_t g_reg_mutex;
static pthread_cond_t g_reg_cond;
static bool g_reg_status = false;

void *g_context = NULL;

//////////////////////////////////////////////////////////////////////////
//private funcation;

static void cis_api_onEvent(void *context, cis_evt_t eid, void *param)
{
  switch (eid)
    {
      case CIS_EVENT_RESPONSE_FAILED:
        LOGD("cis_on_event response failed mid:%d", (int32_t)param);
        break;
      case CIS_EVENT_NOTIFY_FAILED:
        LOGD("cis_on_event notify failed mid:%d", (int32_t)param);
        break;
      case CIS_EVENT_CONNECT_SUCCESS:
        LOGD("cis_on_event connect success");
        break;
      case CIS_EVENT_CONNECT_FAILED:
        LOGD("cis_on_event connect failed");
        break;
      case CIS_EVENT_REG_SUCCESS:
        pthread_mutex_lock(&g_reg_mutex);
        g_reg_status = true;
        pthread_cond_signal(&g_reg_cond);
        pthread_mutex_unlock(&g_reg_mutex);
        LOGD("cis_on_event reg success");
        break;
      case CIS_EVENT_REG_FAILED:
        LOGD("cis_on_event reg failed");
        break;
      case CIS_EVENT_REG_TIMEOUT:
        LOGD("cis_on_event reg timeout");
        break;
      case CIS_EVENT_UPDATE_NEED:
        LOGD("cis_on_event update needed");
        cis_update_reg(context, 3600, false);
        break;
      case CIS_EVENT_UNREG_DONE:
        pthread_mutex_lock(&g_reg_mutex);
        g_reg_status = false;
        pthread_cond_signal(&g_reg_cond);
        pthread_mutex_unlock(&g_reg_mutex);
        LOGD("cis_on_event unreg success");
        break;
      default:
        LOGD("cis_on_event:%d", eid);
        break;
    }
}

static cis_coapret_t cis_api_onWriteRaw(void *context, const uint8_t *data, uint32_t length)
{
  /*TODO*/
  int i;
  int bufLen = 0;
  char *buf = (char *)malloc(2 * length + 1);
  memset(buf, 0x0, 2 * length + 1);
  for (i = 0; i < length; i++)
    {
      bufLen += snprintf(buf + bufLen, 2 * length + 1 - bufLen, "%02X", data[i]);
    }
  LOGI("cis_api_onWriteRaw :%d,%s\n", length, buf);
  free(buf);
  return CIS_RET_OK;
}


static cis_coapret_t cis_api_onRead(void *context, cis_uri_t *uri, cis_mid_t mid)
{
  int i = 0;
  for (i = 0; i < get_object_callback_mapping_num(); i++)
    {
      if (g_object_callback_mapping[i].onRead == NULL)
        {
          continue;
        }
      if (uri->objectId == g_object_callback_mapping[i].objectId)
        {
          return g_object_callback_mapping[i].onRead(context, uri, mid);
        }
    }

  return CIS_RET_ERROR;
}

static cis_coapret_t cis_api_onWrite(void *context, cis_uri_t *uri, const cis_data_t *value, cis_attrcount_t attrcount, cis_mid_t mid)
{
  int i = 0;
  for (i = 0; i < get_object_callback_mapping_num(); i++)
    {
      if (g_object_callback_mapping[i].onWrite == NULL)
        {
          continue;
        }
      if (uri->objectId == g_object_callback_mapping[i].objectId)
        {
          return g_object_callback_mapping[i].onWrite(context, uri, value, attrcount, mid);
        }
    }

  return CIS_RET_ERROR;
}


static cis_coapret_t cis_api_onExec(void *context, cis_uri_t *uri, const uint8_t *value, uint32_t length, cis_mid_t mid)
{
  int i = 0;
  for (i = 0; i < get_object_callback_mapping_num(); i++)
    {
      if (g_object_callback_mapping[i].onExec == NULL)
        {
          continue;
        }
      if (uri->objectId == g_object_callback_mapping[i].objectId)
        {
          return g_object_callback_mapping[i].onExec(context, uri, value, length, mid);
        }
    }

  return CIS_RET_ERROR;
}


static cis_coapret_t cis_api_onObserve(void *context, cis_uri_t *uri, bool flag, cis_mid_t mid)
{
  int i = 0;
  for (i = 0; i < get_object_callback_mapping_num(); i++)
    {
      if (g_object_callback_mapping[i].onObserve == NULL)
        {
          continue;
        }
      if (uri->objectId == g_object_callback_mapping[i].objectId)
        {
          return g_object_callback_mapping[i].onObserve(context, uri, flag, mid);
        }
    }

  return CIS_RET_ERROR;
}


static void prv_observeNotify(void *contextP)
{
  int i = 0;
  for (i = 0; i < get_object_callback_mapping_num(); i++)
    {
      if (g_object_callback_mapping[i].onObserveNotify == NULL)
        {
          continue;
        }
      g_object_callback_mapping[i].onObserveNotify(contextP);
    }

  return;
}

static cis_ret_t prv_make_sample_data(void *contextP)
{
  uint32_t i;
  cis_ret_t ret = 0;
  const object_callback_mapping *ocm = get_object_callback_mappings();

  for (i = 0; i < get_object_callback_mapping_num(); i++)
    {
      ret = ocm->makeSampleData(contextP);
      if (ret != CIS_RET_OK)
        {
          return ret;
        }
    }

  return CIS_RET_OK;
}

static cis_ret_t prv_clean_sample_data(void *contextP)
{
  int i = 0;
  for (i = 0; i < get_object_callback_mapping_num(); i++)
    {
      if (g_object_callback_mapping[i].onClean == NULL)
        {
          continue;
        }
      g_object_callback_mapping[i].onClean(contextP);
    }

  return CIS_RET_OK;
}

int cisapi_initialize(void)
{
  int ret;
  cis_time_t g_lifetime = 3600;
  cis_callback_ctcc_t callback;
  callback.onRead = cis_api_onRead;
  callback.onWrite = cis_api_onWrite;
  callback.onExec = cis_api_onExec;
  callback.onObserve = cis_api_onObserve;
  callback.onEvent = cis_api_onEvent;
  callback.onWriteRaw = cis_api_onWriteRaw;

  pthread_mutex_init(&g_reg_mutex, NULL);
  pthread_cond_init(&g_reg_cond, NULL);

  LOGD("cisapi_initialize enter\n");

  if (cis_init(&g_context, (void *)config_hex, sizeof(config_hex)) != CIS_RET_OK)
    {
      if (g_context != NULL)
        {
          cis_deinit(&g_context);
        }
      LOGE("cis entry init failed.\n");
      return -1;
    }

  prv_make_sample_data(g_context);

  cis_pump_initialize();

  cis_register_ctcc(g_context, g_lifetime, &callback);

  pthread_mutex_lock(&g_reg_mutex);
  while (!g_reg_status)
    {
      pthread_cond_wait(&g_reg_cond, &g_reg_mutex);
    }
  pthread_mutex_unlock(&g_reg_mutex);

  LOGI("sleep 5 seconds before sending\n");
  sleep(5);

  // test data format [len][byte0][byte1]...[byte(len-1)]
  uint8_t data1[] = {0x06, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36};
  uint8_t data2[] = {0x05, 0x35, 0x34, 0x33, 0x32, 0x31};
  uint8_t data3[] = {0x04, 0x47, 0x48, 0x49, 0x4a};

  // test 3 times sending sleep 1s each time
  LOGI("send for 1 time\n");
  prv_observeNotify(g_context);
  cis_notify_raw(g_context, data1, sizeof(data1));
  sleep(1);
  LOGI("send for 2 time\n");
  prv_observeNotify(g_context);
  cis_notify_raw(g_context, data2, sizeof(data2));
  sleep(1);
  LOGI("send for 3 time\n");
  prv_observeNotify(g_context);
  cis_notify_raw(g_context, data3, sizeof(data3));

  while (1)
    {
      sleep(20);
      prv_observeNotify(g_context);
    }

  struct timespec time;

  time.tv_sec = 60;
  time.tv_nsec = 0;

  pthread_mutex_lock(&g_reg_mutex);
  while (g_reg_status)
    {
      pthread_cond_timedwait(&g_reg_cond, &g_reg_mutex, &time);
    }
  pthread_mutex_unlock(&g_reg_mutex);

  cis_unregister(g_context);

  prv_clean_sample_data(g_context);
  cis_deinit(g_context);

  pthread_mutex_destroy(&g_reg_mutex);
  pthread_cond_destroy(&g_reg_cond);
  return ret;
}

//////////////////////////////////////////////////////////////////////////
#endif

