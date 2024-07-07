#include "rtc_handler.h"

#include <ncurses.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <stdlib.h>

#define INPUT_HEIGHT 3
#define MAX_INPUT 256
#define FRAME_TIME 16667 // Time per frame in microseconds for 60fps

pthread_mutex_t lock;
pthread_cond_t cond;
int ws_joined = 0;
int ws_ret_code = 0;

char username[UUID_STR_LEN];
char room[256] = "\0";

WINDOW *chat_win, *input_win;
char **messages;
int message_count = 0;
int message_capacity = 10;

pthread_mutex_t msg_lock;

void initialize_windows();
void refresh_windows();
void append_message(const char *message);
void handle_resize(int sig);
void add_message_to_buffer(const char *message);
void free_messages();
void reprint_messages();

void onMessageOpen(int id, void *ptr) {
    char sent_message[256];
    snprintf(sent_message, 256, "[%d]: %s", id, "Data channel opened");

    add_message_to_buffer(sent_message);
}

void onMessageReceived(int id, const char *message, int size, void *ptr) {
    char sent_message[256];

    json_object *root = json_tokener_parse(message);
    json_object *sender = json_object_object_get(root, "sender");
    json_object *payload = json_object_object_get(root, "payload");
    snprintf(sent_message, size, "[%s]: %s", json_object_get_string(sender),
             json_object_get_string(payload));

    add_message_to_buffer(sent_message);
}

void onMessageClose(int id, void *ptr) {
    char sent_message[256];
    snprintf(sent_message, 256, "[%d]: %s", id, "Data channel closed");

    add_message_to_buffer(sent_message);
}

int ret = 1;

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
    rtc_initialize(iceServers, ws_url, username, room, &lock, &cond, &ws_joined,
                   &ws_ret_code);
    rtc_set_message_opened_callback(onMessageOpen);
    rtc_set_message_received_callback(onMessageReceived);
    rtc_set_message_closed_callback(onMessageClose);

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

    pthread_mutex_init(&msg_lock, NULL);

    char input_buffer[MAX_INPUT];
    int ch, pos = 0;
    struct timespec start, end;
    long frame_duration;

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE); // Make getch() non-blocking

    signal(SIGWINCH, handle_resize); // Catch window resize signal

    messages = malloc(message_capacity * sizeof(char *));
    initialize_windows();

    while (ret) {
        clock_gettime(CLOCK_MONOTONIC, &start); // Start time for frame

        reprint_messages();

        wclear(input_win);
        box(input_win, 0, 0);
        mvwprintw(input_win, 1, 1, "> %s", input_buffer);
        wrefresh(input_win);

        ch = getch();
        if (ch != ERR) { // Check if a key was pressed
            if (ch == '\n') {
                input_buffer[pos] = '\0';
                append_message(input_buffer);
                pos = 0;
                input_buffer[0] = '\0';
            } else if (ch == KEY_BACKSPACE || ch == 127) {
                if (pos > 0) {
                    pos--;
                    input_buffer[pos] = '\0';
                }
            } else if (pos < MAX_INPUT - 1) {
                input_buffer[pos] = ch;
                pos++;
                input_buffer[pos] = '\0'; // Null-terminate the string
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &end); // End time for frame
        frame_duration = (end.tv_sec - start.tv_sec) * 1000000 +
                         (end.tv_nsec - start.tv_nsec) / 1000;

        // Sleep for the remaining frame time to maintain 60fps
        if (frame_duration < FRAME_TIME) {
            usleep(FRAME_TIME - frame_duration);
        }
    }

    pthread_mutex_destroy(&msg_lock);

    free_messages();
    endwin();
    return 0;
}

void initialize_windows() {
    int height, width;
    getmaxyx(stdscr, height, width);

    if (chat_win != NULL) {
        delwin(chat_win);
    }
    if (input_win != NULL) {
        delwin(input_win);
    }

    chat_win = newwin(height - INPUT_HEIGHT, width, 0, 0);
    input_win = newwin(INPUT_HEIGHT, width, height - INPUT_HEIGHT, 0);

    scrollok(chat_win, TRUE);

    refresh_windows();
}

void refresh_windows() {
    wclear(chat_win);
    wclear(input_win);

    box(chat_win, 0, 0);
    box(input_win, 0, 0);

    mvwprintw(input_win, 0, 1, "Input");

    wrefresh(chat_win);
    wrefresh(input_win);
}

void append_message(const char *message) {
    char full_msg[256];
    snprintf(full_msg, 256, "> %s", message);
    add_message_to_buffer(full_msg);

    if (strcmp(message, "help") == 0) {
        add_message_to_buffer("\thelp\t- Open this help prompt\n"
                              "\tclear\t- Clear chat window\n"
                              "\texit\t- Close application\n"
                              "\tsay\t- Send message to peers");
    } else if (strcmp(message, "clear") == 0) {
        pthread_mutex_lock(&msg_lock);
        free_messages();
        messages = malloc(message_capacity * sizeof(char *));
        message_count = 0;
        pthread_mutex_unlock(&msg_lock);
    } else if (strcmp(message, "exit") == 0) {
        ret = 0;
    } else if ((message - strstr(message, "say")) == 0) {
        char full_msg[256];
        snprintf(full_msg, 256, "[me]: %s", message + 4);
        add_message_to_buffer(full_msg);
        rtc_send_message(message + 4);
    } else {
        char full_msg[256];
        snprintf(full_msg, 256, "Command '%s' not recognized", message);
        add_message_to_buffer(full_msg);
    }
}

void handle_resize(int sig) {
    endwin();
    refresh();
    clear();
    initialize_windows();
}

void add_message_to_buffer(const char *message) {
    pthread_mutex_lock(&msg_lock);
    if (message_count >= message_capacity) {
        message_capacity *= 2;
        messages = realloc(messages, message_capacity * sizeof(char *));
    }
    messages[message_count] = strdup(message);
    message_count++;
    pthread_mutex_unlock(&msg_lock);
}

void free_messages() {
    for (int i = 0; i < message_count; i++) {
        free(messages[i]);
    }
    free(messages);
}

void reprint_messages() {
    wclear(chat_win);
    for (int i = 0; i < message_count; i++) {
        wprintw(chat_win, "%s\n", messages[i]);
    }
    wrefresh(chat_win);
}
