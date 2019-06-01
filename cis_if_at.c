/****************************************************************************
 * external/services/iotpf/cis_if_at.c
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
#include "std_object/std_object.h"

typedef void (*cis_atcmd_handler_t)(int fd, char *param);

struct cis_atcmd_table_s
{
  const char            *cmd;
  cis_atcmd_handler_t   handler;
};

typedef enum
{
  CIS_ERROR_INVALID_PARAM = 601,
  CIS_ERROR_STATE_PARAM = 602,
  CIS_ERROR_UNKNOWN = 100,
}CIS_ERROR;


#define ATCMD_BUFMAX            256
#define DEFAULT_AT_TIMEOUT      30
#define COM_SOFT_VERSION      "1.0"

#define MAX_PACKET_SIZE			(256)

#ifndef ARRAY_SIZE
#  define ARRAY_SIZE(x)         (sizeof(x) / sizeof((x)[0]))
#endif


static char *g_cis_at_name = "/dev/ttyp0";

static int g_fd;
static char g_cis_at_cr = '\r';
static char g_cis_at_lf = '\n';



static int g_cis_atlen = 0;
static char g_cis_atbuf[ATCMD_BUFMAX];

extern void cis_miplver_handler(int fd, char *param);
extern void cis_miplcreate_handler(int fd, char *param);
extern void cis_miplopen_handler(int fd, char *param);
extern void cis_miplclose_handler(int fd, char *param);
extern void cis_mipldelete_handler(int fd, char *param);
extern void cis_mipladdobj_handler(int fd, char *param);
extern void cis_mipldelobj_handler(int fd, char *param);
extern void cis_mipldiscoverrsp_handler(int fd, char *param);
extern void cis_miplupdate_handler(int fd, char *param);
extern void cis_miplreadrsp_handler(int fd, char *param);
extern void cis_miplwritersp_handler(int fd, char *param);
extern void cis_miplexecutersp_handler(int fd, char *param);
extern void cis_miplparameterrsp_handler(int fd, char *param);
extern void cis_miplobserversp_handler(int fd, char *param);
extern void cis_miplnotify_handler(int fd, char *param);


static const struct cis_atcmd_table_s g_ciscmd[] =
{
  {"AT+MIPLVER", cis_miplver_handler},
  {"AT+MIPLCREATE", cis_miplcreate_handler},
  {"AT+MIPLOPEN", cis_miplopen_handler},
  {"AT+MIPLCLOSE", cis_miplclose_handler},
  {"AT+MIPLDELETE", cis_mipldelete_handler},
  {"AT+MIPLADDOBJ", cis_mipladdobj_handler},
  {"AT+MIPLDELOBJ", cis_mipldelobj_handler},
  {"AT+MIPLDISCOVERRSP", cis_mipldiscoverrsp_handler},
  {"AT+MIPLUPDATE", cis_miplupdate_handler},
  {"AT+MIPLREADRSP", cis_miplreadrsp_handler},
  {"AT+MIPLWRITERSP", cis_miplwritersp_handler},
  {"AT+MIPLEXECUTERSP", cis_miplexecutersp_handler},
  {"AT+MIPLPARAMETERRSP", cis_miplparameterrsp_handler},
  {"AT+MIPLOBSERVERSP", cis_miplobserversp_handler},
  {"AT+MIPLNOTIFY", cis_miplnotify_handler},
};

void *g_context;

static pthread_mutex_t g_cis_at_mutex;
static pthread_cond_t g_cis_at_cond;

static bool g_cis_observe_flag;
static cis_uri_t *g_cis_uri;

static int g_cisat_pip_fd[2];

static pthread_t g_cisat_onenet_tid = -1;


ssize_t cis_write(int fd, const void *buf, size_t len)
{
  ssize_t once = 0, written = 0;

  while (len > 0)
    {
      once = write(fd, buf, len);
      if (once <= 0)
        {
          break;
        }
      len -= once;
      buf += once;
      written += once;
    }

  return written ? written : once;
}

void cis_response_error(int fd, int errId)
{
  char buf[20] = {0};
  snprintf(buf, sizeof(buf), "%c%c+CIS ERROR:%d%c%c",
    g_cis_at_cr, g_cis_at_lf, errId, g_cis_at_cr, g_cis_at_lf);
  cis_write(fd, buf, strlen(buf));
}

void cis_response_ok(int fd)
{
  char buf[10] = {0};
  snprintf(buf, sizeof(buf), "%c%c+OK%c%c",
    g_cis_at_cr, g_cis_at_lf, g_cis_at_cr, g_cis_at_lf);
  cis_write(fd, buf, strlen(buf));
}


static int cis_at_tok_start(char **p_cur)
{
  if (*p_cur == NULL)
    {
      return -1;
    }

  *p_cur = strchr(*p_cur, '=');

  if (*p_cur == NULL)
    {
      return -1;
    }

  (*p_cur)++;

  return 0;
}

static void cis_wait_at_res(void)
{
  struct timespec time;
  struct timeval now;
  gettimeofday(&now, NULL);
  time.tv_sec = now.tv_sec + DEFAULT_AT_TIMEOUT;
  time.tv_nsec = now.tv_usec * 1000;
  pthread_mutex_lock(&g_cis_at_mutex);
  pthread_cond_timedwait(&g_cis_at_cond, &g_cis_at_mutex, &time);
  pthread_mutex_unlock(&g_cis_at_mutex);
}

static void cis_notify_at_res(void)
{
  pthread_mutex_lock(&g_cis_at_mutex);
  pthread_cond_signal(&g_cis_at_cond);
  pthread_mutex_unlock(&g_cis_at_mutex);
}

void cisat_wakeup_pump(void)
{
  LOGD("at wake up the pump");
  write(g_cisat_pip_fd[1], "w", 1);
}

void *cisat_onenet_thread(void *obj)
{
  pthread_setname_np(pthread_self(), "cisat_onenet_thread");
  while(1)
    {
      struct timeval tv;
      st_context_t *ctx = (st_context_t *)g_context;
      fd_set readfds;
      int result;
      int netFd = -1;
      int maxfd = 0;

      tv.tv_sec = 60;
      tv.tv_usec = 0;

      FD_ZERO(&readfds);
      FD_SET(g_cisat_pip_fd[0], &readfds);
      maxfd = g_cisat_pip_fd[0];
      if (ctx->pNetContext && ctx->pNetContext->sock > 0)
        {
          LOGD("cisat_onenet_thread net sock:%d", ctx->pNetContext->sock);
          FD_SET(ctx->pNetContext->sock, &readfds);
          netFd = ctx->pNetContext->sock;
          if (ctx->pNetContext->sock > maxfd)
            {
              maxfd = ctx->pNetContext->sock;
            }
        }
      result = cis_pump(g_context, &tv.tv_sec);
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
          if(FD_ISSET(g_cisat_pip_fd[0], &readfds))
			{
			  char c[2];
              read(g_cisat_pip_fd[0], c, 1);
              if (c[0] == 'q')
                {
                  LOGD("!!!!!!!!cisat_onenet_thread quit");
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
                  LOGD("\r\n%d bytes received\r\n", numBytes);
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
    }
  pthread_exit(NULL);
}

static cis_coapret_t cis_at_onRead(void *context, cis_uri_t *uri, cis_mid_t mid)
{
  int16_t objectId;
  int16_t instanceId;
  int16_t resourceId;
  char buf[ATCMD_BUFMAX];

  uri_unmake(&objectId, &instanceId, &resourceId, uri);
  snprintf(buf, sizeof(buf), "%c%c+MIPLREAD:0, %d, %d, %d, %d%c%c", g_cis_at_cr, g_cis_at_lf, mid,
    objectId, instanceId, resourceId, g_cis_at_cr, g_cis_at_lf);
  cis_write(g_fd, buf, strlen(buf));
  cis_wait_at_res();
  return CIS_RET_OK;
}

static cis_coapret_t cis_at_onDiscover(void *context, cis_uri_t *uri, cis_mid_t mid)
{
  char buf[ATCMD_BUFMAX];

  snprintf(buf, sizeof(buf), "%c%c+MIPLDISCOVER:0, %d, %d%c%c", g_cis_at_cr, g_cis_at_lf, mid, uri->objectId, g_cis_at_cr, g_cis_at_lf);
  cis_write(g_fd, buf, strlen(buf));
  cis_wait_at_res();
  return CIS_RET_OK;
}

static cis_coapret_t cis_at_onWrite(void *context, cis_uri_t *uri, const cis_data_t *value, cis_attrcount_t attrcount, cis_mid_t mid)
{
  int16_t objectId;
  int16_t instanceId;
  int16_t resourceId;
  char buf[ATCMD_BUFMAX];
  char strBuf[20];
  int index;
  int len;
  int ret;

  uri_unmake(&objectId, &instanceId, &resourceId, uri);
  for (index = 0; index < attrcount; index++)
    {
    #if CIS_OPERATOR_CTCC
      len = snprintf(buf, sizeof(buf), "%c%c+MIPLWRITE:%d", g_cis_at_cr, g_cis_at_lf, mid);
    #else
      len = snprintf(buf, sizeof(buf), "%c%c+MIPLWRITE:0, %d, %d, %d, %d, %d", g_cis_at_cr, g_cis_at_lf, mid,
        objectId, instanceId, (value + index)->id, (value + index)->type);
    #endif
      switch (value->type)
        {
          case cis_data_type_string:
            len += snprintf(buf + len, sizeof(buf) - len, ", %d, \"%s\"", (value + index)->asBuffer.length,
                (value + index)->asBuffer.buffer);
            break;
          case cis_data_type_opaque:
            {
              int i;
              len += snprintf(buf + len, sizeof(buf) - len, ", %d, ", (value + index)->asBuffer.length);
              for (i = 0; i < (value + index)->asBuffer.length; i++)
                {
                  len += snprintf(buf + len, sizeof(buf) - len, "%02X", (value + index)->asBuffer.buffer[i]);
                }
              break;
            }
          case cis_data_type_integer:
            ret = cis_utils_intToText((value + index)->value.asInteger, (uint8_t *)strBuf, sizeof(strBuf));
            len += snprintf(buf + len, sizeof(buf) - len, ", %d, %s", ret,
                strBuf);
            break;
          case cis_data_type_float:
            ret = utils_floatToText((value + index)->value.asFloat, (uint8_t *)strBuf, sizeof(strBuf));
            len += snprintf(buf + len, sizeof(buf) - len, ", %d, %s", ret,
                strBuf);
            break;
          case cis_data_type_bool:
            len += snprintf(buf + len, sizeof(buf) - len, ", 1, %d", (value + index)->value.asBoolean);
            break;
          default:
            LOGD("invalid data type:%d", value->type);
            break;
        }
      len += snprintf(buf + len, sizeof(buf) - len, ", 0, %d%c%c", attrcount - index - 1, g_cis_at_cr, g_cis_at_lf);
      cis_write(g_fd, buf, strlen(buf));
    }
  cis_wait_at_res();
  return CIS_RET_OK;
}


static cis_coapret_t cis_at_onExec(void *context, cis_uri_t *uri, const uint8_t *value, uint32_t length, cis_mid_t mid)
{
  int16_t objectId;
  int16_t instanceId;
  int16_t resourceId;
  char buf[ATCMD_BUFMAX];
  char *pValueBuf;

  uri_unmake(&objectId, &instanceId, &resourceId, uri);
  if (length > 0 && value != NULL)
    {
      pValueBuf = cissys_malloc(length + 1);
      cissys_memset(pValueBuf, 0x0, length + 1);
      cissys_memcpy(pValueBuf, value, length);
      snprintf(buf, sizeof(buf), "%c%c+MIPLEXECUTE:0, %d, %d, %d, %d, %d, %s%c%c", g_cis_at_cr, g_cis_at_lf, mid,
        objectId, instanceId, resourceId, length, pValueBuf, g_cis_at_cr, g_cis_at_lf);
    }
  else
    {
      snprintf(buf, sizeof(buf), "%c%c+MIPLEXECUTE:0, %d, %d, %d, %d%c%c", g_cis_at_cr, g_cis_at_lf, mid,
        objectId, instanceId, resourceId, g_cis_at_cr, g_cis_at_lf);
    }
  cis_write(g_fd, buf, strlen(buf));
  cis_wait_at_res();
  return CIS_RET_OK;
}


static cis_coapret_t cis_at_onObserve(void *context, cis_uri_t *uri, bool flag, cis_mid_t mid)
{
  int16_t objectId;
  int16_t instanceId;
  int16_t resourceId;
  char buf[ATCMD_BUFMAX];
  g_cis_uri = uri;
  g_cis_observe_flag = flag;
  uri_unmake(&objectId, &instanceId, &resourceId, uri);
  snprintf(buf, sizeof(buf), "%c%c+MIPLOBSERVE:0, %d, %d, %d, %d, %d%c%c", g_cis_at_cr, g_cis_at_lf, mid,
        flag, objectId, instanceId, resourceId, g_cis_at_cr, g_cis_at_lf);
  cis_write(g_fd, buf, strlen(buf));
  cis_wait_at_res();
  return CIS_RET_OK;
}

static cis_coapret_t cis_at_onParams(void *context, cis_uri_t *uri, cis_observe_attr_t parameters, cis_mid_t mid)
{
  int16_t objectId;
  int16_t instanceId;
  int16_t resourceId;
  int len;
  char buf[ATCMD_BUFMAX];
  char paramBuf[100];
  int paramLen = 0;

  uri_unmake(&objectId, &instanceId, &resourceId, uri);
  len = snprintf(buf, sizeof(buf), "%c%c+MIPLPARAMETER:0, %d, %d, %d, %d", g_cis_at_cr, g_cis_at_lf, mid,
        objectId, instanceId, resourceId);
  if (parameters.toSet & ATTR_FLAG_MIN_PERIOD)
    {
      paramLen += snprintf(paramBuf + paramLen, sizeof(paramBuf) - paramLen, "pmin=%d;", parameters.minPeriod);
    }
  if (parameters.toSet & ATTR_FLAG_MAX_PERIOD)
    {
      paramLen += snprintf(paramBuf + paramLen, sizeof(paramBuf) - paramLen, "pmax=%d;", parameters.maxPeriod);
    }
  if (parameters.toSet & ATTR_FLAG_GREATER_THAN)
    {
      paramLen += snprintf(paramBuf + paramLen, sizeof(paramBuf) - paramLen, "gt=%f;", parameters.greaterThan);
    }
  if (parameters.toSet & ATTR_FLAG_LESS_THAN)
    {
      paramLen += snprintf(paramBuf + paramLen, sizeof(paramBuf) - paramLen, "lt=%f;", parameters.lessThan);
    }
  if (parameters.toSet & ATTR_FLAG_LESS_THAN)
    {
      paramLen += snprintf(paramBuf + paramLen, sizeof(paramBuf) - paramLen, "stp=%f;", parameters.step);
    }
  snprintf(buf + len, sizeof(buf) - len, " %d, %s%c%c", paramLen, paramBuf, g_cis_at_cr, g_cis_at_lf);
  cis_write(g_fd, buf, strlen(buf));
  cis_wait_at_res();
  return CIS_RET_OK;
}

static void cis_at_onEvent(void *context, cis_evt_t eid, void *param)
{
  char buf[ATCMD_BUFMAX];

  switch (eid)
    {
      case CIS_EVENT_RESPONSE_FAILED:
      case CIS_EVENT_NOTIFY_FAILED:
      case CIS_EVENT_UPDATE_NEED:
        snprintf(buf, sizeof(buf), "%c%c+MIPLEVENT:0,%d,%d%c%c", g_cis_at_cr, g_cis_at_lf, eid, (int)param, g_cis_at_cr, g_cis_at_lf);
        break;
      default:
        snprintf(buf, sizeof(buf), "%c%c+MIPLEVENT:0, %d%c%c", g_cis_at_cr, g_cis_at_lf, eid, g_cis_at_cr, g_cis_at_lf);
        break;
    }
  cis_write(g_fd, buf, strlen(buf));
}



void cis_miplver_handler(int fd, char *param)
{
  char buf[ATCMD_BUFMAX];
  snprintf(buf, sizeof(buf), "%c%c+MIPLVER:%s%c%c",
    g_cis_at_cr, g_cis_at_lf, COM_SOFT_VERSION, g_cis_at_cr, g_cis_at_lf);
  cis_write(fd, buf, strlen(buf));
  cis_response_ok(fd);
}

void cis_miplcreate_handler(int fd, char *param)
{
  int ret;
  char *line;
  int index, flag;
  char *valueStr = NULL;
  char *pConfig = NULL;
  int totalSize = 0;

  line = param;
  ret = cis_at_tok_start(&line);
  if (ret == 0)
    {
      /*TODO: only support config in one AT commnad now*/
      ret = at_tok_nextint(&line, &totalSize);
      if (ret < 0)
        {
          goto error;
        }

      ret = at_tok_nextstr(&line, &valueStr);
      if (ret < 0)
        {
          goto error;
        }

      ret = at_tok_nextint(&line, &index);
      if (ret < 0)
        {
          goto error;
        }

      ret = at_tok_nextint(&line, &flag);
      if (ret < 0)
        {
          goto error;
        }
    }
  if (totalSize > 0)
    {
      pConfig = (char *)cissys_malloc(totalSize);
      utils_hexStrToByte(valueStr, strlen(valueStr), (unsigned char *)pConfig);
    }
  if (cis_init(&g_context, (void *)pConfig, totalSize, NULL) == CIS_RET_OK)
    {
      char buf[ATCMD_BUFMAX];
      pthread_attr_t attr;
      
      ret = pipe(g_cisat_pip_fd);
      pthread_attr_init(&attr);
      pthread_attr_setstacksize(&attr, 4096);
      if (pthread_create(&g_cisat_onenet_tid, &attr, cisat_onenet_thread, NULL))
        {
          LOGE("%s: pthread_create (%s)\n", __func__, strerror(errno));
          goto error;
        }
      snprintf(buf, sizeof(buf), "%c%c+MIPLCREATE:0%c%c",
        g_cis_at_cr, g_cis_at_lf, g_cis_at_cr, g_cis_at_lf);
      cis_write(fd, buf, strlen(buf));
      cis_response_ok(fd);

      return;
    }
