# Google Maps: navegação de carro e moto

Manobras, textos, ícones e regras de interpretação · documento consolidado em 04/11/2026 · versão 1.0 · escopo: automóvel e motocicleta

## 1. Resumo executivo

- **Fonte de verdade para manobras:** o enum `Maneuver` do Navigation SDK do Google (iOS: `GMSNavigationManeuver`, 66 valores de 0 a 65; Android e Flutter usam os mesmos nomes). É o vocabulário mais granular que o Google publica e deve ser a base da família de ícones.
- **Não existe catálogo oficial dos textos das notificações do app Google Maps.** O texto é gerado, localizado e muda com versão, idioma e superfície. O Google publica enums, campos de SDK, descrições de API e rótulos de ajuda. Toda frase de notificação marcada [C] neste documento é proposta de normalização, não texto do Google.
- **Os ícones de manobra são imagens geradas pelo SDK** (bitmap de manobra e de faixa). O Google não publica um pacote de ícones. As setas deste documento são símbolos Unicode auxiliares, só para rotulagem.
- **Moto:** o Google tem modo de viagem próprio (`TWO_WHEELER`) no Routes API e no Navigation SDK, com o Brasil na lista de países suportados (lista atualizada em 28/03/2026). Não há manobras específicas de moto: a taxonomia é a mesma do carro.
- **Arquitetura recomendada:** usar o enum (SDK ou Routes API) como identificador primário. Interpretar texto de notificação apenas como fallback, mapeando para um ID interno próprio, desacoplado do texto do Google.
- **Filtro aplicado aos seis documentos recebidos:** foram removidos transporte público, a pé, bicicleta, Live View, Wear OS, compartilhamento de localização, aeroporto e resumos da Linha do Tempo.

## 2. Níveis de confiança

| Marca | Significado |
|---|---|
| [A] | Google oficial: referência do Google Maps Platform ou Central de Ajuda do Maps |
| [B] | Fonte secundária: parser de código aberto, APK teardown, comunidade. É observação, não contrato |
| [C] | Proposta deste documento ou dos documentos recebidos. Não é dado do Google |

Critério de desempate: dado mais recente e fonte mais próxima do Google. Nesta consolidação foram reverificadas diretamente: a página do enum de manobras do Navigation SDK iOS (atualizada em 27/08/2025), a lista Flutter do mesmo enum, as páginas de Routes API para moto (incluindo a lista de países, de 28/03/2026) e o enum de modos de viagem do Navigation SDK iOS. Itens vindos só dos documentos recebidos ficam sinalizados.

## 3. Fontes e superfícies

| Superfície | O que entrega | Uso recomendado |
|---|---|---|
| Navigation SDK (Android, iOS, Flutter) [A] | Enum de 66 manobras, dados do passo, faixas, estados, ETA, bitmaps | Taxonomia principal |
| Routes API, `navigationInstruction` [A] | `maneuver` com cerca de 21 valores e texto de instrução localizado | Quando só houver rota calculada |
| Directions API legada [A] | 20 valores em `steps[].maneuver` (minúsculas com hífen) e `html_instructions` | Só legado. Para novos projetos está em descontinuação (confirmar no console). O Google orienta exibir o HTML sem fazer parse |
| Central de Ajuda do Maps [A] | Rótulos PT-BR de alertas, incidentes e voz | Literais de produto |
| Notificação do app Google Maps consumidor | Sem contrato público | Fallback, com validação em campo |
| Android Auto e Maps integrado ao carro | Superfícies separadas, com ícones e cards próprios | Não extrapolar para a notificação do celular |

Regra: um enum ou texto documentado numa superfície não prova que a mesma frase, ícone ou notificação exista em outra. Registre sempre a superfície junto do dado.

## 4. Matriz de manobras (Navigation SDK) com equivalências

Os IDs numéricos vêm do enum iOS. No Android e no Flutter, mapeie pelo **nome**, não pelo número. As descrições são as do Google, traduzidas. Colunas Routes API e Directions: equivalência aproximada para quando só houver esses dados.

