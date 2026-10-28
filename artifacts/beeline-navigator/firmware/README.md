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

4. Abra `beeline-esp32.ino`, selecione **ESP32 Dev Module** e envie o sketch.
5. Abra o monitor serial em `115200` baud. O display mostra “Aguardando instrucoes” e o dispositivo anuncia `BeeLine-ESP32`.

## Contrato BLE

- Serviço: `7b1e0001-6d2a-4f7a-9c21-8e8a4b12a001`
- Escrita: `7b1e0002-6d2a-4f7a-9c21-8e8a4b12a001`
- Tipo: UTF-8 JSON, máximo de 180 bytes por comando.

Exemplo:

```json
{"type":"navigation","maneuver":"slight_right","distanceMeters":350,"road":"Rua Haddock Lobo","timestamp":1761600000000}
```

Manobras reconhecidas pelo protocolo: `straight`, `slight_left`, `left`, `slight_right`, `right` e `uturn`. O receptor valida o JSON e o tipo antes de entregar os dados à função da tela.

## Display e variantes da placa

O sketch desenha a seta, distância e nome da via em orientação horizontal. O pinout acima é para a revisão comum da ESP32-2432S028 (CYD); clones e revisões podem usar outro GPIO de backlight ou reset. Se o display ficar apagado, confira a revisão da placa e ajuste o `User_Setup.h` e `TFT_BACKLIGHT_PIN` antes de tentar outra configuração.

## App Android e validação ponta a ponta

O app usa um módulo Android nativo local: um `NotificationListenerService` lê somente título/texto de notificações do pacote Google Maps depois que o usuário habilita o acesso especial nas configurações do Android. BLE usa o serviço e a característica definidos acima. O Maps pode publicar formatos diferentes ou não publicar todos os dados; nesse caso, a instrução é mostrada como incompatível e não é enviada. Nenhuma API privada ou estado interno de rota é consultado.

O preview web e o Expo Go não incluem código Android customizado. Para testar em um telefone, gere uma build Android nativa com Node.js, pnpm, JDK e Android SDK configurados:

1. Envie o firmware à placa usando os passos de preparação acima. Mantenha a placa ligada e abra o monitor serial em `115200`; confirme `BeeLine-ESP32 ready`.
2. No diretório `artifacts/beeline-navigator`, rode `pnpm exec expo prebuild --platform android`.
3. Compile a build de diagnóstico com `cd android && ./gradlew assembleDebug`.
4. Instale `android/app/build/outputs/apk/debug/app-debug.apk` no telefone (por exemplo, `adb install -r app/build/outputs/apk/debug/app-debug.apk`).
5. Abra BeeLine, toque em **Acesso ao Google Maps** e permita o acesso nas configurações de **Acesso a notificações**. Volte ao app e confirme o status autorizado.
6. Toque em **Conectar BeeLine-ESP32** e aceite as permissões Bluetooth solicitadas. O app procura o nome e o UUID do serviço; confira o estado **CONECTADO**.
7. Toque em **Enviar instrução de teste**. O TFT deve mostrar a seta ligeiramente à direita, `350 m` e `Rua Haddock Lobo`; o monitor serial deve registrar `TURN=slight_right`.
8. Desconecte a placa, confirme que o app indica a falha/desconexão, ligue-a e conecte de novo.
9. Inicie uma rota no Google Maps e confirme que uma notificação compatível atualiza a instrução do app e do TFT. Se não houver uma instrução disponível, confirme que o app mostra a mensagem incompatível/aguardando sem enviar JSON inválido.

## Limite de validação

O preview web não acessa BLE nem notificações Android. A build e o fluxo com hardware precisam ser executados em um telefone e numa ESP32-2432S028 físicos; o código do firmware não altera hardware, display ou caixa.

O preview do app não acessa BLE nem notificações do Android. A integração nativa do app ainda precisa ser implementada; a versão visual serve para validar o fluxo e o formato do protocolo, não para parear um ESP32.
