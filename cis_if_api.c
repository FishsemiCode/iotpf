/****************************************************************************
 * external/services/iotpf/cis_if_api.c
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
#include "cis_if_api.h"

#if CIS_ONE_MCU
#define MAX_PACKET_SIZE			(256)

static const uint8_t config_hex[] =
{
  0x13, 0x00, 0x59,
  0xf1, 0x00, 0x03,
  0xf2, 0x00, 0x4b,
  0x04, 0x00 /*mtu*/, 0x11 /*Link & bind type*/, 0x80 /*BS DTLS ENABLED*/,
  0x00, 0x05 /*apn length*/, 0x43, 0x4d, 0x49, 0x4f, 0x54 /*apn: CMIOT*/,
  0x00, 0x00 /*username length*/, /*username*/
  0x00, 0x00 /*password length*/, /*password*/
  0x00, 0x12 /*host length*/, 0x31, 0x38, 0x33, 0x2e, 0x32, 0x33, 0x30, 0x2e,
  0x34, 0x30, 0x2e, 0x33, 0x39, 0x3a, 0x35, 0x36, 0x38, 0x33 /*host: 183.230.40.39
  :5683*/,
  0x00, 0x23 /*userdata length*/, 0x41, 0x75, 0x74, 0x68, 0x43, 0x6f, 0x64, 0x65,
  0x3a, 0x79, 0x61, 0x6e, 0x67, 0x79, 0x61, 0x6e, 0x6c, 0x69, 0x75, 0x3b, 0x50,
  0x53, 0x4b, 0x3a, 0x79, 0x61, 0x6e, 0x67, 0x79, 0x61, 0x6e, 0x6c, 0x69, 0x75,
  0x3b /*userdata: AuthCode:yangyanliu;PSK:yangyanliu;*/,
  0xf3, 0x00, 0x08,
  0xe4 /*log config*/, 0x00, 0xc8 /*LogBufferSize: 200*/,
  0x00, 0x00 /*userdata length*//*userdata*/	
};

static struct st_observe_info *g_observeList = NULL;

static void *g_cmcc_context;
static bool g_shutdown = false;
static bool g_doUnregister = false;
static bool g_doRegister = false;


static cis_time_t g_notifyLast = 0;

static st_sample_object g_objectList[SAMPLE_OBJECT_MAX];
static st_instance_a g_instList_a[SAMPLE_A_INSTANCE_COUNT];
static st_instance_b g_instList_b[SAMPLE_B_INSTANCE_COUNT];

static int g_cisapi_pip_fd[2];
static pthread_t g_cisapi_onenet_tid = -1;

void cisapi_wakeup_pump(void)
{
  write(g_cisapi_pip_fd[1], "w", 1);
}


