/****************************************************************************
 * external/services/iotpf/cis_core.c
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
#include <arpa/inet.h>
#include "cis_internals.h"
#include "cis_api.h"
#include "cis_if_sys.h"
#include "cis_if_net.h"
#include "std_object/std_object.h"
#include "cis_log.h"
#include "cis_object_defs.h"

#if CIS_ENABLE_DTLS
#include "dtls/dtls.h"
#endif

#if CIS_ENABLE_DM
#include "dm_endpoint.h"
#endif

#if CIS_ENABLE_MEMORYTRACE
static cis_time_t g_tracetime = 0;
#endif//CIS_ENABLE_MEMORY_TRACE


//private function.
static void prv_localDeinit(st_context_t **context);
static cis_data_t *prv_dataDup(const cis_data_t *src);
//static st_object_t* prv_findObject(st_context_t* context,cis_oid_t objectid);
static void prv_deleteTransactionList(st_context_t *context);
static void prv_deleteObservedList(st_context_t *context);
static void prv_deleteRequestList(st_context_t *context);
static void prv_deleteNotifyList(st_context_t *context);
static void prv_deleteObjectList(st_context_t *context);
static void prv_deleteServer(st_context_t *context);

static cis_ret_t prv_onNetEventHandler(cisnet_t netctx, cisnet_event_t id, void *param, void *context);
static int prv_makeDeviceName(char **name);
cis_ret_t prv_onSySEventHandler(cissys_event_t id, void *param, void *userData, int *len);

#if CIS_ENABLE_CMIOT_OTA
static int prv_makeDeviceName_ota(char **name);
#endif

static struct st_callback_info *g_callbackList = NULL;
static pthread_t g_cis_taskthread_tid = -1;
static int g_cis_taskthread_quit = -1;


cis_ret_t cis_uri_make(cis_oid_t oid, cis_iid_t iid, cis_rid_t rid, cis_uri_t *uri)
{
  if (uri == NULL)
    {
      return CIS_RET_ERROR;
    }
  cis_memset(uri, 0, sizeof(st_uri_t));
  uri->objectId = 0;
  uri->instanceId = 0;
  uri->resourceId = 0;

  if (oid > 0 && oid <= URI_MAX_ID)
    {
      uri->objectId = (cis_oid_t)oid;
      uri->flag |= URI_FLAG_OBJECT_ID;
    }

  if (iid >= 0 && iid <= URI_MAX_ID)
    {
      uri->instanceId = (cis_iid_t)iid;
      uri->flag |= URI_FLAG_INSTANCE_ID;
    }
  if (rid > 0 && rid <= URI_MAX_ID)
    {
      uri->resourceId = (cis_rid_t)rid;
      uri->flag |= URI_FLAG_RESOURCE_ID;
    }
  return CIS_RET_OK;
}

cis_ret_t cis_uri_update(cis_uri_t *uri)
{
  return cis_uri_make(uri->objectId, uri->instanceId, uri->resourceId, uri);
}

cis_ret_t cis_version(cis_version_t *version)
{
  if (version == NULL)
    {
      return CIS_RET_PARAMETER_ERR;
    }
  version->major = CIS_VERSION_MAJOR;
  version->minor = CIS_VERSION_MINOR;
  version->micro = CIS_VERSION_MICRO;
#if CIS_ENABLE_DM
  version->dm_major = DM_VERSION_MAJOR;
  version->dm_minor = DM_VERSION_MINOR;
  version->dm_micro = DM_VERSION_MICRO;
#endif
  return CIS_RET_OK;
}
#if CIS_ENABLE_DM
char *cis_get_version(char *ver)
{
  char i = 0, j = 15;
  utils_snprintf(ver + (i++), j--, "%c", 'V');
  utils_snprintf(ver + (i++), j--, "%d", CIS_VERSION_MAJOR);
  utils_snprintf(ver + (i++), j--, "%c", '.');
  utils_snprintf(ver + (i++), j--, "%d", CIS_VERSION_MINOR);
  utils_snprintf(ver + (i++), j--, "%c", '.');
  utils_snprintf(ver + (i++), j--, "%d", CIS_VERSION_MICRO);
  utils_snprintf(ver + (i++), j--, "%c", '_');
  utils_snprintf(ver + (i++), j--, "%c", 'D');
  utils_snprintf(ver + (i++), j--, "%d", DM_VERSION_MAJOR);
  utils_snprintf(ver + (i++), j--, "%c", '.');
  utils_snprintf(ver + (i++), j--, "%d", DM_VERSION_MINOR);
  utils_snprintf(ver + (i++), j--, "%c", '.');
  utils_snprintf(ver + (i++), j--, "%d", DM_VERSION_MICRO);
  utils_snprintf(ver + (i++), j--, "%c", '\0');
  return ver;
}
#endif

#if CIS_ENABLE_DTLS
int send_to_peer(
	dtls_context_t  *ctx,
	const session_t *session,
	uint8_t         *data,
	size_t           len)
{
  st_context_t *dev;
  void *sessionH;
  coap_status_t result = COAP_500_INTERNAL_SERVER_ERROR;
  dev = (st_context_t*)ctx->app;

  if (NULL == dev)
    {
      return -1;
    }
  if (dev->stateStep == PUMP_STATE_BOOTSTRAPPING)
    {
      sessionH = dev->bootstrapServer->sessionH;
    }
  else
    {
      sessionH = dev->server->sessionH;
    }
  if (cisnet_write((cisnet_t)sessionH, data, len) == CIS_RET_OK)
    {
      LOGI("-[Pack]Send Buffer %d bytes---", len);
      LOG_BUF("Send", data, len);
      result = COAP_NO_ERROR;
    }
  else
    {
      LOGE("ERROR:send buffer failed.");
      result = COAP_500_INTERNAL_SERVER_ERROR;
    }
  return result;
}

static int read_from_peer(
	dtls_context_t  *ctx,
	const session_t *session,
	uint8_t         *data,
	size_t           len)
{
  st_context_t *dev;
  cisnet_t netContext = NULL;
  dev = (st_context_t*)ctx->app;
  if (NULL == dev)
    {
      return -1;
    }

  if (dev->server == NULL && dev->bootstrapServer == NULL)
    {
      return false;
    }

  if (dev->server != NULL)
    {
      netContext = (cisnet_t)dev->server->sessionH;
    }
  else
    {
      if (dev->bootstrapServer != NULL)
        {
          netContext = (cisnet_t)dev->bootstrapServer->sessionH;
        }
    }

  if (netContext == NULL)
    {
      LOGE("ERROR:net context invalid in packet_step.");
      return false;
    }
  LOG_BUF("dencrypt", data, len);
  packet_handle_packet(dev, (void*)netContext, data, len);
  return 0;
}
extern int get_psk_info(
	dtls_context_t *ctx,
	const session_t *session,
	dtls_credentials_type_t type,
	const unsigned char *id, size_t id_len,
	unsigned char *result, size_t result_length
);

int dtls_event(
	dtls_context_t *ctx,
	const session_t *session,
	dtls_alert_level_t alert_level,
	unsigned short code)
{
  if (code == DTLS_EVENT_CONNECTED)
    {
    #if CIS_TWO_MCU
      cisat_wakeup_pump();
    #else
      cisapi_wakeup_pump();
    #endif
    }
  return 0;
}


static dtls_handler_t dtls_cb =
{
  send_to_peer,
  read_from_peer,
  dtls_event,
#if CIS_ENABLE_PSK
  get_psk_info,
#endif
};
#endif

#if CIS_ENABLE_CMIOT_OTA
uint8_t cis_ota_get_version(char *ver)
{
  char i=0, j=15;
  uint8_t len = 0;
  utils_snprintf(ver+(i++), j--, "%c", 'V');
  utils_snprintf(ver+(i++), j--, "%d", CIS_VERSION_MAJOR);
  utils_snprintf(ver+(i++), j--, "%c", '.');
  utils_snprintf(ver+(i++), j--, "%d", CIS_VERSION_MINOR);
  utils_snprintf(ver+(i++), j--, "%c", '.');
  utils_snprintf(ver+(i++), j--, "%d", CIS_VERSION_MICRO);
  utils_snprintf(ver+(i++), j--, "%c", '_');
  utils_snprintf(ver+(i++), j--, "%c", 'D');
  utils_snprintf(ver+(i++), j--, "%d", OTA_VERSION_MAJOR);
  utils_snprintf(ver+(i++), j--, "%c", '.');
  utils_snprintf(ver+(i++), j--, "%d", OTA_VERSION_MINOR);
  utils_snprintf(ver+(i++), j--, "%c", '.');
  utils_snprintf(ver+(i++), j--, "%d", OTA_VERSION_MICRO);
  utils_snprintf(ver+(i++), j--, "%c", '\0');
  len = strlen(ver);
  return len;
}
#endif

void *taskThread(void *obj)
{
  st_context_t *contextP = (st_context_t *)obj;
  pthread_setname_np(pthread_self(), "cis_onenet_taskthread");
  while (!g_cis_taskthread_quit)
    {
      struct st_callback_info *node;
      if (g_callbackList == NULL)
        {
          cissys_sleepms(100);
          continue;
        }
      node = g_callbackList;
      g_callbackList = g_callbackList->next;

      switch (node->flag)
        {
          case 0:
            break;
          case SAMPLE_CALLBACK_READ:
            {
              contextP->callback.onRead(contextP, &(node->uri), node->mid);
            }
            break;
          case SAMPLE_CALLBACK_DISCOVER:
            {
              contextP->callback.onDiscover(contextP, &(node->uri), node->mid);
            }
            break;
          case SAMPLE_CALLBACK_WRITE:
            {
              //write
              contextP->callback.onWrite(contextP, &(node->uri), node->param.asWrite.value, node->param.asWrite.count, node->mid);
              cis_data_t *data = node->param.asWrite.value;
              cis_attrcount_t count = node->param.asWrite.count;
              for (int i = 0; i < count; i++)
                {
                  if (data[i].type == cis_data_type_string || data[i].type == cis_data_type_opaque)
                    {
                      if (data[i].asBuffer.buffer != NULL)
                        {
                          cis_free(data[i].asBuffer.buffer);
                        }
                    }
                }
              cis_free(data);
            }
            break;
          case SAMPLE_CALLBACK_EXECUTE:
            {
              //exec and notify
              contextP->callback.onExec(contextP, &(node->uri), node->param.asExec.buffer, node->param.asExec.length, node->mid);
              cis_free(node->param.asExec.buffer);
            }
            break;
          case SAMPLE_CALLBACK_SETPARAMS:
            {
              //set parameters and notify
              contextP->callback.onSetParams(contextP, &(node->uri), node->param.asObserveParam.params, node->mid);
            }
            break;
          case SAMPLE_CALLBACK_OBSERVE:
            {
              contextP->callback.onObserve(contextP, &(node->uri), node->param.asObserve.flag, node->mid);
              break;
            }
          case SAMPLE_CALLBACK_EVENT:
            {
              //set parameters and notify
              contextP->callback.onEvent(contextP, node->param.asEventParam.eid, node->param.asEventParam.param);
            }
            break;
          default:
            break;
        }
      cis_free(node);
    }
  pthread_exit(NULL);
  return NULL;
}


cis_ret_t cis_init(void **context, void *configPtr, uint16_t configLen, void *DMconf)
{
  cis_cfgret_t configRet;
  cisnet_callback_t netCallback;
  cissys_callback_t sys_callback;
  st_context_t *contextP;
  char *targetServerHost;
  char *deviceName = NULL;
  cisnet_config_t netConfig;
  st_serialize_t serialize;
  void *configContext = NULL;
#if CIS_ENABLE_DM
  char *dmUpdeviceName = NULL;
#endif
#if CIS_ENABLE_AUTH
  char *auth_code = NULL;
#endif
#if CIS_ENABLE_DTLS
  char *config_psk = NULL;
#endif

  if (*context != NULL)
    {
      return CIS_RET_EXIST;
    }

  LOGD("api cis_init");
  LOGI("version:%d.%d.%d", CIS_VERSION_MAJOR, CIS_VERSION_MINOR, CIS_VERSION_MICRO);

  contextP = (st_context_t*)cis_malloc(sizeof(st_context_t));
  if (NULL == contextP)
    {
      return CIS_RET_ERROR;
    }

#if CIS_ENABLE_UPDATE_MCU
  cis_set_sota_info("2.2.0", 500);
#endif
  cis_memset(contextP, 0, sizeof(st_context_t));
  contextP->nextMid = (uint16_t)cissys_rand();
  (*context) = contextP;

  //deal with configuration data
  if (cis_config_init(&configContext, configPtr, configLen) < 0)
    {
      return CIS_RET_ERROR;
    }

  LOGD("----------------\n");
  LOGD("DEBUG CONFIG INIT INFORMATION.");
#if CIS_ENABLE_DM
  if (NULL != DMconf)
    {
      dmSdkInit(DMconf);
      printf("For Dm\n");
      int ret = genDmRegEndpointName(&deviceName, DMconf);
      if (ret < 0)
        {
          LOGE("ERROR:Get device name error from IMEI/IMSI.");
          return CIS_RET_ERROR;
        }
      ret = genDmUpdateEndpointName(&dmUpdeviceName, DMconf);
      if (ret < 0)
        {
          LOGE("ERROR:Get device name error from IMEI/IMSI.");
          return CIS_RET_ERROR;
        }
      contextP->isDM = TRUE;
      contextP->DMprivData = (char*)dmUpdeviceName;
      LOGD("INIT:endpoint name: %s", contextP->endpointName);
      LOGD("INIT:endpointInfo: %s", contextP->DMprivData);
    }
  else
    {
      if (prv_makeDeviceName(&deviceName) <= 0)
        {
          LOGE("ERROR:Get device name error from IMEI/IMSI.");
          return CIS_RET_ERROR;
        }
    }
#else
  if (prv_makeDeviceName(&deviceName) <= 0)
    {
      LOGE("ERROR:Get device name error from IMEI/IMSI.");
      return CIS_RET_ERROR;
    }
#endif
#if CIS_ENABLE_UPDATE
  contextP->fota_created = false;
  sys_callback.onEvent = prv_onSySEventHandler;
  sys_callback.userData = contextP;
#endif

  cissys_lockcreate(&contextP->lockRequest);
  cissys_lockcreate(&contextP->lockNotify);

  contextP->nextObserveNum = 0;
  contextP->endpointName = (char*)deviceName;
  LOGD("INIT:endpoint name: %s", contextP->endpointName);
  cis_config_get(configContext, cis_cfgid_sys, &configRet);
  cissys_init(contextP, configRet.data.cfg_sys, &sys_callback);
  log_config(configRet.data.cfg_sys->log_enabled,
    configRet.data.cfg_sys->log_ext_output,
    configRet.data.cfg_sys->log_output_level,
    configRet.data.cfg_sys->log_buffer_size);

  /*Get net parameter and create net*/
  netCallback.onEvent = prv_onNetEventHandler;
  cis_config_get(configContext, cis_cfgid_net, &configRet);
  netConfig.mtu = configRet.data.cfg_net->mtu;
  netConfig.data = configRet.data.cfg_net->user_data.data;
  netConfig.datalen = configRet.data.cfg_net->user_data.len;
  netConfig.bsEanbled = configRet.data.cfg_net->bs_enabled;
  netConfig.dtlsEanbled = configRet.data.cfg_net->dtls_enabled;

