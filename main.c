#include <rtc/rtc.h>
#include <json-c/json.h>

#include <stdio.h>
#include <string.h>
#include <uuid/uuid.h>

#define MAX_PEERS 9999
rtcConfiguration config;

int ws_id;

char username[256];
char room[256] = "\0";

int dataChannel[MAX_PEERS];
int dataChannelCount = 0;

bool shouldRepond(json_object *root);

void sendNegotiation(const char *type, json_object *data);
void sendOneToOneNegotiation(const char *type, const char *endpoint,
                             const char *sdp);

void connectPeers(json_object *root);
void processOffer(const char *requestee, const char *remoteOffer);

void onOpen(int id, void *ptr);
void onClosed(int id, void *ptr);
void onError(int id, const char *error, void *ptr);
void onMessage(int id, const char *message, int size, void *ptr);

void sendOfferDescriptionCallback(int pc, const char *sdp, const char *type,
                                  void *ptr);

int pc;
int messageListener = 0;

char *uuid(char out[UUID_STR_LEN]) {
    uuid_t b;
    uuid_generate(b);
    uuid_unparse_lower(b, out);
    return out;
}

int main() {
    const char *stun_server = "stun:stun.l.google.com:19302";
    // const char *ws_url = "ws://localhost:3012";

    char host[256];
    char port[256];

    printf("Host: ");
    scanf("%s", host);
    printf("Port: ");
    scanf("%s", port);

    char ws_url[256];
    snprintf(ws_url, 256, "ws://%s:%s", host, port);

    const char *iceServers[256];
    iceServers[0] = stun_server;
    config.iceServers = iceServers;
    config.iceServersCount = 1;

    // printf("Enter your username: ");
    // scanf("%s", username);
    uuid(username);
    printf("Your uuid is %s\n", username);

    char url[256];
    snprintf(url, 256, "%s?user=%s", ws_url, username);

    ws_id = rtcCreateWebSocket(url);

    rtcSetOpenCallback(ws_id, onOpen);
    rtcSetClosedCallback(ws_id, onClosed);
    rtcSetErrorCallback(ws_id, onError);
    rtcSetMessageCallback(ws_id, onMessage);

    int ret = 1;
    while (ret) {
        printf("0: Exit\t1: Enter room\t2: Send message\n");
        printf("Enter a command: ");
        scanf("%d", &ret);

        switch (ret) {
        case 1:
            printf("Room code: ");
            scanf("%s", room);
            sendNegotiation("HANDLE_CONNECTION", NULL);
            break;
        case 2:
            if (dataChannelCount > 0) {
                char message[256];
                printf("Enter message: ");
                scanf("%s", message);

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
            break;
        }
    }

    printf("Closing...\n");

    return 0;
}

void onOpen(int id, void *ptr) {
    printf("\nwebsocket connection opened (id: %d)\n", id);
}

void onClosed(int id, void *ptr) {
    printf("\nwebsocket connection closed (id: %d)\n", id);
}

void onError(int id, const char *error, void *ptr) {
    printf("\nwebsocket connection error (id: %d)\n", id);
}

void onMessage(int id, const char *message, int size, void *ptr) {
    printf("(id: %d) message: %s\n", id, message);

    json_object *root = json_tokener_parse(message);

    json_object *type = json_object_object_get(root, "type");
    const char *type_str = json_object_get_string(type);

    if (shouldRepond(root)) {
        json_object *type = json_object_object_get(root, "type");
        const char *type_str = json_object_get_string(type);

        if (strcmp(type_str, "HANDLE_CONNECTION") == 0) {
            printf("New peer wants to connect\n");
            connectPeers(root);
        } else if (strcmp(type_str, "offer") == 0) {
            json_object *from = json_object_object_get(root, "from");
            json_object *data = json_object_object_get(root, "data");
            printf("GOT OFFER FROM A NODE WE WANT TO CONNECT TO\n");
            printf("THE NODE IS %s\n", json_object_get_string(from));
            processOffer(json_object_get_string(from),
                         json_object_get_string(data));
        }
    }

    if (messageListener) {
        if (strcmp(type_str, "answer") == 0) {
            json_object *data = json_object_object_get(root, "data");
            printf("--- GOT ANSWER IN CONNECT ---\n");
            rtcSetRemoteDescription(pc, json_object_get_string(data), "answer");
        } else if (strcmp(type_str, "candidate") == 0) {
            json_object *data = json_object_object_get(root, "data");
            rtcAddRemoteCandidate(pc, json_object_get_string(data), NULL);
        }
    }
}

void sendOfferDescriptionCallback(int pc, const char *sdp, const char *type,
                                  void *ptr) {
    const char *requestee = (const char *)ptr;
    rtcSetLocalDescription(pc, sdp);
    sendOneToOneNegotiation("offer", requestee, sdp);
    printf("------ SEND OFFER ------\n");
}

void onDataChannelOpen(int id, void *ptr) {
    printf("\nData channel opened\n");
    dataChannel[dataChannelCount++] = id;
    messageListener = 0;
}

void onDataChannelMessage(int id, const char *message, int size, void *ptr) {
    printf("\n%s\n", message);
}

void onDataChannelClose(int id, void *ptr) {
    printf("\nData channel closed\n");
    messageListener = 0;
}

void candidateConnectPeersCallback(int pc, const char *cand, const char *mid,
                                   void *ptr) {
    const char *requestee = (const char *)ptr;
    if (cand != NULL) {
        printf("sent negotiations\n");
        sendOneToOneNegotiation("candidate", requestee, cand);
    }
}

void connectPeers(json_object *root) {
    printf("CONNECTING PEERS\n");

    json_object *data = json_object_object_get(root, "data");

    pc = rtcCreatePeerConnection(&config);
    rtcSetUserPointer(pc, (void *)json_object_get_string(data));
    rtcSetLocalDescriptionCallback(pc, sendOfferDescriptionCallback);

    int dc = rtcCreateDataChannel(pc, "sendChannel");

    printf("created data channel\n");

    messageListener = 1;

    rtcSetLocalCandidateCallback(pc, candidateConnectPeersCallback);

    rtcSetOpenCallback(dc, onDataChannelOpen);
    rtcSetMessageCallback(dc, onDataChannelMessage);
    rtcSetClosedCallback(dc, onDataChannelClose);
}

void sendAnswerDescriptionCallback(int pc, const char *sdp, const char *type,
                                   void *ptr) {
    const char *requestee = (const char *)ptr;
    rtcSetLocalDescription(pc, sdp);
    sendOneToOneNegotiation("answer", requestee, sdp);
    printf("------ SEND ANSWER ------\n");
}

void candidateProcessOfferCallback(int pc, const char *cand, const char *mid,
                                   void *ptr) {
    const char *requestee = (const char *)ptr;
    sendOneToOneNegotiation("candidate", requestee, cand);
}

void processOfferDataChannelCallback(int pc, int dc, void *ptr) {
    rtcSetOpenCallback(dc, onDataChannelOpen);
    rtcSetMessageCallback(dc, onDataChannelMessage);
    rtcSetClosedCallback(dc, onDataChannelClose);
    // printf("Data channel is open and ready to be used.\n");
    // dataChannel[dataChannelCount++] = dc;
}

void processOffer(const char *requestee, const char *remoteOffer) {
    printf("RUNNING PROCESS OFFER\n");

    int pc = rtcCreatePeerConnection(&config);
    rtcSetUserPointer(pc, (void *)requestee);

    rtcSetLocalDescriptionCallback(pc, sendAnswerDescriptionCallback);

    rtcSetLocalCandidateCallback(pc, candidateProcessOfferCallback);

    rtcSetDataChannelCallback(pc, processOfferDataChannelCallback);

    rtcSetRemoteDescription(pc, remoteOffer, "offer");
}

bool shouldRepond(json_object *root) {
    json_object *from = json_object_object_get(root, "from");
    const char *from_str = json_object_get_string(from);

    json_object *endpoint = json_object_object_get(root, "endpoint");
    const char *endpoint_str = json_object_get_string(endpoint);

    json_object *room_obj = json_object_object_get(root, "room");
    const char *room_str = json_object_get_string(room_obj);

    return strcmp(from_str, username) != 0 &&
           (strcmp(endpoint_str, username) == 0 || strcmp(room_str, room) == 0);
}

void sendNegotiation(const char *type, json_object *data) {
    if (room[0] == '\0') {
        printf("Please provide a room code\n");
        return;
    }

    json_object *root = json_object_new_object();
    json_object_object_add(root, "protocol",
                           json_object_new_string("one-to-all"));
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

void sendOneToOneNegotiation(const char *type, const char *endpoint,
                             const char *sdp) {
    if (room[0] == '\0') {
        printf("Please provide a room code\n");
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
