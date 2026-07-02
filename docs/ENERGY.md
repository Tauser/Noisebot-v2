# NoiseBot 2 — Energia

Orçamento de energia e regras elétricas. Contexto: o v1 perdeu uma Waveshare
N32R16V por curto 5V↔3V3 — este documento existe para isso nunca se repetir.
Números marcados **[medir]** são estimativas a confirmar em bancada (S0.1 e
S6.1); viram contrato após medição registrada.

## 1. Topologia de alimentação

```
Fonte 5V 3A (RPi4 oficial)
 ├─► Trilho A: Waveshare (USB/5V) → regulador 3V3 onboard
 │     └─ display, mic, LEDs (dado), touch, SD, câmera futura
 ├─► Trilho B (servos): MG90S ×2 (perfil B) ou SCS0009 ×2 via TTLinker (A)
 │     ├─ **GND comum com o trilho A; 5V NUNCA compartilhado com a dev board**
 │     ├─ INA219 em série (shunt) — telemetria de corrente = proxy de stall
 │     └─ MOSFET high-side (enable GPIO3, pull-down) — trilho nasce desligado
 └─► LEDs WS2812 (5V do trilho A com level shift de dado, ou trilho próprio)
```

Regras inegociáveis:

1. Servo nunca puxa corrente do 5V da placa dev — trilho B próprio com fuse.
2. GND único em estrela; retorno de servo não passa pela placa.
3. GPIO47/48 da N32R16V são domínio 1.8V — nunca conectar nada 3.3V.
4. Multímetro antes de energizar qualquer montagem nova; fonte de bancada
   com limite de corrente em todo bring-up.

## 2. Orçamento por consumidor (5V)

| Consumidor | Típico | Pico | Nota |
| --- | --- | --- | --- |
| ESP32-S3 (WiFi ativo) | ~150 mA | ~400 mA [medir] | picos de TX WiFi |
| Display ST7789 (sem BL dedicado) | ~30 mA | ~50 mA | config v1 sem pino de BL |
| MAX98357A + speaker | ~50 mA | ~500 mA [medir] | dependente de volume/carga |
| INMP441 + SD + misc | ~30 mA | ~80 mA | |
| WS2812 ×2 + onboard | ~10 mA | ~120 mA | 60 mA/LED em branco máximo |
| **Subtotal trilho A** | ~270 mA | ~1,1 A | dentro de 3 A com folga |
| Servos ×2 (trilho B, MG90S ou SCS0009) | ~200 mA | **~1,4 A stall (2×0,7 A)** [medir por perfil] | dimensiona fuse do trilho B e o threshold do INA219 |

Total pico teórico simultâneo: ~2,5 A — a fonte de 3 A fecha, mas **stall
duplo + pico de WiFi + volume alto** é exatamente o cenário do gate S6.1.

## 3. Brownout e proteção

- Monitor de 5V no GPIO7 (ADC, divisor 68k/56k — herdado do mapa v1):
  `power_monitor` (L2) publica `POWER_BROWNOUT_WARN` na fila de safety.
- Resposta (herdada do v1, mantida): torque-off imediato dos servos
  (< 150 ms), redução de brilho de LED/display, evento à mente.
- Brownout do próprio S3 (regulador): threshold do bootloader ativo;
  coredump + contador NVS para diagnóstico pós-morte.
- Fuse recomendado: trilho B 2 A rápido; trilho A conforme medição S0.1.

## 4. Gate elétrico S6.1 (bloqueante — checklist assinado)

Antes de qualquer torque no robô montado:

- [ ] Proteção contra inversão no trilho B
- [ ] Fuse por trilho instalado e dimensionado por medição
- [ ] Isolação física 5V↔3V3 verificada com multímetro (a falha que matou a
      placa do v1)
- [ ] GND em estrela confirmado; queda de tensão no retorno < 100 mV sob
      stall [medir]
- [ ] Stall induzido com fonte limitada: brownout detectado → torque-off
      < 150 ms, sem reset do S3
- [ ] Perfil B: INA219 calibrado (baseline de corrente por gesto registrado);
      eixo bloqueado → sobrecorrente detectada → corte do trilho via GPIO3
      < 150 ms
- [ ] Perfil B: trilho B comprovadamente desligado durante boot/reset
      (pull-down do enable segura o MOSFET aberto)
- [ ] Fotos + medições arquivadas em `docs/bringup/`

## 5. Bateria (pós-v2.0)

LiPo + charger + boost + fuel gauge entram por I2C (`HARDWARE.md`), **com
revisão completa deste documento na entrada** — orçamento, brownout e trilhos
mudam. Até lá, o produto é de mesa, alimentado por fonte.