| ID | Enum | Significado [A] | Routes API | Directions legada | Seta [C] |
|---|---|---|---|---|---|
| 0 | unknown | Manobra desconhecida | UNSPECIFIED | n/d | ? |
| 1 | depart | Ponto de partida da manobra | DEPART | n/d | ● |
| 2 | destination | Chegada ao destino | não confirmado | n/d | ⚑ |
| 3 / 4 | destinationLeft / Right | Chegada, destino à esquerda / direita da via | não confirmado | n/d | ⚑◀ / ▶⚑ |
| 5 | straight | Seguir em frente | STRAIGHT | straight | ↑ |
| 6 / 7 | turnLeft / turnRight | Curva comum em cruzamento | TURN_LEFT / TURN_RIGHT | turn-left / turn-right | ↰ / ↱ |
| 8 / 9 | turnKeepLeft / Right | Manter-se à esquerda / direita quando a via se divide | n/d | keep-left / keep-right | ⇖ / ⇗ |
| 10 / 11 | turnSlightLeft / Right | Curva leve | TURN_SLIGHT_* | turn-slight-* | ↖ / ↗ |
| 12 / 13 | turnSharpLeft / Right | Curva fechada (acentuada) | TURN_SHARP_* | turn-sharp-* | ↙ / ↘ |
| 14 / 15 | turnUTurnClockwise / CounterClockwise | Retorno para o lado oposto da rua (horário / anti-horário) | UTURN_RIGHT / UTURN_LEFT (correspondência por sentido, validar) | uturn-right / uturn-left | ↷ / ↶ |
| 16 / 17 / 18 | mergeUnspecified / Left / Right | A via atual se une a outra (sem lado / esquerda / direita) | MERGE | merge | ⤵ |
| 19 / 20 | forkLeft / forkRight | A via atual encontra outra ligeiramente à esquerda / direita | FORK_LEFT / FORK_RIGHT | fork-left / fork-right | ⑂ |
| 21 a 31 | onRamp… | Entrar em rodovia ou via expressa (tabela 4.1) | RAMP_LEFT / RAMP_RIGHT | ramp-left / ramp-right | ⤴ + seta |
| 32 a 42 | offRamp… | Sair de rodovia ou via expressa (tabela 4.1) | RAMP_LEFT / RAMP_RIGHT | ramp-left / ramp-right | ⤵ + seta |
| 43 a 62 | roundabout… | Rotatória (tabela 4.2) | ROUNDABOUT_LEFT / RIGHT | roundabout-left / right | ⟲ / ⟳ + seta |
| 63 | ferryBoat | Pegar a balsa | FERRY | ferry | ⛴ |
| 64 | ferryTrain | Pegar o trem-balsa | FERRY_TRAIN | ferry-train | 🚆 |
| 65 | nameChange | O nome da rua muda | NAME_CHANGE | n/d | ↑ + nome |

Observações:

- A página do enum no iOS descreve `turnLeft` com o texto “Starting point of the maneuver”, que é um erro de cópia da documentação. O significado correto é curva comum à esquerda, em linha com `turnRight`.
- A Routes API não tem granularidade de rampa (leve, fechada, manter) nem de rotatória (saída, direção interna). Perde informação em relação ao SDK.
- Os valores `keep-left` e `keep-right` existem na Directions legada e no SDK (`turnKeep*`), mas não no enum da Routes API.
- Contagem: o iOS tem 66 valores (0 a 65, incluindo `unknown`). Um dos documentos recebidos fala em 65 constantes no Android, provavelmente sem contar `UNKNOWN`. Não afeta o mapeamento por nome.

### 4.1 Variantes de rampa (onRamp 21 a 31 e offRamp 32 a 42, mesma ordem)

| Variante | ID onRamp | ID offRamp | Significado |
|---|---|---|---|
| Unspecified | 21 | 32 | Entrar / sair de rodovia, sem detalhe de lado |
| Left / Right | 22 / 23 | 33 / 34 | Curva comum à esquerda / direita |
| KeepLeft / KeepRight | 24 / 25 | 35 / 36 | Manter o lado quando a via diverge |
| SlightLeft / SlightRight | 26 / 27 | 37 / 38 | Curva leve |
| SharpLeft / SharpRight | 28 / 29 | 39 / 40 | Curva fechada |
| UTurnClockwise / CounterClockwise | 30 / 31 | 41 / 42 | Retorno para o lado oposto, horário / anti-horário |

