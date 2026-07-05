#include "nb_mind_link_shell.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nb_event_bus_shell.h"
#include "nb_mind_link_token_shell.h"
#include "nbp2.h"
#include "sdkconfig.h"

#define NB_MIND_LINK_TASK_STACK 6144u
#define NB_MIND_LINK_TASK_PRIO 12
#define NB_MIND_LINK_RECV_TIMEOUT_MS 200u
#define NB_MIND_LINK_FRAME_BUF_CAP (NBP2_FRAME_OVERHEAD + NBP2_MAX_CTRL_PAYLOAD)
#define NB_MIND_LINK_HELLO_SEQ 1u

static const char *TAG = "mind_link";
static nb_mind_link_session_t s_session;
static TaskHandle_t s_task_handle;

/* S3.5: disparo de timer pedido de fora da task (main.c/schedule_core_shell)
 * -- flag simples em vez de socket compartilhado entre tasks (só a task de
 * mind_link possui `sock`; enviar de outra task arriscaria interleaving de
 * send() concorrente). Único produtor (main.c), único consumidor (esta
 * task), suficiente sem fila/mutex. */
static volatile bool s_pending_timer_fired;
static volatile uint32_t s_pending_timer_fired_id;

typedef struct {
    uint8_t data[NB_MIND_LINK_FRAME_BUF_CAP];
    size_t len;
} nb_mind_link_frame_buf_t;

typedef struct {
    uint16_t type;
    uint32_t seq;
    uint8_t payload[NBP2_MAX_CTRL_PAYLOAD];
    uint16_t payload_len;
} nb_mind_link_parsed_frame_t;

static uint32_t nb_mind_link_shell_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static int nb_mind_link_shell_connect(void)
{
    struct sockaddr_in dest_addr;
    int sock;
    int flags;

    if (strlen(CONFIG_NBP2_SERVER_HOST) == 0u) {
        return -1;
    }

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons((uint16_t)CONFIG_NBP2_SERVER_PORT);
    /* CONFIG_NBP2_SERVER_HOST é sempre IPv4 literal nesta fatia — sem
     * getaddrinfo/DNS de propósito: é bem mais pesado em pilha (já causou
     * stack overflow real em bancada) e hostname/mDNS não é escopo de
     * S1.7. */
    if (inet_pton(AF_INET, CONFIG_NBP2_SERVER_HOST, &dest_addr.sin_addr) != 1) {
        ESP_LOGW(TAG, "CONFIG_NBP2_SERVER_HOST '%s' nao e um IPv4 valido", CONFIG_NBP2_SERVER_HOST);
        return -1;
    }

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
        ESP_LOGW(TAG, "connect a %s:%u falhou: errno %d", CONFIG_NBP2_SERVER_HOST,
                 (unsigned)CONFIG_NBP2_SERVER_PORT, errno);
        close(sock);
        return -1;
    }

    /* Não-bloqueante em vez de SO_RCVTIMEO: já vimos em bancada a task
     * mind_link travar para sempre dentro de recv() depois do servidor
     * ficar em silêncio por um tempo — SO_RCVTIMEO não é garantia tão
     * robusta quanto simplesmente nunca bloquear e tratar EAGAIN/EWOULDBLOCK
     * no loop principal (que já fazíamos, mas dependendo do timeout
     * realmente funcionar para ser alcançado). */
    flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        ESP_LOGW(TAG, "fcntl O_NONBLOCK falhou: errno %d", errno);
        close(sock);
        return -1;
    }

    return sock;
}

/* Buffers de envio ficam static de propósito: HELLO/HEARTBEAT são
 * mensagens pequenas, mas empilhar dois buffers deste tamanho na pilha da
 * task (send_hello chama send_frame, os dois ficam vivos ao mesmo tempo) já
 * causou stack overflow real em bancada — ver nb_mind_link_shell_task. Só
 * a task chama estas funções, sequencialmente, então static é seguro aqui. */
#define NB_MIND_LINK_SEND_BUF_CAP 256u

static bool nb_mind_link_shell_send_frame(int sock, uint16_t type, uint32_t seq,
                                          const uint8_t *payload, uint16_t payload_len)
{
    static uint8_t frame[NB_MIND_LINK_SEND_BUF_CAP];
    size_t frame_len;

    if (nbp2_encode_frame(type, seq, payload, payload_len, frame, sizeof(frame), &frame_len) !=
        NBP2_OK) {
        return false;
    }
    return send(sock, frame, frame_len, 0) == (ssize_t)frame_len;
}

