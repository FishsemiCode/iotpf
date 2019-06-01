/****************************************************************************
 * external/services/iotpf/std_object/std_object_binary_app_data_container.c
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


#include "../cis_api.h"
#include "../cis_internals.h"
#include "std_object.h"

#if CIS_OPERATOR_CTCC

typedef struct _binary_app_data_container_data_
{
  struct _binary_app_data_container_data_ *next;
  uint16_t instanceId;
} binary_app_data_container_data_t;

bool std_binary_app_data_container_data_create(st_context_t *contextP,
					   int instanceId,
                       st_object_t *objectP)
{
  binary_app_data_container_data_t *instConn=NULL;
  binary_app_data_container_data_t *targetP = NULL;
  uint8_t instBytes = 0;
  uint8_t instCount = 0;
  cis_iid_t instIndex;
  if (NULL == objectP)
    {
      return false;
    }

  // Manually create a hard-code instance
  targetP = (binary_app_data_container_data_t *)cis_malloc(sizeof(binary_app_data_container_data_t));
  if (NULL == targetP)
    {
      return false;
    }

  cis_memset(targetP, 0, sizeof(binary_app_data_container_data_t));
  targetP->instanceId = (uint16_t)instanceId;

  instConn = (binary_app_data_container_data_t * )std_object_put_binary_app_data_container(contextP, (cis_list_t*)targetP);

  instCount = CIS_LIST_COUNT(instConn);
  if (instCount == 0)
    {
      cis_free(targetP);
      return false;
    }

  if (instCount == 1)
    {
      return true;
    }

  objectP->instBitmapCount = instCount;
  instBytes = (instCount - 1) / 8 + 1;
  if (objectP->instBitmapBytes < instBytes)
    {
      if (objectP->instBitmapBytes != 0 && objectP->instBitmapPtr != NULL)
        {
          cis_free(objectP->instBitmapPtr);
        }
      objectP->instBitmapPtr = (uint8_t*)cis_malloc(instBytes);
      objectP->instBitmapBytes = instBytes;
    }
  cissys_memset(objectP->instBitmapPtr, 0, instBytes);
  targetP = instConn;
  for (instIndex = 0; instIndex < instCount; instIndex++)
    {
      uint8_t instBytePos = targetP->instanceId / 8;
      uint8_t instByteOffset = 7 - (targetP->instanceId % 8);
      objectP->instBitmapPtr[instBytePos] += 0x01 << instByteOffset;

      targetP = targetP->next;
    }
  return true;
}

static uint8_t prv_binary_app_data_container_get_value(st_context_t *contextP,
									st_data_t *dataArrayP,
									int number,
									binary_app_data_container_data_t *binaryAppDataContainerDataP)
{
  uint8_t result;
  uint16_t resId;

  for (int i = 0; i < number; i++)
    {
      if (number == 1)
        {
          resId = dataArrayP->id;
        }
      else
        {
          resId = dataArrayP->value.asChildren.array[i].id;
        }
      switch (resId)//need do error handle
        {
          case RES_BINARY_APP_DATA_CONTAINER_DATA_ID:
            {
              /*btxbtx TODO*/
              result = COAP_205_CONTENT;
              break;
            }
          default:
            return COAP_404_NOT_FOUND;
        }
    }
  return result;
}

uint8_t std_binary_app_data_container_read(st_context_t *contextP,
						uint16_t instanceId,
						int *numDataP,
						st_data_t **dataArrayP,
						st_object_t *objectP)
{
  uint8_t result;
  int i;
  // is the server asking for the full object ?
  if (*numDataP == 0)
    {
      uint16_t resList[] =
        {
          RES_BINARY_APP_DATA_CONTAINER_DATA_ID,
        };
      int nbRes = sizeof(resList) / sizeof(uint16_t);
      (*dataArrayP)->id = 0;
      (*dataArrayP)->type = DATA_TYPE_OBJECT_INSTANCE;
      (*dataArrayP)->value.asChildren.count = nbRes;
      (*dataArrayP)->value.asChildren.array = data_new(nbRes);
      cis_memset((*dataArrayP)->value.asChildren.array, 0, (nbRes) * sizeof(cis_data_t));
      if (*dataArrayP == NULL)
        {
          return COAP_500_INTERNAL_SERVER_ERROR;
        }
      *numDataP = nbRes;

      if (*dataArrayP == NULL)
        {
          return COAP_500_INTERNAL_SERVER_ERROR;
        }
      *numDataP = nbRes;
      for (i = 0; i < nbRes; i++)
        {
          (*dataArrayP)->value.asChildren.array[i].id = resList[i];
        }

      if (prv_binary_app_data_container_get_value(contextP, (*dataArrayP), nbRes, NULL) != COAP_205_CONTENT)
        {
          return COAP_500_INTERNAL_SERVER_ERROR;
        }
    }
  else
    {
      result = prv_binary_app_data_container_get_value(contextP, (*dataArrayP), 1, NULL);
    }

  return result;
}

cis_coapret_t std_binary_app_data_container_write(st_context_t *contextP,
                               const cis_data_t *value, cis_attrcount_t attrcount)
{
  if (value == NULL || attrcount != 1)
    {
      LOGW("%s: invalid param", __func__);
      return COAP_400_BAD_REQUEST;
    }

  /*BTTODO: parse the opaque data to call the contextP->callback to do the work*/
  return COAP_NO_ERROR;
}


void std_binary_app_data_container_clean(st_context_t *contextP)
{
  binary_app_data_container_data_t *deleteInst;
  binary_app_data_container_data_t *binaryAppDataContainerInstance = NULL;
  binaryAppDataContainerInstance = (binary_app_data_container_data_t *)(contextP->binary_app_data_container_inst);
  while (binaryAppDataContainerInstance != NULL)
    {
      deleteInst = binaryAppDataContainerInstance;
      binaryAppDataContainerInstance = deleteInst->next;
      std_object_remove_conn(contextP, (cis_list_t*)(deleteInst));
      cis_free(deleteInst);
    }
}
#endif