### 4.2 Variantes de rotatória (43 a 62, cada uma em Clockwise e CounterClockwise)

| Variante | ID CW / CCW | Significado |
|---|---|---|
| roundabout (entrar) | 43 / 44 | Entrar na rotatória |
| roundaboutStraight | 45 / 46 | Entrar e seguir reto |
| roundaboutLeft | 47 / 48 | Entrar e virar à esquerda |
| roundaboutRight | 49 / 50 | Entrar e virar à direita |
| roundaboutSlightLeft | 51 / 52 | Entrar e curvar levemente à esquerda |
| roundaboutSlightRight | 53 / 54 | Entrar e curvar levemente à direita |
| roundaboutSharpLeft | 55 / 56 | Entrar e curvar de forma fechada à esquerda |
| roundaboutSharpRight | 57 / 58 | Entrar e curvar de forma fechada à direita |
| roundaboutUTurn | 59 / 60 | Entrar e retornar para o lado oposto |
| roundaboutExit | 61 / 62 | Sair da rotatória |

No Brasil a circulação é pela direita e as rotatórias são anti-horárias (CounterClockwise). Não fixe isso no código: use o dado de lado de condução do SDK (`GMSNavigationDrivingSide` no iOS).

## 5. Dados do passo que acompanham a manobra

O Navigation SDK entrega mais do que o enum. Estes campos permitem montar a tela sem depender de texto. Os nomes de classe `GMSNavigationStepInfo`, `GMSNavigationLane`, `GMSNavigationLaneDirection`, `GMSNavigationTimeAndDistance`, `GMSNavigationManeuverImageSize` e `GMSNavigationDrivingSide` foram conferidos na referência do iOS. Os campos marcados com asterisco vieram dos documentos recebidos, que citam páginas do Google (StepInfo e feed turn-by-turn), e não foram reabertos nesta consolidação.

| Dado | Uso na tela |
|---|---|
| Manobra (enum) | Escolhe o ícone |
| Bitmap de manobra gerado pelo SDK* | Alternativa ao ícone próprio (ver restrições na seção 9) |
| Texto completo da instrução (`fullInstructionText`)* | Texto de apoio; não depender dele para lógica |
| Nome completo da via* | Rótulo da próxima via |
| Distância e tempo até o próximo passo* | Contagem regressiva da manobra |
| Número da saída da rotatória (`roundaboutTurnNumber`)* | Ordinal sobre o ícone de rotatória |
| Número da saída de rodovia (`exitNumber`)* | Placa de saída |
| Faixas: direções, faixa recomendada (`isRecommended`) e bitmap* | Guia de faixa |
| ETA, tempo e distância restantes* | Rodapé da navegação |
| Estado: `ENROUTE`, `REROUTING`, `STOPPED`* | Alterna entre manobra, “recalculando” e fim |

Exemplos literais em inglês publicados no StepInfo*: “Turn left onto Lean Ave”, “Use the right lane to take the CA-85 ramp to Mtn View” e “Use the 2nd from the right lane to take exit 9A toward North First Street”. São exemplos de SDK, não prova da frase do app consumidor.

## 6. Textos em português

### 6.1 Literais oficiais PT-BR [A]

São textos reais do Google, mas de superfícies específicas. Não os trate como corpo de notificação.

| Texto | Onde aparece |
|---|---|
| “Vire à esquerda.” / “Siga em frente.” / “Pegue a bifurcação à esquerda.” / “Entre no trânsito.” / “Vire à esquerda na rotatória.” / “Manobra inicial.” | Descrições localizadas dos enums da Routes API* |
| “Traçando novo trajeto…” | Documentação do feed turn-by-turn do SDK (em inglês: “Rerouting...”)* |
| “Pesquisando GPS” | Ajuda do Maps, estado de busca de sinal |
| “Ouvir alertas” / “Você não ouvirá a navegação guiada” | Ajuda do Maps, modo de voz somente com alertas |
| Exemplo: “Vire à esquerda na Innes Avenue” | Directions legada, texto HTML de instrução* |