#if CIS_ENABLE_AUTH
#if CIS_ENABLE_DM
  if (contextP->isDM != TRUE)
#endif
    {
      uint16_t auth_front_len = 9;//len of "AuthCode:"
      char auth_temp[9 + 1] = { 0 };
      cissys_memcpy(auth_temp, configRet.data.cfg_net->user_data.data, auth_front_len);
      if (strcmp((const char *)&auth_temp, "AuthCode:") == 0)
        {
          uint8_t *auth_code_ptr = configRet.data.cfg_net->user_data.data + auth_front_len;
          uint16_t auth_back_len = auth_front_len;
          while ((*auth_code_ptr) != ';')
            {
              auth_code_ptr++;
              auth_back_len++;
            }
          uint16_t auth_len = auth_back_len - auth_front_len;
          if (auth_len > 0)
            {
              auth_code = (char *)cis_malloc(auth_len + 1);
              cissys_assert(auth_code != NULL);
              cis_memset(auth_code, 0, auth_len + 1);
              cis_memcpy(auth_code, configRet.data.cfg_net->user_data.data + auth_front_len, auth_len);
            }
          configRet.data.cfg_net->user_data.data += auth_back_len + 1;
        }
      contextP->authCode = auth_code;
    }
#endif

  cisnet_init(contextP, &netConfig, netCallback);
#if CIS_ENABLE_DTLS
  contextP->isDTLS = netConfig.dtlsEanbled != 0 ? true : false;

  uint16_t psk_front_len = 4;//len of "PSK:"
  char psk_temp[4 + 1] = { 0 };
  cissys_memcpy(psk_temp, configRet.data.cfg_net->user_data.data, psk_front_len);
  if (strcmp((const char *)&psk_temp, "PSK:") == 0)
    {
      uint8_t *psk_ptr = configRet.data.cfg_net->user_data.data + psk_front_len;
      uint16_t psk_back_len = psk_front_len;
      while ((*psk_ptr) != ';')
        {
          psk_ptr++;
          psk_back_len++;
        }
      uint16_t psk_len = psk_back_len - psk_front_len;
      if (psk_len > 0)
        {
          config_psk = (char *)cis_malloc(psk_len + 1);
          cissys_assert(config_psk != NULL);
          cis_memset(config_psk, 0, psk_len + 1);
          cis_memcpy(config_psk, configRet.data.cfg_net->user_data.data + psk_front_len, psk_len);
        }
      configRet.data.cfg_net->user_data.data += psk_back_len + 1;
    }
  contextP->PSK = config_psk;
#endif

  //set bootstrap flag to context;
  contextP->bootstrapEanbled = netConfig.bsEanbled != 0 ? true : false;
  char config_host[16] = {0};
  char config_port[5] = {0};
  strncpy(config_host, (const char *)configRet.data.cfg_net->host.data, strlen((const char *)configRet.data.cfg_net->host.data) - 5);
  strncpy(config_port, (const char *)(configRet.data.cfg_net->host.data + strlen(config_host) + 1), 4);

  targetServerHost = config_host;
  contextP->serverPort = (char *)cis_malloc(strlen(config_port) + 1);
  cissys_assert(contextP->serverPort != NULL);
  cis_memset(contextP->serverPort, 0, strlen(config_port) + 1);
  cis_memcpy(contextP->serverPort, config_port, strlen(config_port));

  if (targetServerHost == NULL || strlen(targetServerHost) <= 0)
    {
      return CIS_RET_ERROR;
    }

  cis_config_destory(&configContext);
  cis_addobject(contextP, std_object_security, NULL, NULL);
  st_object_t *securityObj = prv_findObject(contextP, std_object_security);
  if (securityObj == NULL)
    {
      LOGE("ERROR:Failed to init security object");
      return CIS_RET_ERROR;
    }

  //load serialize from firmware memory
  cissys_load((uint8_t*)&serialize, sizeof(serialize));

  if (contextP->bootstrapEanbled)
    {
      std_security_create(contextP, 0, "", false, securityObj);
      std_security_create(contextP, 1, targetServerHost, true, securityObj);
    }
  else
    {
      std_security_create(contextP, 0, targetServerHost, false, securityObj);
    }

#if CIS_ENABLE_DTLS
  if (contextP->isDTLS == TRUE
#if CIS_ENABLE_DM
    && contextP->isDM != TRUE
#endif
    )
    {
      if (dtls_init_context(&(contextP->dtls), &dtls_cb, contextP))
        {
          cis_deinit((void **)&contextP);
          cis_free(deviceName);
          return CIS_RET_ERROR;
        }
    }
#endif

  contextP->stateStep = PUMP_STATE_INITIAL;
  g_cis_taskthread_quit = 0;
  if (pthread_create(&g_cis_taskthread_tid, NULL, taskThread, contextP))
    {
      return CIS_RET_ERROR;
    }
  return CIS_RET_OK;
}

