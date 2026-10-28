# BeeLine Navigator — firmware ESP32

Este sketch cria um periférico BLE chamado `BeeLine-ESP32` e recebe cada instrução como JSON pela característica GATT de escrita. Ele funciona como base do firmware; a saída de tela está isolada em `renderInstruction()` porque o controlador/display do hardware ainda precisa ser escolhido.

## Preparar

1. Instale o Arduino IDE e o suporte às placas ESP32.
2. Instale as bibliotecas **NimBLE-Arduino 2.x** e **ArduinoJson 7.x**.
3. Abra `beeline-esp32.ino`, selecione sua placa e porta serial e envie o sketch.
4. Abra o monitor serial em `115200` baud. O dispositivo anuncia `BeeLine-ESP32`.

## Contrato BLE

- Serviço: `7b1e0001-6d2a-4f7a-9c21-8e8a4b12a001`
- Escrita: `7b1e0002-6d2a-4f7a-9c21-8e8a4b12a001`
- Tipo: UTF-8 JSON, máximo de 180 bytes por comando.

Exemplo:

```json
{"type":"navigation","maneuver":"slight_right","distanceMeters":350,"road":"Rua Haddock Lobo","timestamp":1761600000000}
```

Manobras reconhecidas pelo protocolo: `straight`, `slight_left`, `left`, `slight_right`, `right` e `uturn`. O receptor valida o JSON e o tipo antes de entregar os dados à função da tela.

## Integração da tela

Implemente `renderInstruction()` para desenhar a seta e a distância no display instalado. O sketch atual só escreve a instrução no monitor serial; ele não presume pinos, resolução ou controlador de tela.

## Limite da primeira versão

O preview do app não acessa BLE nem notificações do Android. O app Android em funcionamento precisa de um build nativo com acesso a notificações e Bluetooth Low Energy; a versão visual no preview serve para validar o fluxo e o formato do protocolo, não para parear um ESP32.
