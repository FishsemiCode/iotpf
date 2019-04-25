/****************************************************************************
 * external/services/iotpf/cis_data.c
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

#include "cis_internals.h"
#include <float.h>
#include "cis_log.h"

// dataP array length is assumed to be 1.
static size_t prv_textSerialize(st_data_t *dataP,
                             uint8_t **bufferP)
{
  switch (dataP->type)
    {
      case DATA_TYPE_STRING:
        {
          *bufferP = (uint8_t *)cis_malloc(dataP->asBuffer.length);
          if (*bufferP == NULL)
            {
              return 0;
            }
          cis_memcpy(*bufferP, dataP->asBuffer.buffer, dataP->asBuffer.length);
          return dataP->asBuffer.length;
        }
      case DATA_TYPE_INTEGER:
        {
          return utils_int64ToPlainText(dataP->value.asInteger, bufferP);
        }
      case DATA_TYPE_FLOAT:
        {
          return utils_float64ToPlainText(dataP->value.asFloat, bufferP);
        }
      case DATA_TYPE_BOOL:
        return utils_boolToPlainText(dataP->value.asBoolean, bufferP);
      case DATA_TYPE_OPAQUE:
      case DATA_TYPE_UNDEFINE:
      default:
        {
          return 0;
        }
    }
}

static int prv_setBuffer(st_data_t *dataP,
                      uint8_t *buffer,
                      size_t bufferLen)
{
  dataP->asBuffer.buffer = (uint8_t *)cis_malloc(bufferLen);
  if (dataP->asBuffer.buffer == NULL)
    {
      data_free(1, dataP);
      return 0;
    }
  dataP->asBuffer.length = bufferLen;
  cis_memcpy(dataP->asBuffer.buffer, buffer, bufferLen);
  return 1;
}

st_data_t *data_new(int size)
{
  st_data_t *dataP;
  if (size <= 0)
    {
      return NULL;
    }
  dataP = (st_data_t *)cis_malloc(size * sizeof(st_data_t));
  if (dataP != NULL)
    {
      cis_memset(dataP, 0, size * sizeof(st_data_t));
    }
  return dataP;
}



bool data_copy(st_data_t *dst,const st_data_t *src)
{
  int32_t i;
  int32_t count;
  if (src == NULL)
    {
      return false;
    }
  if(dst == NULL)
    {
      dst = data_new(1);
      if(dst == NULL)
        {
          return false;
        }
    }
  switch (src->type)
    {
      case DATA_TYPE_OBJECT_INSTANCE:
      case DATA_TYPE_OBJECT:
        {
          dst->id = src->id;
          dst->type = src->type;
          count = src->value.asChildren.count;
          dst->value.asChildren.count = count;
          dst->value.asChildren.array = data_new(count);
          if (dst->value.asChildren.array == NULL)
            {
              cis_free(dst);
              return false;
            }
          for (i = 0;i<count;i++)
            {
              data_copy(&dst->value.asChildren.array[i],&src->value.asChildren.array[i]);
            }
          break;
        }
      default:
        {
          if (src->asBuffer.buffer != NULL && src->asBuffer.length > 0)
            {
              dst->asBuffer.length = src->asBuffer.length;
              dst->asBuffer.buffer = utils_bufferdup(src->asBuffer.buffer,src->asBuffer.length);
            }
          dst->id = src->id;
          dst->type = src->type;
          cissys_memcpy(&dst->value,&src->value,sizeof(src->value));
        }
      break;
    }
  return true;
}




void data_free(int size, st_data_t *dataP)
{
  int i;
  if (size == 0 || dataP == NULL)
    {
      return;
    }
  for (i = 0; i < size; i++)
    {
      switch (dataP[i].type)
        {
          case DATA_TYPE_OBJECT_INSTANCE:
          case DATA_TYPE_OBJECT:
            {
              data_free(dataP[i].value.asChildren.count, dataP[i].value.asChildren.array);
            }
            break;
          case DATA_TYPE_STRING:
          case DATA_TYPE_OPAQUE:
            {
              if (dataP[i].asBuffer.buffer != NULL)
                {
                  cis_free(dataP[i].asBuffer.buffer);
                  dataP[i].asBuffer.length = 0;
                }
            }
            break;
          default:
            break;
        }
    }
  cis_free(dataP);
}

void data_encode_string(const char *string,
                           st_data_t *dataP)
{
  size_t len;
  int res;
  LOGD("\"%s\"", string);
  if (string == NULL)
    {
      len = 0;
    }
  else
    {
      for (len = 0; string[len] != 0; len++);
    }
  if (len == 0)
    {
      dataP->asBuffer.length = 0;
      dataP->asBuffer.buffer = NULL;
      res = 1;
    }
  else
    {
      res = prv_setBuffer(dataP, (uint8_t *)string, len);
    }
  if (res == 1)
    {
      dataP->type = DATA_TYPE_STRING;
    }
  else
    {
      dataP->type = DATA_TYPE_UNDEFINE;
    }
}


void data_encode_opaque(uint8_t *buffer,
 size_t length,
 st_data_t *dataP)
{
  int result;
  if (length == 0)
    {
      dataP->asBuffer.length = 0;
      dataP->asBuffer.buffer = NULL;
      dataP->type = DATA_TYPE_UNDEFINE;
      return;
    }
  result = prv_setBuffer(dataP, buffer, length);
  if (result <= 0)
    {
      return;
    }
  utils_opaqueToInt(buffer, length, &dataP->value.asInteger);
  dataP->type = DATA_TYPE_OPAQUE;
}

void data_encode_nstring(const char *string,
                            size_t length,
                            st_data_t *dataP)
{
  data_encode_opaque((uint8_t *)string, length, dataP);
  if (dataP->type == DATA_TYPE_OPAQUE)
    {
      dataP->type = DATA_TYPE_STRING;
    }
}

void data_encode_int(int64_t value,
                        st_data_t *dataP)
{
  dataP->type = DATA_TYPE_INTEGER;
  dataP->value.asInteger = value;
}

int data_decode_int(const st_data_t *dataP,
                       int64_t *valueP)
{
  int result;
  switch (dataP->type)
    {
      case DATA_TYPE_INTEGER:
        {
          *valueP = dataP->value.asInteger;
          result = 1;
          break;
        }
      case DATA_TYPE_STRING:
        {
          result = utils_plainTextToInt64(dataP->asBuffer.buffer, dataP->asBuffer.length, valueP);
          break;
        }
      case DATA_TYPE_OPAQUE:
        {
          result = utils_opaqueToInt(dataP->asBuffer.buffer, dataP->asBuffer.length, valueP);
          if (result == (int)dataP->asBuffer.length)
            {
              result = 1;
            }
          else
            {
              result = 0;
            }
          break;
        }
      default:
        return 0;
    }
  return result;
}

void data_encode_float(double value,
                          st_data_t *dataP)
{
  dataP->type = DATA_TYPE_FLOAT;
  dataP->value.asFloat = value;
}

int data_decode_float(const st_data_t *dataP,
                         double *valueP)
{
  int result;
  switch (dataP->type)
    {
      case DATA_TYPE_FLOAT:
        {
          *valueP = dataP->value.asFloat;
          result = 1;
          break;
        }
      case DATA_TYPE_INTEGER:
        {
          *valueP = (double)dataP->value.asInteger;
          result = 1;
          break;
        }
      case DATA_TYPE_STRING:
        {
          result = utils_plainTextToFloat64(dataP->asBuffer.buffer, dataP->asBuffer.length, valueP);
          break;
        }
      case DATA_TYPE_OPAQUE:
        {
          result = utils_opaqueToFloat(dataP->asBuffer.buffer, dataP->asBuffer.length, valueP);
          if (result == (int)dataP->asBuffer.length)
            {
              result = 1;
            }
          else
            {
              result = 0;
            }
          break;
        }
      default:
        return 0;
    }
  return result;
}

void data_encode_bool(bool value,
                         st_data_t *dataP)
{
  dataP->type = DATA_TYPE_BOOL;
  dataP->value.asBoolean = value;
}

int data_decode_bool(const st_data_t *dataP,
                        bool *valueP)
{
  int result;
  switch (dataP->type)
    {
      case DATA_TYPE_BOOL:
        {
          *valueP = dataP->value.asBoolean;
          result = 1;
          break;
        }
      case DATA_TYPE_STRING:
        {
          if (dataP->asBuffer.length != 1)
            {
              return 0;
            }
          switch (dataP->asBuffer.buffer[0])
            {
              case '0':
                {
                  *valueP = false;
                  result = 1;
                  break;
                }
              case '1':
                {
                  *valueP = true;
                  result = 1;
                  break;
                }
              default:
                {
                  result = 0;
                  break;
                }
            }
            break;
        }
      case DATA_TYPE_OPAQUE:
        {
          if (dataP->asBuffer.length != 1)
            {
              return 0;
            }
          switch (dataP->asBuffer.buffer[0])
            {
              case 0:
                {
                  *valueP = FALSE;
                  result = 1;
                  break;
                }
              case 1:
                {
                  *valueP = true;
                  result = 1;
                  break;
                }
              default:
                {
                  result = 0;
                  break;
                }
            }
            break;
        }
      default:
        {
          result = 0;
          break;
        }
    }
  return result;
}

void data_include(st_data_t *subDataP,
                     size_t count,
                     st_data_t *dataP)
{
  LOGD("count: %d", count);
  if (subDataP == NULL || count == 0)
    {
      return;
    }
  switch (subDataP[0].type)
    {
      case DATA_TYPE_STRING:
      case DATA_TYPE_OPAQUE:
      case DATA_TYPE_INTEGER:
      case DATA_TYPE_FLOAT:
      case DATA_TYPE_BOOL:
        {
          dataP->type = DATA_TYPE_OBJECT_INSTANCE;
          break;
        }
      case DATA_TYPE_OBJECT_INSTANCE:
        {
          dataP->type = DATA_TYPE_OBJECT;
          break;
        }
      default:
        return;
    }
  dataP->value.asChildren.count = count;
  dataP->value.asChildren.array = subDataP;
}

void data_encode_instances(st_data_t *subDataP,
                              size_t count,
                              st_data_t *dataP)
{
  LOGD("count: %d", count);
  data_include(subDataP, count, dataP);
  dataP->type = DATA_TYPE_OBJECT_INSTANCE;
}

//NOTE: return data COUNT included int dataP
int data_parse(st_uri_t *uriP,
                  uint8_t *buffer,
                  size_t bufferLen,
                  et_media_type_t format,
                  st_data_t **dataP)
{
  LOGD("data_parse format: %s, bufferLen: %d", STR_MEDIA_TYPE(format), bufferLen);
  LOG_URI("data_parse",uriP);
  switch (format)
    {
      case LWM2M_CONTENT_TEXT:
        {
          if (!CIS_URI_IS_SET_RESOURCE(uriP))
            {
              return 0;
            }
          *dataP = data_new(1);
          if (*dataP == NULL)
            {
              return 0;
            }
          (*dataP)->id = uriP->resourceId;
          (*dataP)->type = DATA_TYPE_STRING;
          return prv_setBuffer(*dataP, buffer, bufferLen);
        }
      case LWM2M_CONTENT_OPAQUE:
        {
          if (!CIS_URI_IS_SET_RESOURCE(uriP))
            {
              return 0;
            }
          *dataP = data_new(1);
          if (*dataP == NULL)
            {
              return 0;
            }
          (*dataP)->id = uriP->resourceId;
          (*dataP)->type = DATA_TYPE_OPAQUE;
          return prv_setBuffer(*dataP, buffer, bufferLen);
        }
      case LWM2M_CONTENT_TLV_NEW:
      case LWM2M_CONTENT_TLV:
        {
          return tlv_parse(buffer, bufferLen,dataP);
        }
      default:
        return 0;
    }
}

int data_serialize(st_uri_t *uriP,
                         int size,
                         st_data_t *dataP,
                         et_media_type_t *formatP,
                         uint8_t **bufferP)
{
  if (*formatP == LWM2M_CONTENT_TEXT
    || *formatP == LWM2M_CONTENT_OPAQUE)
    {
      if (size != 1
        || !CIS_URI_IS_SET_RESOURCE(uriP)
        || dataP->type == DATA_TYPE_OBJECT
        || dataP->type == DATA_TYPE_OBJECT_INSTANCE
        || dataP->type == DATA_TYPE_MULTIPLE_RESOURCE)
        {
          *formatP = LWM2M_CONTENT_TLV;
        }
    }
  if (*formatP == LWM2M_CONTENT_TEXT
    && dataP->type == DATA_TYPE_OPAQUE)
    {
      *formatP = LWM2M_CONTENT_OPAQUE;
    }
  LOGD("Serialize final format: %s", STR_MEDIA_TYPE(*formatP));
  switch (*formatP)
    {
      case LWM2M_CONTENT_TEXT:
        {
          return prv_textSerialize(dataP, bufferP);
        }
      case LWM2M_CONTENT_OPAQUE:
        {
          *bufferP = (uint8_t *)cis_malloc(dataP->asBuffer.length);
          if (*bufferP == NULL)
            {
              return 0;
            }
          cis_memcpy(*bufferP, dataP->asBuffer.buffer, dataP->asBuffer.length);
          if(dataP->asBuffer.length == 0)
            {
              return -1;
            }
          return dataP->asBuffer.length;
        }
      case LWM2M_CONTENT_TLV_NEW:
      case LWM2M_CONTENT_TLV:
        {
          uint8_t baseUriStr[URI_MAX_STRING_LEN];
          size_t baseUriLen = 0;
          uri_depth_t rootLevel;
          bool isInstance;
          baseUriLen = uri_toString(uriP, baseUriStr, URI_MAX_STRING_LEN, &rootLevel);
          if (baseUriLen <= 0)
            {
              return -1;
            }
          if (rootLevel == URI_DEPTH_RESOURCE_INSTANCE)
            {
              isInstance = true;
            }
          else
            {
              isInstance = false;
            }
          return cis_tlv_serialize(isInstance, size, dataP, bufferP);
        }
      default:
        return 0;
    }
}