error:
  if (g_context != NULL)
    {
      cis_deinit(&g_context);
    }
  if (g_cisat_pip_fd[0])
    {
      close(g_cisat_pip_fd[0]);
    }
  if (g_cisat_pip_fd[1])
    {
      close(g_cisat_pip_fd[1]);
    }
  cis_response_error(fd, CIS_ERROR_INVALID_PARAM);
  return;
}

void cis_miplopen_handler(int fd, char *param)
{
  int ret;
  char *line;
  int ref, lifetime;
  int timeout = 0;

  line = param;
  ret = cis_at_tok_start(&line);
  if (ret < 0)
    {
      goto error;
    }
  ret = at_tok_nextint(&line, &ref);
  if (ret < 0)
    {
      goto error;
    }
  ret = at_tok_nextint(&line, &lifetime);
  if (ret < 0)
    {
      goto error;
    }
  if (at_tok_hasmore(&line))
    {
      /*TODO: only support config in one AT commnad now*/
      ret = at_tok_nextint(&line, &timeout);
      if (ret < 0)
        {
          goto error;
        }
    }

  cis_callback_t callback;
  callback.onRead = cis_at_onRead;
  callback.onWrite = cis_at_onWrite;
  callback.onExec = cis_at_onExec;
  callback.onObserve = cis_at_onObserve;
  callback.onSetParams = cis_at_onParams;
  callback.onEvent = cis_at_onEvent;
  callback.onDiscover = cis_at_onDiscover;

  if (cis_register(g_context, lifetime, &callback) == CIS_RET_OK)
    {
      cisat_wakeup_pump();
      cis_response_ok(fd);
      return;
    }
  else
    {
      cis_response_error(fd, CIS_ERROR_STATE_PARAM);
      return;
    }
error:
  cis_response_error(fd, CIS_ERROR_INVALID_PARAM);
  return;
}

