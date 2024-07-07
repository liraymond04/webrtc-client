#include "rtc_handler.h"

#define MAX_PEERS 3

#ifdef DEBUG
# define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
# define DEBUG_PRINT(...) do {} while (0)
#endif

static rtcConfiguration config;
static int ws_id;
static int dataChannel[MAX_PEERS];
static int dataChannelCount = 0;
static int messageListener = 0;

static char username[UUID_STR_LEN];
static char room[256];

static pthread_mutex_t *lock;
static pthread_cond_t *cond;
static int *ws_joined;
static int *ws_ret_code;

static void (*message_opened_callback)(int id, void *ptr) = NULL;
static void (*message_received_callback)(int id, const char *message, int size,
                                         void *ptr) = NULL;
static void (*message_closed_callback)(int id, void *ptr) = NULL;

static bool shouldRespond(json_object *root);
static void sendNegotiation(const char *type, json_object *data);
static void sendOneToOneNegotiation(const char *type, const char *endpoint,
                                    const char *sdp);

static void connectPeers(json_object *root);
static void rejectPeers(json_object *root);
static void processOffer(const char *requestee, const char *remoteOffer);

static inline void onOpen(int id, void *ptr);
static inline void onClosed(int id, void *ptr);
static inline void onError(int id, const char *error, void *ptr);
static inline void onMessage(int id, const char *message, int size, void *ptr);

static inline void sendOfferDescriptionCallback(int pc, const char *sdp,
                                                const char *type, void *ptr);
static inline void onDataChannelOpen(int id, void *ptr);
static inline void onDataChannelMessage(int id, const char *message, int size,
                                        void *ptr);
static inline void onDataChannelClose(int id, void *ptr);
static inline void candidateConnectPeersCallback(int pc, const char *cand,
                                                 const char *mid, void *ptr);
static inline void sendAnswerDescriptionCallback(int pc, const char *sdp,
                                                 const char *type, void *ptr);
static inline void candidateProcessOfferCallback(int pc, const char *cand,
                                                 const char *mid, void *ptr);
static inline void processOfferDataChannelCallback(int pc, int dc, void *ptr);

void generate_uuid(char out[UUID_STR_LEN]) {
    uuid_t b;
    uuid_generate(b);
    uuid_unparse_lower(b, out);
}

void rtc_initialize(const char **stun_servers, const char *ws_url,
                    const char *user, const char *rm, pthread_mutex_t *lck,
                    pthread_cond_t *cnd, int *joined, int *ret_code) {
    config.iceServers = stun_servers;
    config.iceServersCount = 1;

    strcpy(username, user);
    strcpy(room, rm);

    lock = lck;
    cond = cnd;
    ws_joined = joined;
    ws_ret_code = ret_code;

    char url[256];
    snprintf(url, 256, "%s?user=%s&room=%s", ws_url, username, room);

    ws_id = rtcCreateWebSocket(url);

    rtcSetOpenCallback(ws_id, onOpen);
    rtcSetClosedCallback(ws_id, onClosed);
    rtcSetErrorCallback(ws_id, onError);
    rtcSetMessageCallback(ws_id, onMessage);
}

void rtc_handle_connection() { sendNegotiation("HANDLE_CONNECTION", NULL); }

void rtc_send_message(const char *message) {
    if (dataChannelCount > 0) {
        json_object *root = json_object_new_object();
        json_object_object_add(root, "sender",
                               json_object_new_string(username));
        json_object_object_add(root, "payload",
                               json_object_new_string(message));

        const char *json_str = json_object_to_json_string(root);
        for (int i = 0; i < dataChannelCount; i++) {
            rtcSendMessage(dataChannel[i], json_str, strlen(json_str));
        }
    }
}

void rtc_set_message_opened_callback(void (*on_message_opened)(int id,
                                                               void *ptr)) {
    message_opened_callback = on_message_opened;
}

void rtc_set_message_received_callback(void (*on_message_received)(
    int id, const char *message, int size, void *ptr)) {
    message_received_callback = on_message_received;
}

inline void
rtc_set_message_closed_callback(void (*on_message_closed)(int id, void *ptr)) {
    message_closed_callback = on_message_closed;
}

static inline void onOpen(int id, void *ptr) {
    DEBUG_PRINT("\nWebSocket connection opened (id: %d)\n", id);
    pthread_mutex_lock(lock);
    *ws_joined = 1;
    pthread_cond_signal(cond);
    pthread_mutex_unlock(lock);
}