### 6.2 Dicionário de frases proposto [C]

Todas as frases abaixo são **não literais**, para classificação e testes. Colchetes indicam variáveis.

| Família (enum) | Padrões de frase | Palavras-chave |
|---|---|---|
| Curva comum | Vire à esquerda/direita na [via] | vire |
| Curva leve | Vire levemente à esquerda/direita | leve, ligeiramente |
| Curva fechada | Faça uma curva fechada/acentuada à esquerda/direita | fechada, acentuada |
| Manter | Mantenha-se à esquerda/direita | mantenha-se |
| Bifurcação | Pegue a bifurcação à esquerda/direita | bifurcação, ramificação |
| Incorporação | Entre no trânsito; incorpore-se à via | incorpore, entre no trânsito |
| Reto | Siga em frente; continue na [via] | siga, continue |
| Retorno | Faça o retorno | retorno, retorne, inversão |
| Entrar em via expressa | Entre na rodovia/via expressa pela esquerda/direita | rampa, alça, entre na rodovia |
| Sair de via expressa | Pegue a saída [n]; saia à direita | saída, saia |
| Rotatória | Na rotatória, pegue a [n]ª saída | rotatória, saída |
| Chegada | Você chegou ao seu destino; o destino fica à direita | chegou, destino |
| Mudança de nome | A rua muda de nome para [via] | muda de nome |
| Balsa | Pegue a balsa | balsa |
| Recálculo | Recalculando a rota… / Traçando novo trajeto… | recalculando, novo trajeto |
| Sem GPS | Pesquisando GPS | GPS |

### 6.3 Regras de classificação [C]

Aplique nesta ordem. Pare na primeira família que casar e só então extraia lado e intensidade.

1. **Estado:** “recalculando”, “traçando novo trajeto” ou “rerouting” indicam recálculo. “Pesquisando GPS” indica perda de sinal.
2. **Chegada:** “chegou” ou “destino”. O lado (“à esquerda/direita”) é parâmetro do destino, não curva.
3. **Rotatória:** “rotatória” ou “roundabout”. Extrair o ordinal da saída.
4. **Retorno:** “retorno”, “retorne”, “inversão” ou “u-turn”.
5. **Rampa ou saída:** “saída”, “saia”, “rampa”, “alça”, “entre na rodovia”.
6. **Bifurcação ou manter:** “mantenha-se”, “bifurcação”.
7. **Incorporação:** “incorpore”, “entre no trânsito”.
8. **Curva:** “leve” ou “ligeiramente” (leve), “fechada” ou “acentuada” (fechada); sem qualificador, curva comum.
9. **Reto:** “siga em frente”, “continue”.
10. **Mudança de nome:** “muda de nome”.

Cuidados:

- A palavra “direita” ou “esquerda” sozinha **não** significa curva. Ela aparece também em saída, faixa, bifurcação e posição do destino.
- Se só houver uma seta ou enum sem texto, classifique como manobra não textual. Não invente “vire”, “saída” ou “retorno”.
- Distância (aceita vírgula decimal e unidades locais):

```
(\d+(?:[.,]\d+)?)\s*(m|km|ft|mi)\b
```

- Nome da via: não extraia por regex de maiúsculas. Um padrão como `[A-Z0-9\.\s-]+` falha com minúsculas e acentos (“Rua das Flores”, “Av. São João”). Use o campo estruturado do SDK ou trate o que vier depois de “na/no/em/para” como melhor esforço.
- Limiar de manobra iminente (por exemplo, 50 m) é regra de design do seu app, não do Google.

## 7. Eventos de via durante a navegação (carro e moto)

Rótulos PT-BR da Ajuda do Google Maps integrado ao carro [A], reaproveitados em outras superfícies. O Google não publica mapeamento de ícone por tipo de incidente, nem a frase final do alerta.

