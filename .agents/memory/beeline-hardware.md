---
name: Diagnóstico físico BeeLine
description: Limites de conclusões sobre o display e alterações que exigem confirmação na placa.
---

Separar melhorias de robustez e desempenho do firmware da solução, ainda não confirmada, de orientação e ruído no display físico.

**Why:** O usuário definiu explicitamente que otimizações não devem ser apresentadas como solução de orientação, ruído ou pinagem.

**How to apply:** Identificar a revisão exata do hardware antes de trocar biblioteca ou driver. Não adotar buffer completo ou double buffering sem medir memória disponível com BLE ativo. Relatar separadamente verificações locais e verificações que dependem da placa e do telefone.
