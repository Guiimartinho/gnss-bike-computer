# Changelog

Mudanças relevantes do projeto. Formato inspirado no [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/); a versão do firmware é a de `APP_VERSION_*` em `zephyr_app/include/app_types.h`. Toda mudança entra em **Não lançado** no mesmo commit do código.

## [Não lançado]

Revisão completa de 2026-09-18: análise do legacy e do port, migração para o NCS v3.3.0, correções críticas, testes de host e documentação. Nada foi testado na placa nem no nRF52840-DK.

### Adicionado

- `.gitattributes` (LF no repositório, CRLF nos `.bat`, `hardware/` e os dados de teste de `tools/TDD/` byte a byte) e `.editorconfig`.

### Alterado

- `.gitignore`: builds, caches do clangd, `__pycache__`, `node_modules` e `.claude/settings.local.json`.
