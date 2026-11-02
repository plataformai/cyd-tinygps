# BeeLine Navigator — firmware ESP32

Este sketch cria um periférico BLE chamado `BeeLine-ESP32`, recebe cada instrução como JSON pela característica GATT de escrita e desenha a seta no display integrado da placa ESP32-2432S028.

## Preparar

1. Instale o Arduino IDE e o suporte às placas ESP32.
2. Instale as bibliotecas **NimBLE-Arduino 2.x**, **ArduinoJson 7.x** e **TFT_eSPI**.
3. Para a placa do usuário (ESP32-2432S028 com revisão de dois conectores USB, já confirmada), configure o `User_Setup.h` do TFT_eSPI:

   ```cpp
   #define ST7789_DRIVER
   #define TFT_WIDTH  240
   #define TFT_HEIGHT 320
   #define TFT_RGB_ORDER TFT_BGR
   #define TFT_INVERSION_OFF
   #define TFT_MISO 12
   #define TFT_MOSI 13
   #define TFT_SCLK 14
   #define TFT_CS   15
   #define TFT_DC    2
   #define TFT_RST  -1
   #define TFT_BL   21
   #define TFT_BACKLIGHT_ON HIGH
   ```

4. Extraia `beeline-firmware.zip` inteiro e abra `beeline-esp32/beeline-esp32.ino`. O sketch não precisa de arquivos auxiliares. A pasta e o `.ino` têm o mesmo nome, como o Arduino IDE exige. Selecione **ESP32 Dev Module** e envie o sketch. `display.setRotation()` é aplicado no primeiro ciclo do loop. A tela de verificação aparece por 1,2 segundo sem bloquear a inicialização do BLE.
5. Abra o monitor serial em `115200` baud. O resultado esperado inclui `BeeLine-ESP32 ready` e, na primeira passagem do loop com a rotação padrão `1`, `[INFO] Rotacao aplicada: 1 (Largura=320 Altura=240)`.
6. Para testar a orientação sem recompilar, configure o monitor serial para **Nova linha** (ou **Ambos NL e CR**) e envie `r0`, `r1`, `r2` ou `r3`. O firmware mostra a rotação e a resolução selecionadas por 1,2 segundo, e depois restaura a tela de navegação ou espera. Em geral, `r0` e `r2` são retrato (240 x 320), enquanto `r1` e `r3` são paisagem (320 x 240); a posição visual pode variar conforme a montagem da placa. Escolha a orientação correta e altere `DEFAULT_DISPLAY_ROTATION` para esse valor antes de gravar a versão definitiva. O teste é temporário: reiniciar a placa restaura esse valor padrão.
7. Se nenhuma rotação resultar em `320 x 240`, confira se `User_Setup_Select.h` seleciona o `User_Setup.h` que você editou, o driver e os pinos da revisão exata da placa. A configuração acima corresponde à placa do usuário que já foi confirmada; outras revisões podem usar um controlador diferente. CS/DC trocados ou driver incorreto normalmente causam falha, imagem corrompida ou ausência de imagem; não são um mecanismo confiável de fallback para retrato. O firmware usa uma única instância global do display.

## Contrato BLE

- Serviço: `7b1e0001-6d2a-4f7a-9c21-8e8a4b12a001`
- Escrita: `7b1e0002-6d2a-4f7a-9c21-8e8a4b12a001`
- Tipo: UTF-8 JSON, máximo de 180 bytes por comando.
- Profundidade máxima do parser: cinco níveis, contando o objeto raiz. Pacotes vazios, acima do limite, com erro de JSON, dados após o objeto, raiz diferente de objeto ou campos consumidos de tipo incorreto são descartados **sem substituir estado ou comandos válidos pendentes**. Campos extras, como `timestamp`, continuam permitidos; o timeout usa o relógio local de recepção, não o relógio do telefone.

Exemplo:

```json
{"type":"navigation","maneuver":"slight_right","distanceMeters":350,"road":"Rua Haddock Lobo","timestamp":1761600000000}
```

Manobras reconhecidas pelo protocolo: `straight`, `slight_left`, `left`, `slight_right`, `right` e `uturn`. O receptor valida o JSON e o tipo antes de entregar os dados à função da tela. `type` e `maneuver` devem ser strings, e a manobra não pode ser vazia. `road` é uma string opcional (padrão vazio) e `distanceMeters` é um inteiro opcional (padrão zero), limitado a 0–65535 como antes. Manobras não reconhecidas mantêm o comportamento anterior de seta para frente. Strings usadas como texto não podem conter NUL embutido.

Atualização de velocidade enviada pelo app Android:

```json
{"type":"speed","speedKmh":72.4,"valid":true,"timestamp":1761600000000}
```

Quando o GPS não fornece uma leitura válida, o app envia `{"type":"speed","speedKmh":null,"valid":false,"timestamp":...}`. A placa mostra `-- km/h` após uma leitura inválida ou oito segundos sem atualização.

