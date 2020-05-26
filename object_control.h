/****************************************************************************
 * external/services/iotpf/object_control.h
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

#ifndef _CIS_OBJECT_CONTROL_H_
#define _CIS_OBJECT_CONTROL_H_

#if CIS_ONE_MCU && CIS_OPERATOR_CTCC

#include "object_light_control.h"

const object_callback_mapping g_object_callback_mapping [] = {
  {
    LIGHT_CONTROL_OBJECT_ID,
    light_control_read,
    light_control_write,
    NULL,
    light_control_observe,
    light_control_notify,
    light_control_clean,
    light_control_make_sample_data
  },
};

const inline object_callback_mapping *get_object_callback_mappings(void)
{
#ifdef CIS_CTWING_SPECIAL_OBJECT
  return g_object_callback_mapping;
#else
  return NULL;
#endif
}

const inline int get_object_callback_mapping_num(void)
{
#ifdef CIS_CTWING_SPECIAL_OBJECT
  return sizeof(g_object_callback_mapping) / sizeof(object_callback_mapping);
#else
  return 0;
#endif
}

#endif

#endif /* _CIS_OBJECT_CONTROL_H_ */
