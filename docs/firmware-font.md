# Fonte de ícones no firmware BeeLine

## Origem e escolha
O usuário escolheu explicitamente manter a fonte TTF, e não substituí-la pelos PNGs/SVGs do arquivo Navigation Icons. A fonte e o `selection.json` fornecidos ficam em `artifacts/beeline-navigator/firmware/font-source/`, sem alterações. A licença MIT do material gráfico acompanhante está preservada no pacote.

## Por que 46 códigos não são 46 manobras
O IcoMoon dividiu ícones multicoloridos em camadas com códigos próprios. `selection.json` contém35 grupos; `properties.codes`, quando presente, lista as camadas na ordem de `icon.paths`. O campo `code` isolado não representa o sinal completo nesses casos. Onde não há `codes`, o glifo único pode já reunir vários contornos.

O gerador confere que todos os46 códigos da área privada no TTF estão mapeados exatamente uma vez. Também compara os limites dos contornos de cada glifo contra os paths correspondentes do selection, com tolerância1,5 unidade por causa da quantização/conversão de curvas. Não se trata de associação por semelhança visual ou de uma garantia de reconhecimento de notificações.

O mapa completo está em [firmware-font-codes.csv](firmware-font-codes.csv). Os nomes IcoMoon, inclusive `nav_direction_arrive_leftpng`, são preservados exatamente como recebidos. Nenhum código é uma letra normal A–Z.

## Uso atual
Somente seis grupos são incorporados: straight=E91E, slight_left=E920, left=E922, slight_right=E91F, right=E921 e uturn=E92D. Todos são glifos únicos. Cada máscara é obtida renderizando seu código no próprio TTF, removendo margens transparentes e centralizando em96×96; bits MSB-first, limiar128. Os seis compartilham o mesmo tamanho de fonte de origem.

TFT_eSPI `drawBitmap` desenha as máscaras PROGMEM em âmbar com zeros transparentes. Não há nova biblioteca, mecanismo de fonte em tempo real, arquivo a carregar na placa ou framebuffer adicional. Custo exato dos pixels:6×96×96/8=6912 bytes de flash. Flash total/heap físico e aparência no painel não foram medidos por essa conta.

Unknown exibe `?` sem seta. A identidade para atualização passa a ser o índice do ícone, não o ângulo antigo; assim unknown↔straight força redesenho.

## Construção e regeneração
O Arduino IDE precisa do `.ino` **e** de `navigation-font.h` na mesma pasta. O exportador de firmware inclui ambos e a licença. O TTF/selection são fontes de geração, não arquivos necessários na placa.

Para quem mantém o repositório, use Python com os pacotes de `scripts/beeline-font-requirements.txt` e execute:
```
python scripts/generate-beeline-font.py --check
python scripts/generate-beeline-font.py --preview
python scripts/package-beeline-firmware.py
```
O primeiro comando verifica o par TTF/selection e derivados; o segundo regenera header/mapa e pranchas. O gerador é determinístico com as versões indicadas.

## Limites de escopo
Não modifica APK, parser de notificações, UUIDs, JSON, velocidade, rumo GPS, distância restante, orientação/pinagem/ST7789/BGR, régua ou cores. A coleção não cobre todas as66 manobras da referência (por exemplo, faltam rotatórias). O catálogo completo não ativa automaticamente35 mensagens no app/firmware. O relatório anterior em docs/mensageria é uma análise histórica; este documento registra a posterior implantação visual.

## Verificação da implantação
O gerador e seus testes conferem cobertura46/35, camadas, correspondência de limites dos contornos, arquivos derivados, tamanho/ordem dos bits e rejeição de mapa/contorno adulterado. Os testes host compilam o sketch real com ArduinoJson7.4.2 e interfaces simuladas: verificam escolha de cada máscara, limites nas quatro rotações, desconhecido sem seta, redraw unknown↔straight e preservação de BLE/telemetria/régua.

O ZIP foi conferido quanto à integridade e igualdade com os quatro arquivos atuais. As pranchas foram geradas a partir do TTF e das máscaras exatas, não de imagens substitutas. Não foi compilado binário para ESP32 nem houve teste no telefone/placa física; as pranchas não são fotografia do display nem medida de legibilidade real.