`valid` deve ser booleano. Com `valid:true`, `speedKmh` deve ser um número; tipos incorretos, ausência ou `null` são pacotes malformados e não alteram o estado. Com `valid:false`, `speedKmh` pode ser numérico, `null` ou ausente, e a velocidade fica indisponível. Um número negativo, não finito após conversão para `float`, ou que não caiba no inteiro exibido também torna a leitura indisponível. O valor interno é zerado quando indisponível: não se exibe uma leitura antiga como atual. O timeout ocorre a partir de **8000 ms** desde a recepção, inclusive se o pacote ficou pendente; cálculos usam subtração sem sinal para suportar a volta de `millis()`.

## Recepção, rajadas e atualização da tela

- O callback BLE apenas limita, interpreta e valida o pacote e publica uma cópia de tamanho fixo. Não desenha, não imprime no serial e não segura um mutex durante desenho ou interpretação JSON.
- Há **duas posições independentes** de armazenamento pendente: uma navegação e uma velocidade. Em cada tipo, a última mensagem válida recebida substitui a anterior ainda pendente. Mensagens intermediárias podem ser combinadas; não há fila crescente nem garantia de exibir cada amostra. Uma rajada de velocidade nunca elimina a última navegação. Uma leitura `valid:false` tem a mesma prioridade de chegada de qualquer outra velocidade.
- Uma seção crítica entre os núcleos protege somente cópias das estruturas fixas e flags. Strings dinâmicas, logs, estado visível e TFT pertencem ao loop principal. Cada passagem consome no máximo uma navegação, uma velocidade e 16 bytes seriais.
- O painel normal é atualizado no máximo uma vez por 100 ms (10 Hz), somente quando necessário. Navegação compara ângulo visível, distância e texto truncado da via; velocidade compara disponibilidade, número arredondado e posição do marcador. Uma leitura repetida renova o prazo de validade sem forçar desenho.
- Alterar somente a velocidade redesenha somente o painel inferior. Alterar a navegação ainda exige desenho da tela inteira; estas melhorias reduzem trabalho redundante, mas não prometem eliminar toda cintilação.
- A tela usa preenchimentos sólidos em cinza-escuro; a régua tem uma base preta visível ao redor das faixas coloridas. Não há framebuffer adicional alocado. O restante da tela só é redesenhado quando uma instrução visível muda; uma alteração da velocidade repinta somente o painel inferior.
- `r0`–`r3` continuam disponíveis. A última rotação completa recebida em uma passagem é aplicada pelo loop e mostra o diagnóstico por 1,2 segundo, fora do limite normal de atualização. Comandos longos são descartados até a próxima quebra de linha. Durante o diagnóstico, navegação e velocidade continuam sendo processadas sem substituir a tela temporária; ao terminar, a última navegação e a disponibilidade atual da velocidade são restauradas.
- Coordenadas e limites do painel ficam em `Layout`. O modo de diagnóstico em retrato preserva o preenchimento sólido e não altera o setup do display.

## Display e variantes da placa

O sketch desenha a seta, distância e nome da via em orientação horizontal. No canto inferior, mostra o valor atual e uma régua colorida de 0 a 140 km/h, com graduações de 20 em 20 e marcações destacadas em 50 e 100 km/h. O indicador visual fica limitado à escala; o número mostra a velocidade recebida. O pinout acima é para a revisão comum da ESP32-2432S028 (CYD); clones e revisões podem usar outro GPIO de backlight ou reset. Se o display ficar apagado, confira a revisão da placa e ajuste o `User_Setup.h` e `TFT_BACKLIGHT_PIN` antes de tentar outra configuração.

### Firmware e download para build local

Para compilar localmente, baixe `beeline-firmware.zip`, extraia os arquivos e abra `beeline-esp32/beeline-esp32.ino` no Arduino IDE. O ZIP inclui o sketch e este README; não são necessários headers ou assets extras. Não é necessária outra biblioteca Arduino além de **NimBLE-Arduino 2.x, ArduinoJson 7.x e TFT_eSPI** já usadas pelo firmware. Configure o `User_Setup.h` do display como antes e compile/grave normalmente.

## App Android e validação ponta a ponta

O app Android lê somente título/texto de notificações do pacote Google Maps depois que o usuário habilita o acesso especial nas configurações do Android. A velocidade vem do GPS/localização do telefone, não da notificação do Maps. Para continuar atualizando com o Maps em primeiro plano, o usuário precisa autorizar localização em segundo plano; o Android mostra uma notificação persistente enquanto o serviço de localização está ativo. BLE usa o serviço e a característica definidos acima. O Maps pode publicar formatos diferentes ou não publicar todos os dados; nesse caso, a instrução é mostrada como incompatível e não é enviada. Nenhuma API privada ou estado interno de rota é consultado.

O preview web e o Expo Go não incluem código Android customizado. Para testar em um telefone, gere uma build Android nativa com Node.js, pnpm, JDK e Android SDK configurados:

