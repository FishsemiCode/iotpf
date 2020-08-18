/****************************************************************************
 * external/services/iotpf/cis_if_api_cmcc.h
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

#ifndef _CIS_IF_API_CMCC_H_
#define _CIS_IF_API_CMCC_H_

#include "cis_api.h"
#include "cis_list.h"

#define SAMPLE_OBJECT_MAX       2

#define SAMPLE_OID_A            (3311)
#define SAMPLE_OID_B            (3340)

#define SAMPLE_A_INSTANCE_COUNT        3
#define SAMPLE_A_INSTANCE_BITMAP      "100"

#define SAMPLE_B_INSTANCE_COUNT        5
#define SAMPLE_B_INSTANCE_BITMAP      "10000"

typedef struct st_sample_object
{
  cis_oid_t oid;
  cis_instcount_t instCount;
  const char *instBitmap;
  const cis_rid_t *attrListPtr;
  uint16_t attrCount;
  const cis_rid_t *actListPtr;
  uint16_t actCount;
}st_sample_object;

//////////////////////////////////////////////////////////////////////////
//a object

typedef struct st_object_a
{
  int32_t intValue;
  double floatValue;
  bool boolValue;
  char strValue[1024];
  uint8_t update;
}st_object_a;

typedef struct st_instance_a
{
  cis_iid_t instId;
  bool enabled;
  st_object_a instance;
}st_instance_a;

enum
{
  attributeA_intValue        = 5852,
  attributeA_floatValue      = 5820,
  attributeA_stringValue     = 5701,
};

enum
{
  actionA_1         = 100,
};

static const cis_rid_t const_AttrIds_a[] =
{
  attributeA_intValue,
  attributeA_floatValue,
  attributeA_stringValue,
};

static const cis_rid_t const_ActIds_a[] =
{
  actionA_1,
};

//////////////////////////////////////////////////////////////////////////
//b object

typedef struct st_object_b
{
  int32_t intValue;
  double floatValue;
  bool boolValue;
  char strValue[1024];

  //flag;
  uint8_t update;
}st_object_b;

typedef struct st_instance_b
{
  cis_iid_t instId;
  bool    enabled;
  st_object_b instance;
}st_instance_b;

enum
{
  attributeB_intValue       = 5501,
  attributeB_floatValue     = 5544,
  attributeB_stringValue    = 5750,
};

enum
{
  actionB_1         = 5523,
};

static const cis_rid_t const_AttrIds_b[] =
{
  attributeB_intValue,
  attributeB_floatValue,
  attributeB_stringValue,
};

static const cis_rid_t const_ActIds_b[] =
{
  actionB_1,
};

struct st_observe_info
{
  struct st_observe_info *next;
  cis_listid_t mid;
  cis_uri_t uri;
  cis_observe_attr_t params;
};

#endif//_CIS_IF_API_CMCC_H_