#if CIS_ENABLE_CMIOT_OTA
cis_ret_t cis_init_ota(void **context, void *configPtr, uint16_t configLen, cis_time_t ota_timeout_duration)
{
  cis_cfgret_t configRet;
  cisnet_callback_t netCallback;
  cissys_callback_t sys_callback;
  st_context_t *contextP;
  char *targetServerHost;
  char *deviceName = NULL, *dmUpdeviceName = NULL;
  cisnet_config_t netConfig;
  st_serialize_t serialize;
  void *configContext = NULL;
  char *auth_code = NULL;
  if (*context != NULL)
    {
      return CIS_RET_EXIST;
    }

  LOGD("api cis_init");
  LOGI("version:%d.%d.%d", OTA_VERSION_MAJOR, OTA_VERSION_MINOR, OTA_VERSION_MICRO);

  contextP = (st_context_t*)cis_malloc(sizeof(st_context_t));
  if (NULL == contextP)
    {
      return CIS_RET_ERROR;
    }
  cis_memset(contextP, 0, sizeof(st_context_t));
  contextP->nextMid = (uint16_t)cissys_rand();
  (*context) = contextP;

  //deal with configuration data
  if (cis_config_init(&configContext, configPtr, configLen) < 0)
    {
      return CIS_RET_ERROR;
    }

  LOGD("----------------\n");
  LOGD("DEBUG CONFIG INIT INFORMATION.");
  if (prv_makeDeviceName_ota(&deviceName) <= 0)
    {
      LOGE("ERROR:Get device name error from Eid/IMSI.");
      return CIS_RET_ERROR;
    }
#if CIS_ENABLE_UPDATE
  contextP->fota_created = false;
  sys_callback.onEvent = prv_onSySEventHandler;
  sys_callback.userData = contextP;
#endif

  cissys_lockcreate(&contextP->lockRequest);
  cissys_lockcreate(&contextP->lockNotify);

  contextP->nextObserveNum = 0;
  contextP->endpointName = (char*)deviceName;
  LOGD("INIT:endpoint name: %s", contextP->endpointName);

  /*Get sys parameter and init*/
  //	sys_callback.onEvent = prv_onSySEventHandler;
  //	sys_callback.userData = contextP;
  cis_config_get(configContext, cis_cfgid_sys, &configRet);
  cissys_init(contextP, configRet.data.cfg_sys, &sys_callback);
  log_config(configRet.data.cfg_sys->log_enabled,
    configRet.data.cfg_sys->log_ext_output,
    configRet.data.cfg_sys->log_output_level,
    configRet.data.cfg_sys->log_buffer_size);

  /*Get net parameter and create net*/
  netCallback.onEvent = prv_onNetEventHandler;
  cis_config_get(configContext, cis_cfgid_net, &configRet);

  netConfig.mtu = configRet.data.cfg_net->mtu;
  netConfig.data = configRet.data.cfg_net->user_data.data;
  netConfig.datalen = configRet.data.cfg_net->user_data.len;
  netConfig.bsEanbled = configRet.data.cfg_net->bs_enabled;
  netConfig.dtlsEanbled = configRet.data.cfg_net->dtls_enabled;

#if CIS_ENABLE_AUTH
  {
    uint16_t auth_front_len = 9;//len of "AuthCode:"
    char auth_temp[9 + 1] = {0};
    cissys_memcpy(auth_temp, configRet.data.cfg_net->user_data.data, auth_front_len);
    if (strcmp((const char *)&auth_temp, "AuthCode:") == 0)
      {
        uint8_t *auth_code_ptr = configRet.data.cfg_net->user_data.data + auth_front_len;
        uint16_t auth_back_len = auth_front_len;
        while ((*auth_code_ptr) != ';')
          {
            auth_code_ptr++;
            auth_back_len++;
          }
        uint16_t auth_len = auth_back_len - auth_front_len;
        if (auth_len > 0)
          {
            auth_code = (char *)cis_malloc(auth_len + 1);
            cissys_assert(auth_code != NULL);
            cis_memset(auth_code, 0, auth_len + 1);
            cis_memcpy(auth_code, configRet.data.cfg_net->user_data.data + auth_front_len, auth_len);
          }
      }
    contextP->authCode = auth_code;
  }
#endif

  cisnet_init(contextP, &netConfig, netCallback);
#if CIS_ENABLE_DTLS
  contextP->isDTLS = netConfig.dtlsEanbled != 0 ? true : false;
#endif
  //set bootstrap flag to context;
  contextP->bootstrapEanbled = netConfig.bsEanbled != 0 ? true : false;
  char config_host[16] = {0};
  char config_port[5] = {0};
  strncpy(config_host, (const char *)configRet.data.cfg_net->host.data, strlen((const char *)configRet.data.cfg_net->host.data) - 5);
  strncpy(config_port, (const char *)(configRet.data.cfg_net->host.data + strlen(config_host) + 1), 4);

  targetServerHost = config_host;
  contextP->serverPort = (char *)cis_malloc(strlen(config_port) + 1);
  cissys_assert(contextP->serverPort != NULL);
  cis_memset(contextP->serverPort, 0, strlen(config_port) + 1);
  cis_memcpy(contextP->serverPort, config_port, strlen(config_port));

  if (targetServerHost == NULL || strlen(targetServerHost) <= 0)
    {
      return CIS_RET_ERROR;
    }

  cis_config_destory(&configContext);

  cis_addobject(contextP, std_object_security, NULL, NULL);
  st_object_t* securityObj = prv_findObject(contextP, std_object_security);
  if (securityObj == NULL)
    {
      LOGE("ERROR:Failed to init security object");
      return CIS_RET_ERROR;
    }

  //load serialize from firmware memory
  cissys_load((uint8_t*)&serialize, sizeof(serialize));

  if (contextP->bootstrapEanbled)
    {
      std_security_create(contextP, 0, "", false, securityObj);
      std_security_create(contextP, 1, targetServerHost, true, securityObj);
    }
  else
    {
      std_security_create(contextP, 0, targetServerHost, false, securityObj);
    }

#if CIS_ENABLE_DTLS
  if (contextP->isDTLS == TRUE)
    {
      if (dtls_init_context(&(contextP->dtls), &dtls_cb, contextP))
        {
          cis_deinit((void **)&contextP);
          cis_free(deviceName);
          return CIS_RET_ERROR;
        }
    }
#endif

  contextP->cmiotOtaState = CMIOT_OTA_STATE_IDIL;
  contextP->ota_timeout_base = cissys_gettime();
  contextP->ota_timeout_duration = ota_timeout_duration*1000;

  cis_res_count_t resconfig;
  resconfig.actCount = 0;
  resconfig.attrCount = 5;
  cis_addobject(contextP, std_object_poweruplog, NULL, &resconfig);
  st_object_t *poweruplogObj = prv_findObject(contextP, std_object_poweruplog);
  if (poweruplogObj == NULL)
    {
      LOGE("ERROR:Failed to init poweruplog");
      cis_free(contextP);
      cis_free(deviceName);
      return CIS_RET_ERROR;
    }
  std_poweruplog_create(contextP, 0, poweruplogObj);

  resconfig.actCount = 1;
  resconfig.attrCount = 5;
  cis_addobject(contextP, std_object_cmdhdefecvalues, NULL, &resconfig);
  st_object_t *cmdhdefecvaluesObj = prv_findObject(contextP, std_object_cmdhdefecvalues);
  if (cmdhdefecvaluesObj == NULL)
    {
      LOGE("ERROR:Failed to init cmdhdefecvaluesObj object");
      cis_free(contextP);
      cis_free(deviceName);
      return CIS_RET_ERROR;
    }
  std_cmdhdefecvalues_create(contextP,0,cmdhdefecvaluesObj);

  contextP->stateStep = PUMP_STATE_INITIAL;
  return CIS_RET_OK;
}
#endif

cis_ret_t cis_deinit(void **context)
{
  st_context_t *ctx = *(st_context_t**)context;
  cissys_assert(context != NULL);
  if (context == NULL)
    {
      return CIS_RET_PARAMETER_ERR;
    }
  if (ctx->registerEnabled)
    {
      return CIS_RET_PENDING;
    }

  LOGD("api cis_deinit");
  if (g_cis_taskthread_tid != -1)
    {
      g_cis_taskthread_quit = 1;
      pthread_kill(g_cis_taskthread_tid, SIGALRM);
      pthread_join(g_cis_taskthread_tid, NULL);
      g_cis_taskthread_tid = -1;
    }
  prv_localDeinit((st_context_t**)context);
  return CIS_RET_OK;
}

cis_ret_t cis_register(void *context, cis_time_t lifetime, const cis_callback_t *cb)
{
  st_context_t *ctx = (st_context_t*)context;
  if (context == NULL)
    {
      return CIS_RET_PARAMETER_ERR;
    }
  LOGD("api cis_register");
  if (cb->onEvent == NULL || cb->onExec == NULL || cb->onObserve == NULL ||
    cb->onRead == NULL || cb->onSetParams == NULL || cb->onWrite == NULL ||
    cb->onDiscover == NULL)
    {
      LOGE("ERROR:cis_register request failed.invalid parameters");
      return CIS_RET_ERROR;
    }

  if (lifetime < LIFETIME_LIMIT_MIN ||
    lifetime > LIFETIME_LIMIT_MAX)
    {
      LOGE("ERROR:invalid lifetime parameter");
      return CIS_RET_ERROR;
    }
  ctx->lifetime = lifetime;
  ctx->registerEnabled = true;
  cis_memcpy(&ctx->callback, cb, sizeof(cis_callback_t));
  return CIS_RET_OK;
}

cis_ret_t cis_unregister(void *context)
{
  st_context_t *ctx = (st_context_t*)context;
  cissys_assert(context != NULL);
  if (context == NULL)
    {
      return CIS_RET_PARAMETER_ERR;
    }
  LOGD("api cis_unregister");
  core_updatePumpState(ctx, PUMP_STATE_UNREGISTER);
  return CIS_RET_OK;
}


