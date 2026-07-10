#ifndef NB_CONFIG_H
#define NB_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro do config tipado (S1.4).
 *
 * Sem FreeRTOS, sem esp_*, sem malloc, sem NVS: cache em RAM validado contra
 * uma tabela central de chaves. Os getters/setters são tipados por chave —
 * chamar o setter do tipo errado para uma chave (ex.: `set_i32` numa chave
 * u32) é rejeitado, assim como valor fora da faixa do descritor. Nenhum dos
 * dois altera o valor anterior. `nb_config_get_*` devolve o default da
 * chave enquanto ela nunca foi setada.
 *
 * A casca (persistência real em NVS) não existe ainda — ver README.md deste
 * componente e docs/ROADMAP.md §S1.4.
 */

typedef enum {
    NB_CONFIG_TYPE_U32 = 0,
    NB_CONFIG_TYPE_I32 = 1,
} nb_config_type_t;

/* Fonte única da verdade das chaves de config. Adicionar chave nova aqui e
 * no descritor em config.c — nunca em outro lugar do código. */
typedef enum {
    /* Nível mínimo de log aceito pelo `logger` (valores de nb_log_level_t:
     * 0=DEBUG .. 4=FAULT), persistido para sobreviver a reboot. */
    NB_CONFIG_KEY_LOG_MIN_LEVEL = 0,
    /* Contador de boots consecutivos que falharam em fase crítica; usado
     * pelo `boot_manager` para decidir SAFE_MODE persistente. */
    NB_CONFIG_KEY_BOOT_FAIL_STREAK = 1,
    /* Último horário conhecido (segundos desde epoch Unix), usado pelo
     * `circadian_core` como fallback quando não há TIME_SYNC real ainda
     * (BEHAVIOR.md §5: "contador local + último horário NVS"). */
    NB_CONFIG_KEY_LAST_KNOWN_UNIX_TIME_S = 2,
    /* Volume local de playback (0..100%) aplicado ao áudio SAY_* vindo da
     * mente. */
    NB_CONFIG_KEY_AUDIO_VOLUME_PERCENT = 3,
    /* Override explícito de modo silencioso vindo da mente (0/1), além do
     * quiet_mode circadiano. */
    NB_CONFIG_KEY_QUIET_MODE_ENABLED = 4,
    NB_CONFIG_KEY_COUNT = 5,
} nb_config_key_t;

typedef union {
    uint32_t u32;
    int32_t i32;
} nb_config_value_t;

typedef struct {
    nb_config_key_t key;
    const char *name;
    nb_config_type_t type;
    nb_config_value_t default_value;
    nb_config_value_t min_value;
    nb_config_value_t max_value;
} nb_config_descriptor_t;

typedef struct {
    nb_config_value_t values[NB_CONFIG_KEY_COUNT];
    bool is_set[NB_CONFIG_KEY_COUNT];
} nb_config_t;

/* Reseta o cache: nenhuma chave marcada como setada (get devolve default). */
void nb_config_init(nb_config_t *config);

/* Descritor estático da chave. Retorna NULL se key >= NB_CONFIG_KEY_COUNT. */
const nb_config_descriptor_t *nb_config_get_descriptor(nb_config_key_t key);

bool nb_config_get_u32(const nb_config_t *config, nb_config_key_t key, uint32_t *out_value);
bool nb_config_set_u32(nb_config_t *config, nb_config_key_t key, uint32_t value);

bool nb_config_get_i32(const nb_config_t *config, nb_config_key_t key, int32_t *out_value);
bool nb_config_set_i32(nb_config_t *config, nb_config_key_t key, int32_t value);

#ifdef __cplusplus
}
#endif

#endif
