#define OLC_PGE_APPLICATION
#include "olcPixelGameEngineC.h"

#include "rtc_handler.h"
#include "containers/zhash-c/zsorted_hash.h"

#include <time.h>
#include <unistd.h>

#define TARGET_FPS 60
#define FRAME_TIME (1000000 / TARGET_FPS) // Time per frame in microseconds

pthread_mutex_t lock;
pthread_cond_t cond;
int ws_joined = 0;
int ws_ret_code = 0;

char username[UUID_STR_LEN];
char room[256] = "\0";

float player_x, player_y;
float player_vel_x, player_vel_y;
float player_speed = 150.0f;

struct Peer {
    float x;
    float y;
};

struct ZSortedHashTable *peers;

void onMessageOpen(int id, void *ptr) {
    struct Peer *new_peer = malloc(sizeof(struct Peer));
    zsorted_hash_set(peers, ptr, new_peer);
}

void onMessageReceived(int id, const char *message, int size, void *ptr) {
    json_object *root = json_tokener_parse(message);
    json_object *sender = json_object_object_get(root, "sender");
    json_object *payload = json_object_object_get(root, "payload");
    json_object *x = json_object_object_get(payload, "player_x");
    json_object *y = json_object_object_get(payload, "player_y");

    struct Peer *peer =
        zsorted_hash_get(peers, (char *)json_object_get_string(sender));
    if (peer != NULL) {
        peer->x = json_object_get_double(x);
        peer->y = json_object_get_double(y);
    }
}

void onMessageClose(int id, void *ptr) {
    struct Peer *peer = zsorted_hash_delete(peers, ptr);
    free(peer);
}

bool OnUserCreate() {
    peers = zcreate_sorted_hash_table();

    return true;
}

struct timespec start, end;
long frame_duration;

bool OnUserUpdate(float fElapsedTime) {
    clock_gettime(CLOCK_MONOTONIC, &start); // Start time for frame

    PGE_Clear(olc_BLACK);
    char fps_str[256];
    snprintf(fps_str, 256, "FPS: %d", PGE_GetFPS());
    PGE_DrawString(10, 10, fps_str, olc_WHITE, 1);

    struct ZIterator *iterator;
    for (iterator = zcreate_iterator(peers); ziterator_exists(iterator);
         ziterator_next(iterator)) {
        struct Peer *peer = (struct Peer *)ziterator_get_val(iterator);
        PGE_FillCircle(peer->x, peer->y, 10, olc_BLUE);
    }
    zfree_iterator(iterator);

    // draw player
    PGE_FillCircle(player_x, player_y, 10, olc_RED);

    player_vel_x = player_vel_y = 0;

    if (PGE_GetKey(olc_A).bHeld) {
        player_vel_x -= player_speed * fElapsedTime;
    }
    if (PGE_GetKey(olc_D).bHeld) {
        player_vel_x += player_speed * fElapsedTime;
    }
    if (PGE_GetKey(olc_W).bHeld) {
        player_vel_y -= player_speed * fElapsedTime;
    }
    if (PGE_GetKey(olc_S).bHeld) {
        player_vel_y += player_speed * fElapsedTime;
    }

    player_x += player_vel_x;
    player_y += player_vel_y;

    // if (player_vel_x != 0 && player_vel_y != 0) {
    json_object *root = json_object_new_object();
    json_object_object_add(root, "player_x", json_object_new_double(player_x));
    json_object_object_add(root, "player_y", json_object_new_double(player_y));
    rtc_send_typed_object("PLAYER_MOVE", root);
    // rtc_send_message(json_object_to_json_string(root));
    json_object_put(root);
    // }

    clock_gettime(CLOCK_MONOTONIC, &end); // End time for frame
    frame_duration = (end.tv_sec - start.tv_sec) * 1000000 +
                     (end.tv_nsec - start.tv_nsec) / 1000;

    // Sleep for the remaining frame time to maintain 60fps
    if (frame_duration < FRAME_TIME) {
        usleep(FRAME_TIME - frame_duration);
    }

    return !PGE_GetKey(olc_ESCAPE).bPressed;
}

bool OnUserDestroy() { return true; }

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

    PGE_SetAppName("Example WebRTC Game");
    if (PGE_Construct(320, 240, 3, 3, false, false))
        PGE_Start(&OnUserCreate, &OnUserUpdate, &OnUserDestroy);

    return 0;
}
