/****************************************************************************
 * external/services/iotpf/std_object/std_object_conn_moniter.c
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
#include "../cis_if_sys.h"

#include "std_object.h"

#if CIS_ENABLE_UPDATE || CIS_ENABLE_MONITER || CIS_OPERATOR_CTCC

typedef struct _conn_moniter_data_
{
  struct _conn_moniter_data_ *next;
  uint16_t instanceId;
  uint8_t network_bearer;
  uint32_t radio_signal_strength;
  uint32_t cell_id;
} conn_moniter_data_t;

bool std_conn_moniter_create(st_context_t *contextP,
					   int instanceId,
                       st_object_t *connObj)
{
  conn_moniter_data_t *instConn=NULL;
  conn_moniter_data_t *targetP = NULL;
  uint8_t instBytes = 0;
  uint8_t instCount = 0;
  cis_iid_t instIndex;
  if (NULL == connObj)
    {
      return false;
    }

  // Manually create a hard-code instance
  targetP = (conn_moniter_data_t *)cis_malloc(sizeof(conn_moniter_data_t));
  if (NULL == targetP)
    {
      return false;
    }

  cis_memset(targetP, 0, sizeof(conn_moniter_data_t));
  targetP->instanceId = (uint16_t)instanceId;

  targetP->network_bearer = 7;
  targetP->radio_signal_strength = 0;
  targetP->cell_id = 0;

  instConn = (conn_moniter_data_t * )std_object_put_conn(contextP, (cis_list_t*)targetP);

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

  connObj->instBitmapCount = instCount;
  instBytes = (instCount - 1) / 8 + 1;
  if (connObj->instBitmapBytes < instBytes)
    {
      if (connObj->instBitmapBytes != 0 && connObj->instBitmapPtr != NULL)
        {
          cis_free(connObj->instBitmapPtr);
        }
      connObj->instBitmapPtr = (uint8_t*)cis_malloc(instBytes);
      connObj->instBitmapBytes = instBytes;
    }
  cissys_memset(connObj->instBitmapPtr, 0, instBytes);
  targetP = instConn;
  for (instIndex = 0; instIndex < instCount; instIndex++)
    {
      uint8_t instBytePos = targetP->instanceId / 8;
      uint8_t instByteOffset = 7 - (targetP->instanceId % 8);
      connObj->instBitmapPtr[instBytePos] += 0x01 << instByteOffset;

      targetP = targetP->next;
    }
  return true;
}

static uint8_t prv_conn_moniter_get_value(st_context_t *contextP,
									st_data_t *dataArrayP,
									int number,
									conn_moniter_data_t *devDataP)
{
  uint8_t result;
  uint16_t resId;
  st_data_t *dataP;

  for (int i = 0; i < number; i++)
    {
      if (number == 1)
        {
          resId = dataArrayP->id;
          dataP = dataArrayP;
        }
      else
        {
          resId = dataArrayP->value.asChildren.array[i].id;
          dataP = dataArrayP->value.asChildren.array + i;
        }
      switch (resId)//need do error handle
        {
          case RES_CONN_NETWORK_BEARER_ID:
            {
              uint32_t network_bear = 7;
              data_encode_int(network_bear, dataP);
              result = COAP_205_CONTENT;
              break;
            }

          case RES_CONN_RADIO_SIGNAL_STRENGTH_ID:
            {
              uint32_t radio_signal_strenght = cissys_getRadioSignalStrength();
              data_encode_int(radio_signal_strenght, dataP);
              result = COAP_205_CONTENT;
              break;
            }

          case RES_CONN_CELL_ID:
            {
              uint32_t cell_id = cissys_getCellId();
              data_encode_int(cell_id, dataP);
              result = COAP_205_CONTENT;
              break;
            }
        #if CIS_ENABLE_MONITER
          case RES_CONN_SINR:
            {
              int8_t sinr = cissys_getSINR();
              if (sinr != CIS_RET_UNSUPPORTIVE)
                {
                  data_encode_int(sinr, dataP);
                }
              break;
            }
        #endif
          default:
            return COAP_404_NOT_FOUND;
        }
    }
  return result;
}

uint8_t std_conn_moniter_read(st_context_t *contextP,
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
          RES_CONN_RADIO_SIGNAL_STRENGTH_ID,
        #if CIS_ENABLE_MONITER
          RES_CONN_CELL_ID,
          RES_CONN_SINR,
        #else
          RES_CONN_CELL_ID
        #endif
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

      if (prv_conn_moniter_get_value(contextP, (*dataArrayP), nbRes, NULL) != COAP_205_CONTENT)
        {
          return COAP_500_INTERNAL_SERVER_ERROR;
        }
    }
  else
    {
      result = prv_conn_moniter_get_value(contextP, (*dataArrayP), 1, NULL);
    }

  return result;
}

uint8_t std_conn_moniter_discover(st_context_t *contextP,
							uint16_t instanceId,
                            int *numDataP,
                            st_data_t **dataArrayP,
                            st_object_t *objectP)
{
  uint8_t result;
  int i;

  // this is a single instance object
  if (instanceId != 0)
    {
      return COAP_404_NOT_FOUND;
    }

  result = COAP_205_CONTENT;

  // is the server asking for the full object ?
  if (*numDataP == 0)
    {
      uint16_t resList[] =
        {
          RES_CONN_RADIO_SIGNAL_STRENGTH_ID,
        #if CIS_ENABLE_MONITER
          RES_CONN_CELL_ID,
          RES_CONN_SINR,
        #else
          RES_CONN_CELL_ID
        #endif
        };
      int nbRes = sizeof(resList) / sizeof(uint16_t);

      (*dataArrayP)->id = 0;
      (*dataArrayP)->type = DATA_TYPE_OBJECT_INSTANCE;
      (*dataArrayP)->value.asChildren.count = nbRes;
      (*dataArrayP)->value.asChildren.array = data_new(nbRes);
      cis_memset((*dataArrayP)->value.asChildren.array, 0, (nbRes)*sizeof(cis_data_t));
      if (*dataArrayP == NULL)
        {
          return COAP_500_INTERNAL_SERVER_ERROR;
        }
      *numDataP = nbRes;
      for (i = 0; i < nbRes; i++)
        {
          (*dataArrayP)->value.asChildren.array[i].id = resList[i];
          (*dataArrayP)->value.asChildren.array[i].type = DATA_TYPE_LINK;
          (*dataArrayP)->value.asChildren.array[i].value.asObjLink.objectId = CIS_CONNECTIVITY_OBJECT_ID;
          (*dataArrayP)->value.asChildren.array[i].value.asObjLink.instId = 0;
        }
    }
  else
    {
      for (i = 0; i < *numDataP && result == COAP_205_CONTENT; i++)
        {
          switch ((*dataArrayP)[i].id)
            {
              case RES_CONN_RADIO_SIGNAL_STRENGTH_ID:
              case RES_CONN_CELL_ID:
                break;
              default:
                result = COAP_404_NOT_FOUND;
                break;
            }
        }
    }

  return result;
}

void std_conn_moniter_clean(st_context_t *contextP)
{
  conn_moniter_data_t *deleteInst;
  conn_moniter_data_t *connInstance = NULL;
  connInstance = (conn_moniter_data_t *)(contextP->conn_inst);
  while (connInstance != NULL)
    {
      deleteInst = connInstance;
      connInstance = connInstance->next;
      std_object_remove_conn(contextP, (cis_list_t*)(deleteInst));
      cis_free(deleteInst);
    }
}
#endif