//////////////////////////////////////////////////////////////////////////
//private funcation;
static void prv_observeNotify(void *context, cis_uri_t *uri, cis_mid_t mid)
{
  uint8_t index;
  st_sample_object *object = NULL;
  cis_data_t value;
  for (index = 0; index < SAMPLE_OBJECT_MAX; index++)
    {
      if (g_objectList[index].oid == uri->objectId)
        {
          object = &g_objectList[index];
        }
    }
  if (object == NULL)
    {
      return;
    }
  if (!CIS_URI_IS_SET_INSTANCE(uri) && !CIS_URI_IS_SET_RESOURCE(uri)) // one object
    {
      switch (uri->objectId)
        {
          case SAMPLE_OID_A:
            {
              for (index = 0; index < SAMPLE_A_INSTANCE_COUNT; index++)
                {
                  st_instance_a *inst = &g_instList_a[index];
                  if (inst != NULL && inst->enabled == true)
                    {
                      cis_data_t tmpdata[3];
                      tmpdata[0].type = cis_data_type_integer;
                      tmpdata[0].value.asInteger = inst->instance.intValue;
                      uri->instanceId = index;
                      uri->resourceId = attributeA_intValue;
                      cis_uri_update(uri);
                      cis_notify(context, uri, &tmpdata[0], mid, CIS_NOTIFY_CONTINUE, false);
                      tmpdata[1].type = cis_data_type_float;
                      tmpdata[1].value.asFloat = inst->instance.floatValue;
                      uri->instanceId = index;
                      uri->resourceId = attributeA_floatValue;
                      cis_uri_update(uri);
                      cis_notify(context, uri, &tmpdata[1], mid, CIS_NOTIFY_CONTINUE, false);
                      tmpdata[2].type = cis_data_type_string;
                      tmpdata[2].asBuffer.length = strlen(inst->instance.strValue);
                      tmpdata[2].asBuffer.buffer = (uint8_t*)(inst->instance.strValue);
                      uri->instanceId = index;
                      uri->resourceId = attributeA_stringValue;
                      cis_uri_update(uri);
                      cis_notify(context, uri, &tmpdata[2], mid, CIS_NOTIFY_CONTENT, false);
                    }
                }
            }
            break;
          case SAMPLE_OID_B:
            {
              for (index = 0; index < SAMPLE_B_INSTANCE_COUNT; index++)
                {
                  st_instance_b *inst = &g_instList_b[index];
                  if (inst != NULL && inst->enabled == true)
                    {
                      cis_data_t tmpdata[4];
                      tmpdata[0].type = cis_data_type_integer;
                      tmpdata[0].value.asInteger = inst->instance.intValue;
                      uri->instanceId = index;
                      uri->resourceId = attributeB_intValue;
                      cis_uri_update(uri);
                      cis_notify(context, uri, &tmpdata[0], mid, CIS_NOTIFY_CONTINUE, false);
                      tmpdata[1].type = cis_data_type_float;
                      tmpdata[1].value.asFloat = inst->instance.floatValue;
                      uri->instanceId = index;
                      uri->resourceId = attributeB_floatValue;
                      cis_uri_update(uri);
                      cis_notify(context, uri, &tmpdata[1], mid, CIS_NOTIFY_CONTINUE, false);
                      tmpdata[2].type = cis_data_type_string;
                      tmpdata[2].asBuffer.length = strlen(inst->instance.strValue);
                      tmpdata[2].asBuffer.buffer = (uint8_t*)(inst->instance.strValue);
                      uri->instanceId = index;
                      uri->resourceId = attributeB_stringValue;
                      cis_uri_update(uri);
                      cis_notify(context, uri, &tmpdata[2], mid, CIS_NOTIFY_CONTENT, false);
                    }
                }
            }
            break;
        }
    }
  else if (CIS_URI_IS_SET_INSTANCE(uri))
    {
      switch (object->oid)
        {
          case SAMPLE_OID_A:
            {
              if (uri->instanceId > SAMPLE_A_INSTANCE_COUNT)
                {
                  return;
                }
              st_instance_a *inst = &g_instList_a[uri->instanceId];
              if (inst == NULL || inst->enabled == false)
                {
                  return;
                }
              if (CIS_URI_IS_SET_RESOURCE(uri))
                {
                  if (uri->resourceId == attributeA_intValue)
                    {
                      value.type = cis_data_type_integer;
                      value.value.asInteger = inst->instance.intValue;
                    }
                  else if (uri->resourceId == attributeA_floatValue)
                    {
                      value.type = cis_data_type_float;
                      value.value.asFloat = inst->instance.floatValue;
                    }
                  else if (uri->resourceId == attributeA_stringValue)
                    {
                      value.type = cis_data_type_string;
                      value.asBuffer.length = strlen(inst->instance.strValue);
                      value.asBuffer.buffer = (uint8_t*)(inst->instance.strValue);
                    }
                  else
                    {
                      return;
                    }
                  cis_notify(context, uri, &value, mid, CIS_NOTIFY_CONTENT, false);
                }
              else
                {
                  cis_data_t tmpdata[3];
                  tmpdata[0].type = cis_data_type_integer;
                  tmpdata[0].value.asInteger = inst->instance.intValue;
                  uri->resourceId = attributeA_intValue;
                  cis_uri_update(uri);
                  cis_notify(context, uri, &tmpdata[0], mid, CIS_NOTIFY_CONTINUE, false);
                  tmpdata[1].type = cis_data_type_float;
                  tmpdata[1].value.asFloat = inst->instance.floatValue;
                  uri->resourceId = attributeA_floatValue;
                  cis_uri_update(uri);
                  cis_notify(context, uri, &tmpdata[1], mid, CIS_NOTIFY_CONTINUE, false);
                  tmpdata[2].type = cis_data_type_string;
                  tmpdata[2].asBuffer.length = strlen(inst->instance.strValue);
                  tmpdata[2].asBuffer.buffer = (uint8_t*)(inst->instance.strValue);
                  uri->resourceId = attributeA_stringValue;
                  cis_uri_update(uri);
                  cis_notify(context, uri, &tmpdata[2], mid, CIS_NOTIFY_CONTENT, false);
                }
            }
            break;
          case SAMPLE_OID_B:
            {
              if (uri->instanceId > SAMPLE_B_INSTANCE_COUNT)
                {
                  return;
                }
              st_instance_b *inst = &g_instList_b[uri->instanceId];
              if (inst == NULL || inst->enabled == false)
                {
                  return;
                }
              if (CIS_URI_IS_SET_RESOURCE(uri))
                {
                  if (uri->resourceId == attributeB_intValue)
                    {
                      value.type = cis_data_type_integer;
                      value.value.asInteger = inst->instance.intValue;
                    }
                  else if (uri->resourceId == attributeB_floatValue)
                    {
                      value.type = cis_data_type_float;
                      value.value.asFloat = inst->instance.floatValue;
                    }
                  else if (uri->resourceId == attributeB_stringValue)
                    {
                      value.type = cis_data_type_string;
                      value.asBuffer.length = strlen(inst->instance.strValue);
                      value.asBuffer.buffer = (uint8_t*)(inst->instance.strValue);
                    }
                  else
                    {
                      return;
                    }
                  cis_notify(context, uri, &value, mid, CIS_NOTIFY_CONTENT, false);
                }
              else
                {
                  cis_data_t tmpdata[3];
                  tmpdata[0].type = cis_data_type_integer;
                  tmpdata[0].value.asInteger = inst->instance.intValue;
                  uri->resourceId = attributeB_intValue;
                  cis_uri_update(uri);
                  cis_notify(context, uri, &tmpdata[0], mid, CIS_NOTIFY_CONTINUE, false);
                  tmpdata[1].type = cis_data_type_float;
                  tmpdata[1].value.asFloat = inst->instance.floatValue;
                  uri->resourceId = attributeB_floatValue;
                  cis_uri_update(uri);
                  cis_notify(context, uri, &tmpdata[1], mid, CIS_NOTIFY_CONTINUE, false);
                  tmpdata[2].type = cis_data_type_string;
                  tmpdata[2].asBuffer.length = strlen(inst->instance.strValue);
                  tmpdata[2].asBuffer.buffer = (uint8_t*)(inst->instance.strValue);
                  uri->resourceId = attributeB_stringValue;
                  cis_uri_update(uri);
                  cis_notify(context, uri, &tmpdata[2], mid, CIS_NOTIFY_CONTENT, false);
                }
            }
            break;
        }
    }
  cisapi_wakeup_pump();
}