void cis_mipldelete_handler(int fd, char *param)
{
  int ret;
  char *line;
  int ref;

  line = param;
  ret = cis_at_tok_start(&line);
  if (ret < 0)
    {
      goto error;
    }
  ret = at_tok_nextint(&line, &ref);
  if (ret < 0)
    {
      goto error;
    }
  if (g_cisat_onenet_tid != -1)
    {
      write(g_cisat_pip_fd[1], "q", 1);
      pthread_join(g_cisat_onenet_tid, NULL);
      close(g_cisat_pip_fd[0]);
      close(g_cisat_pip_fd[1]);
      cis_deinit(&g_context);
      g_cisat_onenet_tid = -1;
    }

  cis_response_ok(fd);
  return;
error:
  cis_response_error(fd, CIS_ERROR_INVALID_PARAM);
  return;
}

void cis_miplclose_handler(int fd, char *param)
{
  int ret;
  char *line;
  int ref;

  line = param;
  ret = cis_at_tok_start(&line);
  if (ret < 0)
    {
      goto error;
    }
  ret = at_tok_nextint(&line, &ref);
  if (ret < 0)
    {
      goto error;
    }
  cis_unregister(g_context);
  cisat_wakeup_pump();
  cis_response_ok(fd);
  return;
error:
  cis_response_error(fd, CIS_ERROR_INVALID_PARAM);
  return;
}