| Evento | Rótulo PT-BR [A] | Observação |
|---|---|---|
| Trânsito lento | “Ponto de lentidão”; “Atrasos devido ao trânsito no caminho…” | Indica atraso, não a causa |
| Acidente | “Acidente” | Não indica gravidade nem bloqueio |
| Obras | “Obras” | Não equivale a fechamento total |
| Faixa interditada | “Faixa interditada” | Diferente de orientação de faixa |
| Objeto na via | “Objeto na via” | Denúncia colaborativa |
| Radar móvel | “Radar de velocidade móvel” | Não é confirmação oficial de fiscalização |
| Polícia | “Polícia” | Idem |
| Via inundada | “Via inundada” | |
| Baixa visibilidade | “Baixa visibilidade” | |
| Neve na pista | “Neve na pista” | Pouco relevante no Brasil |
| Confirmação | “Ainda está aqui?” | Voto de confirmação do motorista |
| Via interditada | “Vias interditadas”, “Interdições” (categoria de notificações do Maps) | Aviso de trajeto frequente, não necessariamente recálculo |
| Rota melhor | “Melhor trajeto”, “trajetos alternativos” | Critério de tempo, não adoção automática |
| Modo só alertas | “Ouvir alertas”: fala trânsito, obras e acidentes, não a navegação guiada | Voz é canal distinto de notificação |

Ressalvas de versão: segundo as notas de versão do Navigation SDK para iOS, citadas num dos documentos recebidos, os callouts de lentidão e de veículo parado foram desativados em julho de 2025. Não trate “Veículo parado” como rótulo vigente. Versione a taxonomia de incidentes.

## 8. A notificação do app Google Maps

**O que é público [A]:** a Central de Ajuda descreve categorias e opções de notificação, mas não publica o corpo das notificações de manobra. Também não há evidência oficial de que cada curva gere uma notificação Android separada. Voz, card sobre o mapa, notificação persistente, heads-up e Android Auto são canais distintos.

**Comportamento do SDK [A, via documentos recebidos]:** o Navigation SDK usa serviço em primeiro plano com notificação persistente (obrigatória a partir do Android 8). A notificação heads-up vem habilitada por padrão e pode ser desligada. Ela permanece com a Activity fechada e some na chegada. O corpo e o ícone dependem do integrador. Isso descreve apps construídos com o SDK, não necessariamente o app Google Maps.

**Formato observado no app consumidor [B]:** um dos documentos relata, com base em parsers de código aberto, título no padrão `[distância] · [instrução]` (por exemplo “60 m · Turn left onto ADM Rd”) e corpo com tempo restante, distância e ETA, categoria de navegação e a seta como imagem dinâmica, não como texto. Não foi possível confirmar isso em fonte do Google. Trate como hipótese a validar em dispositivo.

**Estratégia recomendada [C]:**

1. Se o app puder usar Navigation SDK ou Routes API, use o enum e ignore o texto da notificação.
2. Se for inevitável ler a notificação do Maps (permissão de acesso a notificações): capture título, texto, subtexto e o bitmap do ícone; classifique pelo texto (seção 6.3) e confirme pelo bitmap, comparando com um catálogo de referência gerado em campo.
3. Mantenha um corpus real de notificações em pt-BR (várias versões do app e do Android) como base de testes. O texto muda sem aviso.
4. Em dúvida, aplique o fallback conservador: manobra não textual, sem inventar distância, urgência ou causa.

Estrutura interna sugerida para o resultado do interpretador [C]:

```json
{
  "channel": "notification",
  "family": "ROUNDABOUT",
  "maneuver": "roundaboutCounterClockwise",
  "side": null,
  "exit_number": 2,
  "distance_m": 200,
  "street": "Av. Brasil",
  "evidence": "text+bitmap",
  "is_literal_google": false,
  "confidence": 0.8
}
```

## 9. Família de ícones

**Gramática [C]:** `[contexto] + [direção] + [variante]`, em IDs de arquivo como `turn_slight_right`, `on_ramp_keep_left`, `roundabout_left_ccw`, `off_ramp_uturn_cw`.

