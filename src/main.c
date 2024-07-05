#include "rtc_handler.h"

pthread_mutex_t lock;
pthread_cond_t cond;
int ws_joined = 0;
int ws_ret_code = 0;

char username[UUID_STR_LEN];
char room[256] = "\0";

void onMessageReceived(const char *message);

int main() {
    const char *stun_server = "stun:stun.l.google.com:19302";
    char host[256];
    char port[256];

    printf("Host: ");
    scanf("%s", host);
    printf("Port: ");
    scanf("%s", port);

    char ws_url[256];
    snprintf(ws_url, 256, "ws://%s:%s", host, port);

    generate_uuid(username);
    printf("Your uuid is %s\n", username);

    printf("Room code: ");
    scanf("%s", room);

    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);

    const char *iceServers[256];
    iceServers[0] = stun_server;
    rtc_initialize(iceServers, ws_url, username, room, &lock, &cond, &ws_joined, &ws_ret_code, onMessageReceived);

    pthread_mutex_lock(&lock);
    while (!ws_joined) {
        pthread_cond_wait(&cond, &lock);
    }
    pthread_mutex_unlock(&lock);

    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond);

    if (ws_ret_code) {
        exit(ws_ret_code);
    }

    rtc_handle_connection();

    int ret = 1;
    while (ret) {
        printf("0: Exit\t\t1: Send message\n");
        printf("Enter a command: ");
        scanf("%d", &ret);

        if (ret == 1) {
            char message[256];
            printf("Enter message: ");
            scanf("%s", message);
            rtc_send_message(message);
        }
    }

    printf("Closing...\n");

    return 0;
}

void onMessageReceived(const char *message) {
    printf("\nReceived message: %s\n", message);
}