static cis_coapret_t prv_readResponse(void *context, cis_uri_t *uri, cis_mid_t mid)
{
  uint8_t index;
  st_sample_object *object = NULL;
  cis_data_t value;
  for (index = 0; index < SAMPLE_OBJECT_MAX; index++)
    {
      if (g_objectList[index].oid == uri->objectId)
        {
          object = &g_objectList[index];
        }
    }

  if (object == NULL)
    {
      return CIS_RET_ERROR;
    }

  if (!CIS_URI_IS_SET_INSTANCE(uri) && !CIS_URI_IS_SET_RESOURCE(uri)) // one object
    {
      switch (uri->objectId)
        {
          case SAMPLE_OID_A:
            {
              for (index = 0; index < SAMPLE_A_INSTANCE_COUNT; index++)
                {
                  st_instance_a *inst = &g_instList_a[index];
                  if (inst != NULL && inst->enabled == true)
                    {
                      cis_data_t tmpdata[3];
                      tmpdata[0].type = cis_data_type_integer;
                      tmpdata[0].value.asInteger = inst->instance.intValue;
                      uri->instanceId = inst->instId;
                      uri->resourceId = attributeA_intValue;
                      cis_uri_update(uri);
                      cis_response(context, uri, &tmpdata[0], mid, CIS_RESPONSE_CONTINUE);
                      tmpdata[1].type = cis_data_type_float;
                      tmpdata[1].value.asFloat = inst->instance.floatValue;
                      uri->resourceId = attributeA_floatValue;
                      uri->instanceId = inst->instId;
                      cis_uri_update(uri);
                      cis_response(context, uri, &tmpdata[1], mid, CIS_RESPONSE_CONTINUE);
                      tmpdata[2].type = cis_data_type_string;
                      tmpdata[2].asBuffer.length = strlen(inst->instance.strValue);
                      tmpdata[2].asBuffer.buffer = (uint8_t*)(inst->instance.strValue);
                      uri->resourceId = attributeA_stringValue;
                      uri->instanceId = inst->instId;
                      cis_uri_update(uri);
                      cis_response(context, uri, &tmpdata[2], mid, CIS_RESPONSE_CONTINUE);
                    }
                }
            }
            break;
          case SAMPLE_OID_B:
            {
              for (index = 0; index < SAMPLE_B_INSTANCE_COUNT; index++)
                {
                  st_instance_b *inst = &g_instList_b[index];
                  if (inst != NULL && inst->enabled == true)
                    {
                      cis_data_t tmpdata[3];
                      tmpdata[0].type = cis_data_type_integer;
                      tmpdata[0].value.asInteger = inst->instance.intValue;
                      uri->instanceId = inst->instId;
                      uri->resourceId = attributeB_intValue;
                      cis_uri_update(uri);
                      cis_response(context, uri, &tmpdata[0], mid, CIS_RESPONSE_CONTINUE);
                      tmpdata[1].type = cis_data_type_float;
                      tmpdata[1].value.asFloat = inst->instance.floatValue;
                      uri->resourceId = attributeB_floatValue;
                      uri->instanceId = inst->instId;
                      cis_uri_update(uri);
                      cis_response(context, uri, &tmpdata[1], mid, CIS_RESPONSE_CONTINUE);
                      tmpdata[2].type = cis_data_type_string;
                      tmpdata[2].asBuffer.length = strlen(inst->instance.strValue);
                      tmpdata[2].asBuffer.buffer = (uint8_t*)(inst->instance.strValue);
                      uri->resourceId = attributeB_stringValue;
                      uri->instanceId = inst->instId;
                      cis_uri_update(uri);
                      cis_response(context, uri, &tmpdata[2], mid, CIS_RESPONSE_CONTINUE);
                    }
                }
            }
            break;
        }
      cis_response(context, NULL, NULL, mid, CIS_RESPONSE_READ);
    }
  else
    {
      switch (object->oid)
        {
          case SAMPLE_OID_A:
            {
              if (uri->instanceId > SAMPLE_A_INSTANCE_COUNT)
                {
                  return CIS_RET_ERROR;
                }
              st_instance_a *inst = &g_instList_a[uri->instanceId];
              if (inst == NULL || inst->enabled == false)
                {
                  return CIS_RET_ERROR;
                }
              if (CIS_URI_IS_SET_RESOURCE(uri))
                {
                  if (uri->resourceId == attributeA_intValue)
                    {
                      value.type = cis_data_type_integer;
                      value.value.asInteger = inst->instance.intValue;
                    }
                  else if (uri->resourceId == attributeA_floatValue)
                    {
                      value.type = cis_data_type_float;
                      value.value.asFloat = inst->instance.floatValue;
                    }
                  else if (uri->resourceId == attributeA_stringValue)
                    {
                      value.type = cis_data_type_string;
                      value.asBuffer.length = strlen(inst->instance.strValue);
                      value.asBuffer.buffer = (uint8_t*)(inst->instance.strValue);
                    }
                  else
                    {
                      return CIS_RET_ERROR;
                    }
                  cis_response(context, uri, &value, mid, CIS_RESPONSE_READ);
                }
              else
                {
                  cis_data_t tmpdata[3];
                  tmpdata[0].type = cis_data_type_integer;
                  tmpdata[0].value.asInteger = inst->instance.intValue;
                  uri->resourceId = attributeA_intValue;
                  cis_uri_update(uri);
                  cis_response(context, uri, &tmpdata[0], mid, CIS_RESPONSE_CONTINUE);
                  tmpdata[1].type = cis_data_type_float;
                  tmpdata[1].value.asFloat = inst->instance.floatValue;
                  uri->resourceId = attributeA_floatValue;
                  cis_uri_update(uri);
                  cis_response(context, uri, &tmpdata[1], mid, CIS_RESPONSE_CONTINUE);
                  tmpdata[2].type = cis_data_type_string;
                  tmpdata[2].asBuffer.length = strlen(inst->instance.strValue);
                  tmpdata[2].asBuffer.buffer = (uint8_t*)(inst->instance.strValue);
                  uri->resourceId = attributeA_stringValue;
                  cis_uri_update(uri);
                  cis_response(context, uri, &tmpdata[2], mid, CIS_RESPONSE_READ);
                }
            }
            break;
          case SAMPLE_OID_B:
            {
              if (uri->instanceId > SAMPLE_B_INSTANCE_COUNT)
                {
                  return CIS_RET_ERROR;
                }
              st_instance_b *inst = &g_instList_b[uri->instanceId];
              if (inst == NULL || inst->enabled == false)
                {
                  return CIS_RET_ERROR;
                }
              if (CIS_URI_IS_SET_RESOURCE(uri))
                {
                  if (uri->resourceId == attributeB_intValue)
                    {
                      value.type = cis_data_type_integer;
                      value.value.asInteger = inst->instance.intValue;
                    }
                  else if (uri->resourceId == attributeB_floatValue)
                    {
                      value.type = cis_data_type_float;
                      value.value.asFloat = inst->instance.floatValue;
                    }
                  else if (uri->resourceId == attributeB_stringValue)
                    {
                      value.type = cis_data_type_string;
                      value.asBuffer.length = strlen(inst->instance.strValue);
                      value.asBuffer.buffer = (uint8_t*)(inst->instance.strValue);
                    }
                  else
                    {
                      return CIS_RET_ERROR;
                    }
                  cis_response(context, uri, &value, mid, CIS_RESPONSE_READ);
                }
              else
                {
                  cis_data_t tmpdata[3];
                  tmpdata[0].type = cis_data_type_integer;
                  tmpdata[0].value.asInteger = inst->instance.intValue;
                  uri->resourceId = attributeB_intValue;
                  cis_uri_update(uri);
                  cis_response(context, uri, &tmpdata[0], mid, CIS_RESPONSE_CONTINUE);
                  tmpdata[1].type = cis_data_type_float;
                  tmpdata[1].value.asFloat = inst->instance.floatValue;
                  uri->resourceId = attributeB_floatValue;
                  cis_uri_update(uri);
                  cis_response(context, uri, &tmpdata[1], mid, CIS_RESPONSE_CONTINUE);
                  tmpdata[2].type = cis_data_type_string;
                  tmpdata[2].asBuffer.length = strlen(inst->instance.strValue);
                  tmpdata[2].asBuffer.buffer = (uint8_t*)(inst->instance.strValue);
                  uri->resourceId = attributeB_stringValue;
                  cis_uri_update(uri);
                  cis_response(context, uri, &tmpdata[2], mid, CIS_RESPONSE_READ);
                }
            }
            break;
        }
    }
  cisapi_wakeup_pump();
  return CIS_RET_OK;
}


