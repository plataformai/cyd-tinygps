# Diagnóstico gráfico do Maps

Esta versão coleta evidências; não reconhece manobras a partir de imagens.
O envio de orientação textual e distância continua independente.
Não é necessário alterar o firmware para usar o diagnóstico.

## No Android

1. Instale um APK gerado com este código, mantendo o acesso às notificações.
2. Com o veículo parado, inicie uma navegação no Maps.
3. No PetitGPS, toque em **Diagnóstico da seta do Maps** e depois
   **Capturar notificação atual**.
4. Tire prints das imagens candidatas com seus números e fontes, dos campos
   de texto e dos erros, se houver. Compare com a notificação expandida do Maps.
5. Opcionalmente, use **Compartilhar relatório JSON**. O conteúdo compartilhado
   é texto JSON com as imagens PNG em base64, não um arquivo anexado. O aplicativo
   escolhido pode impor limites de tamanho; nesse caso use os prints.
6. Para outra situação, capture novamente. **Apagar captura desta tela** remove
   o diagnóstico da memória da tela. Não existe histórico de diagnósticos em disco.

## Evidência de 2026-11-09 (Maps 26.45, Android 12)

- A seta da manobra atual está no **largeIcon** da notificação de navegação
  (seta branca sobre fundo transparente). O smallIcon é o logo de navegação.
  Os layouts RemoteViews não são fornecidos.
- O texto traz apenas "em direção a <via>" e o resumo em subText.
- Alertas de local do Maps ("Fechado hoje") são notificações do mesmo pacote e
  sobrescreviam a orientação. Agora só notificações de navegação são usadas:
  categoria `navigation` ou resumo de rota (min + km/m) no texto.

## Coleta de setas

Durante a navegação, cada largeIcon diferente é guardado em memória (até 30,
máscara 24 × 24 + PNG 48 × 48, contagem e último texto). Em **Setas coletadas
na rota**: atualizar, compartilhar (com confirmação) ou apagar. Nada vai por BLE
ou para disco. As amostras servirão para criar modelos de reconhecimento; até
lá, a orientação continua em modo texto.

## Fontes e limites

Consulta somente a notificação ativa mais recente do pacote oficial do Maps:
campos de texto, nomes/tipos de extras, ícones públicos e imagens/descrições nos
RemoteViews fornecidos pelo produtor. Não usa reflexão, serviço de acessibilidade,
captura da tela do celular ou reconhecimento remoto. Sem novas permissões.

Até 8 imagens de 96 × 96 por captura; até 160 nós e profundidade 12 por layout.
Cada PNG tem limite de 48 KB. Falhas de extração são relatadas e não transformadas
em manobras. As descrições não são adicionadas automaticamente ao interpretador:
podem descrever logos ou uma manobra posterior, não a atual.

Os layouts públicos podem estar ausentes mesmo quando o Android exibe uma seta
em um template do sistema. A ausência de candidato não comprova ausência da seta.
Os ícones pequeno/grande também podem ser apenas o logo Maps.

O diagnóstico é sob demanda, na fila principal para acesso às Views, não a cada
notificação. Não vai por BLE, não é enviado a servidor e não altera os textos
armazenados pelo leitor existente. Pode conter vias/destinos; o compartilhamento
é explícito, após confirmação. O usuário escolhe o destinatário.

## Validação

Os testes de contrato locais verificam limites, vínculo nativo e isolamento
do transporte. Não substituem compilar e testar no Android. Validar no aparelho:
notificação ausente; captura com rota; candidatos/layouts ausentes; recaptura;
cancelamento do compartilhamento; limpeza; continuidade de BLE/texto/distância.
Após obter evidências, mapear somente os símbolos confirmados da manobra atual;
desconhecidos devem continuar no modo texto, sem inventar seta.
