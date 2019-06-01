/****************************************************************************
 * external/services/iotpf/cis_if_sys.c
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

#include <cis_if_sys.h>
#include <cis_def.h>
#include <cis_internals.h>
#include <cis_config.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cis_api.h>
#include <assert.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#if CIS_ENABLE_CMIOT_OTA
#include "std_object/std_object.h"
#endif

#include <cis_def.h>
#include <cis_api.h>
#include <cis_if_sys.h>
#include <cis_log.h>
#include <cis_list.h>
#include <cis_api.h>
#include <string.h>

#if CIS_ENABLE_UPDATE
DWORD WINAPI testThread(LPVOID lpParam);
#endif
#if CIS_ENABLE_CMIOT_OTA
#define CIS_CMIOT_OTA_WAIT_UPLINK_FINISHED    1000
#define CIS_CMIOT_OTA_START_QUERY_INTERVAL    10000
#define CIS_CMIOT_OTA_START_QUERY_LOOP   	  6
#define CIS_CMIOT_NEWIMSI_STR   	"460040983303979"
extern uint8_t cissys_changeIMSI(char *buffer, uint32_t len);


DWORD WINAPI testCmiotOta(LPVOID lpParam);
uint8_t std_poweruplog_ota_notify(st_context_t *contextP, cis_oid_t *objectid, uint16_t mid);
uint8_t cissys_remove_psm(void);
cis_ota_history_state cissys_quary_ota_finish_state(void);
uint8_t cissys_change_ota_finish_state(uint8_t flag);
uint8_t cissys_recover_psm(void);

extern cis_ota_history_state g_cmiot_otafinishstate_flag;
extern void *g_context;

DWORD WINAPI testCmiotOta(LPVOID lpParam)
{
}

#endif//CIS_ENABLE_CMIOT_OTA


static uint32_t startTime = 0;
cis_ret_t cissys_init(void *context, const cis_cfg_sys_t *cfg, cissys_callback_t *event_cb)
{
  struct st_cis_context *ctx = (struct st_cis_context *)context;
  cis_memcpy(&(ctx->g_sysconfig), cfg, sizeof(cis_cfg_sys_t));
  return 1;
}

uint32_t cissys_gettime()
{
  uint64_t rtime = 0;
  struct timespec t;
  t.tv_sec = t.tv_nsec = 0;
  clock_gettime(CLOCK_MONOTONIC, &t);
  rtime = (((t.tv_sec) * 1000000000LL + t.tv_nsec));
  rtime = rtime / 1000 / 1000;
  rtime -= startTime;
  return (uint32_t)rtime;
}

void cissys_sleepms(uint32_t ms)
{
  struct timespec request = {0, 0};
  request.tv_sec = ms / 1000;
  request.tv_nsec = (ms % 1000) * 1000000;
  clock_nanosleep(CLOCK_REALTIME, 0, &request, 0);
}


clock_t cissys_tick(void)
{
  return 0;
}


void cissys_logwrite(uint8_t *buffer, uint32_t length)
{
  syslog(LOG_INFO, "%s", buffer);
  return;
}

void *cissys_malloc(size_t length)
{
  return malloc(length);
}

void cissys_free(void *buffer)
{
  free(buffer);
}


void *cissys_memset(void *s, int c, size_t n)
{
  return memset(s, c, n);
}

void *cissys_memcpy(void *dst, const void *src, size_t n)
{
  return memcpy(dst, src, n);
}

void *cissys_memmove(void *dst, const void *src, size_t n)
{
  return memmove(dst, src, n);
}

int cissys_memcmp( const void *s1, const void *s2, size_t n)
{
  return memcmp( s1, s2, n);
}

void cissys_fault(uint16_t id)
{
  syslog(LOG_ERR, "fall in cissys_fault.id=%d\r\n", id);
  exit(0);
}


void cissys_assert(bool flag)
{
  assert(flag);
}


uint32_t cissys_rand()
{
  srand(cissys_gettime());
  union
    {
      uint8_t varray[4];
      uint32_t vint;
    }val;
  for (int i = 0; i < 4; i++)
    {
      val.varray[i] = rand() % 0xFF;
    }
  return val.vint;
}

uint8_t cissys_getIMEI(char *buffer, uint32_t maxlen)
{
  ciscom_getImei(buffer);
  return strlen(buffer);
}
uint8_t cissys_getIMSI(char *buffer, uint32_t maxlen)
{
  ciscom_getImsi(buffer);
  return strlen(buffer);
}

#if CIS_ENABLE_CMIOT_OTA

uint8_t cissys_changeIMSI(char *buffer, uint32_t len)
{
  return false;
}


uint8_t cissys_getEID(char *buffer, uint32_t maxlen)
{
}

uint8_t cissys_remove_psm(void)
{
  return 1;
}

uint8_t cissys_recover_psm(void)
{
  return 1;
}

cis_ota_history_state cissys_quary_ota_finish_state(void)
{
  return g_cmiot_otafinishstate_flag;
}
uint8_t cissys_change_ota_finish_state(cis_ota_history_state flag)
{
  g_cmiot_otafinishstate_flag = flag;
  return 1;
}
#endif

void cissys_lockcreate(void **mutex)
{
  pthread_mutex_t *pMutex = (pthread_mutex_t *)cissys_malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(pMutex, NULL);
  *mutex = pMutex;
}

void cissys_lockdestory(void *mutex)
{
    pthread_mutex_destroy((pthread_mutex_t *)mutex);
    cis_free(mutex);
}

cis_ret_t cissys_lock(void *mutex, uint32_t ms)
{
    assert(mutex != NULL);
    pthread_mutex_lock((pthread_mutex_t *)mutex);
    return CIS_RET_OK;
}

void cissys_unlock(void *mutex)
{
    pthread_mutex_unlock((pthread_mutex_t *)mutex);
}

bool cissys_save(uint8_t *buffer, uint32_t length)
{
  return false;
}

bool cissys_load(uint8_t *buffer, uint32_t length)
{
  return false;
}
char VERSION[16] = "2.2.0";
uint32_t cissys_getFwVersion(uint8_t **version)
{
  int length = strlen(VERSION) + 1;
  cis_memcpy(*version, VERSION, length);
  return length;
}

#define DEFAULT_CELL_ID (95)
#define DEFAULT_RADIO_SIGNAL_STRENGTH (99)

uint32_t cissys_getCellId(void)
{
  int ret = ciscom_getCellId();
  if (ret < 0)
    {
      return DEFAULT_CELL_ID;
    }
  else
    {
      return ret;
    }
}

uint32_t cissys_getRadioSignalStrength()
{
  return DEFAULT_RADIO_SIGNAL_STRENGTH;
}



#if CIS_ENABLE_UPDATE

#define DEFAULT_BATTERY_LEVEL (99)
#define DEFAULT_BATTERY_VOLTAGE (3800)
#define DEFAULT_FREE_MEMORY   (554990)

#if CIS_ENABLE_UPDATE_MCU
bool isupdatemcu = false;
#endif

int LENGTH = 0;
int RESULT = 0;
int STATE = 0;


#if 1
int writeCallback = 0;
int validateCallback = 0;
int eraseCallback = 0;
DWORD WINAPI testThread(LPVOID lpParam)
{
  cissys_callback_t *cb = (cissys_callback_t *)lpParam;
  while (1)
    {
      if (writeCallback == 1) //write callback
        {
          cissys_sleepms(2000);
          writeCallback = 0;
          cb->onEvent(cissys_event_write_success, NULL, cb->userData, NULL);
        }
      else if(eraseCallback == 1) //erase callback
        {
          cissys_sleepms(5000);
          eraseCallback = 0;
          cb->onEvent(cissys_event_fw_erase_success, 0, cb->userData, NULL);
        }
      else if(validateCallback == 1)//validate
        {
          cissys_sleepms(3000);
          validateCallback = 0;
          cb->onEvent(cissys_event_fw_validate_success, 0, cb->userData, NULL);
        }
    }
  return 0;
}

#endif


uint32_t cissys_getFwBatteryLevel()
{
  return DEFAULT_BATTERY_LEVEL;
}

uint32_t cissys_getFwBatteryVoltage()
{
  return DEFAULT_BATTERY_VOLTAGE;
}
uint32_t cissys_getFwAvailableMemory()
{
  return DEFAULT_FREE_MEMORY;
}

int cissys_getFwSavedBytes()
{
  return LENGTH;
}

bool cissys_checkFwValidation(cissys_callback_t *cb)
{
  validateCallback = 1;
  return true;
}



bool cissys_eraseFwFlash(cissys_callback_t *cb)
{
  eraseCallback = 1;
  return true;
}

void cissys_ClearFwBytes(void)
{
  LENGTH = 0;
}

uint32_t cissys_writeFwBytes(uint32_t size, uint8_t *buffer, cissys_callback_t *cb)
{
  cissys_sleepms(2000);
  cb->onEvent(cissys_event_write_success, NULL, cb->userData, (int *)&size);
  return 1;
}


void cissys_savewritebypes(uint32_t size)
{
  LENGTH += size;
}

bool cissys_updateFirmware (cissys_callback_t *cb)
{
  cissys_sleepms(10000);
  cis_memcpy(VERSION, "1801102", sizeof("1801102"));
  return true;
}

bool cissys_readContext(cis_fw_context_t *context)
{
  context->result = RESULT;
  context->savebytes = LENGTH;
  context->state = STATE;
  return 1;
}


bool cissys_setFwState(uint8_t state)
{
  STATE = state;
  return true;
}
bool cissys_setFwUpdateResult(uint8_t result)
{
  RESULT=result;
  return true;
}

#if CIS_ENABLE_UPDATE_MCU

char SOTA_VERSION[16] = "0.0.0";
uint32_t MCU_SotaSize = 100;


bool cissys_setSwState(bool ismcu)
{
  isupdatemcu = ismcu;
  return true;
}

bool cissys_getSwState(void)
{
  return isupdatemcu;
}

void cissys_setSotaMemory(uint32_t size)
{
  MCU_SotaSize = size;
}

uint32_t cissys_getSotaMemory(void)
{
  return MCU_SotaSize;
}

uint32_t cissys_getSotaVersion(uint8_t **version)
{
  int length = strlen(SOTA_VERSION) + 1;
  cis_memcpy(*version, SOTA_VERSION, length);
  return length;
}

void cissys_setSotaVersion(char *version)
{
  int length = strlen(version) + 1;
  cis_memcpy(SOTA_VERSION, version, length);
}

#endif


#endif // CIS_ENABLE_UPDATE

#if CIS_ENABLE_MONITER
int8_t cissys_getSINR()
{
  return 0;
}
int8_t cissys_getBearer()
{
  return 0;
}

int8_t cissys_getUtilization()
{
  return 0;
}

int8_t cissys_getCSQ()
{
  return 0;
}

int8_t cissys_getECL()
{
  return 0;
}

int8_t cissys_getSNR()
{
  return 0;
}

int16_t cissys_getPCI()
{
  return 0;
}

int32_t cissys_getECGI()
{
  return 0;
}

int32_t cissys_getEARFCN()
{
  return 0;
}

int16_t cissys_getPCIFrist()
{
  return 0;
}

int16_t   cissys_getPCISecond()
{
    return 0;
}

int8_t cissys_getRSRPFrist()
{
  return 0;
}

int8_t cissys_getRSRPSecond()
{
  return 0;
}

int8_t     cissys_getPLMN()
{
	return 0;
}

int8_t cissys_getLatitude()
{
  return 0;
}

int8_t cissys_getLongitude()
{
  return 0;
}

int8_t cissys_getAltitude()
{
  return 0;
}
#endif // CIS_ENABLE_MONITER
