/****************************************************************************
 * external/services/iotpf/cis_list.c
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

cis_list_t *cis_list_add(cis_list_t *head,
                           cis_list_t *node)
{
  cis_list_t * target;
  if (NULL == head)
    {
      return node;
    }
  if (head->id > node->id)
    {
      node->next = head;
      return node;
    }
  target = head;
  while (NULL != target->next && target->next->id < node->id)
    {
      target = target->next;
    }
  node->next = target->next;
  target->next = node;
  return head;
}


cis_list_t *cis_list_find(cis_list_t *head,
                            cis_listid_t id)
{
  while (NULL != head && head->id < id)
    {
      head = head->next;
    }
  if (NULL != head && head->id == id)
    {
      return head;
    }
  return NULL;
}

cis_list_t *cis_list_find_u16(cis_list_t *head, uint16_t id)
{
  while (NULL != head && (head->id & 0xFFFF) < id)
    {
      head = head->next;
    }
  if (NULL != head && (head->id & 0xFFFF) == id)
    {
      return head;
    }
  return NULL;
}


cis_list_t *cis_list_remove(cis_list_t *head,
                              cis_listid_t id,
                              cis_list_t **nodeP)
{
  cis_list_t *target;
  if (head == NULL)
    {
      if (nodeP)
        {
          *nodeP = NULL;
        }
      return NULL;
    }
  if (head->id == id)
    {
      if (nodeP)
        {
          *nodeP = head;
        }
      return head->next;
    }
  target = head;
  while (NULL != target->next && target->next->id < id)
    {
      target = target->next;
    }
  if (NULL != target->next && target->next->id == id)
    {
      if (nodeP)
        {
          *nodeP = target->next;
        }
      target->next = target->next->next;
    }
  else
    {
      if (nodeP)
        {
          *nodeP = NULL;
        }
    }
  return head;
}

cis_listid_t cis_list_newId(cis_list_t *head)
{
  cis_listid_t id;
  cis_list_t * target;
  id = 0;
  target = head;
  while (target != NULL && id == target->id)
    {
      id = target->id + 1;
      target = target->next;
    }
  return id;
}

void cis_list_free(cis_list_t *head)
{
  if (head != NULL)
    {
      cis_list_t * nextP;
      nextP = head->next;
      cis_free(head);
      cis_list_free(nextP);
    }
}


cis_listid_t cis_list_count(cis_list_t *head)
{
  cis_listid_t count = 0;
  cis_list_t *target;
  target = head;
  while (target != NULL)
    {
      count++;
      target = target->next;
    }
  return count;
}