cis_ret_t cis_addobject(void *context, cis_oid_t objectid, const cis_inst_bitmap_t *bitmap, const cis_res_count_t *resource)
{
  st_object_t *targetP;
  uint16_t index;
  cis_instcount_t instValidCount;
  cis_instcount_t instCount;
  cis_instcount_t instBytes;
  uint8_t* instPtr;
  bool noneBitmap;

  st_context_t *ctx = (st_context_t*)context;
  cissys_assert(context != NULL);
  if (context == NULL)
    {
      return CIS_RET_PARAMETER_ERR;
    }

  LOGD("api cis_addobject");

  if (bitmap == NULL || bitmap->instanceBitmap == NULL)
    {
      instCount = 1;
      instBytes = 1;
      noneBitmap = true;
    }
  else
    {
      noneBitmap = false;
      instCount = bitmap->instanceCount;
      instBytes = bitmap->instanceBytes;
      if (instBytes <= 0 || instCount <= 0)
        {
          return CIS_RET_ERROR;
        }
      if (instBytes != ((instCount - 1) / 8) + 1)
        {
          return CIS_RET_ERROR;
        }
    }

  targetP = (st_object_t *)CIS_LIST_FIND(ctx->objectList, objectid);
  if (targetP != NULL)
    {
      return CIS_RET_EXIST;
    }

  targetP = (st_object_t *)cis_malloc(sizeof(st_object_t));
  if (targetP == NULL)
    {
      return CIS_RET_MEMORY_ERR;
    }
  cis_memset(targetP, 0, sizeof(st_object_t));
  targetP->objID = objectid;

  instPtr = (uint8_t*)cis_malloc(instBytes);
  if (instPtr == NULL)
    {
      cis_free(targetP);
      return CIS_RET_MEMORY_ERR;
    }
  cis_memset(instPtr, 0, instBytes);

  if (noneBitmap)
    {
      instPtr[0] = 0x80;
    }
  else
    {
      cis_memcpy(instPtr, bitmap->instanceBitmap, instBytes);
    }

  instValidCount = 0;
  for (index = 0; index < instCount; index++)
    {
      if (object_checkInstExisted(instPtr, index))
        {
          instValidCount++;
        }
    }

  if (instValidCount == 0)
    {
      LOGD("Error:instance bitmap invalid.");
      cis_free(targetP);
      cis_free(instPtr);
      return CIS_RET_ERROR;
    }

  targetP->instBitmapBytes = instBytes;
  targetP->instBitmapPtr = instPtr;
  targetP->instBitmapCount = instCount;
  if (resource != NULL)
    {
      targetP->attributeCount = resource->attrCount;
      targetP->actionCount = resource->actCount;
    }
  targetP->instValidCount = instValidCount;
  LOGD("cis_addobject:%d,%d,%d", targetP->objID, instCount, instBytes);
  ctx->objectList = (st_object_t *)CIS_LIST_ADD(ctx->objectList, targetP);
  return CIS_RET_NO_ERROR;
}


cis_ret_t cis_delobject(void *context, cis_oid_t objectid)
{
  st_object_t *targetP;
  st_context_t *ctx = (st_context_t*)context;
  cissys_assert(context != NULL);
  if (context == NULL)
    {
      return CIS_RET_PARAMETER_ERR;
    }
  LOGD("api cis_delobject:%d", objectid);
  ctx->objectList = (st_object_t *)CIS_LIST_RM(ctx->objectList, objectid, &targetP);

  if (targetP == NULL)
    {
      return COAP_404_NOT_FOUND;
    }
  if (targetP->instBitmapPtr != NULL)
    {
      cis_free(targetP->instBitmapPtr);
    }
  cis_free(targetP);
  return CIS_RET_NO_ERROR;
}