static bool nb_mind_link_shell_send_hello(int sock, uint32_t boot_id)
{
    nbp2_msg_hello_t hello;
    static uint8_t payload[NB_MIND_LINK_SEND_BUF_CAP];
    size_t payload_len;

    memset(&hello, 0, sizeof(hello));
    hello.proto_major = NBP2_PROTO_MAJOR;
    hello.proto_minor = NBP2_PROTO_MINOR;
    hello.boot_id = boot_id;
    hello.caps = 0u;
    hello.token_len = (uint16_t)nb_mind_link_token_shell_get(hello.token, sizeof(hello.token));

    if (nbp2_encode_hello(&hello, payload, sizeof(payload), &payload_len) != NBP2_OK) {
        return false;
    }
    return nb_mind_link_shell_send_frame(sock, NBP2_MSG_HELLO, NB_MIND_LINK_HELLO_SEQ, payload,
                                         (uint16_t)payload_len);
}

static bool nb_mind_link_shell_send_heartbeat(int sock, uint32_t counter, uint32_t now_ms)
{
    nbp2_msg_heartbeat_t heartbeat;
    uint8_t payload[32];
    size_t payload_len;

    memset(&heartbeat, 0, sizeof(heartbeat));
    heartbeat.counter = counter;
    heartbeat.uptime_ms = now_ms;

    if (nbp2_encode_heartbeat(&heartbeat, payload, sizeof(payload), &payload_len) != NBP2_OK) {
        return false;
    }
    return nb_mind_link_shell_send_frame(sock, NBP2_MSG_HEARTBEAT, counter, payload,
                                         (uint16_t)payload_len);
}

static bool nb_mind_link_shell_send_timer_fired(int sock, uint32_t timer_id, uint32_t seq)
{
    nbp2_msg_event_timer_fired_t event;
    uint8_t payload[32];
    size_t payload_len;

    event.timer_id = timer_id;

    if (nbp2_encode_event_timer_fired(&event, payload, sizeof(payload), &payload_len) != NBP2_OK) {
        return false;
    }
    return nb_mind_link_shell_send_frame(sock, NBP2_MSG_EVENT_TIMER_FIRED, seq, payload,
                                         (uint16_t)payload_len);
}

/* Extrai um frame completo do acumulador, se houver. Descarta byte a byte
 * até achar um SOF válido (resync) e descarta o frame inteiro em caso de
 * CRC ruim ou tamanho absurdo, para nunca travar em lixo de socket. */
static bool nb_mind_link_frame_buf_try_extract(nb_mind_link_frame_buf_t *buf,
                                               nb_mind_link_parsed_frame_t *out)
{
    uint16_t payload_len;
    size_t total_len;
    nbp2_frame_view_t view;

    if (buf->len < NBP2_FRAME_OVERHEAD) {
        return false;
    }
    if (buf->data[0] != NBP2_SOF) {
        memmove(buf->data, buf->data + 1, buf->len - 1u);
        buf->len -= 1u;
        return false;
    }

    payload_len = (uint16_t)buf->data[1] | ((uint16_t)buf->data[2] << 8u);
    total_len = NBP2_FRAME_OVERHEAD + (size_t)payload_len;
    if (payload_len > NBP2_MAX_CTRL_PAYLOAD || total_len > sizeof(buf->data)) {
        buf->len = 0u;
        return false;
    }
    if (buf->len < total_len) {
        return false;
    }

    if (nbp2_decode_frame(buf->data, total_len, &view) != NBP2_OK) {
        memmove(buf->data, buf->data + total_len, buf->len - total_len);
        buf->len -= total_len;
        return false;
    }

    out->type = view.type;
    out->seq = view.seq;
    out->payload_len = view.payload_len;
    memcpy(out->payload, view.payload, view.payload_len);

    memmove(buf->data, buf->data + total_len, buf->len - total_len);
    buf->len -= total_len;
    return true;
}

