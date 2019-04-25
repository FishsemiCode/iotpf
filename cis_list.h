/****************************************************************************
 * external/services/iotpf/cis_list.h
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


#ifndef _CIS_LIST_H_
#define _CIS_LIST_H_

#include "cis_def.h"
/*
 * Utility functions for sorted linked list
 */
typedef uint32_t cis_listid_t;
typedef struct _cis_list_t
{
  struct _cis_list_t *next;
  cis_listid_t id;
} cis_list_t;

// defined in list.c
// Add 'node' to the list 'head' and return the new list
cis_list_t * cis_list_add(cis_list_t *head, cis_list_t *node);
// Return the node with ID 'id' from the list 'head' or NULL if not found
cis_list_t *cis_list_find(cis_list_t *head, cis_listid_t id);
cis_list_t *cis_list_find_u16(cis_list_t *head,uint16_t id);
// Remove the node with ID 'id' from the list 'head' and return the new list
cis_list_t *cis_list_remove(cis_list_t *head, cis_listid_t id, cis_list_t **nodeP);
// Return the lowest unused ID in the list 'head'
cis_listid_t cis_list_newId(cis_list_t *head);
// Free a list. Do not use if nodes contain allocated pointers as it calls lwm2m_free on nodes only.
// If the nodes of the list need to do more than just "free()" their instances, don't use lwm2m_list_free().
void cis_list_free(cis_list_t *head);

cis_listid_t cis_list_count(cis_list_t *head);

#define CIS_LIST_ADD(H,N)   cis_list_add((cis_list_t *)H, (cis_list_t *)N);
#define CIS_LIST_RM(H,I,N)  cis_list_remove((cis_list_t *)H, I, (cis_list_t **)N);
#define CIS_LIST_FIND(H,I)  cis_list_find((cis_list_t *)H, I)
#define CIS_LIST_FIND_U16(H,I)  cis_list_find_u16((cis_list_t *)H, I)
#define CIS_LIST_FREE(H)    cis_list_free((cis_list_t *)H)
#define CIS_LIST_COUNT(H)   cis_list_count((cis_list_t *)H);

#endif //_CIS_LIST_H_
