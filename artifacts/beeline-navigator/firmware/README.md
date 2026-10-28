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

## Limite da primeira versão

O preview do app não acessa BLE nem notificações do Android. A integração nativa do app ainda precisa ser implementada; a versão visual serve para validar o fluxo e o formato do protocolo, não para parear um ESP32.
