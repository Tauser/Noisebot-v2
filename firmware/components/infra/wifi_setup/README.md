# wifi_setup

Núcleo C17 puro do fluxo de provisioning WiFi (S1.6). Sem FreeRTOS, sem
`esp_*`, sem NVS, sem rede.

Componente chamado `wifi_setup`, não `provisioning` ou `wifi_provisioning`:
o ESP-IDF já tem componentes internos `wifi_provisioning` e `protocomm` — a
casca deste componente é justamente uma casca fina sobre o `wifi_prov_mgr`
do ESP-IDF, e usar nome parecido causaria confusão (e, pelo precedente do
`app_config`, risco real de colisão).

Contrato do núcleo:

- validação de SSID (1–32 bytes) e senha (vazia = rede aberta, ou 8–63
  caracteres ASCII imprimíveis para WPA2-PSK);
- máquina de estados determinística: `NOT_PROVISIONED` → `PROVISIONING` →
  `PROVISIONED`/`FAILED`; `reset` volta para `NOT_PROVISIONED` de qualquer
  estado. `begin` só é aceito partindo de `NOT_PROVISIONED`/`FAILED`;
  `complete`/`fail` só são aceitos durante `PROVISIONING`.

**Ajuste de escopo (ver `docs/ROADMAP.md` §S1.6):** o token NBP/2 não faz
parte deste componente — ele nasce em S1.7 junto do protocolo, onde o
transporte pode ser decidido com o contrato em mãos.

Casca (`shell/nb_wifi_setup_shell.c/.h`): se já há credenciais persistidas
(`wifi_prov_mgr_is_provisioned()`), conecta direto em modo estação. Caso
contrário, sobe SoftAP `NoiseBot2-XXXX` (XXXX = sufixo do MAC) com
`WIFI_PROV_SECURITY_1` (handshake X25519 + PoP, sem TLS) para o app oficial
"ESP SoftAP Provisioning" da Espressif provisionar SSID/senha. Os eventos do
`wifi_prov_mgr` alimentam a máquina de estados do núcleo
(`begin`/`fail`/`PROVISIONED`).

**Limitação conhecida, registrada para não virar surpresa depois:** o PoP
(`nb2setup`) é fixo no firmware, igual em toda a frota — aceitável para
bancada/S1.6, mas errado para produção (deveria ser único por unidade,
impresso num QR/label). Revisitar antes de qualquer deploy real.

**Pendente:** o gate real (provisionar do zero pelo celular sem toolchain)
exige um celular rodando o app oficial contra a placa — não é executável
sem essa etapa manual do usuário.
