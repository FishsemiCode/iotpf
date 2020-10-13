/****************************************************************************
 * external/services/iotpf/iotpf_user.c
 *
 *     Copyright (C) 2020 FishSemi Inc. All rights reserved.
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

#if CIS_ONE_MCU && CIS_OPERATOR_CTCC

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/select.h>
#include <string.h>

#include "at_api.h"
#include "cis_log.h"
#include "cis_api.h"
#include "cis_if_api_ctcc.h"

#include "iotpf_user.h"

#define REPORT_INTERVAL 5  // 5 seconds one report
#define GPS_BUF_SZ 256

static pthread_mutex_t g_exit_mutex;
static bool g_exit;

static pthread_mutex_t g_gps_mutex;
static pthread_cond_t g_gps_cond;
static char g_gps_data[GPS_BUF_SZ];

static void stop_user_thread(void)
{
  pthread_mutex_lock(&g_exit_mutex);
  g_exit = true;
  pthread_mutex_unlock(&g_exit_mutex);
}

static void send_data_to_server(user_thread_context_t *utc,
                                void *data, uint32_t data_len)
{
  user_data_info_t udi;

  udi.data = (uint8_t*)malloc(data_len);
  if (udi.data == NULL)
    {
      return;
    }
  udi.data_len = data_len;
  memcpy(udi.data, data, data_len);

  file_write(&utc->send_pipe_file, &udi, sizeof(user_data_info_t));
}

void cisapi_send_data_to_server(user_thread_context_t *utc)
{
  user_data_info_t udi;

  read(utc->send_pipe_fd[0], &udi, sizeof(user_data_info_t));

  LOGI("user_thread: send data %d bytes", udi.data_len);
  cis_notify_raw(utc->context, udi.data, udi.data_len);
  free(udi.data);
}

void cisapi_recv_data_from_server(user_thread_context_t *utc,
                                  const void *data, uint32_t data_len)
{
  user_data_info_t udi;

  udi.data = (uint8_t*)malloc(data_len);
  if (udi.data == NULL)
    {
      return;
    }
  udi.data_len = data_len;
  memcpy(udi.data, data, data_len);
  write(utc->recv_pipe_fd[1], &udi, sizeof(user_data_info_t));
}

static void recv_data_from_server(user_thread_context_t *utc)
{
  user_data_info_t udi;
  char *buf = NULL;
  int bufLen = 0;
  int i;

  file_read(&utc->recv_pipe_file, &udi, sizeof(user_data_info_t));

  buf = (char *)malloc(2 * udi.data_len + 1);
  memset(buf, 0x0, 2 * udi.data_len + 1);
  for (i = 0; i < udi.data_len; i++)
    {
      bufLen += snprintf(buf + bufLen, 2 * udi.data_len + 1 - bufLen, "%02X", ((char *)(udi.data))[i]);
    }
  LOGI("user_thread: recv data: %d,%s.", udi.data_len, buf);

  free(buf);
  free(udi.data);
}

#define HEARTBEAT_TIMER_SIGNAL 17

#ifdef CONFIG_CAN_PASS_STRUCTS
static void heartbeat_callback(union sigval value)
#else
static void heartbeat_callback(FAR void *sival_ptr)
#endif
{
  uint8_t data[6] = {0x05, 0x40, 0x41, 0x42, 0x43, 0x44};

#ifdef CONFIG_CAN_PASS_STRUCTS
  user_thread_context_t *utc = (user_thread_context_t *)value.sival_int;
#else
  user_thread_context_t *utc = (user_thread_context_t *)sival_ptr;
#endif

  LOGI("@@@@@@@@@@@ heartbeat callback utc: 0x%x and send heartbeat @@@@@@@@@@@", (unsigned int)utc);
  send_data_to_server(utc, data, sizeof(data));
}

static timer_t create_heartbeat_timer(user_thread_context_t *utc)
{
  int ret;
  timer_t timerid;
  struct sigevent notify;
  struct itimerspec timer;

  LOGI("@@@@@@@@@@@ Create heartbeat timer utc 0x%x @@@@@@@@@@@@", (unsigned int)utc);

  file_detach(utc->send_pipe_fd[1], &utc->send_pipe_file);

  notify.sigev_notify            = SIGEV_THREAD;
  notify.sigev_signo             = HEARTBEAT_TIMER_SIGNAL;
  notify.sigev_value.sival_int   = (int)utc;
  notify.sigev_notify_function   = (void *)heartbeat_callback;
  notify.sigev_notify_attributes = NULL;

  ret = timer_create(CLOCK_REALTIME, &notify, &timerid);
  if (ret != 0)
    {
      LOGE("Failed to create timer!\n");
      return NULL;
    }

  LOGI("@@@@@@@@@@@ Start heartbeat timer @@@@@@@@@@@@@");
  timer.it_value.tv_sec     = 5;
  timer.it_value.tv_nsec    = 0;
  timer.it_interval.tv_sec  = 5;
  timer.it_interval.tv_nsec = 0;
  ret = timer_settime(timerid, 0, &timer, NULL);
  if (ret != 0)
    {
      LOGE("Failed to start timer!\n");
      return NULL;
    }

  return timerid;
}

static void destroy_heartbeat_timer(timer_t timerid)
{
  LOGI("@@@@@@@@@@@ Destroy heartbeat timer @@@@@@@@@@@@");
  timer_delete(timerid);
}

static void disconnect_from_server(user_thread_context_t *utc)
{
  core_updatePumpState(utc->context, PUMP_STATE_DISCONNECTED);
  cisapi_wakeup_pump();
  sleep(5);
  ciscom_setRadioPower(false);
  LOGI("Disconnect from server!");
}

static void connect_to_server(user_thread_context_t *utc)
{
  ciscom_setRadioPower(true);
  while (!ciscom_isRegistered(ciscom_getRegisteredStaus())) {
      LOGI("############## CEREG not ready #################");
      sleep(1);
  }
  cisapi_wakeup_pump();
  sleep(5);
  LOGI("Connect to server !");
}

static void handle_gprmc(const char *s)
{
  int ret;
  char *p;
  char *ignore;
  bool ready = false;
  char *line = (char *)s;
  char *p_longitude, *p_latitude, *p_time, *p_date;

  ret = at_tok_nextstr(&line, &p);
  if (ret < 0)
    {
      return;
    }
  ret = at_tok_nextstr(&line, &p_time);
  if (ret < 0)
    {
      return;
    }

  ret = at_tok_nextstr(&line, &p);
  if (ret < 0)
    {
      return;
    }

  if (*p == 'A' || *p == 'V') // If outside, just check p = 'A'
    {
      ready = true;
    }
  ret = at_tok_nextstr(&line, &p_latitude);
  if (ret < 0)
    {
      return;
    }

  ret = at_tok_nextstr(&line, &p);
  if (ret < 0)
    {
      return;
    }

  ret = at_tok_nextstr(&line, &p_longitude);
  if (ret < 0)
    {
      return;
    }

  ret = at_tok_nextstr(&line, &ignore);
  if (ret < 0)
    {
      return;
    }

  ret = at_tok_nextstr(&line, &ignore);
  if (ret < 0)
    {
      return;
    }

  ret = at_tok_nextstr(&line, &ignore);
  if (ret < 0)
    {
      return;
    }

  ret = at_tok_nextstr(&line, &p_date);
  if (ret < 0)
    {
      return;
    }

  if (ready)
    {
      struct tm t;
      time_t seconds;
      uint64_t mSeconds;
      char buf[4] = {0};

      strncpy(buf, p_time + 4, 2);
      t.tm_sec = atoi(buf);
      strncpy(buf, p_time + 2, 2);
      t.tm_min = atoi(buf);
      strncpy(buf, p_time, 2);
      t.tm_hour = atoi(buf);
      strncpy(buf, p_date, 2);
      t.tm_mday = atoi(buf);
      strncpy(buf, p_date + 2, 2);
      t.tm_mon = atoi(buf);
      t.tm_mon -= 1;
      strncpy(buf, p_date + 4, 2);
      t.tm_year = atoi(buf) + 100;
      seconds = mktime(&t);
      strncpy(buf, p_time + 7, 2);
      mSeconds = (uint64_t)(seconds) * 1000 + atoi(buf);
      pthread_mutex_lock(&g_gps_mutex);
      sprintf(g_gps_data, "%s,%s,%s,%s,%013llu,%u",
              p_time, p_latitude, p_longitude, p_date, mSeconds, seconds);
      pthread_cond_signal(&g_gps_cond);
      pthread_mutex_unlock(&g_gps_mutex);
    }
}

static void do_gps_capture(int fd)
{
  pthread_mutex_init(&g_gps_mutex, NULL);
  pthread_cond_init(&g_gps_cond, NULL);
  start_gps(fd, false);

  // you can do your gps job here, for example caputure gps data for 10 minutes
  pthread_mutex_lock(&g_gps_mutex);
  pthread_cond_wait(&g_gps_cond, &g_gps_mutex);
  LOGI("@@@@@@@@@@ gps data = %s @@@@@@@@@@@@@", g_gps_data);
  pthread_mutex_unlock(&g_gps_mutex);

  stop_gps(fd);
  pthread_mutex_destroy(&g_gps_mutex);
  pthread_cond_destroy(&g_gps_cond);
}

void *cisapi_user_send_thread(void *obj)
{
  if(NULL == obj)
    {
      LOGE("[%s]obj is null", __func__);
      return NULL;
    }
  int at_fd;
  user_thread_context_t *utc = (user_thread_context_t *)obj;
  if(utc->iotpf_mode == 1)
    {
      LOGI("[%s]iotpf_mode is 1, means update process, so return user send thread", __func__);
      return NULL;
    }
  uint8_t data[6] = {0x05, 0x31, 0x32, 0x33, 0x34, 0x35};

  pthread_mutex_init(&g_exit_mutex, NULL);
  pthread_setname_np(pthread_self(), "cisapi_user_send_thread");
  pthread_detach(pthread_self());

  LOGI("entering into user send thread send_pipe = %d", utc->send_pipe_fd[1]);
  file_detach(utc->send_pipe_fd[1], &utc->send_pipe_file);

  at_fd = ciscom_getATHandle();
  register_indication(at_fd, "$GPRMC", handle_gprmc);

  while (1)
    {
      pthread_mutex_lock(&g_exit_mutex);
      if (g_exit)
        {
          pthread_mutex_unlock(&g_exit_mutex);
          break;
        }
      pthread_mutex_unlock(&g_exit_mutex);

      send_data_to_server(utc, data, sizeof(data));
      sleep(REPORT_INTERVAL);
#if CIS_ENABLE_UPDATE
      pthread_mutex_lock(get_nb_gps_mutex());
      LOGI("[%s]mutex_lock", __func__);
      if (cis_get_fota_update_state())
        {
          LOGI("[%s]fota update working, return", __func__);
          pthread_mutex_unlock(get_nb_gps_mutex());
          return NULL;
        }
#endif
      disconnect_from_server(utc);
      do_gps_capture(at_fd);
      sleep(REPORT_INTERVAL);
      connect_to_server(utc);
#if CIS_ENABLE_UPDATE
      pthread_mutex_unlock(get_nb_gps_mutex());
      LOGI("[%s]mutex_unlock", __func__);
#endif
    }

  return NULL;
}

void *cisapi_user_recv_thread(void *obj)
{
  if(NULL == obj)
    {
      LOGE("[%s]obj is null", __func__);
      return NULL;
    }
  user_thread_context_t *utc = (user_thread_context_t *)obj;
  if(utc->iotpf_mode == 1)
    {
      LOGI("[%s]iotpf_mode is 1, means update process, so return user recv thread", __func__);
      return NULL;
    }

  pthread_mutex_init(&g_exit_mutex, NULL);
  pthread_setname_np(pthread_self(), "cisapi_user_send_thread");
  pthread_detach(pthread_self());

  file_detach(utc->recv_pipe_fd[0], &utc->recv_pipe_file);

  LOGI("entering into user recv thread");

  while (1)
    {
      pthread_mutex_lock(&g_exit_mutex);
      if (g_exit)
        {
          pthread_mutex_unlock(&g_exit_mutex);
          break;
        }
      pthread_mutex_unlock(&g_exit_mutex);
      recv_data_from_server(utc);
    }

  return NULL;
}
#endif /* CIS_ONE_MCU && CIS_OPERATIOR_CTCC */
