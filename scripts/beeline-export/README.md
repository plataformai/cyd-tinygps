# BeeLine Navigator — projeto Android independente

Este ZIP contém código-fonte, **não um APK compilado**. Depois de extrair, a pasta do
projeto deve conter diretamente `package.json`, `app.json`, `eas.json`, `plugins/` e
os diretórios do app. Não há uma pasta `beeline-apk/` envolvendo esses arquivos.
Trabalhe fora do monorepo. Não crie um projeto vazio no Android Studio nem copie
estes arquivos sobre outro projeto Android.

O app usa código Android local para ler notificações do Google Maps e comunicar
por BLE com a ESP32. A velocidade vem da localização do telefone. **Expo Go e
preview web não executam essas integrações nativas.**

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
  enviar velocidade enquanto o Google Maps estiver em primeiro plano.
- Acesso a câmera, microfone, galeria e armazenamento compartilhado não é necessário.

O módulo `modules/beeline-android` deve ser preservado: o autolinking local do Expo
o registra e o plugin configura o serviço de acesso às notificações. O código
Android depende internamente da infraestrutura Expo Modules, embora o app não
declare `expo-modules-core` diretamente. Não remova essa infraestrutura do Gradle.

## Teste no telefone e na ESP32

1. Ligue a ESP32 com o firmware BeeLine já validado para seu display.
2. Instale o APK, abra o app e conceda acesso às notificações nas configurações.
3. Autorize Bluetooth, conecte a BeeLine-ESP32 e envie a instrução de teste.
4. Ative a velocidade GPS e autorize a localização solicitada, inclusive em
   segundo plano para uso junto com o Maps.
5. Confira instruções de uma rota real, reconexão BLE e perda de atualização GPS.
   Não faça esses testes operando o telefone enquanto dirige.

A geração da estrutura Android (prebuild) não compila Kotlin/Gradle nem valida
hardware. Consulte `VALIDACAO.md`, quando incluído, para os checks realmente feitos
na entrega.
