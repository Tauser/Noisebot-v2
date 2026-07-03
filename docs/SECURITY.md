# NoiseBot 2 — Segurança

Postura: **dia 1, não programa futuro.** No v1, OTA sem assinatura, token em
`strcmp` e token em log foram diagnosticados como críticos e engavetados como
backlog. Aqui cada item tem fase de entrada no roadmap e gate no CI.

## 1. Modelo de ameaça (escopo honesto)

Produto de LAN doméstica com microfone e câmera. Ameaças em escopo:

1. Ator na LAN sobe firmware malicioso (OTA) → robô vira escuta ambiente.
2. Extração física de segredos da flash (NVS em texto puro).
3. Impersonação do server (robô entrega áudio/JPEG a um endpoint falso).
4. Vazamento de credenciais por log/serial/git.
5. Dados pessoais (voz, imagens, memória) saindo da máquina local.

Fora de escopo (aceito e documentado): adversário com acesso físico
prolongado + equipamento de laboratório; TLS no firmware (mbedTLS ~250 KB SRAM
— inviável; mitigado por LAN local + assinatura de OTA + token).

## 2. Controles e fase de entrada

| Controle | Ameaça | Fase | Gate |
| --- | --- | --- | --- |
| Secure Boot v2 (chave em cofre offline) | 1 | S1.8 | boot recusa imagem não assinada (teste de bancada) |
| Flash encryption (NVS de segredos) | 2 | S1.8 | dump de flash não revela token |
| OTA assinada + anti-rollback | 1 | S1.8 | OTA com imagem adulterada é recusada |
| Token NBP/2 obrigatório, timing-safe, 32 B | 3 | S1.7 | teste: HELLO sem/with token errado → conexão encerrada |
| Provisioning WiFi+token via SoftAP → NVS | 4 | S1.6 | proibido `wifi_creds.h`; secrets-scan no CI |
| Segredo nunca em log de produção | 4 | S1.3 | log de token só sob `CONFIG_NB2_DEV_BUILD` |
| `secrets-scan` no CI | 4 | S1.1 | qualquer achado = vermelho |
| Server: bind local + allowlist + Bearer token timing-safe (herdado v1) | 3/5 | S4.4 | testes do v1 portados |
| Processamento local-first (Whisper/Ollama/Piper na máquina) | 5 | S4 | busca web é opt-in e degradável |
| Anexos/resultados web = dados não confiáveis para a LLM | 5 | S4/S7 | política de prompt herdada do v1 |

## 3. Gestão de chaves

- Chave de Secure Boot/assinatura de OTA: gerada offline, guardada fora do
  repo e da máquina de build cotidiana; backup físico. Perda da chave = placa
  presa na última imagem.
- Token NBP/2: gerado no provisioning (`secrets.token_hex(16)` no server),
  gravado via SoftAP na NVS cifrada do robô e em
  `~/.noisebot2/robot_token` no server (arquivo 0600). Rotação: re-provisionar.
- Chaves de API de LLM (se usadas): somente variáveis de ambiente no server,
  lidas no ponto de uso; nunca em config serializada.

Procedimento S1.8 antes de queimar eFuses:

1. Gerar a chave de Secure Boot/assinatura OTA em uma máquina/offline vault
   fora do repo. O caminho da chave nunca entra em `sdkconfig.defaults`; o
   perfil `firmware/sdkconfig.s1_8_secure.defaults` só documenta as opções
   seguras e a chave entra por ambiente local protegido.
2. Fazer dois backups físicos da chave e registrar onde estão no inventário
   privado. Se todos os backups forem perdidos, a recuperação é: manter a
   última imagem assinada em produção, gerar nova chave apenas para placas
   ainda não provisionadas, e tratar a placa já provisionada como presa nessa
   cadeia de assinatura até substituição física.
3. Antes da ativação irreversível: build assinado, OTA A→B→A em bancada,
   imagem adulterada recusada, e leitura de flash confirmando que token NBP/2
   não aparece em claro.
4. Só depois desses gates: decisão explícita do usuário para habilitar Secure
   Boot v2, anti-rollback e flash encryption na unidade N32R16V.

## 4. Privacidade (promessa de produto)

- Áudio de sessão, snapshots e memória nunca saem da LAN por padrão.
- Dashboard mostra indicadores honestos: mic aberto, câmera ativa, mente
  offline (o robô também mostra no status rail).
- Dados apagáveis: comando de wipe (conversas, faces, memória) no dashboard.

## 5. O que auditar a cada release

Checklist de 5 min no smoke: nenhum token em log; OTA adulterada recusada;
HELLO sem token recusado; `secrets-scan` verde; portas abertas no server ==
{NBP/2, HTTP dashboard} bind local.
