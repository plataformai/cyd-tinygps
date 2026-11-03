# BeeLine Navigator — projeto Android independente

Este ZIP contém código-fonte, **não um APK compilado**. Depois de extrair, a pasta do
projeto deve conter diretamente `package.json`, `app.json`, `eas.json`, `plugins/` e
os diretórios do app. Não há uma pasta `beeline-apk/` envolvendo esses arquivos.
Trabalhe fora do monorepo. Não crie um projeto vazio no Android Studio nem copie
estes arquivos sobre outro projeto Android.

O app usa código Android local para ler notificações do Google Maps e comunicar
por BLE com a ESP32. A velocidade e o rumo de deslocamento vêm da localização do telefone. **Expo Go e
preview web não executam essas integrações nativas.**

## Diagnosticar instruções do Maps

Com o BLE conectado e o acesso especial às notificações autorizado, inicie a
navegação no Maps. O listener recupera notificações já ativas quando é conectado
pelo Android e não apaga uma instrução mais recente ao remover outra notificação.
O app distingue estes estados:

- **Aguardando instrução do Maps:** nenhuma notificação foi recebida. Confira se o
  Maps publica a navegação na central de notificações do celular.
- **Notificação do Maps não reconhecida:** chegou conteúdo, mas o texto não tem
  uma manobra compatível. Toque em **Ver texto recebido do Maps** para inspecionar
  os campos acessíveis ao Android. Esse texto é exibido apenas no celular e pode
  conter vias/destinos; não é enviado a servidor.
- **Instrução do Maps enviada ao ESP32:** o app reconheceu a instrução e a escrita
  BLE foi confirmada.

Os testes do parser usam exemplos sintéticos; não substituem a confirmação com
uma rota real. Atualizar este código nativo exige gerar e instalar um novo APK,
e esta versão com bússola/distância restante também exige atualizar o firmware da placa.

## Bússola GPS e distância ao destino

- A bússola mostra rumo de deslocamento, não a direção física do celular. Ative
  **Ativar GPS: velocidade e rumo**, permitindo localização em segundo plano.
- O rumo exige fix de até 5 segundos, precisão horizontal de até 35 m e deslocamento
  de pelo menos 5,4 km/h. Parado ou sem dado confiável aparece `—`; na placa, `--`.
- A distância ao destino vem do resumo público da notificação do Maps, não da
  próxima curva nem de um cálculo em linha reta. Funciona independentemente de
  reconhecer a manobra. Resumos ausentes/incompatíveis ficam indisponíveis.
- O resumo é enviado pelo Android nativo enquanto o Maps está em primeiro plano.
  Reenvios a cada 5 segundos não renovam uma fonte com mais de 120 segundos.
  Remoção da rota ou perda de validade limpa o valor; o firmware também expira o
  resumo em no máximo 30 segundos sem renovação e o rumo em 8 segundos.
- As escritas BLE são serializadas. Se uma confirmação expirar, reconecte à placa.
- **Enviar instrução de teste** envia curva a 350 m, **45° NE e 3,9 km** por até
  oito segundos. O app identifica esses valores como TESTE; dados reais podem
  substituí-los. Eles não são uma medição real de localização.
- Os ZIPs continuam sendo código-fonte: é necessário compilar/gravar o firmware e
  gerar/instalar um novo APK. Não há nova biblioteca Arduino nem sensor adicional.

## Requisitos

- Node **22.13.0** e npm (sugestão: npm 10.9.2, incluído nessa versão do Node).
- Expo SDK **58**, mantido sem downgrade. Expo e React Native desse SDK aceitam
  Node `^22.13.0 || ^24.3.0 || >=26.0.0`. O perfil EAS usa 22.13.0.
- Internet para baixar dependências diretamente do registry público do npm.
- O ZIP não inclui `package-lock.json`. O primeiro `npm install` cria um lockfile
  local para o seu projeto; mantenha esse arquivo no seu próprio Git.

## Instalar e verificar

Abra um terminal **na pasta que contém este README e package.json**:

```sh
npm install --registry=https://registry.npmjs.org
npx expo install --check
npm run typecheck
npx --yes expo-doctor@1.21.5
```

Use npm neste projeto. O comando inicial cria `package-lock.json` somente no projeto
extraído. Depois desse primeiro install, `npm ci` pode ser usado para reinstalações
reproduzíveis. Não copie `node_modules`, lockfiles nem `.npmrc` da pasta pai e não
use `--legacy-peer-deps` para esconder incompatibilidades.
Se o Expo Doctor não conseguir consultar serviços externos, diferencie esse
problema de rede de uma falha de código.

## Gerar APK na nuvem com EAS

Não é preciso ter Android Studio para esta opção. É necessária uma conta Expo;
a build pode consumir a cota ou gerar custo conforme o plano da conta.