void cis_mipladdobj_handler(int fd, char *param)
{
  int ret, i;
  char *line;
  int ref, objectId, instanceCount, attributecount, actioncount;
  char *pInstancebitmap;

  line = param;
  ret = cis_at_tok_start(&line);
  if (ret < 0)
    {
      goto error;
    }
  ret = at_tok_nextint(&line, &ref);
  if (ret < 0)
    {
      goto error;
    }
  ret = at_tok_nextint(&line, &objectId);
  if (ret < 0)
    {
      goto error;
    }
  ret = at_tok_nextint(&line, &instanceCount);
  if (ret < 0)
    {
      goto error;
    }
  ret = at_tok_nextstr(&line, &pInstancebitmap);
  if (ret < 0)
    {
      goto error;
    }
  ret = at_tok_nextint(&line, &attributecount);
  if (ret < 0)
    {
      goto error;
    }
  ret = at_tok_nextint(&line, &actioncount);
  if (ret < 0)
    {
      goto error;
    }
  cis_inst_bitmap_t inst_bitmap;
  cis_res_count_t res_count;
  inst_bitmap.instanceBytes = (instanceCount - 1) / 8 + 1;
  inst_bitmap.instanceCount = instanceCount;

  uint8_t *instPtr = (uint8_t*)cis_malloc(inst_bitmap.instanceBytes);
  cis_memset(instPtr, 0, inst_bitmap.instanceBytes);

  for (i = 0; i < instanceCount; i++)
    {
      cis_instcount_t instBytePos = i / 8;
      cis_instcount_t instByteOffset = 7 - (i % 8);
      if (pInstancebitmap[i] == '1')
        {
          instPtr[instBytePos] += 0x01 << instByteOffset;
        }
    }
  inst_bitmap.instanceBitmap = instPtr;
  res_count.actCount = actioncount;
  res_count.attrCount = attributecount;
  if (cis_addobject(g_context, objectId, &inst_bitmap, &res_count))
    {
      cis_response_error(fd, CIS_ERROR_UNKNOWN);
    }
  else
    {
      cis_response_ok(fd);
    }
  cis_free(instPtr);
  return;
error:
  cis_response_error(fd, CIS_ERROR_INVALID_PARAM);
  return;
}

