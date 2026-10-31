---
name: Diagnóstico físico BeeLine
description: Limites de conclusões sobre o display e alterações que exigem confirmação na placa.
---

Separar melhorias de robustez e desempenho do firmware da solução, ainda não confirmada, de orientação e ruído no display físico.

**Why:** O usuário definiu explicitamente que otimizações não devem ser apresentadas como solução de orientação, ruído ou pinagem.

**How to apply:** Identificar a revisão exata do hardware antes de trocar biblioteca ou driver. Não adotar buffer completo ou double buffering sem medir memória disponível com BLE ativo. Relatar separadamente verificações locais e verificações que dependem da placa e do telefone.

## Variante física apresentada

A foto fornecida pelo usuário mostra uma placa marcada ESP32-2432S028 com dois conectores: USB-C e micro-USB. Não presumir que o nome da placa garanta um controlador ILI9341. O controlador físico permanece não confirmado; ST7789 é uma hipótese fundamentada para teste, não uma identificação definitiva.

**Why:** Há variantes dessa família com controladores diferentes. O perfil comunitário [CYD2USB](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/blob/main/DisplayConfig/CYD2USB/User_Setup.h) usa ST7789; quantidade de portas e dimensões lógicas informadas pelo firmware não identificam sozinhas o chip do painel.

**How to apply:** Distinguir identificação visual, configuração compilada e resultado confirmado na placa. Usar testes controlados e reversíveis, sem apresentar otimizações ou alteração de driver ainda não testada como solução comprovada.
