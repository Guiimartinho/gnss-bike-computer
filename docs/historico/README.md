# Histórico da documentação

Documentos escritos no início do port (25 e 26 de novembro de 2025), guardados como foram escritos. **Não use como referência:** partes estão desatualizadas ou erradas, e a documentação numerada em [`docs/`](../) os substitui.

| Documento | Substituído por | Por que está desatualizado |
|---|---|---|
| [`00_Firmware_Architecture.md`](2025-11/00_Firmware_Architecture.md) | [04-arquitetura-legacy.md](../04-arquitetura-legacy.md), [06-algoritmos.md](../06-algoritmos.md) | diagramas em texto puro; descreve só o legacy |
| [`01_Code_Review_MISRA.md`](2025-11/01_Code_Review_MISRA.md) | [11-qualidade-misra.md](../11-qualidade-misra.md) | revisão do legacy, sem o estado do port |
| [`02_Build_Environment_Setup.md`](2025-11/02_Build_Environment_Setup.md) | [03-ambiente-build.md](../03-ambiente-build.md) | cita nRF5 SDK 17.1 e GCC 10.3 (o legacy usa SDK 16.0 e GCC 6) e um NCS genérico; não cobre o NCS v3.3.0 instalado |
| [`02_Gap_Analysis_Original_vs_Zephyr.md`](2025-11/02_Gap_Analysis_Original_vs_Zephyr.md) | [10-status-do-port.md](../10-status-do-port.md) | lista como ausentes módulos que foram portados em seguida (Kalman de 3 estados, `vecteur`, `liste_points`, zonas, configurações, crash recovery) |
