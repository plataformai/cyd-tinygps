# Diagnóstico do caminho atual

## Evidência e método
Inspeção direta do repositório em 04/11/2026, sem alteração no produto. `N` abaixo designa `artifacts/beeline-navigator/`. As citações são caminhos relativos à raiz. Evidência de código é observação local, não uma classificação [A] do Google. Frases dos experimentos são sintéticas [C], não notificações reais.

## Inventário rastreável
| Etapa | Evidência: arquivo, linhas e trecho | Conclusão e limite |
|---|---|---|
| Entrada Android | `artifacts/beeline-navigator/modules/beeline-android/android/src/main/java/expo/modules/beeline/BeeLineNotificationListenerService.kt:28-30` — `if (sbn.packageName != MAPS_PACKAGE) return` | Filtra pacote, não categoria/canal de navegação. Uma avaliação/comunicação do Maps pode substituir a rota. |
| Campos capturados | mesmo arquivo `:79-103` — `EXTRA_TITLE`, `EXTRA_TEXT`, `EXTRA_BIG_TEXT`, `EXTRA_SUB_TEXT`, `EXTRA_TEXT_LINES`, `postedAt` | Textos públicos em extras; não enum, bitmap, hash de ícone, canal, categoria, versão Maps/Android ou modo de viagem. |
| Persistência e ciclo de vida | mesmo arquivo `:33-65,94-98` — `maxByOrNull { it.postTime }`, `putString(LATEST_NOTIFICATION_KEY, serialized)` | Última notificação em SharedPreferences locais; remoção procura substituta pelo horário, sem identidade de sessão de navegação. Limpa quando não há outra ou listener desconecta; não há política geral de retenção de corpus. |
| Ponte para JS | `artifacts/beeline-navigator/modules/beeline-android/android/src/main/java/expo/modules/beeline/BeeLineAndroidModule.kt:75-95,211-223` — `sendEvent("onMapsNotification", ...)` | Evento e consulta de preferências; chave da notificação capturada pelo listener não é repassada no mapa para JS. Conversões JSONObject nesses trechos não têm catch local. |
| Leitura no app | `artifacts/beeline-navigator/app/index.tsx:63-144` — `setInterval(..., 1500)`; `parseMapsInstruction(notification)` | Evento imediato + polling 1,5 s + retorno ao primeiro plano. Evento chama novamente consultas nativas. Chave inclui postedAt; publicação repetida pode reprocessar texto idêntico. |
| Interpretação | `artifacts/beeline-navigator/lib/navigation.ts:19-65` — normalização NFD, regex de retorno/manter/esquerda/direita/reto | Seis resultados. Todos os campos são concatenados: mistura de instrução, resumo e nomes de rua. Não existe catálogo versionado nem extração por superfície. |
| Distância da manobra | mesmo arquivo `:55-59` — `text.match(...)`, `: 0` | Primeira distância em m/km, limitada a 65535. Ausência vira zero; não suporta ft/mi. Pode consumir distância total se não houver distância de passo. |
| Via | mesmo arquivo `:61-63` — `roadMatch...slice(0,48)` | Melhor esforço com preposições, preserva acentos; 48 unidades UTF-16, não 48 bytes. Não é campo de via estruturado do Google. |
| Envio navegação | `artifacts/beeline-navigator/app/index.tsx:133-144` — `writeCommand(makeNavigationCommand(liveTurn))` | Depende de React; deduplica por manobra/distância/via. Desconexão limpa chave de envio, mas não necessariamente a instrução velha. Erro limpa chave, sem timer de retry dedicado. `makeNavigationCommand` pode lançar sincronicamente antes do `.catch` da promessa. |
| JSON e limite | `artifacts/beeline-navigator/lib/navigation.ts:68-83` — `{type:'navigation',maneuver,distanceMeters,road,timestamp}` | 180 bytes UTF-8 verificados; não há versão, CRC da aplicação, sequência, prazo ou ACK de renderização. Texto multibyte pode estourar o limite apesar do corte por caracteres. |
| Resumo independente | `artifacts/beeline-navigator/modules/beeline-android/android/src/main/java/expo/modules/beeline/RouteSummary.kt:8-47` — `SOURCE_MAX_AGE_MS=120_000`, `FIRMWARE_TTL_MS=30_000` | Captura distância restante junto de duração/ETA ou rótulo explícito. Chegada/encerramento reconhecidos apenas para invalidar esse resumo; não é estado de navegação no TFT. ETA/duração ajudam a interpretar, mas não são enviados como campos. |
| Resumo em segundo plano | `artifacts/beeline-navigator/modules/beeline-android/android/src/main/java/expo/modules/beeline/BeeLineAndroidModule.kt:68-72,259-301` — heartbeat 5000 ms e `currentRoute()` dentro da escrita serializada | Atualiza via escopo nativo enquanto módulo/processo vive; independe dos timers React. Não comprova persistência após morte do processo. Não estende fonte além de 120 s. |
| GPS | `artifacts/beeline-navigator/lib/speed-task.android.ts:9-31`; `artifacts/beeline-navigator/lib/heading.ts:7-13`; `artifacts/beeline-navigator/app/index.tsx:268-279` — `TaskManager.defineTask`, `killServiceOnDestroy:false` | Velocidade e rumo vêm da localização, não Maps. Rumo exige velocidade ≥1,5 m/s, precisão ≤35 m e idade ≤5 s. Sem rumo não significa necessariamente “Maps sem GPS”: pode ser veículo parado. |
| BLE | `artifacts/beeline-navigator/modules/beeline-android/android/src/main/java/expo/modules/beeline/BeeLineAndroidModule.kt:333-424,437-507` — UUID/nome, `writeMutex.withLock`, `withTimeout(4000)`, `WRITE_TYPE_DEFAULT` | Scan até 15 s; pede MTU247, porém não persiste/valida MTU efetivamente obtido. Uma escrita de 180 bytes exige comportamento ATT compatível, a validar no Android real. Não há fragmentação explícita nem reconexão automática contínua. |
| Receptor | `artifacts/beeline-navigator/firmware/beeline-esp32.ino:438-525` — comprimento, profundidade JSON5, tipos, dados restantes | Rejeita malformados antes de publicar mailboxes; preserva último válido. Descartes não têm contador/log de motivo. String de manobra arbitrária não vazia é aceita. |
| Concorrência | mesmo arquivo `:96-126,537-605` — quatro mailboxes, última válida vence | Velocidade não elimina navegação. Parsing fora da seção crítica; TFT no loop. Não é fila de eventos: rajadas podem eliminar eventos intermediários. |
| Validade | mesmo arquivo `:608-621,656-701` — timeout speed/heading/route, sem timeout navigation | Desconexão invalida rumo/resumo, velocidade expira em 8 s; manobra permanece. Remoção da notificação no celular não limpa diretamente a seta. |
| Desenho | mesmo arquivo `:218-258,339-385` — `return 0.0f`, `fillTriangle`, `drawString` | Seis orientações de uma seta vetorial; desconhecido vira reto. Não imprime frase completa da manobra: título fixo, distância e via. Retorno é seta para baixo, sem laço ou sentido CW/CCW. |
| Atualização parcial | mesmo arquivo `:559-566,689-700` — compara ângulo/distância/via e flags | Alteração navegação redesenha tudo; speed só rodapé, heading/route só lateral. Teto 10 Hz; não equivale a latência medida de 100 ms. |
| UI Android | `artifacts/beeline-navigator/app/index.tsx:25-35,408-468` — MaterialCommunityIcons e modal texto bruto | Seis glifos distintos no Android; estado incompatível com map-marker-path. Texto bruto disponível no modal somente quando não reconhecido, não enviado à placa. |
| Hardware | `artifacts/beeline-navigator/firmware/README.md:11-27`; sketch `:27-65,156-215,265-311` | ST7789/BGR documentado como validado; 320×240, região principal240, lateral80, rodapé desde y193; régua0–140. Não alterar configuração ao evoluir mensagens. |