static cis_coapret_t prv_discoverResponse(void *context, cis_uri_t *uri, cis_mid_t mid)
{
  uint8_t index;
  st_sample_object *object = NULL;

  for (index = 0; index < SAMPLE_OBJECT_MAX; index++)
    {
      if (g_objectList[index].oid == uri->objectId)
        {
          object = &g_objectList[index];
        }
    }

  if (object == NULL)
    {
      return CIS_RET_ERROR;
    }

  switch (uri->objectId)
    {
      case SAMPLE_OID_A:
        {
          uri->objectId = URI_INVALID;
          uri->instanceId = URI_INVALID;
          uri->resourceId = attributeA_intValue;
          cis_uri_update(uri);
          cis_response(context, uri, NULL, mid, CIS_RESPONSE_CONTINUE);
          uri->objectId = URI_INVALID;
          uri->instanceId = URI_INVALID;
          uri->resourceId = attributeA_floatValue;
          cis_uri_update(uri);
          cis_response(context, uri, NULL, mid, CIS_RESPONSE_CONTINUE);
          uri->objectId = URI_INVALID;
          uri->instanceId = URI_INVALID;
          uri->resourceId = attributeA_stringValue;
          cis_uri_update(uri);
          cis_response(context, uri, NULL, mid, CIS_RESPONSE_CONTINUE);
          uri->objectId = URI_INVALID;
          uri->instanceId = URI_INVALID;
          uri->resourceId = actionA_1;
          cis_uri_update(uri);
          cis_response(context, uri, NULL, mid, CIS_RESPONSE_CONTINUE);
        }
        break;
      case SAMPLE_OID_B:
        {
          uri->objectId = URI_INVALID;
          uri->instanceId = URI_INVALID;
          uri->resourceId = attributeB_intValue;
          cis_uri_update(uri);
          cis_response(context, uri, NULL, mid, CIS_RESPONSE_CONTINUE);
          uri->objectId = URI_INVALID;
          uri->instanceId = URI_INVALID;
          uri->resourceId = attributeB_floatValue;
          cis_uri_update(uri);
          cis_response(context, uri, NULL, mid, CIS_RESPONSE_CONTINUE);
          uri->objectId = URI_INVALID;
          uri->instanceId = URI_INVALID;
          uri->resourceId = attributeB_stringValue;
          cis_uri_update(uri);
          cis_response(context, uri, NULL, mid, CIS_RESPONSE_CONTINUE);
          uri->objectId = URI_INVALID;
          uri->instanceId = URI_INVALID;
          uri->resourceId = actionB_1;
          cis_uri_update(uri);
          cis_response(context, uri, NULL, mid, CIS_RESPONSE_CONTINUE);
        }
        break;
    }
  cis_response(context, NULL, NULL, mid, CIS_RESPONSE_DISCOVER);
  cisapi_wakeup_pump();
  return CIS_RET_OK;
}