uint32_t cis_pump(void *context, time_t *timeoutP)
{
  uint32_t notifyCount;
  bool readHasData;
  cis_time_t tv_sec;
  st_context_t *ctx = (st_context_t*)context;
  cissys_assert(context != NULL);

  tv_sec = utils_gettime_s();
  if (tv_sec <= 0)
    {
      return PUMP_RET_CUSTOM;
    }
#if CIS_ENABLE_MEMORYTRACE
  if (tv_sec - g_tracetime > CIS_CONFIG_MEMORYTRACE_TIMEOUT)
    {
      g_tracetime = tv_sec;
      int block, i;
      size_t size;
      trace_status(&block, &size);
      for (i = 0; i < block; i++)
        {
          trace_print(i, 1);
        }
    }
#endif//CIS_ENABLE_MEMORY_TRACE

  LOGD("%s: %d,%d,%d", __func__, ctx->registerEnabled, cisnet_attached_state(ctx), ctx->stateStep);
  if (!ctx->registerEnabled && (ctx->stateStep != PUMP_STATE_DISCONNECTED))
    {
      return PUMP_RET_CUSTOM;
    }

  if (!cisnet_attached_state(ctx) &&
    (ctx->stateStep == PUMP_STATE_INITIAL || ctx->stateStep != PUMP_STATE_DISCONNECTED))
    {
      return PUMP_RET_CUSTOM;
    }
  switch (ctx->stateStep)
    {
      case PUMP_STATE_HALT:
        {
          core_callbackEvent(ctx, CIS_EVENT_STATUS_HALT, NULL);
          prv_localDeinit(&ctx);
          cissys_fault(0);
          core_updatePumpState(ctx, PUMP_STATE_INITIAL);
          return PUMP_RET_CUSTOM;
        }
      case PUMP_STATE_INITIAL:
        {
          core_updatePumpState(ctx, PUMP_STATE_BOOTSTRAPPING);
          return PUMP_RET_NOSLEEP;
        }
        break;
      case PUMP_STATE_BOOTSTRAPPING:
        {
          if (!ctx->bootstrapEanbled)
            {
              LOGD("CIS_ENABLE_BOOTSTRAP is disabled");
              core_updatePumpState(ctx, PUMP_STATE_CONNECTING);
              return PUMP_RET_NOSLEEP;
            }
          if (ctx->bootstrapServer == NULL)
            {
              core_callbackEvent(ctx, CIS_EVENT_BOOTSTRAP_START, NULL);
              bootstrap_init(ctx);
              if (ctx->bootstrapServer == NULL)
                {
                  return PUMP_RET_CUSTOM;
                }
            }
          switch (bootstrap_getStatus(ctx))
            {
              case STATE_UNCREATED:
                {
                  ctx->lasttime = tv_sec;
                  bootstrap_create(ctx);
                  return PUMP_RET_NOSLEEP;
                }
                break;
              case STATE_CREATED:
                {
                  ctx->lasttime = tv_sec;
                  bootstrap_connect(ctx);
                  return PUMP_RET_NOSLEEP;
                }
                break;
              case STATE_CONNECT_PENDING:
                {
                  if (ctx->lasttime == 0 || tv_sec - ctx->lasttime > CIS_CONFIG_CONNECT_RETRY_TIME)
                    {
                      ctx->lasttime = tv_sec;
                      bootstrap_connect(ctx);
                    }
                  return PUMP_RET_NOSLEEP;
                }
                break;
              case STATE_BS_INITIATED:
                {
                  //wait;
                }
                break;
              case STATE_BS_PENDING:
                {
                  if (ctx->lasttime == 0 || tv_sec - ctx->lasttime > CIS_CONFIG_BOOTSTRAP_TIMEOUT)
                    {
                      ctx->lasttime = tv_sec;
                      LOGD("bootstrap pending timeout.");
                      if (ctx->bootstrapServer != NULL)
                        {
                          ctx->bootstrapServer->status = STATE_BS_FAILED;
                        }
                    }
                }
                break;
              case STATE_BS_FINISHED:
                {
                  LOGI("Bootstrap finish.");
                  bootstrap_destory(ctx);
                  core_updatePumpState(ctx, PUMP_STATE_CONNECTING);
                  core_callbackEvent(ctx, CIS_EVENT_BOOTSTRAP_SUCCESS, NULL);
                  return PUMP_RET_NOSLEEP;
                }
                break;
              case STATE_BS_FAILED:
                {
                  LOGE("Bootstrap failed.");
                  bootstrap_destory(ctx);
                  core_updatePumpState(ctx, PUMP_STATE_INITIAL);
                  core_callbackEvent(ctx, CIS_EVENT_BOOTSTRAP_FAILED, NULL);
                  return PUMP_RET_CUSTOM;
                }
                break;
              default:
                {
                #if CIS_ENABLE_DTLS
                  if (TRUE == ctx->isDTLS)
                    {
                      session_t saddr;
                      memset(&saddr, 0x0, sizeof(session_t));
                      saddr.sin_family = AF_INET;
                      saddr.sin_port = htons(atoi(ctx->serverPort));
                      saddr.sin_addr.s_addr = inet_addr(((st_server_t *)(ctx->bootstrapServer))->host);
                      if (((st_server_t *)(ctx->bootstrapServer))->status == STATE_CONNECTED &&
                        ctx->dtls.peers_list == NULL)
                        {
                          dtls_connect(&ctx->dtls, &saddr);
                        }
                      else if (ctx->dtls.peers_list != NULL)
                        {
                          dtls_peer_t *peer = dtls_get_peer(&ctx->dtls, &saddr);
                          if (peer != NULL && peer->state == DTLS_STATE_CONNECTED)
                            {
                              bootstrap_step(ctx, tv_sec);
                            }
                        }
                    }
                  else
                    {
                      bootstrap_step(ctx, tv_sec);
                    }
                #else
                  bootstrap_step(ctx, tv_sec);
                #endif
                }
                break;
            }
        }
        break;
      case PUMP_STATE_CONNECTING:
        {
          if (ctx->server == NULL)
            {
              ctx->server = management_makeServerList(ctx, false);
              if (ctx->server == NULL)
                {
                  LOGE("ERROR:makeServer failed.");
                  core_updatePumpState(ctx, PUMP_STATE_INITIAL);
                  return PUMP_RET_CUSTOM;
                }
            }
          switch (ctx->server->status)
            {
              case STATE_UNCREATED:
                {
                  if (ctx->server == NULL)
                    {
                      core_updatePumpState(ctx, PUMP_STATE_INITIAL);
                      return PUMP_RET_CUSTOM;
                    }
                  ctx->lasttime = tv_sec;
                  if (!management_createNetwork(ctx, ctx->server))
                    {
                      core_updatePumpState(ctx, PUMP_STATE_INITIAL);
                      return PUMP_RET_CUSTOM;
                    }
                  return PUMP_RET_NOSLEEP;
                }
                break;
              case STATE_CREATED:
                {
                  if (ctx->server == NULL || ctx->server->sessionH == NULL)
                    {
                      core_updatePumpState(ctx, PUMP_STATE_INITIAL);
                      return PUMP_RET_CUSTOM;
                    }
                  ctx->lasttime = tv_sec;
                  if (!management_connectServer(ctx, ctx->server))
                    {
                      core_updatePumpState(ctx, PUMP_STATE_INITIAL);
                      return PUMP_RET_CUSTOM;
                    }
                  return PUMP_RET_NOSLEEP;
                }
                break;
              case STATE_CONNECT_PENDING:
                {
                  if (ctx->server == NULL || ctx->server->sessionH == NULL)
                    {
                      core_updatePumpState(ctx, PUMP_STATE_INITIAL);
                      return PUMP_RET_CUSTOM;
                    }
                  if (ctx->lasttime == 0 || tv_sec - ctx->lasttime > CIS_CONFIG_CONNECT_RETRY_TIME)
                    {
                      ctx->lasttime = tv_sec;
                      if (!management_connectServer(ctx, ctx->server))
                        {
                          core_updatePumpState(ctx, PUMP_STATE_INITIAL);
                          return PUMP_RET_CUSTOM;
                        }
                    }
                  return PUMP_RET_NOSLEEP;
                }
                break;
              case STATE_CONNECTED:
                {
                #if CIS_ENABLE_DTLS
                #if CIS_ENABLE_DM
                  if (!ctx->isDM && TRUE == ctx->isDTLS)
                #else
                  if (TRUE == ctx->isDTLS)
                #endif
                    {
                      session_t saddr;
                      saddr.sin_family = AF_INET;
                      saddr.sin_port = htons(atoi(ctx->serverPort));
                      saddr.sin_addr.s_addr = inet_addr(((st_server_t *)(ctx->server))->host);
                      dtls_peer_t *peer = dtls_get_peer(&ctx->dtls, &saddr);
                      if (!peer)
                        {
                          dtls_connect(&ctx->dtls, &saddr);
                        }
                      else
                        {
                          if (peer->state == DTLS_STATE_CONNECTED)
                            {
                              ctx->lasttime = 0;
                              core_updatePumpState(ctx, PUMP_STATE_REGISTER_REQUIRED);
                              core_callbackEvent(ctx, CIS_EVENT_CONNECT_SUCCESS, NULL);
                              return PUMP_RET_NOSLEEP;
                            }
                        }
                    }
                  else
                    {
                      ctx->lasttime = 0;
                      core_updatePumpState(ctx, PUMP_STATE_REGISTER_REQUIRED);
                      core_callbackEvent(ctx, CIS_EVENT_CONNECT_SUCCESS, NULL);
                      return PUMP_RET_NOSLEEP;
                    }
                #else
                  ctx->lasttime = 0;
                  core_updatePumpState(ctx, PUMP_STATE_REGISTER_REQUIRED);
                  core_callbackEvent(ctx, CIS_EVENT_CONNECT_SUCCESS, NULL);
                  return PUMP_RET_NOSLEEP;
                #endif
                }
                break;
              case STATE_CONNECT_FAILED:
                {
                  LOGE("server connect failed.");
                  prv_deleteServer(ctx);
                  core_updatePumpState(ctx, PUMP_STATE_INITIAL);
                  core_callbackEvent(ctx, CIS_EVENT_CONNECT_FAILED, NULL);
                  return PUMP_RET_CUSTOM;
                }
                break;
              default:
                {
                  LOGE("unexpected server status:%d", ctx->server->status);
                }
            }
        }
        break;
      case PUMP_STATE_DISCONNECTED:
        {
        #if CIS_ENABLE_UPDATE
          if (ctx->device_inst != NULL)
            {
              std_device_clean(ctx);
            }
          if (ctx->firmware_inst != NULL)
            {
              std_firmware_clean(ctx);
            }
          ctx->fota_created = 0;
        #endif
        #if CIS_ENABLE_DTLS
        #if CIS_ENABLE_DM
          if (!ctx->isDM && TRUE == ctx->isDTLS)
        #else
          if (TRUE == ctx->isDTLS)
        #endif
            {
              dtls_close_context(&(ctx->dtls));
            }
        #endif
          prv_deleteServer(ctx);
          prv_deleteObservedList(ctx);
          prv_deleteTransactionList(ctx);
          prv_deleteRequestList(ctx);
          prv_deleteNotifyList(ctx);
          core_updatePumpState(ctx, PUMP_STATE_CONNECTING);
          return PUMP_RET_NOSLEEP;
        }
        break;
      case PUMP_STATE_UNREGISTER:
        {
          registration_deregister(ctx);
        }
        break;
      case PUMP_STATE_REGISTER_REQUIRED:  //from waiting connection to here
        {
          if (ctx->lasttime == 0 || tv_sec - ctx->lasttime > CIS_CONFIG_REG_INTERVAL_TIME)
            {
              uint32_t result;
              result = cis_registration_start(ctx);
              if (COAP_NO_ERROR != result)
                {
                  return PUMP_RET_CUSTOM;
                }
              core_updatePumpState(ctx, PUMP_STATE_REGISTERING);
              ctx->lasttime = tv_sec;
            }
        }
        break;
      case PUMP_STATE_REGISTERING:
        {
          switch (cis_registration_getStatus(ctx))
            {
              case STATE_REGISTERED:
                {
                  st_serialize_t serialize = {0};
                  ctx->lasttime = 0;
                  core_updatePumpState(ctx, PUMP_STATE_READY);
                  serialize.size = sizeof(serialize);
                  cis_utils_stringCopy((char*)&serialize.host, sizeof(serialize.host), ctx->server->host);
                  cissys_save((uint8_t*)&serialize, sizeof(serialize));
                  return PUMP_RET_NOSLEEP;
                }
                break;
              case STATE_REG_FAILED:
                {
                  core_updatePumpState(ctx, PUMP_STATE_DISCONNECTED);
                  return PUMP_RET_CUSTOM;
                }
                break;
              case STATE_REG_PENDING:
              default:
                // keep on waiting
                break;
            }
        }
        break;
      case PUMP_STATE_READY:
        {
          if (cis_registration_getStatus(ctx) == STATE_REG_FAILED)
            {
              LOGE("ERROR:pump got STATE_REG_FAILED");
              core_updatePumpState(ctx, PUMP_STATE_DISCONNECTED);
              return PUMP_RET_NOSLEEP;
            }
        }
        break;
      default:
        {
        }
        break;
    }

  cis_registration_step(ctx, tv_sec);
  packet_step(ctx, tv_sec);
  transaction_step(ctx, tv_sec, timeoutP);

  readHasData = packet_read(ctx);
  if (readHasData)
    {
      return PUMP_RET_NOSLEEP;
    }

  cissys_lock(ctx->lockNotify, CIS_CONFIG_LOCK_INFINITY);
  notifyCount = CIS_LIST_COUNT((cis_list_t*)ctx->notifyList);
  cissys_unlock(ctx->lockNotify);
  if (notifyCount != 0)
    {
      return PUMP_RET_NOSLEEP;
    }

  ctx->lasttime = tv_sec;
  return PUMP_RET_CUSTOM;
}



cis_ret_t cis_update_reg(void *context, cis_time_t lifetime, bool withObjects)
{
  cissys_assert(context != NULL);
  st_context_t *ctx = (st_context_t*)context;
  if (ctx->stateStep != PUMP_STATE_READY)
    {
      return CIS_RET_INVILID;
    }

  if (lifetime != LIFETIME_INVALID &&
    lifetime > LIFETIME_LIMIT_MIN && lifetime < LIFETIME_LIMIT_MAX)
    {
      ctx->lifetime = lifetime;
    }

  return registration_update_registration(ctx, withObjects);
}


CIS_API cis_ret_t cis_response(void *context, const cis_uri_t *uri, const cis_data_t *value, cis_mid_t mid, cis_coapret_t result)
{
  st_context_t *ctx = (st_context_t*)context;
  st_notify_t *notify = NULL;

  if (context == NULL)
    {
      return CIS_RET_PARAMETER_ERR;
    }

  if (ctx->stateStep != PUMP_STATE_READY)
    {
      return CIS_RET_INVILID;
    }

  if (result < CIS_COAP_204_CHANGED || result > CIS_COAP_503_SERVICE_UNAVAILABLE)
    {
      return CIS_RET_ERROR;
    }

  notify = (st_notify_t*)cis_malloc(sizeof(st_notify_t));
  cissys_assert(notify != NULL);
  if (notify == NULL)
    {
      return CIS_RET_MEMORY_ERR;
    }

  notify->isResponse = true;
  notify->next = NULL;
  notify->id = ++ctx->nextNotifyId;
  notify->mid = mid;
  notify->result = result;
  notify->value = NULL;
  if (value != NULL)
    {
      notify->value = prv_dataDup(value);
      if (notify->value == NULL)
        {
          cis_free(notify);
          return CIS_RET_MEMORY_ERR;
        }
    }
  if (uri != NULL)
    {
      notify->uri = *uri;
      LOGD("cis_response add index:%d mid:%u [%u/%u/%u:%u],result:(%s)",
        ctx->nextNotifyId, mid, notify->uri.objectId, notify->uri.instanceId, notify->uri.resourceId, notify->uri.flag, STR_COAP_CODE(result));
    }
  else
    {
      cis_uri_make(URI_INVALID, URI_INVALID, URI_INVALID, &notify->uri);
      LOGD("cis_response add index:%d mid:%u ,result:[%s]", ctx->nextNotifyId, mid, STR_COAP_CODE(result));
    }

  cissys_lock(ctx->lockNotify, CIS_CONFIG_LOCK_INFINITY);
  ctx->notifyList = (st_notify_t*)CIS_LIST_ADD(ctx->notifyList, notify);
  cissys_unlock(ctx->lockNotify);
  return CIS_RET_OK;
}