static void nb_mind_link_shell_handle_frame(const nb_mind_link_parsed_frame_t *frame,
                                            uint32_t now_ms)
{
    switch (frame->type) {
    case NBP2_MSG_HELLO_ACK: {
        nbp2_msg_hello_ack_t ack;

        if (nbp2_decode_hello_ack(frame->payload, frame->payload_len, &ack) == NBP2_OK) {
            if (nb_mind_link_session_on_hello_ack(&s_session, now_ms, ack.boot_id)) {
                ESP_LOGI(TAG, "sessao READY (boot_id=%u)", (unsigned)ack.boot_id);
            } else {
                ESP_LOGW(TAG, "HELLO_ACK com boot_id inesperado; sessao encerrada");
            }
        }
        break;
    }
    case NBP2_MSG_HEARTBEAT:
        nb_mind_link_session_on_heartbeat_received(&s_session, now_ms);
        break;
    case NBP2_MSG_TIME_SYNC: {
        nbp2_msg_time_sync_t ts;

        if (nbp2_decode_time_sync(frame->payload, frame->payload_len, &ts) == NBP2_OK) {
            /* server_mono_ms (compensação de RTT) fica pra quando isso
             * importar de verdade -- por ora o payload leva só unix_ms.
             * Publica no bus em vez de chamar circadian_core (L4) direto
             * -- mind_link é L3, cross-layer não-adjacente vai por bus
             * (ARCHITECTURE.md §2, S3.4). */
            nb_event_t bus_event = {
                .type = NB_EVENT_TYPE_TIME_SYNC,
                .priority = NB_EVENT_PRIORITY_NORMAL,
                .timestamp_ms = now_ms,
                .payload_len = (uint8_t)sizeof(ts.unix_ms),
            };
            memcpy(bus_event.payload, &ts.unix_ms, sizeof(ts.unix_ms));
            nb_event_bus_shell_publish(&bus_event);
        }
        break;
    }
    case NBP2_MSG_TIMER_SET: {
        nbp2_msg_timer_set_t ts;

        if (nbp2_decode_timer_set(frame->payload, frame->payload_len, &ts) == NBP2_OK) {
            /* Rótulo não cabe nos 16 bytes do payload do bus (fica vazio
             * pro timer criado remotamente, README de schedule_core).
             * Publica no bus em vez de chamar schedule_core (L4) direto
             * -- mesma regra do TIME_SYNC (S3.4). */
            nb_event_t bus_event = {
                .type = NB_EVENT_TYPE_TIMER,
                .priority = NB_EVENT_PRIORITY_NORMAL,
                .timestamp_ms = now_ms,
                .payload_len = (uint8_t)sizeof(nb_schedule_event_payload_t),
            };
            nb_schedule_event_payload_t sched_payload = {
                .fire_at_unix_ms = ts.fire_at_unix_ms,
                .timer_id = ts.timer_id,
                .action = NB_SCHEDULE_EVENT_ACTION_SET,
            };
            memcpy(bus_event.payload, &sched_payload, sizeof(sched_payload));
            nb_event_bus_shell_publish(&bus_event);
        }
        break;
    }
    case NBP2_MSG_TIMER_CANCEL: {
        nbp2_msg_timer_cancel_t tc;

        if (nbp2_decode_timer_cancel(frame->payload, frame->payload_len, &tc) == NBP2_OK) {
            nb_event_t bus_event = {
                .type = NB_EVENT_TYPE_TIMER,
                .priority = NB_EVENT_PRIORITY_NORMAL,
                .timestamp_ms = now_ms,
                .payload_len = (uint8_t)sizeof(nb_schedule_event_payload_t),
            };
            nb_schedule_event_payload_t sched_payload = {
                .fire_at_unix_ms = 0u,
                .timer_id = tc.timer_id,
                .action = NB_SCHEDULE_EVENT_ACTION_CANCEL,
            };
            memcpy(bus_event.payload, &sched_payload, sizeof(sched_payload));
            nb_event_bus_shell_publish(&bus_event);
        }
        break;
    }
    default:
        ESP_LOGD(TAG, "frame tipo 0x%04x ignorado", (unsigned)frame->type);
        break;
    }
}

void nb_mind_link_shell_notify_timer_fired(uint32_t timer_id)
{
    s_pending_timer_fired_id = timer_id;
    s_pending_timer_fired = true;
}

