#define WEN_ENABLE_WS
#define WEN_IMPLEMENTATION
#include "wen.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_WS_PAYLOAD 125
#define MAX_WS_MESSAGE (64 * 1024)

typedef struct {
    int fragmented;
    uint8_t frag_opcode;
    size_t frag_len;
    uint8_t frag_buf[MAX_WS_MESSAGE];

    wen_link *link;
} ws_codec_state;


#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

void sha1_base64(const char *input, char *output, size_t outlen) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)input, strlen(input), hash);

    BIO *bmem = BIO_new(BIO_s_mem());
    BIO *b64 = BIO_new(BIO_f_base64());
    b64 = BIO_push(b64, bmem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, hash, SHA_DIGEST_LENGTH);
    BIO_flush(b64);
    int len = BIO_read(bmem, output, outlen - 1);
    output[len] = 0;

    BIO_free_all(b64);
}

static wen_handshake_status ws_handshake(void *codec_state,
                                         const void *in, unsigned long in_len,
                                         unsigned long *consumed,
                                         void *out, unsigned long out_cap,
                                         unsigned long *out_len)
{
    (void)codec_state;

    // copy & NUL-terminate
    char buf[2048];
    if (in_len >= sizeof(buf)) return WEN_HANDSHAKE_FAILED;
    memcpy(buf, in, in_len);
    buf[in_len] = 0;

    if (!strstr(buf, "GET "))                      return WEN_HANDSHAKE_FAILED;
    if (!strcasestr(buf, "Upgrade: websocket"))    return WEN_HANDSHAKE_FAILED;
    if (!strcasestr(buf, "Connection: Upgrade"))   return WEN_HANDSHAKE_FAILED;
    if (!strstr(buf, "Sec-WebSocket-Version: 13")) return WEN_HANDSHAKE_FAILED;

    const char *ptr = strcasestr(buf, "Sec-WebSocket-Key:");
    if (!ptr) return WEN_HANDSHAKE_INCOMPLETE;

    ptr += strlen("Sec-WebSocket-Key:");
    while (*ptr == ' ') ptr++;

    char key[128];
    int i = 0;
    while (*ptr && *ptr != '\r' && *ptr != '\n' && i < (int)sizeof(key) - 1)
        key[i++] = *ptr++;
    key[i] = 0;

    char concat[256];
    snprintf(concat, sizeof(concat), "%s%s", key, WEN_WS_GUID);

    char accept[64];
    sha1_base64(concat, accept, sizeof(accept));

    int len = snprintf(out, out_cap,
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n\r\n",
        accept);

    if (len <= 0 || (unsigned long)len > out_cap)
        return WEN_HANDSHAKE_FAILED;

    *consumed = in_len;
    *out_len  = (unsigned long)len;
    return WEN_HANDSHAKE_COMPLETE;
}

static uint64_t read_be64(const uint8_t *p) {
    return ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48) |
           ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32) |
           ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) |
           ((uint64_t)p[6] << 8)  | ((uint64_t)p[7]);
}

static int is_control(uint8_t opcode) {
    return opcode == 0x8 || opcode == 0x9 || opcode == 0xA;
}


static wen_result ws_decode(void *codec_state, const void *data, unsigned long len)
{
    ws_codec_state *st = codec_state;
    const uint8_t *b = data;

    if (len < 2) return WEN_OK;

    uint8_t fin    = b[0] & 0x80;
    uint8_t opcode = b[0] & 0x0F;
    uint8_t masked = b[1] & 0x80;
    uint64_t plen  = b[1] & 0x7F;
    unsigned hdr   = 2;

    if (!masked) return WEN_ERR_PROTOCOL;

    if (plen == 126) {
        if (len < hdr + 2) return WEN_OK;
        uint16_t t;
        memcpy(&t, b + hdr, 2);
        plen = ntohs(t);
        hdr += 2;
    } else if (plen == 127) {
        if (len < hdr + 8) return WEN_OK;
        plen = read_be64(b + hdr);
        hdr += 8;
    }

    if (len < hdr + 4 + plen) return WEN_OK;

    if ((opcode & 0x08) && (!fin || plen > 125)) return WEN_ERR_PROTOCOL;

    unsigned long frame_len = hdr + 4 + plen;

    wen_event fev = {
        .type = WEN_EV_FRAME,
        .as.frame = {
            .fin = !!fin,
            .masked = 1,
            .opcode = opcode,
            .length = plen
        }
    };
    wen_evq_push(&((wen_link *)st->link)->evq, &fev);

    if (opcode == WEN_WS_OP_PING) {
        wen_event pev = { .type = WEN_EV_PING };
        wen_evq_push(&((wen_link *)st->link)->evq, &pev);
    } else if (opcode == WEN_WS_OP_PONG) {
        wen_event pev = { .type = WEN_EV_PONG };
        wen_evq_push(&((wen_link *)st->link)->evq, &pev);
    }

    ((wen_link *)st->link)->frame_len = frame_len;
    return WEN_OK;
}

