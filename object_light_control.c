/****************************************************************************
 * external/services/iotpf/object_light_control.c
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

#include <string.h>
#include "cis_api.h"
#include "cis_log.h"
#include "cis_if_api_ctcc.h"
#include "object_light_control.h"

static cis_list_t *light_control_inst;
static st_observe_info *light_control_observe_list = NULL;

typedef struct _light_control_data_
{
  struct _light_control_data_ *next;
  uint16_t instanceId;
  bool onOff;
  uint8_t dimmer;
  uint32_t onTime;
  float cumulativePower;
  float powerFactor;
  char *color;
  char *sensorUints;
  char *applicationType;
} light_control_data_t;

enum
{
  LIGHT_CONTROL_RESOURCE_ID_ONOFF = 5850,
  LIGHT_CONTROL_RESOURCE_ID_DIMMER = 5851,
  LIGHT_CONTROL_RESOURCE_ID_ONTIME = 5852,
  LIGHT_CONTROL_RESOURCE_ID_CUMULATIVEPOWER = 5805,
  LIGHT_CONTROL_RESOURCE_ID_POWERFACTOR = 5820,
  LIGHT_CONTROL_RESOURCE_ID_COLOR = 5706,
  LIGHT_CONTROL_RESOURCE_ID_SENSORUNITS = 5701,
  LIGHT_CONTROL_RESOURCE_ID_APPLICATIONTYPE = 5750,
};

static uint16_t g_resList[] =
{
  LIGHT_CONTROL_RESOURCE_ID_ONOFF,
  LIGHT_CONTROL_RESOURCE_ID_DIMMER,
  LIGHT_CONTROL_RESOURCE_ID_ONTIME,
  LIGHT_CONTROL_RESOURCE_ID_CUMULATIVEPOWER,
  LIGHT_CONTROL_RESOURCE_ID_POWERFACTOR,
  LIGHT_CONTROL_RESOURCE_ID_COLOR,
  LIGHT_CONTROL_RESOURCE_ID_SENSORUNITS,
  LIGHT_CONTROL_RESOURCE_ID_APPLICATIONTYPE,
};


void light_control_initialize_data(int instanceId, light_control_data_t *targetP)
{
  memset(targetP, 0, sizeof(light_control_data_t));

  targetP->instanceId = (uint16_t)instanceId;

  if (instanceId == 0)
    {
      targetP->onOff = true;
      targetP->dimmer = 50;
      targetP->onTime = 2000;
      targetP->cumulativePower = 24.5;
      targetP->powerFactor = 0.5;
      targetP->color = strdup("warmwhite");
      targetP->sensorUints = strdup("Celcius");
      targetP->applicationType = strdup("bulb");
    }
  else
    {
      targetP->onOff = false;
      targetP->dimmer = 75;
      targetP->onTime = 1250;
      targetP->cumulativePower = 18.3;
      targetP->powerFactor = 1.8;
      targetP->color = strdup("red");
      targetP->sensorUints = strdup("Kelvin");
      targetP->applicationType = strdup("airCondition");
    }
}

uint8_t light_control_create(void *contextP, int instanceId,
                            st_object_t *lightControlObj)
{
  light_control_data_t *targetP = NULL;
  uint8_t instBytes = 0;
  uint8_t instCount = 0;
  cis_iid_t instIndex;

  if (NULL == lightControlObj)
    {
      return CIS_RET_ERROR;
    }

  // Manually create a hard-code instance
  targetP = (light_control_data_t *)malloc(sizeof(light_control_data_t));
  if (NULL == targetP)
    {
      return CIS_RET_ERROR;
    }
  light_control_initialize_data(instanceId, targetP);
  lightControlObj->attributeCount = sizeof(g_resList) / sizeof(uint16_t);

  light_control_inst = CIS_LIST_ADD(light_control_inst, targetP);

  instCount = CIS_LIST_COUNT(light_control_inst);

  if (instCount == 0)
    {
      free(targetP);
      return CIS_RET_ERROR;
    }

  if (instCount == 1)
    {
      return CIS_RET_OK;
    }

  lightControlObj->instBitmapCount = instCount;
  instBytes = (instCount - 1) / 8 + 1;
  if (lightControlObj->instBitmapBytes < instBytes)
    {
      if (lightControlObj->instBitmapBytes != 0 && lightControlObj->instBitmapPtr != NULL)
        {
          free(lightControlObj->instBitmapPtr);
        }
      lightControlObj->instBitmapPtr = (uint8_t*)malloc(instBytes);
      lightControlObj->instBitmapBytes = instBytes;
    }
  memset(lightControlObj->instBitmapPtr, 0, instBytes);
  targetP = (light_control_data_t *)light_control_inst;
  for (instIndex = 0; instIndex < instCount; instIndex++)
    {
      uint8_t instBytePos = targetP->instanceId / 8;
      uint8_t instByteOffset = 7 - (targetP->instanceId % 8);
      lightControlObj->instBitmapPtr[instBytePos] += 0x01 << instByteOffset;

      targetP = targetP->next;
    }
  return CIS_RET_OK;
}

static uint8_t light_control_get_value(void *contextP,
    cis_data_t *cisDataP,
    light_control_data_t *lightControlDataP,
    int resId)
{
  uint8_t result = CIS_RET_OK;
  switch (resId)
    {
    case LIGHT_CONTROL_RESOURCE_ID_ONOFF:
      {
        cis_data_encode_bool(lightControlDataP->onOff, cisDataP);
        break;
      }
    case LIGHT_CONTROL_RESOURCE_ID_DIMMER:
      {
        cis_data_encode_int(lightControlDataP->dimmer, cisDataP);
        break;
      }
    case LIGHT_CONTROL_RESOURCE_ID_ONTIME:
      {
        cis_data_encode_int(lightControlDataP->onTime, cisDataP);
        break;
      }
    case LIGHT_CONTROL_RESOURCE_ID_CUMULATIVEPOWER:
      {
        cis_data_encode_float(lightControlDataP->cumulativePower, cisDataP);
        break;
      }
    case LIGHT_CONTROL_RESOURCE_ID_POWERFACTOR:
      {
        cis_data_encode_float(lightControlDataP->powerFactor, cisDataP);
        break;
      }
    case LIGHT_CONTROL_RESOURCE_ID_COLOR:
      {
        cis_data_encode_string(lightControlDataP->color, cisDataP);
        break;
      }
    case LIGHT_CONTROL_RESOURCE_ID_SENSORUNITS:
      {
        cis_data_encode_string(lightControlDataP->sensorUints, cisDataP);
        break;
      }
    case LIGHT_CONTROL_RESOURCE_ID_APPLICATIONTYPE:
      {
        cis_data_encode_string(lightControlDataP->applicationType, cisDataP);
        break;
      }
    default:
      return CIS_RET_ERROR;
    }
  return result;
}

uint8_t light_control_read(void *context, cis_uri_t *uri, cis_mid_t mid)
{
  uint8_t result = CIS_RET_OK;
  int i;
  cis_data_t cisData;
  cis_list_t *pInstNode = light_control_inst;

  int nbRes = sizeof(g_resList) / sizeof(uint16_t);
  if (!CIS_URI_IS_SET_INSTANCE(uri) && !CIS_URI_IS_SET_RESOURCE(uri))
    {
      while (pInstNode)
        {
          for (i = 0; i < nbRes; i++)
            {
              uri->instanceId = ((light_control_data_t *)pInstNode)->instanceId;
              uri->resourceId = g_resList[i];
              light_control_get_value(context, &cisData, (light_control_data_t *)pInstNode,
                g_resList[i]);
              cis_uri_update(uri);
              if (i < nbRes - 1)
                {
                  cis_response(context, uri, &cisData, mid, CIS_RESPONSE_CONTINUE);
                }
              else
                {
                  cis_response(context, uri, &cisData, mid, CIS_RESPONSE_READ);
                }
            }
          pInstNode = pInstNode->next;
        }
    }
  else if (!CIS_URI_IS_SET_RESOURCE(uri))
    {
      while (pInstNode)
        {
          if (((light_control_data_t *)pInstNode)->instanceId == uri->instanceId)
            {
              for (i = 0; i < nbRes; i++)
                {
                  uri->instanceId = ((light_control_data_t *)pInstNode)->instanceId;
                  uri->resourceId = g_resList[i];
                  light_control_get_value(context, &cisData, (light_control_data_t *)pInstNode,
                    g_resList[i]);
                  cis_uri_update(uri);
                  if (i < nbRes - 1)
                    {
                      cis_response(context, uri, &cisData, mid, CIS_RESPONSE_CONTINUE);
                    }
                  else
                    {
                      cis_response(context, uri, &cisData, mid, CIS_RESPONSE_READ);
                    }
                }
              }
          pInstNode = pInstNode->next;
        }
    }
  else
    {
      while (pInstNode)
        {
          if (((light_control_data_t *)pInstNode)->instanceId == uri->instanceId)
            {
              light_control_get_value(context, &cisData, (light_control_data_t *)pInstNode,
                uri->resourceId);
              cis_response(context, uri, &cisData, mid, CIS_RESPONSE_READ);
            }
          pInstNode = pInstNode->next;
        }
    }

  return result;
}

uint8_t light_control_write(void *context, cis_uri_t *uri, const cis_data_t *value, cis_attrcount_t attrcount, cis_mid_t mid)
{
  cis_list_t *pInstNode = light_control_inst;
  int i;
  int64_t integerValue;
  double floatValue;

  if (!CIS_URI_IS_SET_INSTANCE(uri))
    {
      return CIS_RET_ERROR;
    }

  while (pInstNode)
    {
      if (((light_control_data_t *)pInstNode)->instanceId == uri->instanceId)
        {
          light_control_data_t *targetP = (light_control_data_t *)pInstNode;
          for (i = 0; i < attrcount; i++)
            {
              LOGD("%s, resId:%d,%d\n", __func__, value[i].id, value[i].type);
              switch (value[i].id)
                {
                  case LIGHT_CONTROL_RESOURCE_ID_ONOFF:
                    {
                      cis_data_decode_bool(value + i, &(targetP->onOff));
                      break;
                    }
                  case LIGHT_CONTROL_RESOURCE_ID_DIMMER:
                    {
                      cis_data_decode_int(value + i, &integerValue);
                      targetP->dimmer = integerValue;
                      break;
                    }
                  case LIGHT_CONTROL_RESOURCE_ID_ONTIME:
                    {
                      cis_data_decode_int(value + i, &integerValue);
                      targetP->onTime = integerValue;
                      break;
                    }
                  case LIGHT_CONTROL_RESOURCE_ID_CUMULATIVEPOWER:
                    {
                      cis_data_decode_float(value + i, &floatValue);
                      targetP->cumulativePower = floatValue;
                      break;
                    }
                  case LIGHT_CONTROL_RESOURCE_ID_POWERFACTOR:
                    {
                      cis_data_decode_float(value + i, &floatValue);
                      targetP->powerFactor = floatValue;
                      break;
                    }
                  case LIGHT_CONTROL_RESOURCE_ID_COLOR:
                    {
                      free(targetP->color);
                      targetP->color = calloc(value[i].asBuffer.length + 1, 1);
                      memcpy(targetP->color, value[i].asBuffer.buffer, value[i].asBuffer.length);
                      break;
                    }
                  case LIGHT_CONTROL_RESOURCE_ID_SENSORUNITS:
                    {
                      free(targetP->sensorUints);
                      targetP->sensorUints = calloc(value[i].asBuffer.length + 1, 1);
                      memcpy(targetP->sensorUints, value[i].asBuffer.buffer, value[i].asBuffer.length);
                      break;
                    }
                  case LIGHT_CONTROL_RESOURCE_ID_APPLICATIONTYPE:
                    {
                      free(targetP->applicationType);
                      targetP->applicationType = calloc(value[i].asBuffer.length + 1, 1);
                      memcpy(targetP->applicationType, value[i].asBuffer.buffer, value[i].asBuffer.length);
                      break;
                    }
                  default:
                    break;
                }
            }
        }
      pInstNode = pInstNode->next;
    }

  cis_response(context, NULL, NULL, mid, CIS_RESPONSE_WRITE);
  return CIS_RET_OK;
}


uint8_t light_control_observe(void *context, cis_uri_t *uri, bool flag, cis_mid_t mid)
{
  if (flag)
    {
      st_observe_info *observe_new = (st_observe_info*)malloc(sizeof(st_observe_info));
      observe_new->mid = mid;
      memcpy(&(observe_new->uri), uri, sizeof(cis_uri_t));
      observe_new->next = NULL;
      light_control_observe_list = (st_observe_info*)cis_list_add((cis_list_t*)light_control_observe_list, (cis_list_t*)observe_new);

      LOGD("light_control_observe set:%d/%d/%d",
        observe_new->uri.objectId,
        CIS_URI_IS_SET_INSTANCE(&observe_new->uri) ? observe_new->uri.instanceId : -1,
        CIS_URI_IS_SET_RESOURCE(&observe_new->uri) ? observe_new->uri.resourceId : -1);
      cis_response(context, NULL, NULL, mid, CIS_RESPONSE_OBSERVE);
    }
  else
    {
      st_observe_info *delnode = light_control_observe_list;
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
          delnode = (st_observe_info *)(delnode->next);
        }
      if (delnode != NULL)
        {
          light_control_observe_list = (st_observe_info *)cis_list_remove((cis_list_t *)light_control_observe_list, delnode->mid, (cis_list_t **)&delnode);
          LOGD("cis_on_observe cancel: %d/%d/%d\n",
            delnode->uri.objectId,
            CIS_URI_IS_SET_INSTANCE(&delnode->uri) ? delnode->uri.instanceId : -1,
            CIS_URI_IS_SET_RESOURCE(&delnode->uri) ? delnode->uri.resourceId : -1);

          free(delnode);
          cis_response(context, NULL, NULL, mid, CIS_RESPONSE_OBSERVE);
        }
      else
        {
          return CIS_RESPONSE_NOT_FOUND;
        }
    }
  return CIS_RET_OK;
}

void light_control_notify(void *context)
{
  st_observe_info *node = light_control_observe_list;
  cis_uri_t uri;
  cis_data_t cisData;
  cis_list_t *pInstNode;
  int nbRes = sizeof(g_resList) / sizeof(uint16_t);
  int i;
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
      pInstNode = light_control_inst;
      memcpy(&uri, &(node->uri), sizeof(cis_uri_t));
      LOGD("%s notify:%d/%d/%d",
        __func__,
        node->uri.objectId,
        CIS_URI_IS_SET_INSTANCE(&node->uri) ? node->uri.instanceId : -1,
        CIS_URI_IS_SET_RESOURCE(&node->uri) ? node->uri.resourceId : -1);
      if (!CIS_URI_IS_SET_INSTANCE(&uri) && !CIS_URI_IS_SET_RESOURCE(&uri))
        {
          while (pInstNode)
            {
              for (i = 0; i < nbRes; i++)
                {
                  uri.instanceId = ((light_control_data_t *)pInstNode)->instanceId;
                  uri.resourceId = g_resList[i];
                  light_control_get_value(context, &cisData, (light_control_data_t *)pInstNode,
                    g_resList[i]);
                  cis_uri_update(&uri);
                  if (i < nbRes - 1)
                    {
                      cis_notify(context, &uri, &cisData, node->mid, CIS_NOTIFY_CONTINUE, false);
                    }
                  else
                    {
                      cis_notify(context, &uri, &cisData, node->mid, CIS_NOTIFY_CONTENT, false);
                    }
                }
              pInstNode = pInstNode->next;
            }
        }
      else if (!CIS_URI_IS_SET_RESOURCE(&uri))
        {
          while (pInstNode)
            {
              LOGD("btxbtx %d,%d\n", pInstNode->id, uri.instanceId);
              if (((light_control_data_t *)pInstNode)->instanceId == uri.instanceId)
                {
                  for (i = 0; i < nbRes; i++)
                    {
                      uri.instanceId = ((light_control_data_t *)pInstNode)->instanceId;
                      uri.resourceId = g_resList[i];
                      light_control_get_value(context, &cisData, (light_control_data_t *)pInstNode,
                        g_resList[i]);
                      cis_uri_update(&uri);
                      if (i < nbRes - 1)
                        {
                          cis_notify(context, &uri, &cisData, node->mid, CIS_NOTIFY_CONTINUE, false);
                        }
                      else
                        {
                          cis_notify(context, &uri, &cisData, node->mid, CIS_NOTIFY_CONTENT, false);
                        }
                      }
                  }
              pInstNode = pInstNode->next;
            }
        }
      else
        {
          while (pInstNode)
            {
              if (((light_control_data_t *)pInstNode)->instanceId == uri.instanceId)
                {
                  light_control_get_value(context, &cisData, (light_control_data_t *)pInstNode,
                    uri.resourceId);
                  cis_notify(context, &uri, &cisData, node->mid, CIS_NOTIFY_CONTENT, false);
                }
              pInstNode = pInstNode->next;
            }
        }
      node = (st_observe_info *)(node->next);
    }
}

void light_control_clean(void *contextP)
{
  light_control_data_t *deleteInst;
  light_control_data_t *instance = NULL;
  st_observe_info *deleteObserveNode;
  st_observe_info *observeNode = NULL;
  instance = (light_control_data_t *)light_control_inst;

  while (instance != NULL)
    {
      deleteInst = instance;
      instance = instance->next;
      light_control_inst = CIS_LIST_RM(light_control_inst, ((cis_list_t *)deleteInst)->id, NULL);
      if (deleteInst->color)
        {
          free(deleteInst->color);
        }
      if (deleteInst->sensorUints)
        {
          free(deleteInst->sensorUints);
        }
      if (deleteInst->applicationType)
        {
          free(deleteInst->applicationType);
        }
      free(deleteInst);
    }

  observeNode = light_control_observe_list;

  while (observeNode != NULL)
    {
      deleteObserveNode = observeNode;
      observeNode = (st_observe_info *)(observeNode->next);
      light_control_observe_list = (st_observe_info *)CIS_LIST_RM(light_control_observe_list, ((cis_list_t *)deleteObserveNode)->id, NULL);
      free(deleteObserveNode);
    }
}

cis_ret_t light_control_make_sample_data(void *contextP)
{
#ifdef CIS_CTWING
  cis_addobject(contextP, LIGHT_CONTROL_OBJECT_ID, NULL, NULL);
  st_object_t *lightControlObj = cis_findObject(contextP, LIGHT_CONTROL_OBJECT_ID);
  if (lightControlObj == NULL)
    {
      LOGE("Failed to init light control object");
      return CIS_RET_ERROR;
    }
  light_control_create(contextP, 0, lightControlObj);
  light_control_create(contextP, 1, lightControlObj);
#endif
  return CIS_RET_OK;
}
