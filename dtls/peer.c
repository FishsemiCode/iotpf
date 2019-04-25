/****************************************************************************
 * external/services/iotpf/dtls/peer.c
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
#include "peer.h"
#include "cis_internals.h"

static inline dtls_peer_t *dtls_malloc_peer(void)
{
  return (dtls_peer_t *)cis_malloc(sizeof(dtls_peer_t));
}

void dtls_free_peer(dtls_peer_t *peer)
{
  dtls_handshake_free(peer->handshake_params);
  dtls_security_free(peer->security_params[0]);
  dtls_security_free(peer->security_params[1]);
  if (peer->session != NULL)
    {
      cis_free((void *)(peer->session));
    }
  cis_free( peer );
}

dtls_peer_t *dtls_new_peer(const session_t *session)
{
  dtls_peer_t *peer;

  peer = dtls_malloc_peer();
  if (peer)
    {
      cis_memset(peer, 0, sizeof(dtls_peer_t));
      peer->session = (session_t *)cis_malloc(sizeof(session_t ));
      if (!peer->session)
        {
          dtls_free_peer(peer);
          return NULL;
        }
      cis_memcpy((void *)(peer->session), (void *)session, sizeof(session_t));
      peer->security_params[0] = dtls_security_new();

      if (!peer->security_params[0])
        {
          dtls_free_peer(peer);
          return NULL;
        }
    }
  return peer;
}