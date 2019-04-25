/****************************************************************************
 * external/services/iotpf/dtls/list.h
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

#ifndef NBIOT_SOURCE_DTLS_LIST_H_
#define NBIOT_SOURCE_DTLS_LIST_H_
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct list
{
  struct list *next;
};
typedef void** list_t;

#define LIST_CONCAT(s1,s2) s1##s2
#define LIST_STRUCT(name) \
        void  *LIST_CONCAT(name,_list); \
        list_t name
#define LIST_STRUCT_INIT(struct_ptr,name) \
        { \
            (struct_ptr)->name = &((struct_ptr)->LIST_CONCAT(name,_list)); \
            (struct_ptr)->LIST_CONCAT(name,_list) = NULL; \
        }

static inline void* list_head(list_t list)
{
  return *list;
}

static inline void list_remove(list_t list,
                                void  *item)
{
  struct list *_list = *(struct list**)list;
  struct list *_item = (struct list*)item;

  if (_list)
    {
      if (_list == item)
        {
          _list = _list->next;
        }
      else
        {
          while ( _list->next && _list->next != _item)
            {
              _list = _list->next;
            }

          if (_list->next)
            {
              _list->next = _item->next;
            }
        }

      *(struct list**)list = _list;
    }
}

static inline void list_add(list_t list,
                             void *item)
{
  struct list *_list;
  struct list *_item;
  list_remove(list, item);

  _list = *(struct list**)list;
  _item =  (struct list*)item;
  if (_list)
    {
      _item->next = _list;
      while (_item->next->next)
        {
          _item->next = _item->next->next;
        }
      _item->next->next = _item;
    }
  else
    {
      _list = _item;
    }

  _item->next = NULL;
  *(struct list**)list = _list;
}

static inline void list_push(list_t list,
                              void *item)
{
  struct list *_list = *(struct list**)list;
  struct list *_item = (struct list*)item;

  _item->next = _list;
  _list = _item;
  *(struct list**)list = _item;
}

static inline void *list_pop(list_t list)
{
  void *head = list_head( list );

  if (head)
    {
      list_remove(list, head);
    }

  return head;
}

static inline void list_insert(list_t list,
                                void *previtem,
                                void *newitem)
{
  if (previtem == NULL)
    {
      list_push(list, newitem);
    }
  else
    {
      ((struct list*)newitem)->next = ((struct list*)previtem)->next;
      ((struct list*)previtem)->next =(struct list*)newitem;
    }
}

static inline void *list_item_next(void *item)
{
  return item == NULL ? NULL : ((struct list*)item)->next;
}

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* NBIOT_SOURCE_DTLS_LIST_H_ */