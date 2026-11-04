# Perguntas e lacunas

## Respondidas pelo código
- Fonte atual: extras textuais do pacote Maps; não SDK/Routes nem bitmap.
- Há seis glifos Android e uma seta geométrica ESP32 com seis orientações; não existem66 imagens de manobras integradas.
- Bússola é rumo de deslocamento GPS; não substituir por orientação física.
- Texto completo não chega hoje como instrução livre ao TFT; apenas manobra/distância/via. Distância restante é independente.

## Decisões antes da implantação, não bloqueios desta análise
1. Aprovar a manutenção de notificações como fonte inicial? Recomendação: sim, sem custo/integração de SDK nesta fase. Alternativa SDK/API exige decisão de produto e avaliação de termos.
2. Aprovar mensagem não classificada em texto sem seta quando a notificação de navegação é identificável? Recomendação: sim; evitar confundir texto bruto com instrução normalizada pelo BeeLine.
3. Prazo de silêncio aceitável em reta longa? Proposta8s de TTL com renovações limitadas pela fonte; idade máxima inicial30s requer captura real antes de fixar.
4. Eventos devem aparecer por quanto tempo e podem interromper manobra iminente? Proposta preservar a manobra e não tornar alerta informativo persistente; definir prioridade/tempo com o usuário.
5. Aceitar exigir firmware v2 para textos/estados ricos? Recomendação: sim; firmware antigo não consegue clear/TTL/unknown seguro.

## Corpus ausente — validação manual
Não há corpus real versionado com campos originais, resultado esperado e versões Maps/Android no repositório examinado. Prints de conversas não equivalem a corpus reproduzível.

Capturar voluntariamente pt-BR em carro e moto: início, reta longa, comuns/leves/fechadas, manter/bifurcação, incorporação, rampas, saída numerada, rotatória ordinal, retorno, chegada, recálculo, GPS ausente, alternativa, incidentes e só-alertas, **quando realmente observáveis**. Não exigir inventar amostras que o Maps não publique.

Anotar modelo/Android/Maps, idioma, modo selecionado, origem/campos, category/channel quando capturáveis, chave pseudonimizada de sessão, tempos, resultado humano e razão. Registrar hash de ícone somente se captura for autorizada e implementada posteriormente; nesta fase null com motivo. Remover endereços pessoais antes de compartilhar. Separar casos reais, sintéticos e reproduções manuais.

Operar em bancada ou com passageiro; nunca interagir com telefone dirigindo. Testar tela apagada, Maps em primeiro plano, encerramento/revogação, morte de processo, BLE perdido e reconectado.

## Lacunas externas
Itens do registro fontes.yaml não foram reverificados. Enum de uma plataforma não comprova correspondência exata na outra. Bits/textos de notificação consumidor não têm contrato demonstrado pelos anexos. Sem dado de circulação, faixa ou modo, conservar desconhecido.

## Hardware e tipografia — validação manual
Tamanho/versões reais de heap, flash, core ESP32 e bibliotecas na máquina do usuário; MTU negociado; fonte TFT usada para acentos; leitura ao sol; latência física e perdas sob carga. Não há métricas obtidas aqui.

## Assets posteriores
Solicitar arquivos/IDs, direitos de uso, dimensões e variantes CW/CCW/lado. Inventário identifica o que falta, não autoriza copiar imagens Google. Setas de saída/retorno não devem ser espelhadas sem verificar semântica.
