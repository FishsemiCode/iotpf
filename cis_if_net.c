/****************************************************************************
 * external/services/iotpf/cis_if_net.c
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

#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <cis_if_net.h>
#include <cis_list.h>
#include <cis_log.h>
#include <cis_if_sys.h>
#include <cis_internals.h>


bool cisnet_attached_state(void *ctx)
{
  return ((struct st_cis_context *)(ctx))->netAttached;
}

cis_ret_t cisnet_init(void *context, const cisnet_config_t *config, cisnet_callback_t cb)
{
  cis_memcpy(&((struct st_cis_context *)context)->netConfig, config, sizeof(cisnet_config_t));
  ((struct st_cis_context *)context)->netCallback.onEvent = cb.onEvent;
  if (ciscom_isRegistered(ciscom_getRegisteredStaus()))
    {
      ((struct st_cis_context *)context)->netAttached = 1;
    }
  else
    {
      ((struct st_cis_context *)context)->netAttached = 0;
    }
  return CIS_RET_OK;
}

cis_ret_t cisnet_create(cisnet_t *netctx, const char *host, void *context)
{
  st_context_t *ctx = (struct st_cis_context *)context;

  if (ctx->netAttached != true)
    {
      return CIS_RET_ERROR;
    }

  (*netctx) = (cisnet_t)cis_malloc(sizeof(struct st_cisnet_context));
  cissys_memset(*netctx, 0x0, sizeof(struct st_cisnet_context));
  (*netctx)->sock = 0;
  (*netctx)->port = atoi((const char *)(ctx->serverPort));
  (*netctx)->state = 0;
  (*netctx)->quit = 0;
  (*netctx)->g_packetlist = NULL;
  (*netctx)->context = context;
  strncpy((*netctx)->host, host, sizeof((*netctx)->host));
  ctx->pNetContext = *netctx;
  return CIS_RET_OK;
}

void cisnet_destroy(cisnet_t netctx, void *context)
{
  st_context_t *ctx = (struct st_cis_context *)context;
  cis_free(netctx);
  ctx->pNetContext = NULL;
}

static int prvCreateSocket(uint16_t localPort, int ai_family)
{
  int sock = socket(ai_family, SOCK_DGRAM, 0);
  if (sock < 0)
    {
      return -1;
    }
  return sock;
}

cis_ret_t cisnet_connect(cisnet_t netctx)
{
  int sock = -1;
  int ret = -1;
  struct addrinfo hints;
  struct addrinfo *servinfo = NULL;
  struct addrinfo *p;
  char portStr[6] = {0};
  sock = prvCreateSocket(0, AF_INET);
  if (sock < 0)
    {
      LOGI("prvCreateSocket fail");
      return CIS_RET_ERROR;
    }
  if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK))
    {
      LOGW("fcntl fail");
    }
  netctx->state = 1;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  snprintf(portStr, sizeof(portStr), "%d", netctx->port);
  if (0 != getaddrinfo(netctx->host, portStr, &hints, &servinfo) || servinfo == NULL)
    {
      LOGI("getaddrinfo fail:%s", netctx->host);
      close(sock);
      return CIS_RET_ERROR;
    }
  for (p = servinfo; p != NULL && ret == -1; p = p->ai_next)
    {
      if ((ret = connect(sock, p->ai_addr, p->ai_addrlen)) == -1)
        {
          close(sock);
        }
      LOGI("connect to %s ret %d", netctx->host, ret);
    }
  if (NULL != servinfo)
    {
      free(servinfo);
    }
  ((struct st_cis_context *)(netctx->context))->netCallback.onEvent(netctx, cisnet_event_connected, NULL, netctx->context);
  netctx->sock = sock;
  return CIS_RET_OK;
}

cis_ret_t cisnet_disconnect(cisnet_t netctx)
{
  netctx->state = 0;
  close(netctx->sock);
  netctx->sock = -1;
  ((struct st_cis_context *)(netctx->context))->netCallback.onEvent(netctx, cisnet_event_disconnect, NULL, netctx->context);
  return 1;
}

cis_ret_t cisnet_write(cisnet_t netctx, const uint8_t *buffer, uint32_t length)
{
  int nbSent;
  size_t offset;
  LOGI("cisnet_write %d,%d,%s", netctx->sock, netctx->port, netctx->host);

  offset = 0;
  while (offset != length)
    {
      nbSent = send(netctx->sock, (const char*)buffer + offset, length - offset, 0);
      if (nbSent == -1)
        {
          LOGI("cisnet_write fail:%d,%s", errno, strerror(errno));
          return -1;
        }
      LOGI("cisnet_write:%d,%d,%d", nbSent, offset, length);
      offset += nbSent;
    }

  return CIS_RET_OK;
}

cis_ret_t cisnet_read(cisnet_t netctx, uint8_t **buffer, uint32_t *length)
{
  if(netctx->g_packetlist != NULL)
    {
      struct st_net_packet *delNode;
      *buffer = netctx->g_packetlist->buffer;
      *length = netctx->g_packetlist->length;
      delNode =netctx->g_packetlist;
      netctx->g_packetlist = netctx->g_packetlist->next;
      cis_free(delNode);
      return CIS_RET_OK;
    }
  return CIS_RET_ERROR;
}

cis_ret_t cisnet_free(cisnet_t netctx, uint8_t *buffer, uint32_t length)
{
  cis_free(buffer);
  return CIS_RET_ERROR;
}