static cis_coapret_t prv_writeResponse(void *context, cis_uri_t *uri, const cis_data_t *value, cis_attrcount_t count, cis_mid_t mid)
{
  uint8_t index;
  st_sample_object *object = NULL;

  if (!CIS_URI_IS_SET_INSTANCE(uri))
    {
      return CIS_RET_ERROR;
    }
  for (index = 0; index < SAMPLE_OBJECT_MAX; index++)
    {
      if (g_objectList[index].oid == uri->objectId)
        {
          object = &g_objectList[index];
        }
    }

  if (object == NULL)
    {
      return CIS_RET_ERROR;
    }

  switch (object->oid)
    {
      case SAMPLE_OID_A:
        {
          if (uri->instanceId > SAMPLE_B_INSTANCE_COUNT)
            {
              return CIS_RET_ERROR;
            }
          st_instance_a *inst = &g_instList_a[uri->instanceId];
          if (inst == NULL || inst->enabled == false)
            {
              return CIS_RET_ERROR;
            }

          for (int i = 0; i < count; i++)
            {
              switch (value[i].id)
                {
                  case attributeA_intValue:
                    {
                      inst->instance.intValue = value[i].value.asInteger;
                    }
                    break;
                  case attributeA_floatValue:
                    {
                      inst->instance.floatValue = value[i].value.asFloat;
                    }
                    break;
                  case  attributeA_stringValue:
                    {
                      memset(inst->instance.strValue, 0, sizeof(inst->instance.strValue));
                      strncpy(inst->instance.strValue, (char*)value[i].asBuffer.buffer, value[i].asBuffer.length);
                    }
                    break;
                }
            }
        }
        break;
      case SAMPLE_OID_B:
        {
          if (uri->instanceId > SAMPLE_B_INSTANCE_COUNT)
            {
              return CIS_RET_ERROR;
            }
          st_instance_b *inst = &g_instList_b[uri->instanceId];
          if (inst == NULL || inst->enabled == false)
            {
              return CIS_RET_ERROR;
            }
          for (int i = 0; i < count; i++)
            {
              switch (value[i].id)
                {
                  case attributeB_intValue:
                    {
                      inst->instance.intValue = value[i].value.asInteger;
                    }
                    break;
                  case attributeB_floatValue:
                    {
                      inst->instance.floatValue = value[i].value.asFloat;
                    }
                    break;
                  case attributeB_stringValue:
                    {
                      memset(inst->instance.strValue, 0, sizeof(inst->instance.strValue));
                      strncpy(inst->instance.strValue, (char*)value[i].asBuffer.buffer, value[i].asBuffer.length);
                    }
                    break;
                }
            }
        }
        break;
    }
  cis_response(context, NULL, NULL, mid, CIS_RESPONSE_WRITE);
  cisapi_wakeup_pump();
  return CIS_RET_OK;
}


static cis_coapret_t prv_execResponse(void *context, cis_uri_t *uri, const uint8_t *value, uint32_t length, cis_mid_t mid)
{
  uint8_t index;
  st_sample_object *object = NULL;

  for (index = 0; index < SAMPLE_OBJECT_MAX; index++)
    {
      if (g_objectList[index].oid == uri->objectId)
        {
          object = &g_objectList[index];
        }
    }
  if (object == NULL)
    {
      return CIS_RET_ERROR;
    }

  switch (object->oid)
    {
      case SAMPLE_OID_A:
        {
          if (uri->instanceId > SAMPLE_B_INSTANCE_COUNT)
            {
              return CIS_RET_ERROR;
            }
          st_instance_a *inst = &g_instList_a[uri->instanceId];
          if (inst == NULL || inst->enabled == false)
            {
              return CIS_RET_ERROR;
            }
          if (uri->resourceId == actionA_1)
            {
              cis_response(context, NULL, NULL, mid, CIS_RESPONSE_EXECUTE);
            }
          else
            {
              return CIS_RET_ERROR;
            }
        }
        break;
      case SAMPLE_OID_B:
        {
          if (uri->instanceId > SAMPLE_B_INSTANCE_COUNT)
            {
              return CIS_RET_ERROR;
            }
          st_instance_b *inst = &g_instList_b[uri->instanceId];
          if (inst == NULL || inst->enabled == false)
            {
              return CIS_RET_ERROR;
            }
          if (uri->resourceId == actionB_1)
            {
              cis_response(context, NULL, NULL, mid, CIS_RESPONSE_EXECUTE);
            }
          else
            {
              return CIS_RET_ERROR;
            }
        }
        break;
    }
  cisapi_wakeup_pump();
  return CIS_RET_OK;
}


static cis_coapret_t prv_paramsResponse(void *context, cis_uri_t *uri, cis_observe_attr_t parameters, cis_mid_t mid)
{
  uint8_t index;
  st_sample_object* object = NULL;

  if (!CIS_URI_IS_SET_INSTANCE(uri))
    {
      return CIS_RET_ERROR;
    }

  for (index = 0; index < SAMPLE_OBJECT_MAX; index++)
    {
      if (g_objectList[index].oid == uri->objectId)
        {
          object = &g_objectList[index];
        }
    }

  if (object == NULL)
    {
      return CIS_RET_ERROR;
    }

  /*set parameter to observe resource*/
  /*do*/

  cis_response(context, NULL, NULL, mid, CIS_RESPONSE_OBSERVE_PARAMS);
  cisapi_wakeup_pump();
  return CIS_RET_OK;
}


static cis_coapret_t prv_observeResponse(void *context, cis_uri_t *uri, bool flag, cis_mid_t mid)
{
  if (flag)
    {
      uint16_t count = 0;
      struct st_observe_info *observe_new = (struct st_observe_info*)cis_malloc(sizeof(struct st_observe_info));
      observe_new->mid = mid;
      observe_new->uri = *uri;
      observe_new->next = NULL;
      g_observeList = (struct st_observe_info*)cis_list_add((cis_list_t*)g_observeList, (cis_list_t*)observe_new);

      LOGD("cis_on_observe set(%d): %d/%d/%d",
        count,
        observe_new->uri.objectId,
        CIS_URI_IS_SET_INSTANCE(&observe_new->uri) ? observe_new->uri.instanceId : -1,
        CIS_URI_IS_SET_RESOURCE(&observe_new->uri) ? observe_new->uri.resourceId : -1);
      cis_response(g_cmcc_context, NULL, NULL, mid, CIS_RESPONSE_OBSERVE);
    }
  else
    {
      struct st_observe_info *delnode = g_observeList;
      while (delnode)
        {
          if (uri->flag == delnode->uri.flag && uri->objectId == delnode->uri.objectId)
            {
              if (uri->instanceId == delnode->uri.instanceId)
                {
                  if (uri->resourceId == delnode->uri.resourceId)
                    {
                      break;
                    }
                }
            }
          delnode = delnode->next;
        }
      if (delnode != NULL)
        {
          g_observeList = (struct st_observe_info *)cis_list_remove((cis_list_t *)g_observeList, delnode->mid, (cis_list_t **)&delnode);
          LOGD("cis_on_observe cancel: %d/%d/%d\n",
            delnode->uri.objectId,
            CIS_URI_IS_SET_INSTANCE(&delnode->uri) ? delnode->uri.instanceId : -1,
            CIS_URI_IS_SET_RESOURCE(&delnode->uri) ? delnode->uri.resourceId : -1);

          cis_free(delnode);
          cis_response(g_cmcc_context, NULL, NULL, mid, CIS_RESPONSE_OBSERVE);
        }
      else
        {
          return CIS_RESPONSE_NOT_FOUND;
        }
    }
  cisapi_wakeup_pump();
  return CIS_RET_OK;
}