static void nb_mind_link_shell_task(void *arg)
{
    /* static, não na pilha: NB_MIND_LINK_FRAME_BUF_CAP (~4.1 KB) não cabe
     * com folga na pilha de 6 KB da task — já causou stack overflow com
     * corrupção de memória (Cache error / InstrFetchProhibited aleatórios)
     * em bancada real antes desta correção. */
    static nb_mind_link_frame_buf_t buf;
    uint32_t heartbeat_counter;

    (void)arg;

    for (;;) {
        int sock = nb_mind_link_shell_connect();
        uint32_t boot_id;

        if (sock < 0) {
            nb_mind_link_session_on_disconnected(&s_session);
            vTaskDelay(pdMS_TO_TICKS(nb_mind_link_backoff_delay_ms(
                nb_mind_link_session_get_reconnect_attempts(&s_session))));
            continue;
        }

        boot_id = esp_random();
        heartbeat_counter = 0u;
        buf.len = 0u;

        nb_mind_link_session_on_connected(&s_session, nb_mind_link_shell_now_ms(), boot_id);

        if (!nb_mind_link_shell_send_hello(sock, boot_id)) {
            ESP_LOGW(TAG, "falha ao enviar HELLO");
            close(sock);
            nb_mind_link_session_on_disconnected(&s_session);
            vTaskDelay(pdMS_TO_TICKS(nb_mind_link_backoff_delay_ms(
                nb_mind_link_session_get_reconnect_attempts(&s_session))));
            continue;
        }
        ESP_LOGI(TAG, "HELLO enviado (boot_id=%u), aguardando HELLO_ACK", (unsigned)boot_id);

        while (nb_mind_link_session_get_state(&s_session) != NB_MIND_LINK_STATE_DEAD) {
            uint32_t now_ms = nb_mind_link_shell_now_ms();
            ssize_t n;
            /* static pelo mesmo motivo de `buf` acima: ~4 KB não cabe na
             * pilha da task. */
            static nb_mind_link_parsed_frame_t frame;

            if (nb_mind_link_session_tick(&s_session, now_ms)) {
                if (!nb_mind_link_shell_send_heartbeat(sock, ++heartbeat_counter, now_ms)) {
                    ESP_LOGW(TAG, "falha ao enviar HEARTBEAT");
                    nb_mind_link_session_on_disconnected(&s_session);
                    break;
                }
            }

            /* S3.5: EVENT_TIMER_FIRED é fire-and-forget -- sem READY, o
             * reflexo local já disparou de qualquer jeito (schedule_core_
             * shell/reflex_engine_shell não dependem do envio pro server),
             * então só descarta o pendente sem tentar reenviar depois
             * (protocolo não define backlog/reenvio pra este evento). */
            if (s_pending_timer_fired) {
                const uint32_t timer_id = s_pending_timer_fired_id;
                s_pending_timer_fired = false;
                if (nb_mind_link_session_get_state(&s_session) == NB_MIND_LINK_STATE_READY) {
                    if (nb_mind_link_shell_send_timer_fired(sock, timer_id, ++heartbeat_counter)) {
                        ESP_LOGI(TAG, "EVENT_TIMER_FIRED enviado -- id=%u", (unsigned)timer_id);
                    } else {
                        ESP_LOGW(TAG, "falha ao enviar EVENT_TIMER_FIRED -- id=%u", (unsigned)timer_id);
                    }
                }
            }

            n = recv(sock, buf.data + buf.len, sizeof(buf.data) - buf.len, 0);
            if (n > 0) {
                buf.len += (size_t)n;
            } else if (n == 0) {
                ESP_LOGW(TAG, "server fechou a conexao");
                nb_mind_link_session_on_disconnected(&s_session);
                break;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* Socket não-bloqueante: nada chegou ainda. Dorme um pouco
                 * antes de tentar de novo, senão o loop gira sem parar. */
                vTaskDelay(pdMS_TO_TICKS(NB_MIND_LINK_RECV_TIMEOUT_MS));
            } else {
                ESP_LOGW(TAG, "recv falhou: errno %d", errno);
                nb_mind_link_session_on_disconnected(&s_session);
                break;
            }

            while (nb_mind_link_frame_buf_try_extract(&buf, &frame)) {
                nb_mind_link_shell_handle_frame(&frame, now_ms);
            }
        }

        close(sock);
        ESP_LOGW(TAG, "sessao morta, reconectando em %u ms",
                 (unsigned)nb_mind_link_backoff_delay_ms(
                     nb_mind_link_session_get_reconnect_attempts(&s_session)));
        vTaskDelay(pdMS_TO_TICKS(nb_mind_link_backoff_delay_ms(
            nb_mind_link_session_get_reconnect_attempts(&s_session))));
    }
}

esp_err_t nb_mind_link_shell_init(void)
{
    esp_err_t err = nb_mind_link_token_shell_init();

    if (err != ESP_OK) {
        return err;
    }

    nb_mind_link_session_init(&s_session);

    if (xTaskCreate(nb_mind_link_shell_task, "mind_link", NB_MIND_LINK_TASK_STACK, NULL,
                    NB_MIND_LINK_TASK_PRIO, &s_task_handle) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

nb_mind_link_state_t nb_mind_link_shell_get_state(void)
{
    return nb_mind_link_session_get_state(&s_session);
}