static inline void onClosed(int id, void *ptr) {
    DEBUG_PRINT("\nWebSocket connection closed (id: %d)\n", id);
    pthread_mutex_lock(lock);
    *ws_joined = 1;
    *ws_ret_code = 0;
    pthread_cond_signal(cond);
    pthread_mutex_unlock(lock);
}

static inline void onError(int id, const char *error, void *ptr) {
    DEBUG_PRINT("\nWebSocket connection error (id: %d)\n", id);
    pthread_mutex_lock(lock);
    *ws_joined = 1;
    *ws_ret_code = 1;
    pthread_cond_signal(cond);
    pthread_mutex_unlock(lock);
}

static inline void onMessage(int id, const char *message, int size, void *ptr) {
    DEBUG_PRINT("(id: %d) message: %s\n", id, message);

    json_object *root = json_tokener_parse(message);

    json_object *type = json_object_object_get(root, "type");
    const char *type_str = json_object_get_string(type);

    if (shouldRespond(root)) {
        json_object *type = json_object_object_get(root, "type");
        const char *type_str = json_object_get_string(type);

        if (strcmp(type_str, "HANDLE_CONNECTION") == 0) {
            DEBUG_PRINT("New peer wants to connect\n");
            if (dataChannelCount < MAX_PEERS) {
                connectPeers(root);
            } else {
                DEBUG_PRINT("Max peers connected\n");
                rejectPeers(root);
            }
        } else if (strcmp(type_str, "offer") == 0) {
            json_object *from = json_object_object_get(root, "from");
            json_object *data = json_object_object_get(root, "data");
            DEBUG_PRINT("GOT OFFER FROM A NODE WE WANT TO CONNECT TO\n");
            DEBUG_PRINT("THE NODE IS %s\n", json_object_get_string(from));
            processOffer(json_object_get_string(from),
                         json_object_get_string(data));
        } else if (strcmp(type_str, "REJECT_CONNECTION") == 0) {
            json_object *data = json_object_object_get(root, "data");
            DEBUG_PRINT("Connection offer rejected: %s\n",
                   json_object_get_string(data));
        }
    }

    if (messageListener) {
        if (strcmp(type_str, "answer") == 0) {
            json_object *data = json_object_object_get(root, "data");
            DEBUG_PRINT("--- GOT ANSWER IN CONNECT ---\n");
            rtcSetRemoteDescription(messageListener,
                                    json_object_get_string(data), "answer");
        } else if (strcmp(type_str, "candidate") == 0) {
            json_object *data = json_object_object_get(root, "data");
            rtcAddRemoteCandidate(messageListener, json_object_get_string(data),
                                  NULL);
        }
    }
}

static inline void sendOfferDescriptionCallback(int pc, const char *sdp,
                                                const char *type, void *ptr) {
    const char *requestee = (const char *)ptr;
    rtcSetLocalDescription(pc, sdp);
    sendOneToOneNegotiation("offer", requestee, sdp);
    DEBUG_PRINT("------ SEND OFFER ------\n");
}

static inline void onDataChannelOpen(int id, void *ptr) {
    DEBUG_PRINT("\nData channel opened\n");
    dataChannel[dataChannelCount++] = id;
    messageListener = 0;

    if (message_opened_callback) {
        message_opened_callback(id, ptr);
    }
}

static inline void onDataChannelMessage(int id, const char *message, int size,
                                        void *ptr) {
    if (message_received_callback) {
        message_received_callback(id, message, size, ptr);
    }
}

static inline void onDataChannelClose(int id, void *ptr) {
    DEBUG_PRINT("\nData channel closed\n");
    int found = 0;
    for (int i = 0; i < dataChannelCount - 1; i++) {
        if (dataChannel[i] == id)
            found = 1;
        if (found)
            dataChannel[i] = dataChannel[i + 1];
    }
    dataChannelCount--;
    messageListener = 0;

    if (message_closed_callback) {
        message_closed_callback(id, ptr);
    }
}

static inline void candidateConnectPeersCallback(int pc, const char *cand,
                                                 const char *mid, void *ptr) {
    const char *requestee = (const char *)ptr;
    if (cand != NULL) {
        DEBUG_PRINT("sent negotiations\n");
        sendOneToOneNegotiation("candidate", requestee, cand);
    }
}