cis_coapret_t cis_result_to_cis_response(cis_res_result_t result)
{
  switch (result)
    {
      case cis_res_result_content:
        return CIS_COAP_205_CONTENT;
      case cis_res_result_changed:
        return CIS_COAP_204_CHANGED;
      case cis_res_result_badResult:
        return CIS_COAP_400_BAD_REQUEST;
      case cis_res_result_unuthorized:
        return CIS_COAP_401_UNAUTHORIZED;
      case cis_res_result_notFound:
        return CIS_COAP_404_NOT_FOUND;
      case cis_res_result_methodNotAllowed:
        return CIS_COAP_405_METHOD_NOT_ALLOWED;
      case cis_res_result_notAcceptable:
        return CIS_COAP_406_NOT_ACCEPTABLE;
      default:
        LOGE("invalid result:%d", result);
        return CIS_COAP_406_NOT_ACCEPTABLE;
    }
}

cis_ret_t cis_discover_response(void *context, cis_mid_t msgId, cis_res_result_t result, char *valueStr)
{
  char delim[] = ";";
  char *token;
  cis_uri_t uri;
  char *s = strdup(valueStr);

  for (token = strsep(&s, delim); token != NULL; token = strsep(&s, delim))
    {
      uri.objectId = URI_INVALID;
      uri.instanceId = URI_INVALID;
      uri.resourceId = atoi(token);
      cis_uri_update(&uri);
      cis_response(context, &uri, NULL, msgId, CIS_RESPONSE_CONTINUE);
    }

  cis_response(context, NULL, NULL, msgId, cis_result_to_cis_response(result));
  return CIS_RET_OK;
}


CIS_API cis_ret_t cis_notify(void *context, const cis_uri_t *uri, const cis_data_t *value, cis_mid_t mid, cis_coapret_t result, bool needAck)
{
  st_context_t *ctx = (st_context_t*)context;
  st_notify_t *notify = NULL;

  if (context == NULL)
    {
      return CIS_RET_PARAMETER_ERR;
    }

  if (ctx->stateStep != PUMP_STATE_READY)
    {
      return CIS_RET_INVILID;
    }

  if (result != CIS_NOTIFY_CONTENT && result != CIS_NOTIFY_CONTINUE)
    {
      return CIS_RET_PARAMETER_ERR;
    }

  notify = (st_notify_t*)cis_malloc(sizeof(st_notify_t));
  cissys_assert(notify != NULL);
  if (notify == NULL)
    {
      return CIS_RET_MEMORY_ERR;
    }

  notify->isResponse = false;
  notify->next = NULL;
  notify->id = ++ctx->nextNotifyId;
  notify->mid = mid;
  notify->result = result;
  notify->value = NULL;
  if (value != NULL)
    {
      notify->value = prv_dataDup(value);
      if (notify->value == NULL)
        {
          cis_free(notify);
          return CIS_RET_MEMORY_ERR;
        }
    }

  if (uri != NULL)
    {
      notify->uri = *uri;
      LOGD("cis_notify add index:%d mid:0x%x [%d/%d/%d]", ctx->nextNotifyId, mid, uri->objectId, uri->instanceId, uri->resourceId);
    }
  else
    {
      cis_uri_make(URI_INVALID, URI_INVALID, URI_INVALID, &notify->uri);
      LOGD("cis_notify add index:%d mid:0x%x", ctx->nextNotifyId, mid);
    }

  cissys_lock(ctx->lockNotify, CIS_CONFIG_LOCK_INFINITY);
  ctx->notifyList = (st_notify_t*)CIS_LIST_ADD(ctx->notifyList, notify);
  cissys_unlock(ctx->lockNotify);
  return CIS_RET_OK;
}




//////////////////////////////////////////////////////////////////////////
void  core_callbackEvent(st_context_t *context, cis_evt_t id, void *param)
{
  cis_onEvent(context, id, param);
}

void core_updatePumpState(st_context_t *context, et_client_state_t state)
{
  LOGD("Update State To %s(%d)", STR_STATE(state), state);
  context->stateStep = state;
}


//////////////////////////////////////////////////////////////////////////
//static private function.
//


void prv_localDeinit(st_context_t **context)
{
  st_context_t *ctx = *context;

  std_security_clean(ctx);

  prv_deleteObservedList(ctx);
  prv_deleteTransactionList(ctx);
  prv_deleteRequestList(ctx);
  prv_deleteNotifyList(ctx);
  prv_deleteObjectList(ctx);
  prv_deleteServer(ctx);

#if CIS_ENABLE_UPDATE
  if (ctx->device_inst != NULL)
    {
      std_device_clean(ctx);
    }
  if (ctx->firmware_inst != NULL)
    {
      std_firmware_clean(ctx);
    }
  ctx->fota_created = 0;
#endif
#if CIS_ENABLE_UPDATE || CIS_ENABLE_MONITER
  if (ctx->conn_inst != NULL)
    {
      std_conn_moniter_clean(ctx);
    }
#endif

#if CIS_ENABLE_MONITER
  if (ctx->moniter_inst != NULL)
    {
      std_moniter_clean(ctx);
    }
  ctx->moniter_created = 0;
#endif

#if CIS_ENABLE_CMIOT_OTA
  if (ctx->poweruplog_inst != NULL)
    {
      std_poweruplog_clean(ctx);
    }

  if (ctx->cmdhdefecvalues_inst != NULL)
    {
      std_cmdhdefecvalues_clean(ctx);
    }
#endif

  cissys_lockdestory(ctx->lockRequest);
  cissys_lockdestory(ctx->lockNotify);
#if CIS_ENABLE_AUTH
  if (ctx->authCode != NULL)
    {
      cis_free(ctx->authCode);
      ctx->authCode = NULL;
    }
#endif
  if (ctx->endpointName != NULL)
    {
      cis_free(ctx->endpointName);
      ctx->endpointName = NULL;
    }
#if CIS_ENABLE_DM
  if (ctx->DMprivData)
    {
      cis_free(ctx->DMprivData);
      ctx->DMprivData = NULL;
    }
#endif

#if CIS_ENABLE_DTLS
#if CIS_ENABLE_DM
  if (!ctx->isDM && TRUE == ctx->isDTLS)
#else
  if (TRUE == ctx->isDTLS)
#endif
    {
      if (ctx->PSK != NULL)
        {
          cis_free(ctx->PSK);
          ctx->PSK = NULL;
        }
      dtls_close_context(&(ctx->dtls));
    }
#endif
  if (ctx->serverPort)
    {
      cis_free(ctx->serverPort);
      ctx->serverPort = NULL;
    }

  if (ctx)
    {
      cis_free(ctx);
      (*context) = NULL;
    }

#if CIS_ENABLE_MEMORYTRACE
  g_tracetime = 0;
#endif//CIS_ENABLE_MEMORYTRACE
}



static cis_data_t *prv_dataDup(const cis_data_t *src)
{
  if (src == NULL)
    {
      return NULL;
    }
  cis_data_t *newData = (cis_data_t*)cis_malloc(sizeof(cis_data_t));
  if (newData == NULL)
    {
      return NULL;
    }

  if (src->type == cis_data_type_opaque || src->type == cis_data_type_string)
    {
      newData->asBuffer.buffer = (uint8_t*)cis_malloc(src->asBuffer.length);
      if (newData == NULL || newData->asBuffer.buffer == NULL)
        {
          return NULL;
        }
      newData->asBuffer.length = src->asBuffer.length;
      cissys_memcpy(newData->asBuffer.buffer, src->asBuffer.buffer, src->asBuffer.length);
    }
  cissys_memcpy(&newData->value.asInteger, &src->value.asInteger, sizeof(src->value.asInteger));
  newData->type = src->type;
  return newData;
}


st_object_t *prv_findObject(st_context_t *context, cis_oid_t objectid)
{
  st_object_t *targetP;
  targetP = (st_object_t *)CIS_LIST_FIND(context->objectList, objectid);
  return targetP;
}


static void prv_deleteTransactionList(st_context_t *context)
{
  LOGD("fall in transaction_removeAll\n");
  transaction_removeAll(context);
}

static void prv_deleteObservedList(st_context_t *context)
{
  LOGD("fall in observe_removeAll\n");
  observe_removeAll(context);
}

static void prv_deleteRequestList(st_context_t *context)
{
  LOGD("fall in packet_request_removeAll\n");
  packet_asynRemoveRequestAll(context);
}

static void prv_deleteNotifyList(st_context_t *context)
{
	LOGD("fall in packet_notify_removeAll\n");
	packet_asynRemoveNotifyAll(context);
}

static void prv_deleteObjectList(st_context_t* context)
{
  LOGD("fall in object_removeAll\n");
  object_removeAll(context);
}


static void prv_deleteServer(st_context_t *context)
{
  if (context->server)
    {
      management_destoryServer(context, context->server);
      context->server = NULL;
    }
  if (context->bootstrapServer)
    {
      management_destoryServer(context, context->bootstrapServer);
      context->bootstrapServer = NULL;
    }
}

#if CIS_ENABLE_UPDATE
cis_ret_t reset_fotaIDIL(st_context_t *context, uint32_t msgid)
{
  cis_ret_t ret = 0;

  std_device_clean(context);
  std_firmware_clean(context);
  context->fota_created = 0;
  cis_update_reg(context, 0, true);
#if CIS_ENABLE_UPDATE_MCU
  if(context->isupdate_mcu == false)
    {
      core_callbackEvent(context, msgid, NULL);
    }
#else
  core_callbackEvent(context, msgid, NULL);
#endif
  return ret;
}