void cis_mipldelobj_handler(int fd, char *param)
{
  int ret;
  char *line;
  int ref, objectId;

  line = param;
  ret = cis_at_tok_start(&line);
  if (ret < 0)
    {
      goto error;
    }
  ret = at_tok_nextint(&line, &ref);
  if (ret < 0)
    {
      goto error;
    }

  ret = at_tok_nextint(&line, &objectId);
  if (ret < 0)
    {
      goto error;
    }

  if (cis_delobject(g_context, objectId))
    {
      cis_response_error(fd, CIS_ERROR_UNKNOWN);
    }
  else
    {
      cis_response_ok(fd);
    }
  return;
error:
  cis_response_error(fd, CIS_ERROR_INVALID_PARAM);
  return;
}

void cis_miplupdate_handler(int fd, char *param)
{
  int ret;
  char *line;
  int ref, lifetime, withObjectFlag;

  line = param;
  ret = cis_at_tok_start(&line);
  if (ret < 0)
    {
      goto error;
    }
  ret = at_tok_nextint(&line, &ref);
  if (ret < 0)
    {
      goto error;
    }

  ret = at_tok_nextint(&line, &lifetime);
  if (ret < 0)
    {
      goto error;
    }

  ret = at_tok_nextint(&line, &withObjectFlag);
  if (ret < 0)
    {
      goto error;
    }

  if (cis_update_reg(g_context, lifetime, withObjectFlag == 1 ? true : false))
    {
      cis_response_error(fd, CIS_ERROR_UNKNOWN);
    }
  else
    {
      cis_response_ok(fd);
      cisat_wakeup_pump();
    }
  return;
error:
  cis_response_error(fd, CIS_ERROR_INVALID_PARAM);
  return;
}