- Contexto: straight, turn, keep, fork, merge, on_ramp, off_ramp, roundabout, uturn, ferry, depart, destination, name_change.
- Direção (eixo de intensidade): reto, leve, normal, fechada, retorno. Lado: esquerda ou direita.
- Variante: sentido horário ou anti-horário (rampa em retorno e rotatória).

**Economia de desenho:** com espelhamento esquerda/direita e horário/anti-horário, os 66 valores se reduzem a cerca de 35 formas-base. Compondo rampas e rotatórias a partir de um glifo de via mais uma seta de curva, reduz-se mais. No Brasil, as variantes anti-horárias e de curva à esquerda no retorno são as mais frequentes, mas desenhe as duas famílias.

**Estados do app além das manobras [C]:** chegada, recálculo, sem GPS, trânsito, incidente (por tipo), via interditada e rota alternativa.

**Origem visual:**

- O Google não publica pacote de ícones de manobra. O SDK gera `maneuverBitmap` e `laneBitmap`. Antes de reutilizar esses bitmaps na sua interface, confira os termos da Google Maps Platform sobre uso e armazenamento de conteúdo.
- Não copie a aparência do Google Maps. Desenhe uma família própria, em SVG, com grade, espessura de traço e área de proteção padronizadas.
- Um dos documentos sugere nomes do Material Symbols (por exemplo `turn_left`, `turn_slight_left`, `u_turn_left`, `fork_left`, `roundabout_left`) como ponto de partida. A afirmação de que o Google Maps usa esses ícones nas notificações **não tem evidência**. Como base de uma família própria é uma opção razoável, mas verifique a existência de cada nome e a licença.
- Para moto, o enum de manobra não muda. Qualquer diferenciação visual (por exemplo, pictograma de moto no marcador de posição) é decisão de interface sua [C].

## 10. Moto

- [A] `TWO_WHEELER` é o modo de viagem para motocicletas e veículos motorizados de duas rodas, distinto de bicicleta. A cobrança é maior que a dos outros modos. As rotas de duas rodas estão em beta e podem não incluir o caminho ideal para motos, e o Google pede que se exiba um aviso ao usuário.
- [A] O Brasil consta na lista de países com rotas de duas rodas suportadas (página atualizada em 28/03/2026).
- [A] No Navigation SDK para iOS, `GMSNavigationTravelMode` tem driving (0), cycling (1), walking (2), twoWheeler (3) e taxicab (4, experimental). Em driving e twoWheeler, o curso do usuário é baseado na direção do movimento. Um documento recebido cita `TWO_WHEELER` também no `RoutingOptions.TravelMode` do Android: confirmar na referência do Android antes de depender disso.
- Não foi publicada taxonomia de manobras específica para moto. Rotas de moto e de carro podem diferir em trajeto e tempo, mas usam as mesmas manobras.
- Se o app precisar do modo moto no app Google Maps consumidor (e não via API), isso **não foi verificado** e deve ser testado em campo.

## 11. Reconciliação dos documentos recebidos