cis_ret_t prv_onSySEventHandler(cissys_event_t id, void *param, void *userData, int *len)
{
  cis_ret_t ret = 0;
  st_uri_t uri;
  uint16_t instanceId = 0;
  if (param != NULL)
    {
      instanceId = (uint16_t)param;
    }
  st_context_t *context = (st_context_t*)userData;
  firmware_data_t *firmwareInstance = (firmware_data_t *)(context->firmware_inst);
  if (firmwareInstance == NULL)
    {
      LOGE("No firmware instance");
      return ret;
    }
  cis_fw_context_t *fw_info = (cis_fw_context_t*)(firmwareInstance->fw_info);
  if (fw_info == NULL)
    {
      LOGE("No fota information");
      return ret;
    }
  context->fw_request.tv_t = utils_gettime_s();
  switch (id)
    {
      case cissys_event_write_success:
        {
          LOGD("writed package ok");
          context->fw_request.write_state = PACKAGE_WRITE_IDIL;
          cissys_savewritebypes(context->fw_request.last_packet_size);
          context->fw_request.code = COAP_204_CHANGED;
          //single write,wait for the validate result
          if (context->fw_request.single_packet_write == true)
            {
            #if CIS_ENABLE_UPDATE_MCU
              if(context->isupdate_mcu == false)
                {
                  if (!cissys_checkFwValidation(&(context->g_fotacallback)))
                    {
                      //set validate result
                      fw_info->result = FOTA_RESULT_CHECKFAIL;
                      cissys_setFwUpdateResult(fw_info->result);
                      context->fw_request.code = COAP_500_INTERNAL_SERVER_ERROR;
                      object_asynAckNodata(context, context->fw_request.response_ack, COAP_500_INTERNAL_SERVER_ERROR);
                      reset_fotaIDIL(context, CIS_EVENT_FIRMWARE_DOWNLOAD_FAILED);
                    }
                }
              else
                {
                  context->fw_request.code = COAP_204_CHANGED;
                  context->fw_request.single_packet_write = false;
                  object_asynAckNodata(context, context->fw_request.response_ack, COAP_204_CHANGED);
                  fw_state_change(context, FOTA_STATE_DOWNLOADED);
                  cissys_setFwState(FOTA_STATE_DOWNLOADED);
                  core_callbackEvent(context, CIS_EVENT_SOTA_DOWNLOAED, NULL);
                }
            #else
              if (!cissys_checkFwValidation(&(context->g_fotacallback)))
                {
                  //set validate result
                  fw_info->result = FOTA_RESULT_CHECKFAIL;
                  cissys_setFwUpdateResult(fw_info->result);
                  context->fw_request.code = COAP_500_INTERNAL_SERVER_ERROR;
                  object_asynAckNodata(context, context->fw_request.response_ack, COAP_500_INTERNAL_SERVER_ERROR);
                  reset_fotaIDIL(context, CIS_EVENT_FIRMWARE_DOWNLOAD_FAILED);
                }
            #endif
              //wait for validation result to send ack
              context->fw_request.code = COAP_204_CHANGED;
              return ret;
            }
          //block write result
          if (context->fw_request.block1_more == 0) //the last block for write
            {
            #if CIS_ENABLE_UPDATE_MCU
              if (context->isupdate_mcu == false)
                {
                  if (!cissys_checkFwValidation(&(context->g_fotacallback)))
                    {
                      fw_info->result = FOTA_RESULT_CHECKFAIL;
                      cissys_setFwUpdateResult(fw_info->result);
                      context->fw_request.code = COAP_500_INTERNAL_SERVER_ERROR;
                      object_asynAckBlockWrite(context, context->fw_request.response_ack, COAP_500_INTERNAL_SERVER_ERROR, context->fw_request.ack_type);
                      reset_fotaIDIL(context, CIS_EVENT_FIRMWARE_DOWNLOAD_FAILED);
                    }
                }
              else
                {
                  context->fw_request.code = COAP_204_CHANGED;
                  object_asynAckBlockWrite(context, context->fw_request.response_ack, COAP_204_CHANGED, context->fw_request.ack_type);
                  fw_state_change(context, FOTA_STATE_DOWNLOADED);
                  cissys_setFwState(FOTA_STATE_DOWNLOADED);
                  core_callbackEvent(context, CIS_EVENT_SOTA_DOWNLOAED, NULL);
                }
            #else
              if (!cissys_checkFwValidation(&(context->g_fotacallback)))
                {
                  fw_info->result = FOTA_RESULT_CHECKFAIL;
                  cissys_setFwUpdateResult(fw_info->result);
                  context->fw_request.code = COAP_500_INTERNAL_SERVER_ERROR;
                  object_asynAckBlockWrite(context, context->fw_request.response_ack, COAP_500_INTERNAL_SERVER_ERROR, context->fw_request.ack_type);
                  reset_fotaIDIL(context, CIS_EVENT_FIRMWARE_DOWNLOAD_FAILED);
                }
            #endif
            }
          else//not the last write
            {
              context->fw_request.code = COAP_231_CONTINUE;
              object_asynAckBlockWrite(context, context->fw_request.response_ack, COAP_231_CONTINUE, context->fw_request.ack_type);
            }
        }
        break;
      case cissys_event_write_fail:
        {
          LOGD("writed package failed,return 500 to server");
          if (context->fw_request.single_packet_write == true)   //single write
            {
              context->fw_request.single_packet_write = false;
              context->fw_request.code = COAP_500_INTERNAL_SERVER_ERROR;
              object_asynAckNodata(context, context->fw_request.response_ack, COAP_500_INTERNAL_SERVER_ERROR);
            }
          else
            {
              context->fw_request.code = COAP_500_INTERNAL_SERVER_ERROR;
              object_asynAckBlockWrite(context, context->fw_request.response_ack, COAP_500_INTERNAL_SERVER_ERROR, context->fw_request.ack_type);
            }
          fw_info->result = FOTA_RESULT_CHECKFAIL;
          cissys_setFwUpdateResult(fw_info->result);
          context->fw_request.write_state = PACKAGE_WRITE_IDIL;
          reset_fotaIDIL(context, CIS_EVENT_FIRMWARE_DOWNLOAD_FAILED);
        }
        break;
      case cissys_event_fw_erase_success:
        {
          context->fw_request.write_state = PACKAGE_WRITE_IDIL;
          fw_info->result = FOTA_RESULT_INIT;
          fw_info->state = FOTA_STATE_IDIL;
          fw_state_change(context, FOTA_STATE_IDIL);
          cissys_setFwState(FOTA_STATE_IDIL);
          cissys_ClearFwBytes();
          cissys_setFwUpdateResult(fw_info->result);
          object_asynAckNodata(context, context->fw_request.response_ack, COAP_204_CHANGED);
          reset_fotaIDIL(context, CIS_EVENT_FIRMWARE_ERASE_SUCCESS);
        #if CIS_ENABLE_UPDATE_MCU
          if (true == context->isupdate_mcu)
            {
              context->isupdate_mcu = false;
              cissys_setSwState(context->isupdate_mcu);
            }
        #endif
        }
        break;
      case cissys_event_fw_erase_fail:
        {
          context->fw_request.write_state = PACKAGE_WRITE_FAIL;
          fw_info->result = FOTA_RESULT_UPDATEFAIL;
          cissys_setFwUpdateResult(fw_info->result);
          context->fw_request.code = COAP_500_INTERNAL_SERVER_ERROR;
          object_asynAckNodata(context, context->fw_request.response_ack, COAP_500_INTERNAL_SERVER_ERROR);
          reset_fotaIDIL(context, CIS_EVENT_FIRMWARE_ERASE_FAIL);
        }
        break;
      case cissys_event_fw_validate_success: //emit after validation
        {
          fw_state_change(context, FOTA_STATE_DOWNLOADED);
          cissys_setFwState(FOTA_STATE_DOWNLOADED);
          core_callbackEvent(context, CIS_EVENT_FIRMWARE_DOWNLOADED, NULL);
          context->fw_request.code = COAP_204_CHANGED;
          if (context->fw_request.single_packet_write == true)   //single write validate
            {
              context->fw_request.single_packet_write = false;
              object_asynAckNodata(context, context->fw_request.response_ack, COAP_204_CHANGED);
            }
          else
            {
              object_asynAckBlockWrite(context, context->fw_request.response_ack, COAP_204_CHANGED, context->fw_request.ack_type);
            }
        }
        break;
      case cissys_event_fw_validate_fail:
        {
          fw_info->result = FOTA_RESULT_CHECKFAIL;
          cissys_setFwUpdateResult(fw_info->result);
          context->fw_request.write_state = PACKAGE_WRITE_FAIL;
          context->fw_request.code = COAP_500_INTERNAL_SERVER_ERROR;
          if (context->fw_request.single_packet_write == true)   //single write validate
            {
              context->fw_request.single_packet_write = false;
              object_asynAckNodata(context, context->fw_request.response_ack, COAP_500_INTERNAL_SERVER_ERROR);
            }
          else
            {
              object_asynAckBlockWrite(context, context->fw_request.response_ack, COAP_500_INTERNAL_SERVER_ERROR, context->fw_request.ack_type);
            }
          reset_fotaIDIL(context, CIS_EVENT_FIRMWARE_DOWNLOAD_FAILED);
        }
        break;
      case cissys_event_fw_update_success:
        {
          fw_info->result = FOTA_RESULT_SUCCESS;
          cissys_setFwUpdateResult(fw_info->result);
        }
        break;
      case cissys_event_fw_update_fail:
        {
          fw_info->result = FOTA_RESULT_UPDATEFAIL;
          cissys_setFwUpdateResult(fw_info->result);
          reset_fotaIDIL(context, CIS_EVENT_FIRMWARE_UPDATE_FAILED);
        }
        break;
      case cissys_event_unknow:
        {
          reset_fotaIDIL(context, CIS_EVENT_FIRMWARE_UPDATE_FAILED);
        }
        break;
      default:
        break;
    }
  return ret;
}
#endif

static cis_ret_t prv_onNetEventHandler(cisnet_t netctx, cisnet_event_t id, void *param, void *context)
{
  st_context_t *contextP = (st_context_t*)context;
  switch (id)
    {
      case cisnet_event_unknow:
        {
          core_updatePumpState(contextP, PUMP_STATE_HALT);
        }
        break;
      case cisnet_event_connected:
        {
          if (contextP->stateStep == PUMP_STATE_BOOTSTRAPPING)
            {
              contextP->bootstrapServer->status = STATE_CONNECTED;
              LOGI("bootstrap server connected.");
            }
          else if (contextP->stateStep == PUMP_STATE_CONNECTING)
            {
              contextP->server->status = STATE_CONNECTED;
              LOGI("server connected.");
            }
        }
        break;
      case cisnet_event_disconnect:
        {
          if (contextP->stateStep == PUMP_STATE_READY)
            {
              core_updatePumpState(contextP, PUMP_STATE_DISCONNECTED);
            }
          else if (contextP->stateStep == PUMP_STATE_BOOTSTRAPPING)
            {
              contextP->bootstrapServer->status = STATE_BS_FAILED;
            }
          else if (contextP->stateStep == PUMP_STATE_CONNECTING)
            {
              contextP->server->status = STATE_CONNECT_FAILED;
            }
        }
        break;
      default:
        break;
    }
  return CIS_RET_ERROR;
}

