/****************************************************************************
 * external/services/iotpf/cis_transaction.c
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
#include "cis_log.h"

static int prv_checkFinished(st_transaction_t *transacP,
                             coap_packet_t *receivedMessage)
{
  int len;
  const uint8_t *token;
  coap_packet_t *transactionMessage = (coap_packet_t*)transacP->message;

  if (COAP_DELETE < transactionMessage->code)
    {
      LOGE("checkFinish invalid message code:%d,ret:%d", transactionMessage->code, transacP->ackReceived);
      // response
      return transacP->ackReceived ? 1 : 0;
    }
  if (!IS_OPTION(transactionMessage, COAP_OPTION_TOKEN))
    {
      LOGE("checkFinish no token option,ret:%d", transacP->ackReceived);
      // request without token
      return transacP->ackReceived ? 1 : 0;
    }

  len = coap_get_header_token(receivedMessage, &token);
  if (transactionMessage->token_len == len)
    {
      if (cis_memcmp(transactionMessage->token, token, len) == 0)
        {
          return 1;
        }
    }

  LOGE("checkFinish different token,ret:%d", transacP->ackReceived);
  return 0;
}

st_transaction_t *transaction_new(coap_message_type_t type,
                                      coap_method_t method,
                                      char *altPath,
                                      st_uri_t *uriP,
                                      uint16_t mID,
                                      uint8_t token_len,
                                      uint8_t *token)
{
  st_transaction_t *transacP;
  int result;

  LOGD("transaction_new type: %d, method: %d, mID: %d, token_len: %d",
    type, method, mID, token_len);
  LOG_URI("transaction_new", uriP);

  // no transactions for ack or rst
  if (COAP_TYPE_ACK == type || COAP_TYPE_RST == type)
    {
      return NULL;
    }

  if (COAP_TYPE_NON == type)
    {
      // no transactions for NON responses
      if (COAP_DELETE < method)
        {
          return NULL;
        }
      // no transactions for NON request without token
      if (0 == token_len)
        {
          return NULL;
        }
    }

  transacP = (st_transaction_t *)cis_malloc(sizeof(st_transaction_t));

  if (NULL == transacP)
    {
      return NULL;
    }
  cis_memset(transacP, 0, sizeof(st_transaction_t));

  transacP->message = cis_malloc(sizeof(coap_packet_t));
  if (NULL == transacP->message)
    {
      goto error;
    }

  coap_init_message(transacP->message, type,(uint8_t)method, mID);

  transacP->mID = mID;

  if (altPath != NULL)
    {
      //TODO: Support multi-segment alternative path
      coap_set_header_uri_path_segment(transacP->message, altPath + 1);
    }
  if (NULL != uriP)
    {
      result = utils_intCopy(transacP->objStringID, STRING_ID_MAX_LEN, uriP->objectId);
      if (result < 0)
        {
          goto error;
        }

      coap_set_header_uri_path_segment(transacP->message, transacP->objStringID);

      if (CIS_URI_IS_SET_INSTANCE(uriP))
        {
          result = utils_intCopy(transacP->instanceStringID, STRING_ID_MAX_LEN, uriP->instanceId);
          if (result < 0)
            {
              goto error;
            }
          coap_set_header_uri_path_segment(transacP->message, transacP->instanceStringID);
        }
      else
        {
          if (CIS_URI_IS_SET_RESOURCE(uriP))
            {
              coap_set_header_uri_path_segment(transacP->message, NULL);
            }
        }
      if (CIS_URI_IS_SET_RESOURCE(uriP))
        {
          result = utils_intCopy(transacP->resourceStringID, STRING_ID_MAX_LEN, uriP->resourceId);
          if (result < 0)
            {
              goto error;
            }
          coap_set_header_uri_path_segment(transacP->message, transacP->resourceStringID);
        }
    }
  if (0 < token_len)
    {
      if (NULL != token)
        {
          coap_set_header_token(transacP->message, token, token_len);
        }
      else
        {
          // generate a token
          uint8_t temp_token[COAP_TOKEN_LEN];
          cis_time_t tv_sec = utils_gettime_s();
          // initialize first 6 bytes, leave the last 2 random
          temp_token[0] = (uint8_t)mID;
          temp_token[1] = (uint8_t)(mID >> 8);
          temp_token[2] = (uint8_t)(tv_sec);
          temp_token[3] = (uint8_t)(tv_sec >> 8);
          temp_token[4] = (uint8_t)(tv_sec >> 16);
          temp_token[5] = (uint8_t)(tv_sec >> 24);
          // use just the provided amount of bytes
          coap_set_header_token(transacP->message, temp_token, token_len);
        }
    }

  return transacP;

error:
  LOGD("Exiting on failure");
  if (transacP->message)
    {
      cis_free(transacP->message);
    }
  if (transacP)
    {
      cis_free(transacP);
    }
  return NULL;
}

void transaction_free(st_transaction_t *transacP)
{
  if (transacP->message)
    {
      cis_free(transacP->message);
    }
  if (transacP->buffer)
    {
      cis_free(transacP->buffer);
    }
  cis_free(transacP);
}

void transaction_remove(st_context_t *contextP,
                        st_transaction_t *transacP)
{
  contextP->transactionList = (st_transaction_t *)CIS_LIST_RM(contextP->transactionList, transacP->mID, NULL);
  LOGD("transaction_remove mID: %d", transacP->mID);
  transaction_free(transacP);
}

void transaction_removeAll(st_context_t *contextP)
{
  while (NULL != contextP->transactionList)
    {
      st_transaction_t *transaction;
      transaction = contextP->transactionList;
      contextP->transactionList = transaction->next;
      transaction_free(transaction);
    }
  contextP->transactionList = NULL;
}

bool transaction_handleResponse(st_context_t *contextP,
                                 coap_packet_t *message,
                                 coap_packet_t *response)
{
  bool found = false;
  bool reset = false;
  st_transaction_t *transacP;

  LOGD("fall in transaction_handleResponse");
  transacP = contextP->transactionList;

  while (NULL != transacP)
    {
      if (!transacP->ackReceived)
        {
          if ((COAP_TYPE_ACK == message->type) || (COAP_TYPE_RST == message->type))
            {
              if (transacP->mID == message->mid)
                {
                  found = true;
                  transacP->ackReceived = true;
                  reset = (COAP_TYPE_RST == message->type ? true : false);
                  LOGD("trans resp id:%d,reset:%d", message->mid, reset);
                }
              else
                {
                  LOGD("trans resp mid with (%d vs %d)", transacP->mID, message->mid);
                }
            }
        }
      if (reset || prv_checkFinished(transacP, message))
        {
          // HACK: If a message is sent from the monitor callback,
          // it will arrive before the registration ACK.
          // So we resend transaction that were denied for authentication reason.
          if (!reset)
            {
              if (COAP_TYPE_CON == message->type && NULL != response)
                {
                  coap_init_message(response, COAP_TYPE_ACK, 0, message->mid);
                  packet_send(contextP, response);
                }
              if ((COAP_401_UNAUTHORIZED == message->code) && (COAP_MAX_RETRANSMIT > transacP->retransCounter))
                {
                  transacP->ackReceived = false;
                  transacP->retransTime += COAP_RESPONSE_TIMEOUT;
                  return true;
                }
            }
          if (transacP->callback != NULL)
            {
              transacP->callback(transacP, message);
            }
          transaction_remove(contextP, transacP);
          return true;
        }
      // if we found our guy, exit
      if (found)
        {
          cis_time_t tv_sec = utils_gettime_s();
          if (0 <= tv_sec)
            {
              transacP->retransTime = tv_sec;
            }
          if (transacP->responseTimeout)
            {
              transacP->retransTime += transacP->responseTimeout;
            }
          else
            {
              transacP->retransTime += COAP_RESPONSE_TIMEOUT * transacP->retransCounter;
            }
          LOGD("trans resp is found,not finish.");
          return true;
        }
      transacP = transacP->next;
    }
  return false;
}

bool transaction_send(st_context_t *contextP,
	st_transaction_t *transacP)
{
  bool maxRetriesReached = false;

  if (transacP->buffer == NULL)
    {
      transacP->buffer_len = coap_serialize_get_size(transacP->message);
      if (transacP->buffer_len == 0)
        {
          return false;
        }

      transacP->buffer = (uint8_t*)cis_malloc(transacP->buffer_len);
      if (transacP->buffer == NULL)
        {
          return false;
        }

      transacP->buffer_len = coap_serialize_message(transacP->message, transacP->buffer);
      if (transacP->buffer_len == 0)
        {
          cis_free(transacP->buffer);
          transacP->buffer = NULL;
          transacP->callback(transacP, NULL);
          transaction_remove(contextP, transacP);
          return false;
        }
    }

  if (!transacP->ackReceived)
    {
      long unsigned timeout = 0;
      if (0 == transacP->retransCounter)
        {
          cis_time_t tv_sec = utils_gettime_s();
          if (0 <= tv_sec)
            {
              transacP->retransTime = tv_sec + COAP_RESPONSE_TIMEOUT;
              transacP->retransCounter = 1;
              timeout = 0;
            }
          else
            {
              maxRetriesReached = true;
            }
        }
      else
        {
          timeout = COAP_RESPONSE_TIMEOUT << (transacP->retransCounter - 1);
        }

      if (COAP_MAX_RETRANSMIT + 1 >= transacP->retransCounter)
        {
          void *targetSessionH = NULL;

          if (transacP->server == NULL)
            {
              LOGE("ERROR:transaction_send failed,server is NULL");
              if (transacP->callback)
                {
                  transacP->callback(transacP, NULL);
                }
              transaction_remove(contextP, transacP);
              return false;
            }
          targetSessionH = ((st_server_t *)transacP->server)->sessionH;

        #if CIS_ENABLE_DTLS
        #if CIS_ENABLE_DM
          if (contextP->isDTLS == TRUE && contextP->isDM != TRUE)
        #else
          if (contextP->isDTLS == TRUE)
        #endif
            {
              session_t saddr;
              saddr.sin_family = AF_INET;
              saddr.sin_port = htons(((cisnet_t)targetSessionH)->port);
              saddr.sin_addr.s_addr = inet_addr(((cisnet_t)targetSessionH)->host);
              if (dtls_write(&contextP->dtls, &saddr, transacP->buffer, transacP->buffer_len) >= CIS_RET_OK)
                {
                  LOGI("-[Pack]Send Buffer %d bytes---", transacP->buffer_len);
                  LOG_BUF("Send", transacP->buffer, transacP->buffer_len);
                }
              else
                {
                  LOGE("ERROR:send buffer failed.");
                }
            }
          else
            {
              if (cisnet_write((cisnet_t)targetSessionH, transacP->buffer, transacP->buffer_len) == CIS_RET_OK)
                {
                  LOGI("-[Pack]Send Buffer %d bytes---", transacP->buffer_len);
                  LOG_BUF("Send", transacP->buffer, transacP->buffer_len);
                }
              else
                {
                  LOGE("ERROR:send buffer failed.");
                }
            }
        #else
          cis_ret_t result = CIS_RET_OK;
          LOGI("-[Trans]Send Buffer %d bytes---", transacP->buffer_len);
          LOG_BUF("Send", transacP->buffer, transacP->buffer_len);
          result = cisnet_write((cisnet_t)targetSessionH, transacP->buffer, transacP->buffer_len);
          if (result != CIS_RET_OK)
            {
              LOGE("ERROR:cisnet_write return failed (%d)", result);
            }
        #endif
          transacP->retransTime += timeout;
          transacP->retransCounter += 1;
        }
      else
        {
          maxRetriesReached = true;
        }
    }
  if (transacP->ackReceived || maxRetriesReached)
    {
      if (transacP->callback)
        {
          transacP->callback(transacP, NULL);
        }
      transaction_remove(contextP, transacP);
      return false;
    }
  return true;
}

void transaction_step(st_context_t *contextP,
                      cis_time_t currentTime, time_t *timeoutP)
{
  st_transaction_t *transacP;
#if CIS_ENABLE_DTLS
#if CIS_ENABLE_DM
  if (!contextP->isDM && TRUE == contextP->isDTLS)
#else
  if (TRUE == contextP->isDTLS)
#endif
    {
      dtls_check_retransmit(&contextP->dtls);
    }
#endif
  transacP = contextP->transactionList;
  while (transacP != NULL)
    {
      // transaction_send() may remove transaction from the linked list
      st_transaction_t *nextP = transacP->next;
      bool un_remove = true;
      if (transacP->retransTime <= currentTime)
        {
          un_remove = transaction_send(contextP, transacP);
        }
      if (un_remove)
        {
          cis_time_t interval;
          if (transacP->retransTime > currentTime)
            {
              interval = transacP->retransTime - currentTime;
            }
          else
            {
              interval = 1;
            }
          if (*timeoutP > interval)
            {
              *timeoutP = interval;
            }
        }
      else
        {
          *timeoutP = 1;
        }
      transacP = nextP;
    }
}


int transaction_count(st_context_t *contextP)
{
  st_transaction_t *transacP;
  int result = 0;
  transacP = contextP->transactionList;
  while (transacP != NULL)
    {
      result++;
      transacP = transacP->next;
    }
  return result;
}