static cis_coapret_t cis_api_onRead(void *context, cis_uri_t *uri, cis_mid_t mid)
{
  return prv_readResponse(context, uri, mid);
}

static cis_coapret_t cis_api_onDiscover(void *context, cis_uri_t *uri, cis_mid_t mid)
{
  return prv_discoverResponse(context, uri, mid);
}

static cis_coapret_t cis_api_onWrite(void *context, cis_uri_t *uri, const cis_data_t *value, cis_attrcount_t attrcount, cis_mid_t mid)
{
  return prv_writeResponse(context, uri, value, attrcount, mid);
}


static cis_coapret_t cis_api_onExec(void *context, cis_uri_t *uri, const uint8_t *value, uint32_t length, cis_mid_t mid)
{
  return prv_execResponse(context, uri, value, length, mid);
}


static cis_coapret_t cis_api_onObserve(void *context, cis_uri_t *uri, bool flag, cis_mid_t mid)
{
  return prv_observeResponse(context, uri, flag, mid);
}

static cis_coapret_t cis_api_onParams(void *context, cis_uri_t *uri, cis_observe_attr_t parameters, cis_mid_t mid)
{
  return prv_paramsResponse(context, uri, parameters, mid);
}

static void cis_api_onEvent(void *context, cis_evt_t eid, void *param)
{
#if CIS_ENABLE_CMIOT_OTA || CIS_ENABLE_UPDATE_MCU
  st_context_t *ctx = (st_context_t*)context;
#endif
#if CIS_ENABLE_CMIOT_OTA
  cissys_assert(context != NULL);
#endif
  LOGD("cis_on_event(%d):%s\n", eid, STR_EVENT_CODE(eid));
  switch (eid)
    {
      case CIS_EVENT_RESPONSE_FAILED:
        LOGD("cis_on_event response failed mid:%d\n", (int32_t)param);
        break;
      case CIS_EVENT_NOTIFY_FAILED:
        LOGD("cis_on_event notify failed mid:%d\n", (int32_t)param);
        break;
      case CIS_EVENT_UPDATE_NEED:
        LOGD("cis_on_event need to update,reserve time:%ds\n", (int32_t)param);
        cis_update_reg(g_cmcc_context, LIFETIME_INVALID, false);
        break;
      case CIS_EVENT_REG_SUCCESS:
        {
          struct st_observe_info* delnode;
          while (g_observeList != NULL)
            {
              g_observeList = (struct st_observe_info *)CIS_LIST_RM((cis_list_t *)g_observeList, g_observeList->mid, (cis_list_t **)&delnode);
              cis_free(delnode);
            }
        }
        break;
    #if CIS_ENABLE_UPDATE
      case CIS_EVENT_FIRMWARE_DOWNLOADING:
        LOGD("FIRMWARE_DOWNLOADING\r\n");
        break;
      case CIS_EVENT_FIRMWARE_DOWNLOAD_FAILED:
        LOGD("FIRMWARE_DOWNFAILED\r\n");
        break;
      case CIS_EVENT_FIRMWARE_DOWNLOADED:
        LOGD("FIRMWARE_DOWNLOADED:\r\n");
        break;
      case CIS_EVENT_FIRMWARE_UPDATING:
        LOGD("FIRMWARE_UPDATING:\r\n");
        break;
      case CIS_EVENT_FIRMWARE_TRIGGER:
        LOGD("please update the firmware\r\n");
        break;
    #if CIS_ENABLE_UPDATE_MCU
      case CIS_EVENT_SOTA_DOWNLOADING:
        LOGD("SOTA_DOWNLOADING:\r\n");
        break;
      case CIS_EVENT_SOTA_DOWNLOAED:
        LOGD("SOTA_DOWNLOAED:\r\n");
        break;
      case CIS_EVENT_SOTA_FLASHERASE:
        LOGD("SOTA_FLASHERASE:\r\n");
        cissys_sleepms(2000);
        cis_notify_sota_result(ctx, sota_erase_success);
        break;
      case CIS_EVENT_SOTA_UPDATING:
        cis_set_sota_info( "1801102", 500);
        LOGD("SOTA_UPDATING:\r\n");
        break;
    #endif
    #endif
    #if CIS_ENABLE_CMIOT_OTA
      case CIS_EVENT_CMIOT_OTA_START:
        LOGD("cis_on_event CMIOT OTA start:%d\n", (int32_t)param);
        if (ctx->cmiotOtaState == CMIOT_OTA_STATE_IDIL)
          {
            ctx->cmiotOtaState = CMIOT_OTA_STATE_START;
            ctx->ota_timeout_base = cissys_gettime();
            ctx->ota_timeout_duration = CIS_CMIOT_OTA_START_TIME_OUT_MSEC;
            g_cmiot_ota_at_return_code = OTA_FINISH_AT_RETURN_TIMEOUT_SECOND;
          }
        break;
      case CIS_EVENT_CMIOT_OTA_SUCCESS:
        if (ctx->cmiotOtaState == CMIOT_OTA_STATE_START)
          {
            LOGD("cis_on_event CMIOT OTA success %d\n", (int32_t)param);
            ctx->cmiotOtaState = CMIOT_OTA_STATE_IDIL;
            cissys_recover_psm();
          }
        break;
      case CIS_EVENT_CMIOT_OTA_FAIL:
        if (ctx->cmiotOtaState == CMIOT_OTA_STATE_START)
          {
            ctx->cmiotOtaState = CMIOT_OTA_STATE_IDIL;
            LOGD("cis_on_event CMIOT OTA fail %d\n", (int32_t)param);
            cissys_recover_psm();
          }
        break;
      case CIS_EVENT_CMIOT_OTA_FINISH:
        if (ctx->cmiotOtaState == CMIOT_OTA_STATE_START || ctx->cmiotOtaState == CMIOT_OTA_STATE_IDIL)
          {
            ctx->cmiotOtaState = CMIOT_OTA_STATE_FINISH;
            LOGD("cis_on_event CMIOT OTA finish %d\n", *((int8_t *)param));
            cissys_recover_psm();
            switch (*((int8_t *)param))
              {
                case OTA_FINISH_COMMAND_SUCCESS_CODE:
                  g_cmiot_otafinishstate_flag = OTA_HISTORY_STATE_FINISHED; //indicate ota procedure has finished
                  g_cmiot_ota_at_return_code = OTA_FINISH_AT_RETURN_SUCCESS;
                  LOGD("OTA finish Success!\n");
                  break;
                case OTA_FINISH_COMMAND_UNREGISTER_CODE:
                  g_cmiot_ota_at_return_code = OTA_FINISH_AT_RETURN_NO_REQUEST;
                  LOGD("OTA finish fail: no OTA register on platform!\n");//indicates there is no ota request on the CMIOT platform
                  break;
                case OTA_FINISH_COMMAND_FAIL_CODE:
                  g_cmiot_ota_at_return_code = OTA_FINISH_AT_RETURN_FAIL;
                  LOGD("OTA finish fail: target NUM error!\n");//indicates the IMSI is not changed or the new IMSI is illegal
                  break;
                default://unkown error from the platform
                  LOGD("OTA finish fail: unknow error!\n");
              }
          }
          break;
    #endif
      default:
        break;
    }
}

