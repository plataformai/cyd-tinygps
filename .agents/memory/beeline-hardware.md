---
name: Diagnóstico físico BeeLine
description: Limites de conclusões sobre o display e alterações que exigem confirmação na placa.
---

Separar melhorias de robustez e desempenho do firmware da solução, ainda não confirmada, de orientação e ruído no display físico.

**Why:** O usuário definiu explicitamente que otimizações não devem ser apresentadas como solução de orientação, ruído ou pinagem.

**How to apply:** Identificar a revisão exata do hardware antes de trocar biblioteca ou driver. Não adotar buffer completo ou double buffering sem medir memória disponível com BLE ativo. Relatar separadamente verificações locais e verificações que dependem da placa e do telefone.

## Variante física apresentada

A placa fotografada é marcada ESP32-2432S028 e possui USB-C e micro-USB. O usuário confirmou que o problema de exibição foi resolvido após aplicar o perfil recomendado para a variante de dois USB: ST7789, ordem BGR e inversão de cores desativada, mantendo os pinos ESP32 existentes.

**Why:** O firmware com a configuração ILI9341 apresentava imagem cortada/espelhada nessa unidade; a confirmação do usuário valida que a configuração de driver do painel era o problema prático, não uma razão para trocar a pinagem.

**How to apply:** Para novas alterações nesta placa específica, preservar o setup que resolveu o display e não voltar ao ILI9341 nem sugerir troca de fios sem evidência nova. Isso não prova que todas as placas ESP32-2432S028 tenham o mesmo controlador.

## Identificação BLE na bancada

O usuário confirmou no nRF Connect o UUID BeeLine correto, mas sem nome BeeLine visível. A confirmação veio após a orientação de conectar a dispositivos N/A; não tratá-la como prova de que o UUID estava no anúncio de rádio.

**Why:** Um serviço GATT correto pode existir mesmo quando o nome ou UUID esperado não aparece no advertising; confundir essas duas observações leva a diagnósticos incorretos do scanner Android.

**How to apply:** Separar UUID anunciado de UUID descoberto após conexão. Não concluir que a placa está ausente só porque aparece como N/A, nem apresentar testes simulados como confirmação da conexão física.