1. Envie o firmware à placa usando os passos de preparação acima. Mantenha a placa ligada e abra o monitor serial em `115200`; confirme `BeeLine-ESP32 ready`.
2. No diretório `artifacts/beeline-navigator`, rode `pnpm exec expo prebuild --platform android`.
3. Compile a build de diagnóstico com `cd android && ./gradlew assembleDebug`.
4. Instale `android/app/build/outputs/apk/debug/app-debug.apk` no telefone (por exemplo, `adb install -r app/build/outputs/apk/debug/app-debug.apk`).
5. Abra BeeLine, toque em **Acesso ao Google Maps** e permita o acesso nas configurações de **Acesso a notificações**. Volte ao app e confirme o status autorizado.
6. Toque em **Conectar BeeLine-ESP32** e aceite as permissões Bluetooth solicitadas. O app procura o nome e o UUID do serviço; confira o estado **CONECTADO**.
7. Toque em **Enviar instrução de teste**. O TFT deve mostrar a seta ligeiramente à direita, `350 m` e `Rua Haddock Lobo`; o monitor serial deve registrar `TURN=slight_right`.
8. Toque em **Ativar velocidade GPS**, permita localização durante o uso e depois localização em segundo plano. A notificação persistente do Android deve informar que a velocidade está sendo enviada.
9. Em local aberto, mova-se com o telefone para obter uma leitura GPS e confira o valor no app, o preenchimento do marcador na régua do TFT e uma linha `SPEED=... km/h` no monitor serial. Parado, a velocidade pode aparecer como zero conforme a leitura do GPS.
10. Desative o GPS, revogue a permissão ou interrompa as atualizações e confirme que a placa troca para `-- km/h` ao receber leitura inválida ou após oito segundos sem atualização. O aplicativo permite parar a captura pelo mesmo botão.
11. Desconecte a placa, confirme que o app indica a desconexão, ligue-a e conecte de novo. Inicie uma rota no Google Maps e confirme que uma notificação compatível ainda atualiza a instrução do app e do TFT.

## Limite de validação

O preview web não acessa BLE, notificações Android nem localização em segundo plano. A build e o fluxo com hardware precisam ser executados em um telefone e numa ESP32-2432S028 físicos; o código do firmware não altera hardware, display ou caixa.

### Verificações disponíveis e limite desta alteração

O pacote de firmware foi inspecionado para garantir que contém o sketch e a configuração de placa documentada.

A suíte do sketch foi executada com `ArduinoJson` **7.4.2** e interfaces simuladas de Arduino, FreeRTOS, NimBLE e TFT. Passaram os testes de recepção, navegação, velocidade, preenchimento cinza, base preta da régua e rotação. Não foi compilado nem gravado um binário para ESP32, nem medidos heap e latência na placa.

Para repetir, use uma cópia externa de ArduinoJson 7.x (não é necessário adicioná-la ao repositório):

```sh
ARDUINOJSON_DIR=/caminho/para/ArduinoJson \
  bash artifacts/beeline-navigator/firmware/tests/run-host-tests.sh
```

O diretório indicado deve conter `src/ArduinoJson.h`. O teste não baixa dependências automaticamente; usa `g++` disponível ou a variável `CXX`.

Casos cobertos pela suíte completa:

- Pacotes vazios, 181 bytes, JSON malformado, tipos incorretos, campos obrigatórios ausentes, NULs embutidos, dados após o objeto e profundidade seis são rejeitados sem alterar o último estado válido. Os limites de exatamente 180 bytes e profundidade cinco são aceitos.
- Uma navegação seguida de 10.000 velocidades continua disponível; cada posição pendente mantém somente a última mensagem válida de seu tipo. Um pacote rejeitado não limpa navegação pendente.
- Velocidade repetida renova a recepção sem desenho; leitura explicitamente inválida, negativa ou excessiva mostra indisponível. Timeout em 8000 ms, amostra pendente já vencida e volta do contador de tempo foram verificados.
- Limite de atualização, número e marcador visíveis iguais sem redesenho, escala de 0–140 com marcações adicionais 50/100, manobras, distância limitada, base preta da régua, preenchimento sólido, `r0`–`r3` e limite de leitura serial foram exercitados.
- As interfaces simuladas falham se o callback de escrita tentar desenhar ou imprimir; também falham se houver operações de TFT/serial dentro da seção crítica. O callback de desconexão foi invocado e reiniciou o advertising simulado.

### Ainda exige placa e telefone físicos

Não havia `arduino-cli` nem toolchain ESP32 configurada neste ambiente; **não foi compilado nem gravado um binário para a placa**. Antes de usar, compilar com as versões instaladas de ESP32, NimBLE-Arduino 2.x, ArduinoJson 7.x e TFT_eSPI, seguindo a preparação acima.

Na bancada, executar os passos Android já descritos, testar envio real de pacotes inválidos/rajadas, perda e recuperação de BLE e GPS, diagnóstico durante tráfego, timeout e restauração após rotação. Medir memória livre, latência e comportamento sob carga prolongada com BLE ativo. A reinicialização do advertising no teste local não confirma reconexão de rádio com telefone.

**O setup ST7789/BGR desta placa foi confirmado pelo usuário como solução do problema anterior de display.** Não alterar esse setup nem a pinagem sem evidência nova. Os UUIDs/JSON do Android não foram alterados.
