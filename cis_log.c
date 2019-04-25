/****************************************************************************
 * external/services/iotpf/cis_log.c
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


#include "cis_log.h"
#include "cis_api.h"
#include "cis_internals.h"

#include <ctype.h>

bool     gLogEnabled = true;
uint8_t  gLogLevel = LOG_LEVEL_DEBUG;
uint8_t  gLogExoutput = 0x00;
uint16_t gLogBufferLength = LOG_TEXT_SIZE_MIN;

void log_config(bool enable, uint8_t exoutput,uint8_t level,uint16_t bufsize)
{
  gLogEnabled = enable;
  gLogLevel = level;
  gLogExoutput = exoutput;
  bufsize =  CIS_MAX(bufsize,LOG_TEXT_SIZE_MIN);
  bufsize =  CIS_MIN(bufsize,LOG_TEXT_SIZE_MAX);
  gLogBufferLength = bufsize;
}

#if LOG_DUMP_ENABLED

static void prvPrintIndent(int num)
{
  int i;
  for (i = 0; i < num; i++)
    {
      LOG_PRINT("    ");
    }
}

void log_dump(const char* title, const uint8_t *buffer, int length, int indent)
{
  int i;

  if (!gLogEnabled)
    {
      return;
    }

  if (length <= 0)
    {
      LOG_PRINT("\n");
      return;
    }

  if (title != NULL)
    {
      LOG_PRINT("->[%s]>>---\r\n", title);
    }
  else
    {
      LOG_PRINT("-----\r\n");
    }

  i = 0;
  while (i < length)
    {
      uint8_t array[16];
      int j;
      prvPrintIndent(indent);
      cis_memcpy(array, buffer + i, 16);
      for (j = 0 ; j < 16 && i+j < length; j++)
        {
          LOG_PRINT( "%02X ",array[j]);
          if (j % 4 == 3)
            {
              LOG_PRINT( " ");
            }
        }
      if (length > 16)
        {
          while (j < 16)
            {
              LOG_PRINT( "   ");
              if (j%4 == 3)
                {
                  LOG_PRINT( " ");
                }
              j++;
            }
        }

       LOG_PRINT( " ");
     #ifdef PRINT_DUMP_CHAR
       for (j = 0 ; j < 16 && i + j < length; j++)
        {
          if (isprint(array[j]))
            {
              LOG_PRINT( "%c", array[j]);
            }
          else
            {
              LOG_PRINT( ".");
            }
        }
     #endif
       LOG_PRINT("\n");
       i += 16;
    }
  if (title != NULL)
    {
      LOG_PRINT("<-[%s]<<---\r\n",title);
    }
  else
    {
      LOG_PRINT("-----\r\n");
    }
}
#endif//LOG_DUMP_ENABLED