## Falhas prioritárias
1. **Classificação insegura:** posição do destino, indicação de faixa ou nome de rua com esquerda/direita vira curva. Texto de recálculo com “continue” pode virar reto. Leve e fechada perdem intensidade.
2. **Seta antiga:** não há invalidação/TTL da navegação no TFT. Uma notificação nova incompatível limpa o card Android, mas não manda clear-navigation; reconectar pode reenviar um resultado antigo.
3. **Distância falsa:** ausência vira 0 e primeira distância pode ser resumo. Android exibe zero como “—” (`distanceMeters || '—'`), TFT como “0 m”.
4. **Segundo plano desigual:** resumo tem entrega nativa; manobra depende de efeito React. Deve ser medido com Maps em primeiro plano e tela apagada, não deduzido de preview.
5. **Conteúdo versus transporte:** sucesso GATT não prova parsing ou desenho. Não há instrumentação que conecte publicação, recebimento e render.
6. **Exceções e fila:** construções JSONObject não protegidas localmente e serialização TS antes do `.catch`; espera pelo mutex não está abrangida pelo timeout de 4 s de cada escrita. Sem orçamento explícito de fila/idade de navegação.
7. **Texto no TFT:** corte `substring` por bytes pode dividir UTF-8; compatibilidade dos glifos/fontes para acentos não foi comprovada em hardware nesta análise.
8. **Privacidade:** notificação bruta persiste localmente e log serial de navegação contém via (`sketch:569-570`). Corpus futuro deve ser voluntário, sanitizado e ter retenção definida.

## O que já é útil preservar
Separação da distância total; GATT serializado com fechamento após timeout; parser defensivo; mailboxes independentes; ausência de framebuffer extra; escala, cores, pinagem e bússola. Preservação visual não significa preservar interpretações erradas.

## Experimentos locais
Resultados em [experimentos-parser.json](experimentos-parser.json). Exercitam diretamente o parser existente, não uma reimplementação. As frases são [C]. A suíte existente testa casos positivos e alguns nulos, mas não prova cobertura das 66 manobras nem comportamento físico.

## Limites
Nenhum APK ou binário ESP32 compilado nesta entrega documental. Não foram medidos rádio, heap físico, p50/p95, comportamento real do Maps ou validade de termos externos. Rodar testes sintéticos não transforma hipóteses de notificação [B]/[C] em contrato oficial.
