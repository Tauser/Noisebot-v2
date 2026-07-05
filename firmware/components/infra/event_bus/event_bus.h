#ifndef NB_EVENT_BUS_H
#define NB_EVENT_BUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NB_EVENT_BUS_MAX_SLOTS 32u
#define NB_EVENT_BUS_SAFETY_RESERVED_SLOTS 4u
#define NB_EVENT_BUS_NORMAL_SLOTS \
    (NB_EVENT_BUS_MAX_SLOTS - NB_EVENT_BUS_SAFETY_RESERVED_SLOTS)
#define NB_EVENT_BUS_MAX_PAYLOAD_BYTES 16u
#define NB_EVENT_BUS_AUDIT_CAPACITY 64u

typedef enum {
    NB_EVENT_BUS_OK = 0,
    NB_EVENT_BUS_ERR_INVALID_ARG = 1,
    NB_EVENT_BUS_ERR_PAYLOAD_TOO_LARGE = 2,
    NB_EVENT_BUS_ERR_FULL = 3,
    NB_EVENT_BUS_ERR_EMPTY = 4,
} nb_event_bus_status_t;

typedef enum {
    NB_EVENT_PRIORITY_NORMAL = 0,
    NB_EVENT_PRIORITY_SAFETY = 1,
} nb_event_priority_t;

/* IDs de nb_event_t.type. Reservados desde S3.2 para os produtores
 * previstos em S4/S6 (BEHAVIOR.md §2) mesmo sem shell de entrada ainda --
 * evita rediscutir o enum quando voz/safety/mind-hint chegarem. */
typedef enum {
    NB_EVENT_TYPE_TOUCH = 0,
    NB_EVENT_TYPE_VOICE = 1,
    NB_EVENT_TYPE_SAFETY = 2,
    NB_EVENT_TYPE_MIND_HINT = 3,
    /* S3.4: mind_link_shell (L3) publica TIME_SYNC aqui em vez de chamar
     * circadian_core (L4) direto -- "camada chama só pra baixo", cross-
     * layer não-adjacente sempre via bus (ARCHITECTURE.md §2). */
    NB_EVENT_TYPE_TIME_SYNC = 4,
    /* S3.5: mind_link_shell publica TIMER_SET/TIMER_CANCEL aqui em vez de
     * chamar schedule_core (L4) direto -- mesma regra do TIME_SYNC. */
    NB_EVENT_TYPE_TIMER = 5,
} nb_event_type_t;

/* Payload de NB_EVENT_TYPE_TIMER -- cabe nos 16 bytes de nb_event_t.payload
 * (8+4+4). Definido aqui (L1 infra, não em schedule_core/L4) porque quem
 * publica (mind_link_shell, L3) não pode incluir um header de camada
 * superior só por causa de um tipo de payload -- event_bus é o ponto
 * neutro que os dois lados (mind_link L3, schedule_core/reflex_engine L4)
 * já incluem (S3.5). Rótulo de TIMER_SET remoto não cabe aqui (fica vazio,
 * ver README de schedule_core). */
typedef enum {
    NB_SCHEDULE_EVENT_ACTION_SET = 0,
    NB_SCHEDULE_EVENT_ACTION_CANCEL = 1,
} nb_schedule_event_action_t;

typedef struct {
    uint64_t fire_at_unix_ms; /* só relevante pra SET */
    uint32_t timer_id;
    nb_schedule_event_action_t action;
} nb_schedule_event_payload_t;

typedef enum {
    NB_EVENT_AUDIT_PUBLISHED = 0,
    NB_EVENT_AUDIT_DROPPED = 1,
    NB_EVENT_AUDIT_POLLED = 2,
} nb_event_audit_action_t;

typedef struct {
    uint16_t type;
    nb_event_priority_t priority;
    uint32_t timestamp_ms;
    uint8_t payload_len;
    uint8_t payload[NB_EVENT_BUS_MAX_PAYLOAD_BYTES];
} nb_event_t;

typedef struct {
    nb_event_audit_action_t action;
    nb_event_bus_status_t status;
    uint16_t type;
    nb_event_priority_t priority;
    uint32_t timestamp_ms;
    uint8_t normal_pending;
    uint8_t safety_pending;
} nb_event_audit_entry_t;

typedef struct {
    uint32_t published_normal;
    uint32_t published_safety;
    uint32_t dropped_normal;
    uint32_t dropped_safety;
    uint32_t polled_normal;
    uint32_t polled_safety;
    uint8_t normal_pending;
    uint8_t safety_pending;
    uint8_t free_normal_slots;
    uint8_t free_safety_slots;
} nb_event_bus_stats_t;

typedef struct {
    nb_event_t event;
    bool in_use;
    bool safety_reserved;
} nb_event_slot_t;

typedef struct {
    nb_event_slot_t slots[NB_EVENT_BUS_MAX_SLOTS];
    uint8_t normal_queue[NB_EVENT_BUS_NORMAL_SLOTS];
    uint8_t safety_queue[NB_EVENT_BUS_MAX_SLOTS];
    uint8_t normal_head;
    uint8_t normal_tail;
    uint8_t normal_count;
    uint8_t safety_head;
    uint8_t safety_tail;
    uint8_t safety_count;
    nb_event_audit_entry_t audit[NB_EVENT_BUS_AUDIT_CAPACITY];
    uint8_t audit_head;
    uint8_t audit_count;
    nb_event_bus_stats_t stats;
} nb_event_bus_t;

void nb_event_bus_init(nb_event_bus_t *bus);

nb_event_bus_status_t nb_event_bus_publish(nb_event_bus_t *bus,
                                           const nb_event_t *event);

nb_event_bus_status_t nb_event_bus_poll(nb_event_bus_t *bus,
                                        nb_event_t *out_event);

void nb_event_bus_get_stats(const nb_event_bus_t *bus,
                            nb_event_bus_stats_t *out_stats);

size_t nb_event_bus_copy_audit(const nb_event_bus_t *bus,
                               nb_event_audit_entry_t *out_entries,
                               size_t max_entries);

#ifdef __cplusplus
}
#endif

#endif
