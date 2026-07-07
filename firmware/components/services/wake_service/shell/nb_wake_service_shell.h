#ifndef NB_WAKE_SERVICE_SHELL_H
#define NB_WAKE_SERVICE_SHELL_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "event_bus.h"
#include "wake_service.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*nb_wake_service_shell_voice_sink_t)(const nb_wake_output_t *out,
                                                   const int16_t *pcm, uint32_t samples,
                                                   float wake_score, void *ctx);

/*
 * Casca mínima do wake_service (S4.2): dona da instância única, publica os
 * eventos de voz no event_bus e deixa os produtores reais (WakeNet/ESP-SR,
 * áudio de sessão) entrarem por API explícita. Nesta fatia ainda não cria
 * task própria nem lê hardware direto.
 */

esp_err_t nb_wake_service_shell_init(void);

void nb_wake_service_shell_set_vad_available(bool available);
void nb_wake_service_shell_set_routes(bool mind_connected, bool local_intent_available);
void nb_wake_service_shell_set_voice_sink(nb_wake_service_shell_voice_sink_t sink, void *ctx);

/* Gancho para o futuro produtor de wake word (WakeNet, botão de bancada,
 * harness etc.). score entra em [0,1] só pra telemetria/evento. */
void nb_wake_service_shell_trigger_wake(float score);

/* Gancho para o futuro produtor de áudio/VAD de sessão (ESP-SR). */
void nb_wake_service_shell_on_audio_frame(const int16_t *pcm, bool vad_detected_speech,
                                          uint32_t samples, nb_voice_audio_level_t audio_level);

/* Fecha silêncio/duração máxima e feedback por perda de rota. */
void nb_wake_service_shell_tick(void);

nb_wake_state_t nb_wake_service_shell_get_state(void);

#ifdef __cplusplus
}
#endif

#endif