static void prv_make_sample_data(void)
{
  int i = 0;
  cis_instcount_t instIndex;
  cis_instcount_t instCount;
  for (i = 0; i < SAMPLE_OBJECT_MAX; i++)
    {
      st_sample_object *obj = &g_objectList[i];
      switch (i)
        {
          case 0:
            {
              obj->oid = SAMPLE_OID_A;
              obj->instBitmap = SAMPLE_A_INSTANCE_BITMAP;
              instCount = SAMPLE_A_INSTANCE_COUNT;
              for (instIndex = 0; instIndex < instCount; instIndex++)
                {
                  if (obj->instBitmap[instIndex] != '1')
                    {
                      g_instList_a[instIndex].instId = instIndex;
                      g_instList_a[instIndex].enabled = false;
                    }
                  else
                    {
                      g_instList_a[instIndex].instId = instIndex;
                      g_instList_a[instIndex].enabled = true;
                      g_instList_a[instIndex].instance.floatValue = cissys_rand() * 10.0 / 0xFFFFFFFF;
                      g_instList_a[instIndex].instance.intValue = 666;
                      strcpy(g_instList_a[instIndex].instance.strValue, "hello onenet");
                    }
                }
              obj->attrCount = sizeof(const_AttrIds_a) / sizeof(cis_rid_t);
              obj->attrListPtr = const_AttrIds_a;
              obj->actCount = sizeof(const_ActIds_a) / sizeof(cis_rid_t);
              obj->actListPtr = const_ActIds_a;
            }
            break;
          case 1:
            {
              obj->oid = SAMPLE_OID_B;
              obj->instBitmap = SAMPLE_B_INSTANCE_BITMAP;
              instCount = SAMPLE_B_INSTANCE_COUNT;
              for (instIndex = 0; instIndex < instCount; instIndex++)
                {
                  if (obj->instBitmap[instIndex] != '1')
                    {
                      g_instList_b[instIndex].instId = instIndex;
                      g_instList_b[instIndex].enabled = false;
                    }
                  else
                    {
                      g_instList_b[instIndex].instId = instIndex;
                      g_instList_b[instIndex].enabled = true;
                      g_instList_b[instIndex].instance.boolValue = true;
                      g_instList_b[instIndex].instance.floatValue = cissys_rand() * 10.0 / 0xFFFFFFFF;
                      g_instList_b[instIndex].instance.intValue = 555;
                      strcpy(g_instList_b[instIndex].instance.strValue, "test");
                    }
                }
              obj->attrCount = sizeof(const_AttrIds_b) / sizeof(cis_rid_t);
              obj->attrListPtr = const_AttrIds_b;
              obj->actCount = sizeof(const_ActIds_b) / sizeof(cis_rid_t);
              obj->actListPtr = const_ActIds_b;
            }
            break;
        }
    }
}


