---
name: Origem da orientação
description: Escolha do usuário para o significado da bússola do PetitGPS.
---

A bússola representa a direção do deslocamento pelo GPS, não a direção física para a qual o celular aponta.

**Why:** O usuário escolheu explicitamente “Deslocamento pelo GPS (recomendado)” ao distinguir essas duas alternativas.

**How to apply:** Usar essa interpretação ao implementar ou revisar a orientação do navegador; não substituir por sensores de orientação do telefone sem nova decisão do usuário.

O último rumo válido deve permanecer temporariamente na RAM até reiniciar a
placa, inclusive ao parar, perder GPS ou desconectar Bluetooth. Não persistir
esse rumo na flash.

**Why:** O usuário pediu a solução mais simples e confirmou manter até reiniciar
também nos cenários de perda de GPS/Bluetooth.

**How to apply:** Não apagar a direção lembrada quando a leitura deixa de ser
atual; distinguir visualmente o último rumo de um rumo novo. Não inventar
direção antes da primeira leitura válida.
