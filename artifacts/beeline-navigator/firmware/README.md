# BeeLine Navigator — firmware ESP32

Este sketch cria um periférico BLE chamado `BeeLine-ESP32`, recebe cada instrução como JSON pela característica GATT de escrita e desenha a seta no display integrado da placa ESP32-2432S028.

## Preparar

1. Instale o Arduino IDE e o suporte às placas ESP32.
2. Instale as bibliotecas **NimBLE-Arduino 2.x**, **ArduinoJson 7.x** e **TFT_eSPI**.
3. Configure o `User_Setup.h` do TFT_eSPI para o CYD comum:

   ```cpp
   #define ILI9341_DRIVER
   #define TFT_WIDTH  240
   #define TFT_HEIGHT 320
   #define TFT_MISO 12
   #define TFT_MOSI 13
   #define TFT_SCLK 14
   #define TFT_CS   15
   #define TFT_DC    2
   #define TFT_RST  -1
   #define TFT_BL   21
   #define TFT_BACKLIGHT_ON HIGH
   ```

4. Abra `beeline-esp32.ino`, selecione **ESP32 Dev Module** e envie o sketch. Na inicialização, a tela mostra por 1,2 segundo uma moldura de teste, a orientação detectada e a resolução; depois volta à tela normal do BeeLine.
5. Abra o monitor serial em `115200` baud. O resultado esperado é `TFT orientation: 320 x 240 (expected 320 x 240)`, seguido da tela normal e do anúncio `BeeLine-ESP32`.
6. Se a resolução não for `320 x 240`, confira se `User_Setup_Select.h` seleciona o `User_Setup.h` que você editou e grave o sketch novamente. A seta, BLE, navegação e velocidade continuam sendo desenhadas pelo firmware BeeLine.

## Contrato BLE

- Serviço: `7b1e0001-6d2a-4f7a-9c21-8e8a4b12a001`
- Escrita: `7b1e0002-6d2a-4f7a-9c21-8e8a4b12a001`
- Tipo: UTF-8 JSON, máximo de 180 bytes por comando.

Exemplo:

```json
{"type":"navigation","maneuver":"slight_right","distanceMeters":350,"road":"Rua Haddock Lobo","timestamp":1761600000000}
```

Manobras reconhecidas pelo protocolo: `straight`, `slight_left`, `left`, `slight_right`, `right` e `uturn`. O receptor valida o JSON e o tipo antes de entregar os dados à função da tela.

Atualização de velocidade enviada pelo app Android:

```json
{"type":"speed","speedKmh":72.4,"valid":true,"timestamp":1761600000000}
```

Quando o GPS não fornece uma leitura válida, o app envia `{"type":"speed","speedKmh":null,"valid":false,"timestamp":...}`. A placa mostra `-- km/h` após uma leitura inválida ou oito segundos sem atualização.

## Display e variantes da placa

O sketch desenha a seta, distância e nome da via em orientação horizontal. No canto inferior, mostra o valor atual e uma régua colorida de 0 a 140 km/h, com graduações de 20 em 20 e marcações destacadas em 50 e 100 km/h. O indicador visual fica limitado à escala; o número mostra a velocidade recebida. O pinout acima é para a revisão comum da ESP32-2432S028 (CYD); clones e revisões podem usar outro GPIO de backlight ou reset. Se o display ficar apagado, confira a revisão da placa e ajuste o `User_Setup.h` e `TFT_BACKLIGHT_PIN` antes de tentar outra configuração.

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