static wen_result ws_encode(void *codec_state, unsigned opcode,
                            const void *data, unsigned long len,
                            void *out, unsigned long out_cap,
                            unsigned long *out_len) {
    (void)codec_state;

    uint8_t *b = out;
    unsigned hdr = 2;

    if (len <= 125) {
        if (out_cap < hdr + len) return WEN_ERR_OVERFLOW;
        b[1] = len;
    } else if (len <= 0xFFFF) {
        hdr += 2;
        if (out_cap < hdr + len) return WEN_ERR_OVERFLOW;
        b[1] = 126;
        *(uint16_t *)(b + 2) = htons(len);
    } else {
        hdr += 8;
        if (out_cap < hdr + len) return WEN_ERR_OVERFLOW;
        b[1] = 127;
        uint64_t be = htobe64(len);
        memcpy(b + 2, &be, 8);
    }
    if ((opcode & 0x08) && len > 125)
        return WEN_ERR_PROTOCOL;

    b[0] = 0x80 | (opcode & 0x0F); // FIN + opcode
    memcpy(b + hdr, data, len);

    *out_len = hdr + len;
    return WEN_OK;
}

static const wen_codec ws_codec = {
    .name = "wen-ws",
    .handshake = ws_handshake,
    .decode = ws_decode,
    .encode = ws_encode,
};

static long sock_read(void *user, void *buf, unsigned long len) {
    int fd = *(int *)user;
    return read(fd, buf, len);
}
static long sock_write(void *user, const void *buf, unsigned long len) {
    int fd = *(int *)user;
    return write(fd, buf, len);
}

void run_ws(int sockfd) {
    wen_link link;
    wen_event ev;
    wen_io io = {.user = &sockfd, .read = sock_read, .write = sock_write};
    wen_link_init(&link, io);

    unsigned ecode = WEN_EV_CLOSE;
    unsigned opcode = WEN_WS_OP_CLOSE;

    ws_codec_state state = {
        .link = &link
    };
    wen_link_attach_codec(&link, &ws_codec, &state);
    for (;;) {
        if (!wen_poll(&link, &ev)) continue;

        switch (ev.type) {

        case WEN_EV_OPEN:
            printf("[WS] Handshake complete\n");
            wen_send(&link, WEN_WS_OP_TEXT, "Hello from wen!", 15);
            break;

        case WEN_EV_SLICE: {
            uint8_t *b = (uint8_t *)ev.as.slice.data;

            uint64_t plen = b[1] & 0x7F;
            uint8_t *mask = b + 2;
            uint8_t *payload = b + 6;

            for (uint64_t i = 0; i < plen; i++)
                payload[i] ^= mask[i & 3];

            if (opcode == WEN_WS_OP_PING) {
                wen_send(&link, WEN_WS_OP_PONG, payload, plen);
                wen_release(&link, ev.as.slice);
                break;
            }

            if (opcode == WEN_WS_OP_TEXT) {
                if (plen && payload[plen - 1] == '\n')
                    plen--;

                printf("[WS] %.*s\n", (int)plen, payload);
                wen_send(&link, WEN_WS_OP_TEXT, payload, plen);
            }

            wen_release(&link, ev.as.slice);
            break;
        }

        case WEN_EV_CLOSE:
            printf("[WS] Connection closed\n");
            ecode = WEN_EV_CLOSE;
            goto next_client;

        case WEN_EV_ERROR:
            fprintf(stderr, "[WS] Error: %d\n", ev.as.error);
            ecode = WEN_EV_ERROR;
            goto next_client;

        case WEN_EV_PING:
            printf("[PING]\n");
            break;

        case WEN_EV_PONG:
            printf("[PONG]\n");
            break;

        default:
            break;
        }
    }

next_client:
    wen_close(&link, ecode, opcode);
    ;
}

int main(void) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) <
        0) {
        perror("setsockopt");
        exit(1);
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8001);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(listen_fd, 5) < 0) {
        perror("listen");
        exit(1);
    }
    printf("Server listening on port 8001...\n");

    while (1) {
        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        printf("Client connected!\n");
        run_ws(client_fd);
        close(client_fd);
    }
}
