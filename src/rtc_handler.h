#ifndef RTC_HANDLER_H
#define RTC_HANDLER_H

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <uuid/uuid.h>

#include <rtc/rtc.h>
#include <json-c/json.h>

void generate_uuid(char out[UUID_STR_LEN]);
void rtc_initialize(const char **stun_servers, int stun_servers_count,
                    const char *ws_url, const char *username, const char *room,
                    pthread_mutex_t *lock, pthread_cond_t *cond, int *ws_joined,
                    int *ws_ret_code);
void rtc_handle_connection();
void rtc_send_message(const char *message);
void rtc_send_typed_object(const char *type, json_object *obj);

void rtc_set_message_opened_callback(void (*on_message_opened)(int id,
                                                               void *ptr));
void rtc_set_message_received_callback(void (*on_message_received)(
    int id, const char *message, int size, void *ptr));
void rtc_set_message_closed_callback(void (*on_message_closed)(int id,
                                                               void *ptr));

#endif // RTC_HANDLER_H
