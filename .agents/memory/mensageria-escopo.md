---
name: Escopo da mensageria
description: Regras do usuário para comparação com Google Maps e evolução das mensagens.
---

A evolução de mensageria tem escopo carro e moto e pt-BR inicialmente, deixando estrutura para outros idiomas. O usuário separou a gestão de textos da etapa posterior de imagens, flechas e ícones, que ele fornecerá.

**Why:** O usuário pediu comparação do comportamento existente com uma referência funcional, para definir um plano antes de enriquecer a mensageria.

**How to apply:** Preservar a referência fornecida sem edições diretas; correções vão em diff separado. Respeitar as marcas [A] oficial, [B] secundária e [C] proposta, registrando a superfície: capacidades do SDK/API não comprovam disponibilidade nas notificações do app Maps. Distinguir verificações locais de validação manual no Android/ESP32.

Durante a evolução textual, reaproveitar as representações visuais existentes quando forem semanticamente compatíveis. Manter uma lista completa de manobras e ícones, distinguindo existentes, parciais e faltantes; desconhecido não deve virar seta de seguir em frente.

**Why:** O usuário solicitou explicitamente esses dois critérios para o plano, deixando novos assets para etapa posterior.

**How to apply:** Avaliar o significado do desenho, não apenas sua disponibilidade; listar separadamente glifo/vetor reaproveitável e imagem dedicada ainda necessária.

O usuário escolheu manter a fonte TTF fornecida como origem dos ícones, em vez de substituí-la pelos PNGs/SVGs do pacote acompanhante.

**Why:** Escolha explícita do usuário ao comparar as duas alternativas de implantação.

**How to apply:** Usar o TTF e seu mapa IcoMoon para definir os desenhos; não trocar silenciosamente a fonte pelos arquivos de imagem. Não tratar uma camada isolada de um ícone composto como uma manobra diferente.

Fotos fornecidas em 2026-11-08 confirmam uma seta gráfica na própria notificação expandida do Android, enquanto os campos de texto capturados contêm apenas orientação contextual, sem verbo de manobra.

**Why:** Não se trata apenas da diferença entre a tela de navegação do Maps e sua notificação. Repetir ajustes de palavras-chave não resolve informação disponível somente como gráfico.

**How to apply:** Investigar a fonte gráfica exposta no Android real antes de prometer reconhecimento. Um ícone extraído pode ser o logo do Maps ou outra manobra; não atribuir significado só por estar disponível.