| Origem | Afirmação | Veredito | Tratamento |
|---|---|---|---|
| Gemini | Formatos de notificação como “Em 200 m, vire à esquerda na [via]” apresentados como padrão do Google Maps | Não verificável | Mantido só como template [C] |
| Gemini | Ícones das notificações seguem o Material Design | Sem evidência | Rejeitado como fato; Material Symbols fica como opção de base |
| Gemini | Classes “ManeuverType” e “NotificationContentProvider” do Navigation SDK | Não localizadas; o enum real chama-se `Maneuver` | Rejeitado |
| Gemini | Manobra iminente quando distância ≤ 50 m; regex de nome de via com maiúsculas | Regra de design; regex falha com minúsculas e acentos | Limiar como [C]; regex descartado |
| Grok | Título `distância · instrução` e corpo com ETA, ícone como bitmap | Plausível, fonte secundária | Mantido como [B] a validar |
| Grok | STRAIGHT e DEPART como a mesma manobra; nomes `KEEP_LEFT`, `TURN_U_TURN_*` | Errado: são valores distintos; no SDK é `TURN_KEEP_LEFT` | Corrigido pela matriz oficial |
| Grok | Live Updates no Android 16, alerta “Part of route may not be drivable” (restrições de veículo) | Não verificado; este último vem de APK teardown | Não incorporado; reavaliar com teste em campo |
| Perplexity | Lista do enum do Navigation SDK, 20 constantes de rotatória, 20 valores da Directions legada | Conferido com a fonte | Incorporado |
| Perplexity | “65 constantes” no Android | Provável diferença por não contar UNKNOWN | Mapear por nome |
| ChatGPT | Routes API com 21 valores “incluindo DESTINATION” | A conta fecha com DEPART e NAME_CHANGE; DESTINATION não confirmado | Marcado “não confirmado” |
| ChatGPT | `keep-left/right` só na Directions legada, ausentes na Routes API | Correto; no SDK existem como `turnKeep*` | Incorporado |
| ChatGPT | “direita” sozinha não classifica curva; separar interpretação de ícone | Correto e útil | Incorporado nas regras |
| Manus | Separação literal oficial × enum × template; muitos literais PT-BR da Ajuda | Rigoroso; boa parte é fora do escopo (transporte, a pé) | Filtrado e reaproveitado nas seções 6, 7 e 8 |
| Claude (rodada anterior) | Routes API com “15 a 20 valores” e sem rampa | Impreciso: são cerca de 21 e têm RAMP_LEFT/RIGHT, sem as variantes finas | Corrigido nas seções 3 e 4 |

## 12. Lacunas e validação recomendada

- Capturar notificações reais do app Google Maps em pt-BR, em mais de uma versão do app e do Android, com carro e com moto, registrando título, texto, subtexto e bitmap.
- Testar os estados: antes de iniciar, ativa, recálculo, chegada, atividade fechada, sem GPS e modo só alertas.
- Confirmar na referência do Android: contagem do enum, campos `exitNumber` e `roundaboutTurnNumber`, e modo `TWO_WHEELER`.
- Confirmar termos de uso para reaproveitar bitmaps do SDK e licença de qualquer biblioteca de ícones.
- Versionar este catálogo: o Google avisa que novos valores de manobra podem surgir sem aviso. Qualquer valor desconhecido deve cair em `unknown`, nunca quebrar a renderização.

## 13. Referências

- Navigation SDK iOS, enum de manobras: https://developers.google.com/maps/documentation/navigation/ios-sdk/reference/objc/Enums/GMSNavigationManeuver
- Navigation SDK iOS, modos de viagem: https://developers.google.com/maps/documentation/navigation/ios-sdk/reference/objc/Enums/GMSNavigationTravelMode
- Navigation SDK Android, `Maneuver`: https://developers.google.com/maps/documentation/navigation/android-sdk/reference/com/google/android/libraries/mapsplatform/turnbyturn/model/Maneuver
- Feed turn-by-turn: https://developers.google.com/maps/documentation/navigation/android-sdk/tbt-feed
- Notificações do SDK: https://developers.google.com/maps/documentation/navigation/android-sdk/consolidating-notifications
- Interrupções em tempo real: https://developers.google.com/maps/documentation/navigation/android-sdk/real-time-disruptions
- Routes API, rota para duas rodas: https://developers.google.com/maps/documentation/routes/route_two_wheel
- Routes API, países com duas rodas: https://developers.google.com/maps/documentation/routes/coverage-two-wheeled
- Routes API, enums: https://developers.google.com/maps/documentation/routes/reference/rpc/google.maps.routing.v2
- Directions API legada, respostas: https://developers.google.com/maps/documentation/directions/responses
- Ajuda do Maps, navegação: https://support.google.com/maps/answer/3273406
- Ajuda do Maps integrado ao carro: https://support.google.com/maps/answer/9770907
- Ajuda do Maps, notificações: https://support.google.com/maps/answer/6149565
- Ajuda do Maps, trânsito e trajetos: https://support.google.com/maps/answer/6337401
- Flutter, enum `Maneuver`: https://pub.dev/documentation/google_navigation_flutter/0.7.0/google_navigation_flutter/Maneuver.html