```sh
git init
npx eas-cli@latest login
npx eas-cli@latest init
git add .
git commit -m "Initial BeeLine Android project"
npx eas-cli@latest build -p android --profile preview
```

No `init`, crie/vincule um projeto **da sua própria conta** e mantenha as
configurações do perfil preview no `eas.json`. A CLI pode adicionar o projectId ao
app.json; faça o commit depois dessa etapa, incluindo o `package-lock.json` local
gerado pelo `npm install`. O EAS Build precisa de um repositório Git inicializado e
com ao menos um commit. Se o Git solicitar uma identidade, configure-a localmente.
Não há projectId de outra pessoa nem credenciais neste pacote.
O perfil preview pede distribuição interna e saída APK. Baixe o APK no endereço
fornecido ao final pelo EAS e instale no telefone autorizando a origem do arquivo.
Nenhuma build EAS foi iniciada automaticamente para preparar este ZIP.

## Gerar APK localmente com Android Studio

Instale Android Studio, Android SDK e o JDK exigido pelo projeto Gradle gerado.
Configure `ANDROID_HOME` e `JAVA_HOME`. Em seguida, na raiz deste projeto:

```sh
npm install --registry=https://registry.npmjs.org
npx expo install --check
npx expo prebuild --platform android
```

Agora abra no Android Studio **a pasta android gerada**, não um novo projeto.
Deixe o Gradle sincronizar e instale os componentes do SDK solicitados.
Para um APK de teste que contém o bundle JavaScript e não precisa de Metro:

Windows, PowerShell:

```powershell
cd android
.\gradlew.bat assembleRelease
```

macOS/Linux:

```sh
cd android
./gradlew assembleRelease
```

O resultado costuma estar em
`android/app/build/outputs/apk/release/app-release.apk`. Confira o caminho
informado pelo Gradle. O projeto gerado pelo Expo normalmente usa a chave de debug
na variante release: isso serve para teste local, **não é assinatura de produção**.
A assinatura usada numa build EAS pode ser diferente; não misture os dois métodos
para atualizar a mesma instalação sem planejar a assinatura.

`assembleDebug`/`npm run android` são alternativas de desenvolvimento, normalmente
dependentes de Metro; não confunda esse APK com uma build de distribuição autônoma.

## Identificador Android e permissões

- Nome no celular: **BeeLine Navigator**.
- Identificador: **app.beeline.navigator**.
- Esse app é diferente do antigo `com.anonymous.beelinenavigator`: não o atualiza
  automaticamente e não herda seus dados nem autorizações especiais.
- Ao trocar de instalação, será necessário autorizar novamente acesso às
  notificações, Bluetooth e localização.
- Somente as notificações do pacote Google Maps são tratadas pela integração.
- Localização em segundo plano e serviço de localização são necessários para
  enviar velocidade e rumo enquanto o Google Maps estiver em primeiro plano.
- Acesso a câmera, microfone, galeria e armazenamento compartilhado não é necessário.

O módulo `modules/beeline-android` deve ser preservado: o autolinking local do Expo
o registra e o plugin configura o serviço de acesso às notificações. O código
Android depende internamente da infraestrutura Expo Modules, embora o app não
declare `expo-modules-core` diretamente. Não remova essa infraestrutura do Gradle.

## Teste no telefone e na ESP32

1. Ligue a ESP32 com o firmware BeeLine já validado para seu display.
2. Instale o APK, abra o app e conceda acesso às notificações nas configurações.
3. Autorize Bluetooth, conecte a BeeLine-ESP32 e envie a instrução de teste.
   A busca aceita o UUID anunciado ou o nome do dispositivo; a conexão só é
   confirmada depois de localizar o serviço e a característica GATT.
   Se a busca expirar, confira a placa no nRF Connect. Um dispositivo N/A pode
   ser a ESP32: verifique os UUIDs, sem escrever valores em dispositivos
   desconhecidos. Ver um UUID depois de conectar não comprova que ele era
   anunciado antes da conexão. Desconecte o nRF Connect antes de usar o BeeLine.
4. Ative o GPS (velocidade e rumo) e autorize a localização solicitada, inclusive em
   segundo plano para uso junto com o Maps.
5. Confira instruções de uma rota real, reconexão BLE e perda de atualização GPS.
   Não faça esses testes operando o telefone enquanto dirige.
6. Compare a distância restante com o resumo do Maps. Parado, o rumo deve ficar
   indisponível; em deslocamento confiável, confirme graus/cardinal e as letras
   girando no display. Encerre a rota e confira que não fica uma distância antiga.

A geração da estrutura Android (prebuild) não compila Kotlin/Gradle nem valida
hardware. Consulte `VALIDACAO.md`, quando incluído, para os checks realmente feitos
na entrega.
