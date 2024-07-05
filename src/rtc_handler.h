#ifndef RTC_HANDLER_H
#define RTC_HANDLER_H

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <uuid/uuid.h>

#include <rtc/rtc.h>
#include <json-c/json.h>

void generate_uuid(char out[UUID_STR_LEN]);
void rtc_initialize(const char **stun_servers, const char *ws_url, const char *username, const char *room, pthread_mutex_t *lock, pthread_cond_t *cond, int *ws_joined, int *ws_ret_code, void (*on_message_received)(const char *message));
void rtc_handle_connection();
void rtc_send_message(const char *message);

#endif // RTC_HANDLER_H
