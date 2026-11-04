# Arquitetura proposta [C]

## Decisão principal
Continuar como leitor de notificações na primeira implantação. SDK/API ficam como fornecedores alternativos futuros, mediante decisão de produto e verificação oficial. Eles não estão integrados hoje e não se deve assumir que exponham a sessão aberta em outro app.

Fluxo proposto:
`listener nativo → seleção de sessão/campo → catálogo + interpretador → estado válido/TTL → fila BLE limitada → parser ESP32 → estado → desenho por região`.
React observa o estado e apresenta diagnóstico; deixa de ser responsável exclusivo pelo envio da manobra.

### Catálogo único e geração
Um futuro `catalog/maneuvers.json`, schemaVersion e catalogVersion independentes; 66 entradas da referência mais estados/eventos em seções próprias. IDs internos estáveis atribuídos por registro (não posição no array, não número iOS); `0` reservado para unknown. Nunca reaproveitar IDs removidos. Locale inicial `pt-BR`, textos e padrões em seção própria.

Exemplo ilustrativo, não tabela implantada:
```json
{
  "schemaVersion": 1,
  "catalogVersion": "1.0.0",
  "id": 1001,
  "key": "turn_left",
  "sourceNames": {"iosReference": "turnLeft", "androidVerified": null, "routes": "TURN_LEFT"},
  "family": "turn",
  "side": "left",
  "intensity": "normal",
  "circulation": null,
  "futureIconId": "turn_left",
  "legacyVisual": {"android": "arrow-left", "esp32": "left"},
  "evidenceSource": "reference-v1.0-not-reverified",
  "locales": {"pt-BR": {"label": "Vire à esquerda", "ruleIds": ["turn.left.explicit"]}}
}
```
Nomes por plataforma devem ser verificados, não convertidos automaticamente por casing. Routes MERGE e RAMP_* não recuperam variantes perdidas. Gerador determinístico produz tabelas Kotlin/TypeScript/C++ a partir do mesmo catálogo, strings de flash e mapas por ID, sem edição manual dos derivados. CI compara regeneração e testa unicidade, estabilidade e hashes.

### Evidência e interpretação
Ordem de fornecedores: enum disponível → lookup confiável de bitmap → regras pt-BR → unknown. Nesta fase somente texto está disponível; lookup de imagem fica desligado até corpus validado, não tratado como reconhecimento garantido.

Selecionar a notificação de navegação por critérios testados em dispositivo; categoria/canal são sinais, não contratos do Maps. Preservar campos separados e identidade/sessionKey. Frase de instrução é distinta de resumo e nomes de via. Não processar texto livre da via como comando.

Regras ordenadas por família da seção6.3 com correções propostas: estados explícitos, chegada explícita, rotatória, retorno, rampa/saída, manter/bifurcação, incorporação, curva, reto, mudança de nome. Palavra “destino” isolada não é chegada; direita/esquerda isolada não é curva; “continue” em um aviso não autoriza reto. Qualificadores pertencem à cláusula de comando, não ao nome da rua.

Modelo interno separa `navigationState`, `maneuver`, `roadEvent`, `sourceHealth`, distâncias do passo/rota, tempo restante/ETA, saída alfanumérica, ordinal de rotatória, faixa, modo, lado de condução e circulação. Ausência é null, nunca zero. Preservar `rawInstruction` local, `displayText` limitado, origem/versão da regra e evidência categórica (explícita, parcial, desconhecida); não atribuir “0,8” como probabilidade sem calibração.

Um texto “Na rotatória, pegue a 2ª saída” permite família e ordinal, **não** sentido CW/CCW. Manter `maneuverId=unknown` com família ROUNDABOUT e `exitOrdinal=2` se faltar enum específico; não escolher uma das20 variantes por Brasil presumido. O mesmo vale para retorno sem sentido.