void cis_miplobserversp_handler(int fd, char *param)
{
  int ret;
  char *line;
  int ref, msgId, result;

  line = param;
  ret = cis_at_tok_start(&line);
  if (ret < 0)
    {
      goto error;
    }
  ret = at_tok_nextint(&line, &ref);
  if (ret < 0)
    {
      goto error;
    }

  ret = at_tok_nextint(&line, &msgId);
  if (ret < 0)
    {
      goto error;
    }

  ret = at_tok_nextint(&line, &result);
  if (ret < 0)
    {
      goto error;
    }

  if (cis_response(g_context, NULL, NULL, msgId, cis_result_to_cis_response(result)) == CIS_RET_OK)
    {
      cisat_wakeup_pump();
      cis_response_ok(fd);
      cis_notify_at_res();
      return;
    }
error:
  cis_response_error(fd, CIS_ERROR_INVALID_PARAM);
  cis_notify_at_res();
  return;
}

void cis_mipldiscoverrsp_handler(int fd, char *param)
{
  int ret;
  char *line;
  int ref, msgId, result, len;
  char *valueStr;

  line = param;
  ret = cis_at_tok_start(&line);
  if (ret < 0)
    {
      goto error;
    }
  ret = at_tok_nextint(&line, &ref);
  if (ret < 0)
    {
      goto error;
    }

  ret = at_tok_nextint(&line, &msgId);
  if (ret < 0)
    {
      goto error;
    }

  ret = at_tok_nextint(&line, &result);
  if (ret < 0)
    {
      goto error;
    }

  ret = at_tok_nextint(&line, &len);
  if (ret < 0)
    {
      goto error;
    }

  ret = at_tok_nextstr(&line, &valueStr);
  if (ret < 0)
    {
      goto error;
    }

  if (cis_discover_response(g_context, msgId, result, valueStr) == CIS_RET_OK)
    {
      cisat_wakeup_pump();
      cis_response_ok(fd);
      cis_notify_at_res();
      return;
    }
error:
  cis_response_error(fd, CIS_ERROR_INVALID_PARAM);
  cis_notify_at_res();
  return;
}

void cis_miplnotify_handler(int fd, char *param)
{
  int ret;
  char *line;
  int objectId, instanceId, resourceId, valuetype, len, index, flag;
  int msgId = 0;
  char *valueStr;
  cis_uri_t uri;
  cis_coapret_t result;

  line = param;
  ret = cis_at_tok_start(&line);
  if (ret < 0)
    {
      goto error;
    }
#if CIS_OPERATOR_CTCC
  objectId = std_object_binary_app_data_container;
  instanceId = 0;
  resourceId = 0;
  valuetype = cis_data_type_opaque;
  st_observed_t *targetP;
  targetP = ((st_context_t *)g_context)->observedList;
  if (targetP)
    {
      msgId = targetP->msgid;
    }
#else
  int ref;
  ret = at_tok_nextint(&line, &ref);
  if (ret < 0)
    {
      goto error;
    }

  ret = at_tok_nextint(&line, &msgId);
  if (ret < 0)
    {
      goto error;
    }

  ret = at_tok_nextint(&line, &objectId);
  if (ret < 0)
    {
      goto error;
    }

  ret = at_tok_nextint(&line, &instanceId);
  if (ret < 0)
    {
      goto error;
    }

  ret = at_tok_nextint(&line, &resourceId);
  if (ret < 0)
    {
      goto error;
    }

  ret = at_tok_nextint(&line, &valuetype);
  if (ret < 0)
    {
      goto error;
    }
#endif
  ret = at_tok_nextint(&line, &len);
  if (ret < 0)
    {
      goto error;
    }

  ret = at_tok_nextstr(&line, &valueStr);
  if (ret < 0)
    {
      goto error;
    }

  ret = at_tok_nextint(&line, &index);
  if (ret < 0)
    {
      goto error;
    }

  ret = at_tok_nextint(&line, &flag);
  if (ret < 0)
    {
      goto error;
    }

  uri_make(objectId, instanceId, resourceId, &uri);
  if (index == 0)
    {
      result = CIS_NOTIFY_CONTENT;
    }
  else
    {
      result = CIS_NOTIFY_CONTINUE;
    }
  st_data_t data;
  data.type = valuetype;
  switch (valuetype)
    {
      case cis_data_type_string:
        data_encode_string(valueStr, &data);
        break;
      case cis_data_type_opaque:
        {
          char *p = (char *)cissys_malloc(len);
          utils_hexStrToByte(valueStr, strlen(valueStr), (unsigned char *)p);
          data_encode_opaque((uint8_t *)p, len, &data);
          cissys_free(p);
          break;
        }
      case cis_data_type_integer:
        data_encode_int(atol(valueStr), &data);
        break;
      case cis_data_type_float:
        {
          double value;
          utils_plainTextToFloat64((uint8_t *)valueStr, strlen(valueStr), &value);
          data_encode_float(value, &data);
          break;
        }
      case cis_data_type_bool:
        data_encode_bool(atoi(valueStr) == 1 ? true : false, &data);
        break;
    }
  if (cis_notify(g_context, &uri, (cis_data_t *)&data, msgId, result, false) == CIS_RET_OK)
    {
      cisat_wakeup_pump();
      cis_response_ok(fd);
      cis_notify_at_res();
      return;
    }
error:
  cis_response_error(fd, CIS_ERROR_INVALID_PARAM);
  cis_notify_at_res();
  return;
}