int cisapi_sample_entry(const uint8_t *config_bin, uint32_t config_size)
{
  int index = 0;
  cis_callback_t callback;
  callback.onRead = cis_api_onRead;
  callback.onWrite = cis_api_onWrite;
  callback.onExec = cis_api_onExec;
  callback.onObserve = cis_api_onObserve;
  callback.onSetParams = cis_api_onParams;
  callback.onEvent = cis_api_onEvent;
  callback.onDiscover = cis_api_onDiscover;

  cis_time_t g_lifetime = 720;
  /*init sample data*/
  prv_make_sample_data();
  if (cis_init(&g_cmcc_context, (void *)config_bin, config_size) != CIS_RET_OK)
    {
      if (g_cmcc_context != NULL)
        {
          cis_deinit(&g_cmcc_context);
        }
      LOGE("cis entry init failed.\n");
      return -1;
    }

  for (index = 0; index < SAMPLE_OBJECT_MAX; index++)
    {
      cis_inst_bitmap_t bitmap;
      cis_res_count_t rescount;
      cis_instcount_t instCount, instBytes;
      const char *instAsciiPtr;
      uint8_t *instPtr;
      cis_oid_t oid;
      int16_t i;
      st_sample_object* obj = &g_objectList[index];

      oid = obj->oid;
      instCount = utils_strlen(obj->instBitmap);
      instBytes = (instCount - 1) / 8 + 1;
      instAsciiPtr = obj->instBitmap;
      instPtr = (uint8_t*)cis_malloc(instBytes);
      cissys_assert(instPtr != NULL);
      cis_memset(instPtr, 0, instBytes);

      for (i = 0; i < instCount; i++)
        {
          cis_instcount_t instBytePos = i / 8;
          cis_instcount_t instByteOffset = 7 - (i % 8);
          if (instAsciiPtr[i] == '1')
            {
              instPtr[instBytePos] += 0x01 << instByteOffset;
            }
        }
      bitmap.instanceCount = instCount;
      bitmap.instanceBitmap = instPtr;
      bitmap.instanceBytes = instBytes;

      rescount.attrCount = obj->attrCount;
      rescount.actCount = obj->actCount;

      cis_addobject(g_cmcc_context, oid, &bitmap, &rescount);
      cis_free(instPtr);
    }

  g_shutdown = false;
  g_doUnregister = false;

  //register enabled
  g_doRegister = true;

  while (!g_shutdown)
    {
      struct timeval tv;
      fd_set readfds;
      int result;
      int netFd = -1;
      int maxfd = 0;
      st_context_t *ctx = (st_context_t *)g_cmcc_context;
      tv.tv_sec = 60;
      tv.tv_usec = 0;
      {
        if (g_doRegister)
          {
            g_doRegister = false;
            cis_register(g_cmcc_context, g_lifetime, &callback);
          }
        if (g_doUnregister)
          {
            g_doUnregister = false;
            cis_unregister(g_cmcc_context);
            struct st_observe_info* delnode;
            while (g_observeList != NULL)
              {
                g_observeList = (struct st_observe_info *)CIS_LIST_RM((cis_list_t *)g_observeList, g_observeList->mid, (cis_list_t **)&delnode);
                cis_free(delnode);
              }
            cissys_sleepms(1000);
            g_doRegister = 1;
          }
      }

      FD_ZERO(&readfds);
      FD_SET(g_cisapi_pip_fd[0], &readfds);
      maxfd = g_cisapi_pip_fd[0];
      if (ctx->pNetContext && ctx->pNetContext->sock > 0)
        {
          LOGE("cisapi_sample_entry net sock:%d", ctx->pNetContext->sock);
          FD_SET(ctx->pNetContext->sock, &readfds);
          netFd = ctx->pNetContext->sock;
          if (ctx->pNetContext->sock > maxfd)
            {
              maxfd = ctx->pNetContext->sock;
            }
        }
      result = cis_pump(g_cmcc_context, &tv.tv_sec);
      LOGD("cis_pump result:%d,%d", result, tv.tv_sec);
      if (result == PUMP_RET_NOSLEEP)
        {
          tv.tv_sec = 0;
          tv.tv_usec = 1;
        }
      result = select(maxfd + 1, &readfds, NULL, NULL, &tv);
      if (result < 0)
        {
          LOGE("select error: %d %s", errno, strerror(errno));
        }
      else if (result > 0)
        {
          if(FD_ISSET(g_cisapi_pip_fd[0], &readfds))
			{
			  char c[2];
              read(g_cisapi_pip_fd[0], c, 1);
              if (c[0] == 'q')
                {
                  LOGE("!!!!!!!!cisapi_onenet_thread quit");
                  break;
                }
			}
          if (netFd > 0 && FD_ISSET(netFd, &readfds))
            {
              uint8_t buffer[MAX_PACKET_SIZE];
              int numBytes;
              numBytes = recv(netFd, (char*)buffer, MAX_PACKET_SIZE, 0);
              if (numBytes < 0)
                {
                  LOGE("Error in recvfrom(): %d %s\r\n", errno, strerror(errno));
                }
              else if (numBytes > 0)
                {
                  LOGD("%d bytes received", numBytes);
                  uint8_t* data = (uint8_t*)cis_malloc(numBytes);
                  cis_memcpy(data,buffer,numBytes);
                  struct st_net_packet *packet = (struct st_net_packet*)cis_malloc(sizeof(struct st_net_packet));
                  packet->next = NULL;
                  packet->buffer = data;
                  packet->length = numBytes;
                  ctx->pNetContext->g_packetlist = (struct st_net_packet*)CIS_LIST_ADD(ctx->pNetContext->g_packetlist, packet);
                }
            }
        }
      uint32_t nowtime;
      /*data observe data report*/
      nowtime = cissys_gettime();
      struct st_observe_info *node = g_observeList;
      if (nowtime - g_notifyLast > 60 * 1000)
        {
          g_notifyLast = nowtime;
          while (node != NULL)
            {
              if (node->mid == 0)
                {
                  continue;
                }
              if (node->uri.flag == 0)
                {
                  continue;
                }
              cis_uri_t uriLocal;
              uriLocal = node->uri;
              prv_observeNotify(g_cmcc_context, &uriLocal, node->mid);
              node = node->next;
            }
        }
    }

  cis_deinit(&g_cmcc_context);
  struct st_observe_info *delnode;
  while (g_observeList != NULL)
    {
      g_observeList = (struct st_observe_info *)CIS_LIST_RM((cis_list_t *)g_observeList, g_observeList->mid, (cis_list_t **)&delnode);
      cis_free(delnode);
    }

  return 0;
}

void *cisapi_onenet_thread(void *obj)
{
  pthread_setname_np(pthread_self(), "cisapi_thread");
  cisapi_sample_entry(config_hex, sizeof(config_hex));
  return NULL;
}

int cisapi_initialize(void)
{
  int ret;
  pipe(g_cisapi_pip_fd);
  if (pthread_create(&g_cisapi_onenet_tid, NULL, cisapi_onenet_thread, NULL))
    {
      LOGE("%s: pthread_create (%s)\n", __func__, strerror(errno));
      ret = -1;;
      goto clean;
    }
  /*TODO*/
  while(1)
    {
      sleep(60 * 60);
    }
clean:
    if (g_cisapi_pip_fd[0])
    {
      close(g_cisapi_pip_fd[0]);
    }
  if (g_cisapi_pip_fd[1])
    {
      close(g_cisapi_pip_fd[1]);
    }
  return ret;
}

//////////////////////////////////////////////////////////////////////////
#endif
