# Implantação futura — sujeita à aprovação

Nenhuma etapa abaixo foi implementada nesta entrega. Ordem dependente; não executar fases concorrentes sobre o mesmo protocolo.

| Etapa | Entrega e aceite | Riscos | Testes e dependência |
|---|---|---|---|
| 1. Evidência real e catálogo | Corpus local sanitizado com campos separados, versões Android/Maps e modo selecionado pelo usuário; 66 entradas estáveis, estados/eventos e aliases por plataforma explicitamente verificados ou pendentes; tabelas geradas idênticas entre plataformas | Corpo da notificação varia; não há sinal para algumas categorias | Fixtures reais separadas de sintéticas; unicidade/estabilidade de IDs e geração determinística. Captura é validação manual. Sem implementar ícones |
| 2. Interpretador conservador | Regras por família e campos, null distinto de0; chegada/rotatória/faixa não viram curva; unknown preserva texto útil de navegação sem inventar lado/sentido; parser sem escape de exceções | Aumentar falsos negativos em formatos legítimos | Casos positivos/negativos por família, acentos, campos conflitantes, nome de rua com direita; comparar corpus e regras antigas. Depende1 |
| 3. Contrato v2 e estado | Congelar layouts restantes, vetores CRC, capabilities e MTU; firmware v2 recebe v1/v2, app negocia; invalidação/TTL e sessão impedem seta antiga | Compatibilidade e fragmentação/MTU; app antigo não envia TTL | Matriz app1/fw1, app1/fw2, app2/fw1, app2/fw2; corrupção, duplicata, ordem, reboot, millis wrap, pacote180/181. Depende1, layouts aprovados |
| 4. Texto ponta a ponta | Entrega nativa independente de timers React, fila limitada com prioridade de clear, UI observa mesmo estado; TFT exibe texto/rótulo e conserva visuais existentes só quando compatíveis | Segundo plano real, prioridade de incidentes, limites de texto/fontes | Integrar2+3, testes host de estados e UI; validação manual foreground/background, tela apagada e reconexão. Não criar ícones |
| 5. Desempenho e campo | Deduplicação com heartbeat limitado pela fonte, redraw por região; relatório mediana/p95 com perdas, incerteza dos relógios e versões | Reta longa expira por silêncio; carga BLE e consumo | Telemetria concorrente, estado terminal, tempos de fila, sequência→renderAck. Metas250/750ms provisórias; medir em hardware após4 |
| 6. Vigilância das fontes | Script oficial allowlist gera Markdown/JSON e diff, sem editar catálogo/referência; páginas inacessíveis=inconclusivo | HTML muda, bloqueio, falso “sem mudança” | Fixtures HTML, remoção de seletor,429/403, contagem alterada, Brasil ausente; aprovação humana de cada baseline. Independente do runtime, depois da definição de catálogo1 |
| 7. Imagens fornecidas | Nova família encaixada por IDs estáveis e inventário de faltantes, mantendo texto/unknown e economia de memória | Licença, inversão incorreta, contraste | Amostra por família no CYD, CW/CCW e lado, orçamento flash/heap, regressão das seis representações. Depende dos assets do usuário; fora da fase textual |

## Preservação e release
Manter ST7789/BGR, pinagem, rotação, 320×240, régua0–140 e marcas50/100, fundo cinza e rodapé preto, bússola GPS com N/E/S/W. Não substituir por magnetômetro.

Versionar separadamente referência, catálogo, protocolo e firmware/app; changelog registra compatibilidade e migração. Bundle de release anota versões e hashes; não usar número de tarefa ou índice da referência como versão.

Referência1.0 é baseline recebido; nenhum catálogo v1 ou protocolo v2 foi lançado por esta análise. Aplicar alterações somente após aprovação dos diffs. Preparar distribuição futura com exportador existente, sem alterar monorepo/lockfiles do usuário ou gerar pacotes nesta tarefa.

## Critério de sucesso da fase textual
Mensagem reconhecida produz texto e representação coerentes no Android e ESP32; mensagem incompleta não inventa direção/distância; fonte perdida não deixa seta antiga; uma notificação informativa não interrompe a manobra definitivamente. O relatório declara categorias indisponíveis, não anuncia “66 suportadas” apenas por ter catálogo.
