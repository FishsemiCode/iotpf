/****************************************************************************
 * external/services/iotpf/iotpf.c
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

#include <nuttx/config.h>

#include <nuttx/serial/pty.h>
#include <sys/select.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <poll.h>
#include <sched.h>
#include <string.h>
#include <strings.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netutils/base64.h>
#include <cis_def.h>
#include <cis_api.h>

#include "dtls/dtls_debug.h"

/****************************************************************************
 * Pre-processor definitions
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Public Funtions
 ****************************************************************************/
static int iotpf_daemon(int argc, char *argv[])
{
  int ret;
  ret = ciscom_initialize();
  if (ret < 0)
    {
      LOGE("ciscom_initialize error");
      return -1;
    }
#if CIS_TWO_MCU
  ret = cisat_initialize();
  if (ret < 0)
    {
      LOGE("cisat_initialize error");
      return -1;
    }
  cisat_readloop(ret);
#elif CIS_ONE_MCU
  ret = cisapi_initialize();
  if (ret < 0)
    {
      LOGE("cisapi_initialize error");
      return -1;
    }
#endif
  return 0;
}

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, char *argv[])
#else
int iotpf_main(int argc, char *argv[])
#endif
{
  int ret;
  ret = task_create(argv[0],
          CONFIG_SERVICES_IOTPF_PRIORITY,
          CONFIG_SERVICES_IOTPF_STACKSIZE,
          iotpf_daemon,
          argv + 1);
  return ret > 0 ? 0 : ret;
}
