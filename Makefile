############################################################################
# apps/external/services/iotpf/Makefile
#
#   Copyright (C) 2019 FishSemi Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
# 3. Neither the name NuttX nor the names of its contributors may be
#    used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
############################################################################
-include $(TOPDIR)/Make.defs
APPNAME = iotpf
PRIORITY = CONFIG_SERVICES_IOTPF_PRIORITY
STACKSIZE = CONFIG_SERVICES_IOTPF_STACKSIZE

MAINSRC = iotpf.c


CFLAGS_STR := "$(CFLAGS)"
CFLAGS += -I $(TOPDIR)/../vendor/services/ril/at_client
CFLAGS += -I $(TOPDIR)/../vendor/services/iotpf_lib

ifeq ($(CONFIG_SERVICES_IOTPF_OPERATOR), "ctcc")
CFLAGS += -DCIS_OPERATOR_CTCC
endif

ifeq ($(CONFIG_SERVICES_IOTPF_MODE), "at")
CFLAGS += -DCIS_TWO_MCU
else
ifeq ($(CONFIG_SERVICES_IOTPF_OPERATOR), "ctcc")
CSRCS   += cis_if_api_ctcc.c
CSRCS   += object_light_control.c
else
CSRCS   += cis_if_api.c
endif
CFLAGS += -DCIS_ONE_MCU
endif

PROGNAME = iotpf(EXEEXT)
include $(APPDIR)/Application.mk