### Mensagem textual e visuais existentes
Mostrar texto publicamente capturado e sanitizado quando a instrução de navegação é identificável, mesmo que a manobra específica seja unknown; identificar “orientação não classificada”, sem seta. Não repassar notificações genéricas do Maps como navegação. Texto normalizado de catálogo é marcado como texto do BeeLine [C], não literal Google.

Preservar os glifos atuais Android e a geometria atual ESP32 para reto, curvas comuns e leves corretamente classificadas. Manter/retorno podem reutilizar o desenho legado apenas com rótulo explícito e sem alegar representar a variante completa; nas combinações ambíguas, texto neutro. Não mapear rotatória, chegada ou evento a seta simples só porque inclui “direita”. A matriz visual especifica cobertura e lacunas.

No TFT manter coluna de bússola80px e faixa inferior, alterar apenas a área principal para acomodar rótulo/instrução em até duas linhas, distância e via. Proposta de prioridade: estado terminal/recálculo > manobra > evento informativo. Evento não substitui definitivamente a manobra. Acomodação exata, acentos e legibilidade precisam de validação manual; não criar imagens agora. Unknown pode usar texto “Orientação recebida” e **nenhuma seta**, sem asset novo.

## Protocolo: manter v1, negociar v2
JSON v1 é legível e já funciona, mas nomes extensos e campos novos pressionam 180 bytes. Recomenda-se frame binário v2 compacto **em característica separada**, mantendo v1 para compatibilidade. Alternativa JSON compacto+CRC exige serialização canônica e orçamento rigoroso; não adotar JSON sem limites. Nova característica capability READ/NOTIFY informa versões, tamanho máximo e versão/hash de catálogo; UUIDs novos devem ser reservados sem colidir com os dois atuais. Firmware v2 continua aceitando v1.

App v2 em placa v1 opera somente recursos legados explícitos e mostra aviso; não envia estados novos como `navigation` com strings desconhecidas (a placa antiga desenharia reto). Atualizar firmware primeiro; só habilitar mensageria rica depois de capability v2 confirmada. Em placa v1, não prometer limpeza/expiração que ela não implementa. App v1 em placa v2 continua funcional, com política conservadora documentada para legacy navigation.

### Frame v2 proposto, máximo180 bytes
Little endian para inteiros; texto UTF-8 válido. Cabeçalho18 bytes:
| Offset | Campo | Bytes |
|---|---|---|
| 0 | magic constante ASCII BL | 2 |
| 2 | protocolMajor=2 | 1 |
| 3 | tipo (navigation/state/event/telemetry/control) | 1 |
| 4 | session nonce negociado | 4 |
| 8 | sequência por tipo | 4 |
| 12 | TTL restante ms | 2 |
| 14 | catalogMajor | 2 |
| 16 | payloadLength | 2 |

Payload ≤160 bytes, CRC16 final2 bytes; tamanho total =20+payloadLength. CRC-16/CCITT-FALSE: poly0x1021, init0xFFFF, refin/refout=false, xorout0; abrange cabeçalho+payload, sem o CRC. Check conhecido ASCII `123456789` →0x29B1; valor do CRC no fio little endian. Testar vetores completos em Kotlin/C++, incluindo acentos, corrupção e truncamento.

Payload navigation proposto: maneuverId u16, estado u8, flags u8, distância passo u32, distância total u32, comprimento saída u8 + até8 bytes, comprimento via u8 + até48 bytes, comprimento texto u8 + até64 bytes. Máximo135 bytes (15 fixos +120 texto), total155. `0xFFFFFFFF` representa distância ausente; 0 significa zero medido. Faixas válidas: passo0–65535m por compatibilidade, total0–40.000.000m. Ordinal e dados de faixa usam mensagens estruturadas separadas/versionadas, não texto interpretado pelo ESP32. Eventos possuem tipo enumerado, TTL e texto limitado; definir layouts restantes antes de implementação da etapa de protocolo.

Rejeitar tamanho inconsistente, tipos/flags reservados incompatíveis, UTF-8 inválido, CRC errado, sessão incorreta ou sequência anterior; contar motivos sem gravar texto privado. ID desconhecido em catálogo compatível vira unknown; major incompatível exige renegociação. Nunca atualizar parcialmente um snapshot.