void cis_miplreadrsp_handler(int fd, char *param)
{
  int ret;
  char *line;
  int ref, msgId, objectId, instanceId, resourceId, valuetype, len, index, flag;
  int result;
  char *valueStr;
  cis_uri_t uri;
  cis_coapret_t cisResult;
  st_data_t data;

  line = param;
  ret = cis_at_tok_start(&line);
  if (ret < 0)
    {
      goto error;
    }
  ret = at_tok_nextint(&line, &ref);
  if (ret < 0)
    {
      goto error;
    }

  ret = at_tok_nextint(&line, &msgId);
  if (ret < 0)
    {
      goto error;
    }

  ret = at_tok_nextint(&line, &result);
  if (ret < 0)
    {
      goto error;
    }

  if (result == 1)
    {
      ret = at_tok_nextint(&line, &objectId);
      if (ret < 0)
        {
          goto error;
        }

      ret = at_tok_nextint(&line, &instanceId);
      if (ret < 0)
        {
          goto error;
        }

      ret = at_tok_nextint(&line, &resourceId);
      if (ret < 0)
        {
          goto error;
        }

      ret = at_tok_nextint(&line, &valuetype);
      if (ret < 0)
        {
          goto error;
        }

      ret = at_tok_nextint(&line, &len);
      if (ret < 0)
        {
          goto error;
        }

      ret = at_tok_nextstr(&line, &valueStr);
      if (ret < 0)
        {
          goto error;
        }

      ret = at_tok_nextint(&line, &index);
      if (ret < 0)
        {
          goto error;
        }

      ret = at_tok_nextint(&line, &flag);
      if (ret < 0)
        {
          goto error;
        }

      uri_make(objectId, instanceId, resourceId, &uri);
      if (index == 0)
        {
          cisResult = CIS_NOTIFY_CONTENT;
        }
      else
        {
          cisResult = CIS_NOTIFY_CONTINUE;
        }
      data.type = valuetype;
      switch (valuetype)
        {
          case cis_data_type_string:
            data_encode_string(valueStr, &data);
            break;
          case cis_data_type_opaque:
            {
              char *p = (char *)cissys_malloc(len);
              utils_hexStrToByte(valueStr, strlen(valueStr), (unsigned char *)p);
              data_encode_opaque((uint8_t *)p, len, &data);
              cissys_free(p);
              break;
            }
          case cis_data_type_integer:
            data_encode_int(atol(valueStr), &data);
            break;
          case cis_data_type_float:
            {
              double value;
              utils_plainTextToFloat64((uint8_t *)valueStr, strlen(valueStr), &value);
              data_encode_float(value, &data);
              break;
            }
          case cis_data_type_bool:
            data_encode_bool(atoi(valueStr) == 1 ? true : false, &data);
            break;
        }
      if (cis_response(g_context, &uri, (cis_data_t *)&data, msgId, cisResult) == CIS_RET_OK)
        {
          cis_response_ok(fd);
          if (cisResult == CIS_NOTIFY_CONTENT)
            {
              cisat_wakeup_pump();
              cis_notify_at_res();
            }
          return;
        }
    }
  else
    {
      if (cis_response(g_context, NULL, NULL, msgId, cis_result_to_cis_response(result)) == CIS_RET_OK)
        {
          cisat_wakeup_pump();
          cis_response_ok(fd);
          cis_notify_at_res();
          return;
        }
    }
error:
  cis_response_error(fd, CIS_ERROR_INVALID_PARAM);
  cis_notify_at_res();
  return;
}

