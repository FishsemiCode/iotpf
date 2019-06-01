/****************************************************************************
 * external/services/iotpf/std_object/std_object.c
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
#include "../dtls/dtls_debug.h"


extern cis_coapret_t std_security_read(st_context_t *contextP, cis_iid_t instanceId, int *numDataP, st_data_t **dataArrayP, st_object_t *objectP);
extern cis_coapret_t std_security_write(st_context_t *contextP, cis_iid_t instanceId, int numData, st_data_t *dataArray, st_object_t *objectP);
extern cis_coapret_t std_security_discover(st_context_t *contextP, uint16_t instanceId, int *numDataP, st_data_t **dataArrayP, st_object_t *objectP);

#if CIS_ENABLE_UPDATE || CIS_ENABLE_MONITER || CIS_OPERATOR_CTCC
extern cis_coapret_t std_conn_moniter_read(st_context_t *contextP, cis_iid_t instanceId, int *numDataP, st_data_t **dataArrayP, st_object_t *objectP);
extern cis_coapret_t std_conn_moniter_discover(st_context_t *contextP, uint16_t instanceId, int *numDataP, st_data_t **dataArrayP, st_object_t *objectP);
#endif

#if CIS_OPERATOR_CTCC
extern cis_coapret_t std_binary_app_data_container_read(st_context_t *contextP, cis_iid_t instanceId, int *numDataP, st_data_t **dataArrayP, st_object_t *objectP);
#endif


#if CIS_ENABLE_UPDATE
extern cis_coapret_t std_device_read(st_context_t *contextP, cis_iid_t instanceId, int *numDataP, st_data_t **dataArrayP, st_object_t *objectP);
extern cis_coapret_t std_device_write(st_context_t *contextP, cis_iid_t instanceId, int numData, st_data_t *dataArray, st_object_t *objectP);
extern cis_coapret_t std_device_discover(st_context_t *contextP, uint16_t instanceId, int *numDataP, st_data_t **dataArrayP, st_object_t *objectP);



extern cis_coapret_t std_firmware_read(st_context_t *contextP, cis_iid_t instanceId, int *numDataP, st_data_t **dataArrayP, st_object_t *objectP);
extern cis_coapret_t std_firmware_write(st_context_t *contextP, cis_iid_t instanceId, int numData, st_data_t *dataArray, st_object_t *objectP);

extern cis_coapret_t std_firmware_execute(st_context_t *contextP, uint16_t instanceId, uint16_t resourceId, uint8_t *buffer, int length, st_object_t *objectP);
extern cis_coapret_t std_firmware_discover(st_context_t *contextP, uint16_t instanceId, int *numDataP, st_data_t **dataArrayP, st_object_t *objectP);

cis_list_t *std_object_get_firmware(st_context_t *contextP, cis_iid_t instanceId)
{
  contextP->firmware_inst = CIS_LIST_FIND(contextP->firmware_inst, instanceId);
  return contextP->firmware_inst;
}
cis_list_t *std_object_put_firmware(st_context_t *contextP, cis_list_t *targetP)
{
  contextP->firmware_inst = CIS_LIST_ADD(contextP->firmware_inst, targetP);
  return contextP->firmware_inst;
}


void std_object_remove_firmware(st_context_t *contextP, cis_list_t *targetP)
{
  contextP->firmware_inst = CIS_LIST_RM(contextP->firmware_inst, targetP->id, NULL);
}

#endif
#if CIS_ENABLE_CMIOT_OTA
extern cis_coapret_t std_poweruplog_read(st_context_t *contextP, cis_iid_t instanceId, int *numDataP, st_data_t **dataArrayP, st_object_t *objectP);
extern cis_coapret_t std_poweruplog_discover(st_context_t *contextP, uint16_t instanceId, int *numDataP, st_data_t **dataArrayP, st_object_t *objectP);

extern cis_coapret_t std_cmdhdefecvalues_read(st_context_t *contextP, cis_iid_t instanceId, int *numDataP, st_data_t **dataArrayP, st_object_t *objectP);
extern cis_coapret_t std_cmdhdefecvalues_discover(st_context_t *contextP, uint16_t instanceId, int *numDataP, st_data_t **dataArrayP, st_object_t *objectP);
extern cis_coapret_t std_cmdhdefecvalues_write(st_context_t *contextP, cis_iid_t instanceId, int numDataP, st_data_t *dataArrayP, st_object_t *objectP);
#endif

#if CIS_ENABLE_MONITER
extern cis_coapret_t std_moniter_read(st_context_t *contextP, cis_iid_t instanceId, int *numDataP, st_data_t **dataArrayP, st_object_t *objectP);
extern cis_coapret_t std_moniter_discover(st_context_t *contextP, uint16_t instanceId, int *numDataP, st_data_t **dataArrayP, st_object_t *objectP);
#endif //CIS_ENABLE_MONITER

#if CIS_ENABLE_UPDATE
#define _UPDATE_PROLOAD_OBJECT_NUMBER  (3)
#elif  CIS_OPERATOR_CTCC
#define _UPDATE_PROLOAD_OBJECT_NUMBER  (2)
#else
#define _UPDATE_PROLOAD_OBJECT_NUMBER  (0)
#endif

#if CIS_ENABLE_CMIOT_OTA
#define _CMIOT_OTA_PROLOAD_OBJECT_NUMBER  (2)
#else
#define _CMIOT_OTA_PROLOAD_OBJECT_NUMBER  (0)
#endif

#if CIS_ENABLE_MONITER
#if CIS_ENABLE_UPDATE
#define _MONITER_PROLOAD_OBJECT_NUMBER  (1)
#else
#define _MONITER_PROLOAD_OBJECT_NUMBER  (2)
#endif
#else
#define _MONITER_PROLOAD_OBJECT_NUMBER  (0)
#endif


#define PROLOAD_OBJECT_NUMBER (1 + _UPDATE_PROLOAD_OBJECT_NUMBER + _CMIOT_OTA_PROLOAD_OBJECT_NUMBER + _MONITER_PROLOAD_OBJECT_NUMBER)

static const struct st_std_object_callback_mapping std_object_callback_mapping[PROLOAD_OBJECT_NUMBER] =
{
  {
    CIS_SECURITY_OBJECT_ID, std_security_read, std_security_write, NULL, std_security_discover
  },

#if CIS_ENABLE_UPDATE
  {
    CIS_DEVICE_OBJECT_ID, std_device_read, std_device_write, NULL, std_device_discover
  },
#endif

#if CIS_ENABLE_UPDATE || CIS_OPERATOR_CTCC

  {
    CIS_CONNECTIVITY_OBJECT_ID, std_conn_moniter_read, NULL, NULL, std_conn_moniter_discover
  },
#endif

#if CIS_ENABLE_UPDATE
  {
    CIS_FIRMWARE_OBJECT_ID, std_firmware_read, std_firmware_write, std_firmware_execute, std_firmware_discover
  },
#endif

#if CIS_ENABLE_CMIOT_OTA
  {
    CIS_POWERUPLOG_OBJECT_ID, std_poweruplog_read, NULL, NULL, std_poweruplog_discover
  },
  ,
  {
    CIS_CMDHDEFECVALUES_OBJECT_ID, std_cmdhdefecvalues_read, std_cmdhdefecvalues_write, NULL, std_cmdhdefecvalues_discover
  },
#endif

#if CIS_ENABLE_MONITER
#if CIS_ENABLE_UPDATE
  {
    CIS_MONITOR_OBJECT_ID, std_moniter_read, NULL, NULL, std_moniter_discover
  },
#else
  {
    CIS_CONNECTIVITY_OBJECT_ID, std_conn_moniter_read, NULL, NULL, std_conn_moniter_discover
  },
  {
    CIS_MONITOR_OBJECT_ID, std_moniter_read, NULL, NULL, std_moniter_discover
  },
#endif
#endif//CIS_ENABLE_MONITER

#if CIS_OPERATOR_CTCC

  {
    CIS_BINARY_APP_DATA_CONTAINER_OBJECT_ID, std_binary_app_data_container_read, NULL, NULL, NULL
  },
#endif

};

bool std_object_isStdObject(cis_oid_t oid)
{
  switch (oid)
    {
    #if CIS_ENABLE_UPDATE
      case CIS_DEVICE_OBJECT_ID:
      case CIS_FIRMWARE_OBJECT_ID:
    #endif //CIS_ENABLE_UPDATE
    #if CIS_ENABLE_MONITER || CIS_ENABLE_UPDATE || CIS_OPERATOR_CTCC
      case CIS_CONNECTIVITY_OBJECT_ID:
    #endif
      case CIS_SECURITY_OBJECT_ID:
    #if CIS_ENABLE_MONITER
      case CIS_MONITOR_OBJECT_ID:
    #endif //CIS_ENABLE_MONITER
    #if CIS_ENABLE_CMIOT_OTA
      case CIS_POWERUPLOG_OBJECT_ID:
      case CIS_CMDHDEFECVALUES_OBJECT_ID:
    #endif //CIS_ENABLE_CMIOT_OTA
    #if CIS_OPERATOR_CTCC
      case CIS_BINARY_APP_DATA_CONTAINER_OBJECT_ID:
    #endif
        return true;
      default:
        return false;
    }
}

cis_list_t *std_object_get_securitys(st_context_t *contextP)
{
  return contextP->instSecurity;
}

cis_list_t *std_object_put_securitys(st_context_t *contextP, cis_list_t *targetP)
{
  contextP->instSecurity = CIS_LIST_ADD(contextP->instSecurity, targetP);
  return contextP->instSecurity;
}

void std_object_remove_securitys(st_context_t *contextP, cis_list_t *targetP)
{
  contextP->instSecurity = CIS_LIST_RM(contextP->instSecurity, targetP->id, NULL);
}


cis_coapret_t std_object_exec_handler(st_context_t *contextP, cis_iid_t instanceId, uint16_t resourceId, uint8_t *buffer, int length, st_object_t *objectP)
{
  int i = 0;
  for (i = 0; i < sizeof(std_object_callback_mapping) / sizeof(struct st_std_object_callback_mapping); i++)
    {
      if (std_object_callback_mapping[i].onExec == NULL)
        {
          continue;
        }
      if (objectP->objID == std_object_callback_mapping[i].stdObjectId)
        {
          return std_object_callback_mapping[i].onExec(contextP, instanceId, resourceId, buffer, length, objectP);
        }
    }

  return COAP_503_SERVICE_UNAVAILABLE;
}

cis_coapret_t std_object_discover_handler(st_context_t *contextP, cis_iid_t instanceId, int *numDataP, st_data_t **dataArrayP, st_object_t *objectP)
{
  int i = 0;
  for (i = 0; i < sizeof(std_object_callback_mapping) / sizeof(struct st_std_object_callback_mapping); i++)
    {
      if (std_object_callback_mapping[i].onDiscover == NULL)
        {
          continue;
        }
      if (objectP->objID == std_object_callback_mapping[i].stdObjectId)
        {
          std_object_callback_mapping[i].onDiscover(contextP, instanceId, numDataP, dataArrayP, objectP);
          return COAP_205_CONTENT;
        }
    }

  return COAP_503_SERVICE_UNAVAILABLE;
}

cis_coapret_t std_object_read_handler(st_context_t *contextP, cis_iid_t instanceId, int *numDataP, st_data_t **dataArrayP, st_object_t *objectP)
{
  int i = 0;
  for (i = 0; i < sizeof(std_object_callback_mapping) / sizeof(struct st_std_object_callback_mapping); i++)
    {
      if (std_object_callback_mapping[i].onRead == NULL)
        {
          continue;
        }
      if (objectP->objID == std_object_callback_mapping[i].stdObjectId)
        {
          std_object_callback_mapping[i].onRead(contextP, instanceId, numDataP, dataArrayP, objectP);
          return COAP_205_CONTENT;
        }
    }

  return COAP_503_SERVICE_UNAVAILABLE;
}


cis_coapret_t std_object_write_handler(st_context_t *contextP, cis_iid_t instanceId, int numData, st_data_t *dataArray, st_object_t *objectP)
{
  int i = 0;
  for (i = 0; i < sizeof(std_object_callback_mapping) / sizeof(struct st_std_object_callback_mapping); i++)
    {
      if (std_object_callback_mapping[i].onWrite == NULL)
        {
          continue;
        }
      if (objectP->objID == std_object_callback_mapping[i].stdObjectId)
        {
          std_object_callback_mapping[i].onWrite(contextP, instanceId, numData, dataArray, objectP);
          return CIS_COAP_204_CHANGED;
        }
    }
  return COAP_503_SERVICE_UNAVAILABLE;
}

#if CIS_ENABLE_UPDATE || CIS_ENABLE_MONITER || CIS_OPERATOR_CTCC
cis_list_t *std_object_get_conn(st_context_t *contextP, cis_iid_t instanceId)
{
  return CIS_LIST_FIND(contextP->conn_inst, instanceId);
}

cis_list_t* std_object_put_conn(st_context_t *contextP, cis_list_t *targetP)
{
  contextP->conn_inst = CIS_LIST_ADD(contextP->conn_inst, targetP);
  return contextP->conn_inst;
}

void std_object_remove_conn(st_context_t *contextP, cis_list_t *targetP)
{
  contextP->conn_inst = CIS_LIST_RM(contextP->conn_inst, targetP->id, NULL);
}
#endif

#if CIS_OPERATOR_CTCC
cis_list_t *std_object_get_binary_app_data_container(st_context_t *contextP, cis_iid_t instanceId)
{
  return CIS_LIST_FIND(contextP->binary_app_data_container_inst, instanceId);
}

cis_list_t* std_object_put_binary_app_data_container(st_context_t *contextP, cis_list_t *targetP)
{
  contextP->binary_app_data_container_inst = CIS_LIST_ADD(contextP->binary_app_data_container_inst, targetP);
  return contextP->binary_app_data_container_inst;
}

void std_object_remove_binary_app_data_container(st_context_t *contextP, cis_list_t *targetP)
{
  contextP->binary_app_data_container_inst = CIS_LIST_RM(contextP->binary_app_data_container_inst, targetP->id, NULL);
}
#endif


#if CIS_ENABLE_UPDATE
cis_list_t *std_object_get_device(st_context_t *contextP, cis_iid_t instanceId)
{
  contextP->device_inst = CIS_LIST_FIND(contextP->device_inst, instanceId);
  return contextP->device_inst;
}

cis_list_t* std_object_put_device(st_context_t *contextP, cis_list_t *targetP)
{
  contextP->device_inst = CIS_LIST_ADD(contextP->device_inst, targetP);
  return contextP->device_inst;
}

void std_object_remove_device(st_context_t *contextP, cis_list_t *targetP)
{
  contextP->device_inst = CIS_LIST_RM(contextP->device_inst, targetP->id, NULL);
}
#endif

#if CIS_ENABLE_MONITER

cis_list_t *std_object_get_moniter(st_context_t *contextP, cis_iid_t instanceId)
{
  contextP->conn_inst = CIS_LIST_FIND(contextP->moniter_inst, instanceId);
  return contextP->conn_inst;
}

cis_list_t *std_object_put_moniter(st_context_t *contextP, cis_list_t *targetP)
{
  contextP->moniter_inst = CIS_LIST_ADD(contextP->moniter_inst, targetP);
  return contextP->moniter_inst;
}

void std_object_remove_moniter(st_context_t *contextP, cis_list_t *targetP)
{
  contextP->moniter_inst = CIS_LIST_RM(contextP->moniter_inst, targetP->id, NULL);
}

#endif //CIS_ENABLE_MONITER

cis_coapret_t std_object_writeInstance(st_context_t *contextP, st_uri_t *uriP, st_data_t *dataP)
{
  st_object_t *targetP;
  cis_coapret_t result;

  targetP = NULL;
  if (uriP->objectId == std_object_security)
    {
      targetP = (st_object_t *)CIS_LIST_FIND(contextP->objectList, uriP->objectId);
    }

  if (NULL == targetP)
    {
      return CIS_COAP_404_NOT_FOUND;
    }
  result = std_object_write_handler(contextP, uriP->instanceId, 1, dataP, targetP);
  return result;
}


#if CIS_ENABLE_CMIOT_OTA
cis_list_t *std_object_get_poweruplog(st_context_t *contextP, cis_iid_t instanceId)
{
  contextP->poweruplog_inst = CIS_LIST_FIND(contextP->poweruplog_inst, instanceId);
  return contextP->poweruplog_inst;
}

cis_list_t *std_object_put_poweruplog(st_context_t *contextP, cis_list_t *targetP)
{
  contextP->poweruplog_inst = CIS_LIST_ADD(contextP->poweruplog_inst, targetP);
  return contextP->poweruplog_inst;
}

void std_object_remove_poweruplog(st_context_t *contextP, cis_list_t *targetP)
{
  contextP->poweruplog_inst = CIS_LIST_RM(contextP->poweruplog_inst, targetP->id, NULL);
}


cis_list_t* std_object_get_cmdhdefecvalues(st_context_t *contextP, cis_iid_t instanceId)
{
  contextP->cmdhdefecvalues_inst = CIS_LIST_FIND(contextP->cmdhdefecvalues_inst, instanceId);
  return contextP->cmdhdefecvalues_inst;
}

cis_list_t* std_object_put_cmdhdefecvalues(st_context_t *contextP, cis_list_t *targetP)
{
  contextP->cmdhdefecvalues_inst = CIS_LIST_ADD(contextP->cmdhdefecvalues_inst, targetP);
  return contextP->cmdhdefecvalues_inst;
}

void std_object_remove_cmdhdefecvalues(st_context_t *contextP, cis_list_t *targetP)
{
  contextP->cmdhdefecvalues_inst = CIS_LIST_RM(contextP->cmdhdefecvalues_inst, targetP->id, NULL);
}
#endif