CRC da aplicação ajuda a testar enquadramento e contrato; o enlace BLE já tem integridade. **CRC não autentica o telefone**, nem garante conteúdo verdadeiro. Segurança contra escrita por terceiro requer avaliação separada de pareamento/autorização.

Negociar limite efetivo pelo ATT MTU. V2 inicial só envia frames que caibam em MTU−3 e limite anunciado; caso insuficiente, avisar incompatibilidade e não fragmentar silenciosamente. MTU baixo exige projeto explícito de fragmentação/reassembly antes de habilitar v2.

### Validade, reconexão e estado
TTL navegação proposto8 s, máximo30 s; heartbeat a cada3 s somente enquanto fonte ainda válida. Idade máxima inicial de fonte30 s para manobra, 120 s para resumo já existente, ambas hipóteses para calibrar com corpus real (reta longa pode atualizar pouco). O TTL enviado é min(prazo do tipo, validade restante da fonte), calculado depois da fila. Heartbeat não renova publicação Maps indefinidamente.

Remoção, troca de sessão, permissão revogada, parada explícita, chegada ou perda de fonte causam invalidação da manobra; evento desconhecido não mantém instrução antiga como atual. Pacote malformado conserva último válido somente até seu TTL. Desconexão limpa navegação v2 e campos relevantes; velocidade, rumo e rota conservam watchdogs próprios. Nova conexão negocia sessão e envia apenas snapshot fresco; mensagens antigas são descartadas.

Uma caixa latest-wins por tipo de snapshot, limites fixos e controle prioritário para invalidar; eventos transitórios com fila pequena limitada e descarte documentado. Sequência uint32 comparada modularmente dentro da sessão, janela menor que2^31; reinício muda nonce. Duplicata não renova TTL, heartbeat tem nova sequência mas mantém limite da fonte.

## Desempenho e medição
Lookup por ID; padrões compilados uma vez; evitar concatenações repetidas no caminho quente. Degraus propostos: passo5m abaixo100m, 10m até1km e50m acima, sem esconder passagem por zero; alteração de manobra/estado imediata, distância no máximo5Hz, heartbeat independente. Valores e metas são decisões [C], não regras Google.

TFT usa dirty flags por região e por conteúdo efetivo; mudar estado deve invalidar sem depender de ângulo igual. Testar inundação de velocidade sem impedir clear/navigation. Não prometer latência usando apenas o limite10Hz.

Instrumentar t0 Android monotônico no listener, t1 classificação, t2 fila, t3 início escrita, t4 confirmação GATT, t5 recepção ESP32, t6 final desenho. Session/sequence correlacionam amostras; característica NOTIFY responde renderAck com tempos locais. Ping/pong estima offset por menor RTT e registra intervalo de incerteza; sem sincronização, reportar limite superior pelo retorno do ACK e tempos locais, não subtrair relógios diretamente.

Metas provisórias: mediana≤250ms, p95≤750ms listener→desenho, em BLE estável. Publicação Maps→listener é métrica separada; postedAt não é relógio monotônico da placa. Relatar n, perdas/timeouts, carga, versões, modo foreground/background, RTT/incerteza e método de percentil; não excluir perdas silenciosamente. Meta não foi medida aqui.

## Memória dos ícones futuros
Exemplo orçamentário [C], não assets existentes: 64×64 RGB565 ocupa8192 bytes; 35 bases=286720 bytes flash, 66=540672. Máscara1bpp=512 bytes/base (35=17920); paleta e transparência têm custo extra. Uma linha RGB565=128 bytes RAM; framebuffer320×240=153600 bytes, evitar. Desenho vetorial atual não exige array de bitmap. Dimensões reais, paleta, espelhamento permitido e número de bases dependem das imagens fornecidas e leitura em sol; medir heap/flash em build e placa.
