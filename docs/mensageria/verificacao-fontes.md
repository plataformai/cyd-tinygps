# Fluxo de verificação das fontes — especificação, não implementação

## Estado desta entrega
As15 URLs da seção13 foram registradas em `../referencia/fontes.yaml`. Nenhuma página foi buscada ou reverificada; data/hash de coleta são null. Não houve tentativa de rede, portanto não se afirma que a rede esteja indisponível. O documento anexo continua sendo a referência atribuída, não evidência de uma nova consulta.

Google developers/support são candidatos oficiais [A]. Pub.dev é hospedagem externa: fonte secundária [B] neste registro para fins de verificação automática; o anexo cita Flutter como corroboração. A classificação do domínio não confirma nenhuma afirmação específica. Fontes secundárias anônimas do anexo não têm URL resolvível: registrar pendência, não inventar links.

## Registro
Campos: id, URL, marca, superfície, extração desejada, verificação {data,resultado,sha256}, baseline aprovado, perguntas pendentes. Hash é do conteúdo normalizado extraído, **não** do HTML inteiro nem do texto do anexo. Sem coleta, null.

## Script futuro
Implementação proposta Python3 stdlib + parser HTML explicitamente escolhido e testado, ou Node/TypeScript se favorecer o catálogo existente. Saída sem edição do catálogo ou referência:
1. Carregar allowlist somente das URLs oficiais listadas, bloquear redirects para outros domínios e conteúdo inesperado; Pub.dev fica em revisão manual.
2. Uma requisição por vez, intervalo mínimo2s, timeout20s, até2 tentativas com backoff/Retry-After e teto da execução. Salvar metadados HTTP sem credenciais.
3. Extratores por página/superfície e versão de seletor. Separar nomes de enum, descrições, modos, países, campos StepInfo/estados e blocos de ajuda. Remover menus/rodapés, normalizar espaço/Unicode sem apagar pontuação significativa.
4. Validar âncoras e completude. HTTP200 com login/captcha, seletor vazio ou extração parcial = inconclusivo. Contagem zero jamais significa remoção de todos os enums.
5. Comparar contra baseline **aprovado** e catálogo por nome externo verificado. Primeira coleta válida = primeira_coleta, não sem_mudanca. Detectar adição, remoção, descrição, contagem e divergência entre plataformas.
6. Comparar países e presença do Brasil; lista inacessível não implica Brasil removido. Ajuda do carro, consumidor e SDK permanecem superfícies distintas.
7. Gerar relatório Markdown/JSON, snapshots normalizados e proposta de diff. Nada altera os originais ou catálogo sem revisão humana.

Resultados permitidos: `nao_verificado`, `primeira_coleta`, `sem_mudanca`, `mudanca`, `inconclusivo`. Só sem_mudanca com extração válida e baseline comparável. HTTP403/404/429, rede, conteúdo incompleto, seletor falho e mudança de layout recebem motivo próprio.

## Exemplo de relatório JSON (fictício, não execução)
```json
{
  "reportVersion": 1,
  "exampleOnly": true,
  "runAt": null,
  "catalogVersion": null,
  "referenceVersion": "1.0",
  "pages": [{
    "sourceId": "google-navigation-android-maneuver",
    "result": "inconclusivo",
    "reason": "exemplo: seletor não encontrou tabela completa",
    "httpStatus": 200,
    "extractedSha256": null,
    "baselineSha256": null,
    "countBefore": null,
    "countAfter": null,
    "added": [], "removed": [], "descriptionChanges": [],
    "brazilSupported": null
  }],
  "proposedDiff": null
}
```
Markdown correspondente: resumo por fonte, estado, data, hashes, contagens/itens adicionados-removidos, descrições alteradas, Brasil antes/depois, diferenças de ajuda, falhas e decisões pendentes. Arrays vazios com resultado inconclusivo **não** significam ausência de mudanças.

## Pontos explicitamente pendentes
Contagem e nomes Android; existência/semântica de DESTINATION na Routes; campos exitNumber/roundaboutTurnNumber, fullInstructionText, via, distância/tempo do passo, faixas/isRecommended/bitmaps, ETA/restante e estados do feed; TWO_WHEELER Android; Brasil, beta/cobrança/avisos atuais; equivalência UTURN por sentido; descontinuação Directions; comportamento heads-up SDK; desativação de callouts de lentidão/veículo parado; literais PT-BR marcados com asterisco; termos/licenças de bitmaps e fontes.

Termos e release notes não constam com URLs na seção13: o script restrito não deve buscar páginas extras silenciosamente. Solicitar aprovação para ampliar allowlist ou verificar manualmente com URL registrada.

## Validação futura
Fixtures para enums novos/removidos, descrições traduzidas versus originais, mudanças de espaços sem significado, país Brasil removido, erro HTTP e captcha200. Testar que execução não modifica referência/catalogo, que hashes são reproduzíveis e que extração parcial não vira diff destrutivo.

Se a rede não funcionar na execução futura: relatório inconclusivo com erro concreto; alternativa é snapshot oficial obtido pelo usuário com URL/data/proveniência e verificação manual. Nunca declarar “sem mudança” porque a página não pôde ser aberta.