void cis_miplwritersp_handler(int fd, char *param)
{
  int ret;
  char *line;
  int ref, msgId, result;

  line = param;
  ret = cis_at_tok_start(&line);
  if (ret < 0)
    {
      goto error;
    }
  ret = at_tok_nextint(&line, &ref);
  if (ret < 0)
    {
      goto error;
    }

  ret = at_tok_nextint(&line, &msgId);
  if (ret < 0)
    {
      goto error;
    }

  ret = at_tok_nextint(&line, &result);
  if (ret < 0)
    {
      goto error;
    }

  if (cis_response(g_context, NULL, NULL, msgId, cis_result_to_cis_response(result)) == CIS_RET_OK)
    {
      cisat_wakeup_pump();
      cis_response_ok(fd);
      cis_notify_at_res();
      return;
    }
error:
  cis_response_error(fd, CIS_ERROR_INVALID_PARAM);
  cis_notify_at_res();
  return;
}

void cis_miplexecutersp_handler(int fd, char *param)
{
  int ret;
  char *line;
  int ref, msgId, result;

  line = param;
  ret = cis_at_tok_start(&line);
  if (ret < 0)
    {
      goto error;
    }
  ret = at_tok_nextint(&line, &ref);
  if (ret < 0)
    {
      goto error;
    }

  ret = at_tok_nextint(&line, &msgId);
  if (ret < 0)
    {
      goto error;
    }

  ret = at_tok_nextint(&line, &result);
  if (ret < 0)
    {
      goto error;
    }

  if (cis_response(g_context, NULL, NULL, msgId, cis_result_to_cis_response(result)) == CIS_RET_OK)
    {
      cisat_wakeup_pump();
      cis_response_ok(fd);
      cis_notify_at_res();
      return;
    }
error:
  cis_response_error(fd, CIS_ERROR_INVALID_PARAM);
  cis_notify_at_res();
  return;
}

void cis_miplparameterrsp_handler(int fd, char *param)
{
  int ret;
  char *line;
  int ref, msgId, result;

  line = param;
  ret = cis_at_tok_start(&line);
  if (ret < 0)
    {
      goto error;
    }
  ret = at_tok_nextint(&line, &ref);
  if (ret < 0)
    {
      goto error;
    }

  ret = at_tok_nextint(&line, &msgId);
  if (ret < 0)
    {
      goto error;
    }

  ret = at_tok_nextint(&line, &result);
  if (ret < 0)
    {
      goto error;
    }

  if (cis_response(g_context, NULL, NULL, msgId, cis_result_to_cis_response(result)) == CIS_RET_OK)
    {
      cisat_wakeup_pump();
      cis_response_ok(fd);
      cis_notify_at_res();
      return;
    }
error:
  cis_response_error(fd, CIS_ERROR_INVALID_PARAM);
  cis_notify_at_res();
  return;
}


static int cis_atcmd_handler(int fd)
{
  char *pbuf, *end;
  int i;

  g_cis_atlen += read(fd, g_cis_atbuf + g_cis_atlen, ATCMD_BUFMAX - g_cis_atlen - 1);
  g_cis_atbuf[g_cis_atlen] = '\0';
  pbuf = g_cis_atbuf;
  while (1)
    {
      if ((end = strchr(pbuf, '\r')) != NULL)
        {
          /* Process AT cmd */

          end[0] = '\0';

          pbuf = strcasestr(pbuf, "at");
          if (!pbuf)
            {
              pbuf = end + 1;
              continue;
            }

          /* Cmd handle */
          LOGD("cis at rx buf:%s\n", pbuf);
          for (i = 0; i < ARRAY_SIZE(g_ciscmd); i++)
            {
              if (strncasecmp(pbuf, g_ciscmd[i].cmd, strlen(g_ciscmd[i].cmd)) == 0)
                {
                  g_ciscmd[i].handler(fd, pbuf);
                  break;
                }
            }

          /* Unknown cmd handle */
          pbuf = end + 1;
        }
      else
        {
          break;
        }
    }
  memmove(g_cis_atbuf, pbuf, g_cis_atlen - (pbuf - g_cis_atbuf));
  g_cis_atlen -= pbuf - g_cis_atbuf;
  return 0;
}


int cisat_readloop(int fd)
{
  int ret;
  pthread_setname_np(pthread_self(), "cisat_readloop");
  while(1)
    {
      fd_set readset;
      int maxfd = 0;
      FD_ZERO(&readset);
      FD_SET(fd, &readset);
      maxfd= fd;
      ret = select(maxfd + 1, &readset, NULL, NULL, NULL);
      if (ret < 0)
        {
          LOGE("%s: select fail\n", __func__);
          break;
        }
      if (FD_ISSET(fd, &readset))
        {
          cis_atcmd_handler(fd);
        }
    }
  return -1;
}


int cisat_initialize(void)
{
  int fd;
  pthread_mutex_init(&g_cis_at_mutex, NULL);
  pthread_cond_init(&g_cis_at_cond, NULL);
  fd = open(g_cis_at_name, O_RDWR);
  if (fd < 0)
    {
      LOGE("open %s error", g_cis_at_name);
      return -1;
    }
  g_fd = fd;
  return fd;
}