static void connectPeers(json_object *root) {
    DEBUG_PRINT("CONNECTING PEERS\n");

    json_object *data = json_object_object_get(root, "data");

    int pc = rtcCreatePeerConnection(&config);
    rtcSetUserPointer(pc, (void *)json_object_get_string(data));
    rtcSetLocalDescriptionCallback(pc, sendOfferDescriptionCallback);

    int dc = rtcCreateDataChannel(pc, "sendChannel");

    DEBUG_PRINT("created data channel\n");

    messageListener = pc;

    rtcSetLocalCandidateCallback(pc, candidateConnectPeersCallback);

    rtcSetOpenCallback(dc, onDataChannelOpen);
    rtcSetMessageCallback(dc, onDataChannelMessage);
    rtcSetClosedCallback(dc, onDataChannelClose);
}

static inline void sendAnswerDescriptionCallback(int pc, const char *sdp,
                                                 const char *type, void *ptr) {
    const char *requestee = (const char *)ptr;
    rtcSetLocalDescription(pc, sdp);
    sendOneToOneNegotiation("answer", requestee, sdp);
    DEBUG_PRINT("------ SEND ANSWER ------\n");
}

static inline void candidateProcessOfferCallback(int pc, const char *cand,
                                                 const char *mid, void *ptr) {
    const char *requestee = (const char *)ptr;
    sendOneToOneNegotiation("candidate", requestee, cand);
}

static inline void processOfferDataChannelCallback(int pc, int dc, void *ptr) {
    rtcSetOpenCallback(dc, onDataChannelOpen);
    rtcSetMessageCallback(dc, onDataChannelMessage);
    rtcSetClosedCallback(dc, onDataChannelClose);
}

static void processOffer(const char *requestee, const char *remoteOffer) {
    DEBUG_PRINT("RUNNING PROCESS OFFER\n");

    int pc = rtcCreatePeerConnection(&config);
    rtcSetUserPointer(pc, (void *)requestee);

    rtcSetLocalDescriptionCallback(pc, sendAnswerDescriptionCallback);

    rtcSetLocalCandidateCallback(pc, candidateProcessOfferCallback);

    rtcSetDataChannelCallback(pc, processOfferDataChannelCallback);

    rtcSetRemoteDescription(pc, remoteOffer, "offer");
}

static bool shouldRespond(json_object *root) {
    json_object *from = json_object_object_get(root, "from");
    const char *from_str = json_object_get_string(from);

    json_object *endpoint = json_object_object_get(root, "endpoint");
    const char *endpoint_str = json_object_get_string(endpoint);

    json_object *room_obj = json_object_object_get(root, "room");
    const char *room_str = json_object_get_string(room_obj);

    return strcmp(from_str, username) != 0 &&
           (strcmp(endpoint_str, username) == 0 || strcmp(room_str, room) == 0);
}

static void rejectPeers(json_object *root) {
    json_object *from = json_object_object_get(root, "from");
    sendOneToOneNegotiation("REJECT_CONNECTION", json_object_get_string(from),
                            "Max peers connected");
}

static void sendNegotiation(const char *type, json_object *data) {
    if (room[0] == '\0') {
        DEBUG_PRINT("Please provide a room code\n");
        return;
    }

    json_object *root = json_object_new_object();
    json_object_object_add(root, "protocol",
                           json_object_new_string("one-to-room"));
    json_object_object_add(root, "room", json_object_new_string(room));
    json_object_object_add(root, "from", json_object_new_string(username));
    json_object_object_add(root, "endpoint", json_object_new_string("any"));
    json_object_object_add(root, "type", json_object_new_string(type));
    if (data != NULL)
        json_object_object_add(root, "data", data);
    else
        json_object_object_add(root, "data", json_object_new_string(username));

    const char *json_string = json_object_to_json_string(root);

    rtcSendMessage(ws_id, json_string, strlen(json_string));
}

static void sendOneToOneNegotiation(const char *type, const char *endpoint,
                                    const char *sdp) {
    if (room[0] == '\0') {
        DEBUG_PRINT("Please provide a room code\n");
        return;
    }

    json_object *root = json_object_new_object();
    json_object_object_add(root, "protocol",
                           json_object_new_string("one-to-one"));
    json_object_object_add(root, "room", json_object_new_string(room));
    json_object_object_add(root, "from", json_object_new_string(username));
    json_object_object_add(root, "endpoint", json_object_new_string(endpoint));
    json_object_object_add(root, "type", json_object_new_string(type));
    json_object_object_add(root, "data", json_object_new_string(sdp));

    const char *json_string = json_object_to_json_string(root);

    rtcSendMessage(ws_id, json_string, strlen(json_string));
}
