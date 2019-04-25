/****************************************************************************
 * external/services/iotpf/dtls/netq.h
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
#ifndef NBIOT_SOURCE_DTLS_NETQ_H_
#define NBIOT_SOURCE_DTLS_NETQ_H_

#include "peer.h"
#include "list.h"
#include "global.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NETQ_MAXCNT
#define NETQ_MAXCNT 5 /**< maximum number of elements in netq structure */
#endif

/**
 * Datagrams in the netq_t structure have a fixed maximum size of
 * DTLS_MAX_BUF to simplify memory management on constrained nodes.
*/
typedef unsigned char netq_packet_t[DTLS_MAX_BUF];

typedef struct netq_t
{
  struct netq_t *next;

  clock_t t;              /**< when to send PDU for the next time */
  unsigned int timeout;        /**< randomized timeout value */

  dtls_peer_t *peer;           /**< remote address */
  uint16_t epoch;
  uint8_t type;
  unsigned char retransmit_cnt; /**< retransmission counter, will be removed when zero */

  size_t length;         /**< actual length of data */
  unsigned char data[1024];		/**< the datagram to send */
} netq_t;

/**
 * Adds a node to the given queue, ordered by their time-stamp t.
 * This function returns @c 0 on error, or non-zero if @p node has
 * been added successfully.
 *
 * @param queue A pointer to the queue head where @p node will be added.
 * @param node  The new item to add.
 * @return @c 0 on error, or non-zero if the new item was added.
*/
int netq_insert_node(list_t queue,
                      netq_t *node);

/**
 * Destroys specified node and releases any memory that was allocated
 * for the associated datagram.
*/
void netq_node_free(netq_t *node);

/**
 * Removes all items from given queue and frees the allocated storage
*/
void netq_delete_all(list_t queue);

/**
 * Creates a new node suitable for adding to a netq_t queue.
*/
netq_t *netq_node_new(size_t size);

/**
 * Returns a pointer to the first item in given queue or NULL if
 * empty.
*/
netq_t *netq_head(list_t queue);

netq_t *netq_next(netq_t *p);
void netq_remove(list_t queue,
                  netq_t *p);

/**
 * Removes the first item in given queue and returns a pointer to the
 * removed element. If queue is empty when netq_pop_first() is called,
 * this function returns NULL.
*/
netq_t *netq_pop_first(list_t queue);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* NBIOT_SOURCE_DTLS_NETQ_H_ */