static int prv_makeDeviceName(char **name)
{
  (*name) = (char*)cis_malloc(NBSYS_IMEI_MAXLENGTH + NBSYS_IMSI_MAXLENGTH + 2);
  if ((*name) == NULL)
    {
      return -1;
    }
  cis_memset((*name), 0, NBSYS_IMEI_MAXLENGTH + NBSYS_IMSI_MAXLENGTH + 2);
  uint8_t imei = cissys_getIMEI((*name), NBSYS_IMEI_MAXLENGTH);
  *((char*)((*name) + imei)) = ';';
  uint8_t imsi = cissys_getIMSI((*name) + imei + 1, NBSYS_IMSI_MAXLENGTH);
  if (imei <= 0 || imsi <= 0 || utils_strlen((char*)(*name)) <= 0)
    {
      LOGE("ERROR:Get IMEI/IMSI ERROR.\n");
      return 0;
    }
  return imei + imsi;
}
#if CIS_ENABLE_CMIOT_OTA
static int prv_makeDeviceName_ota(char **name)
{
  (*name) = (char*)cis_malloc(NBSYS_EID_MAXLENGTH + NBSYS_IMSI_MAXLENGTH + 2);
  if ((*name) == NULL)
    {
      return -1;
    }
  cis_memset((*name), 0, NBSYS_EID_MAXLENGTH + NBSYS_IMSI_MAXLENGTH + 1);
  uint8_t eid = cissys_getEID((*name), NBSYS_EID_MAXLENGTH);
  *((char*)((*name) + eid)) = ';';
  uint8_t imsi = cissys_getIMSI((*name) + eid + 1, NBSYS_IMSI_MAXLENGTH);
  if (eid <= 0 || imsi <= 0 || utils_strlen((char*)(*name)) <= 0)
    {
      LOGE("ERROR:Get Eid/IMSI ERROR.\n");
      return 0;
    }
  return eid + imsi;
}
#endif

#if CIS_ENABLE_UPDATE_MCU
cis_ret_t cis_set_sota_info(char *version, uint16_t size)
{
  if (version == NULL)
    {
      return CIS_RET_ERROR;
    }
  cissys_setSotaVersion(version);
  cissys_setSotaMemory(size);
  return CIS_RET_OK;
}

cis_ret_t cis_notify_sota_result(void *context, uint8_t eventid)
{
  st_context_t *ctx = (st_context_t*)context;
  if (ctx == NULL)
    {
      return CIS_RET_ERROR;
    }
  ctx->g_fotacallback.onEvent((cissys_event_t)eventid, NULL, ctx, 0);
  return CIS_RET_OK;
}
#endif

static cis_data_t *sample_dataDup(const cis_data_t *value, cis_attrcount_t attrcount)
{
  cis_data_t *newData;
  newData = (cis_data_t*)cis_malloc(attrcount * sizeof(cis_data_t));
  if (newData == NULL)
    {
      return NULL;
    }
  cis_attrcount_t index;
  for (index = 0; index < attrcount; index++)
    {
      newData[index].id = value[index].id;
      newData[index].type = value[index].type;
      newData[index].asBuffer.length = value[index].asBuffer.length;
      newData[index].asBuffer.buffer = (uint8_t*)cis_malloc(value[index].asBuffer.length);
      cis_memcpy(newData[index].asBuffer.buffer, value[index].asBuffer.buffer, value[index].asBuffer.length);
      cis_memcpy(&newData[index].value.asInteger, &value[index].value.asInteger, sizeof(newData[index].value));
    }
  return newData;
}

cis_coapret_t cis_onRead(void *context, cis_uri_t *uri, cis_mid_t mid)
{
  struct st_callback_info *newNode = (struct st_callback_info*)cis_malloc(sizeof(struct st_callback_info));
  newNode->next = NULL;
  newNode->flag = SAMPLE_CALLBACK_READ;
  newNode->mid = mid;
  newNode->uri = *uri;
  g_callbackList = (struct st_callback_info*)CIS_LIST_ADD(g_callbackList, newNode);
  printf("cis_onRead:(%d/%d/%d)\n", uri->objectId, uri->instanceId, uri->resourceId);
  return CIS_CALLBACK_CONFORM;
}

cis_coapret_t cis_onDiscover(void *context, cis_uri_t *uri, cis_mid_t mid)
{
  struct st_callback_info* newNode = (struct st_callback_info*)cis_malloc(sizeof(struct st_callback_info));
  newNode->next = NULL;
  newNode->flag = SAMPLE_CALLBACK_DISCOVER;
  newNode->mid = mid;
  newNode->uri = *uri;
  g_callbackList = (struct st_callback_info*)CIS_LIST_ADD(g_callbackList, newNode);

  printf("cis_onDiscover:(%d/%d/%d)\n", uri->objectId, uri->instanceId, uri->resourceId);
  return CIS_CALLBACK_CONFORM;
}

cis_coapret_t cis_onWrite(void *context, cis_uri_t *uri, const cis_data_t *value, cis_attrcount_t attrcount, cis_mid_t mid)
{
  if (CIS_URI_IS_SET_RESOURCE(uri))
    {
      printf("cis_onWrite:(%d/%d/%d)\n", uri->objectId, uri->instanceId, uri->resourceId);
    }
  else
    {
      printf("cis_onWrite:(%d/%d)\n", uri->objectId, uri->instanceId);
    }
#if CIS_ENABLE_UPDATE_MCU
  st_context_t *contextP = (st_context_t*)context;
  if (5 == uri->objectId)
    {
      cissys_sleepms(2000);
      cis_notify_sota_result(contextP,sota_write_success);
      return CIS_CALLBACK_CONFORM;
    }
#endif

  struct st_callback_info *newNode = (struct st_callback_info*)cis_malloc(sizeof(struct st_callback_info));
  newNode->next = NULL;
  newNode->flag = SAMPLE_CALLBACK_WRITE;
  newNode->mid = mid;
  newNode->uri = *uri;
  newNode->param.asWrite.count = attrcount;
  newNode->param.asWrite.value = sample_dataDup(value, attrcount);
  g_callbackList = (struct st_callback_info*)CIS_LIST_ADD(g_callbackList, newNode);
  return CIS_CALLBACK_CONFORM;
}


cis_coapret_t cis_onExec(void *context, cis_uri_t *uri, const uint8_t *value, uint32_t length, cis_mid_t mid)
{
  if (CIS_URI_IS_SET_RESOURCE(uri))
    {
      printf("cis_onExec:(%d/%d/%d)\n", uri->objectId, uri->instanceId, uri->resourceId);
    }
  else
    {
      return CIS_CALLBACK_METHOD_NOT_ALLOWED;
    }

  if (!CIS_URI_IS_SET_INSTANCE(uri))
    {
      return CIS_CALLBACK_BAD_REQUEST;
    }

  struct st_callback_info *newNode = (struct st_callback_info*)cis_malloc(sizeof(struct st_callback_info));
  newNode->next = NULL;
  newNode->flag = SAMPLE_CALLBACK_EXECUTE;
  newNode->mid = mid;
  newNode->uri = *uri;
  newNode->param.asExec.buffer = (uint8_t*)cis_malloc(length);
  newNode->param.asExec.length = length;
  cis_memcpy(newNode->param.asExec.buffer, value, length);
  g_callbackList = (struct st_callback_info*)CIS_LIST_ADD(g_callbackList, newNode);
  return CIS_CALLBACK_CONFORM;
}


cis_coapret_t cis_onObserve(void *context, cis_uri_t *uri, bool flag, cis_mid_t mid)
{
  printf("cis_onObserve mid:%d uri:(%d/%d/%d)\n",
    mid,
    uri->objectId,
    CIS_URI_IS_SET_INSTANCE(uri) ? uri->instanceId : -1,
    CIS_URI_IS_SET_RESOURCE(uri) ? uri->resourceId : -1);

  struct st_callback_info *newNode = (struct st_callback_info*)cis_malloc(sizeof(struct st_callback_info));
  newNode->next = NULL;
  newNode->flag = SAMPLE_CALLBACK_OBSERVE;
  newNode->mid = mid;
  newNode->uri = *uri;
  newNode->param.asObserve.flag = flag;
  g_callbackList = (struct st_callback_info*)CIS_LIST_ADD(g_callbackList, newNode);
  return CIS_CALLBACK_CONFORM;
}


cis_coapret_t cis_onParams(void *context, cis_uri_t* uri, cis_observe_attr_t parameters, cis_mid_t mid)
{
  if (CIS_URI_IS_SET_RESOURCE(uri))
    {
      printf("cis_on_params:(%d/%d/%d)\n", uri->objectId, uri->instanceId, uri->resourceId);
    }

  if (!CIS_URI_IS_SET_INSTANCE(uri))
    {
      return CIS_CALLBACK_BAD_REQUEST;
    }

  struct st_callback_info *newNode = (struct st_callback_info*)cis_malloc(sizeof(struct st_callback_info));
  newNode->next = NULL;
  newNode->flag = SAMPLE_CALLBACK_SETPARAMS;
  newNode->mid = mid;
  newNode->uri = *uri;
  newNode->param.asObserveParam.params = parameters;
  g_callbackList = (struct st_callback_info*)CIS_LIST_ADD(g_callbackList, newNode);

  return CIS_CALLBACK_CONFORM;
}



void cis_onEvent(void *context, cis_evt_t eid, void *param)
{
  struct st_callback_info *newNode = (struct st_callback_info*)cis_malloc(sizeof(struct st_callback_info));
  newNode->next = NULL;
  newNode->flag = SAMPLE_CALLBACK_EVENT;
  newNode->param.asEventParam.eid = eid;
  newNode->param.asEventParam.param = param;
  g_callbackList = (struct st_callback_info*)CIS_LIST_ADD(g_callbackList, newNode);
  return;
}
