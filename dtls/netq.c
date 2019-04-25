/****************************************************************************
 * external/services/iotpf/dtls/netq.c
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

#include "netq.h"
#include "dtls_debug.h"
#include "cis_internals.h"

static inline netq_t* netq_malloc_node(size_t size)
{
  return (netq_t *)cis_malloc(sizeof(netq_t)+size);
}

static inline void netq_free_node(netq_t *node)
{
  cis_free( node );
}

int netq_insert_node(list_t  queue,
                      netq_t *node)
{
  netq_t *p;

  if (NULL == queue || NULL == node)
    {
      return 0;
    }

  p = (netq_t *)list_head(queue);
  while (p && p->t <= node->t && list_item_next(p))
    {
      p =(netq_t *)list_item_next(p);
    }

  if (p)
    {
      list_insert(queue, p, node);
    }
  else
    {
      list_push(queue, node);
    }

  return 1;
}

netq_t *netq_head(list_t queue)
{
  if (!queue)
    {
      return NULL;
    }

  return (netq_t* )list_head(queue);
}

netq_t *netq_next(netq_t *p)
{
  if (!p)
    {
      return NULL;
    }

  return (netq_t*)list_item_next(p);
}

void netq_remove(list_t  queue,
                  netq_t *p)
{
  if (!queue || !p)
    {
      return;
    }

  list_remove(queue, p);
}

netq_t *netq_pop_first(list_t queue)
{
  if (!queue)
    {
      return NULL;
    }

  return (netq_t* )list_pop(queue);
}

netq_t *netq_node_new(size_t size)
{
  netq_t *node;
  node = netq_malloc_node(size);

  if (!node)
    {
      dtls_warn("netq_node_new: malloc\n");
    }

  if (node != NULL)
    {
      cis_memset(node, 0, sizeof(netq_t));
    }

  return node;
}

void netq_node_free(netq_t *node)
{
  if (node)
    {
      netq_free_node(node);
    }
}

void netq_delete_all(list_t queue)
{
  netq_t *p;
  if (queue)
    {
      while ((p = (netq_t *)list_pop(queue)) !=NULL)
        {
          netq_free_node(p);
        }
    }
}