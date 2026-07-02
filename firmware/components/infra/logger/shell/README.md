# logger shell

A casca imperativa do `logger` será a dona de:

- serializar chamadas concorrentes ao núcleo;
- manter o watermark do worker SD;
- fazer flush assíncrono para FATFS quando S0.3 liberar SD na árvore real;
- acionar dump do ring nos caminhos de shutdown e panic.

Ela não nasce com I/O falso: S1.3 permanece `EM ANDAMENTO` até o gate de
bancada comprovar coredump legível e dump do ring.
