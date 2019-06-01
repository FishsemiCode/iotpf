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
CSRCS   += cis_block1.c cis_coap.c cis_bootstrap.c
CSRCS   += cis_config.c cis_core.c cis_data.c cis_discover.c cis_if_net.c cis_if_sys.c
CSRCS   += cis_list.c cis_log.c cis_management.c cis_memtrace.c
CSRCS   += cis_objects.c cis_observe.c cis_packet.c cis_registration.c
CSRCS   += cis_tlv.c cis_transaction.c cis_uri.c cis_utils.c
CSRCS   += cis_if_at.c cis_if_com.c cis_if_api.c
CSRCS   += std_object/std_object.c std_object/std_object_security.c std_object/std_object_conn_moniter.c std_object/std_object_binary_app_data_container.c
CSRCS   += dtls/ccm.c dtls/cis_aes.c dtls/crypto.c dtls/dtls_debug.c dtls/dtls.c
CSRCS   += dtls/hmac.c dtls/netq.c dtls/peer.c dtls/sha2.c


CFLAGS_STR := "$(CFLAGS)"
CFLAGS += -I $(TOPDIR)/../vendor/services/ril/at_client
PROGNAME = iotpf(EXEEXT)
include $(APPDIR)/Application.mk
