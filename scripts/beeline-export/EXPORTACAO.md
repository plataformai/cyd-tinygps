# Manter o pacote Android para EAS

## Fonte e comando de geração

A fonte do aplicativo permanece em `artifacts/beeline-navigator`. O ZIP deve ser
produzido pelo exportador versionado, não por cópia ou edição manual de uma
exportação antiga:

```sh
python3 scripts/export-beeline.py
```

O resultado é `generated-artifacts/beeline-apk.zip`, diretório ignorado por Git e
deploy. Ao extrair o arquivo, `package.json`, `app.json`, `eas.json`, `plugins/` e
os diretórios do app ficam diretamente na raiz do projeto. Não deve haver pasta
`beeline-apk/` envolvendo o projeto nem uma cópia da raiz do monorepo.

O pacote é código-fonte para build, não um APK pronto. O exportador não executa
`eas build`, não cria credenciais, não publica na Play Store e não altera o app ou
os serviços do monorepo.

## Conteúdo obrigatório e exclusões

Incluir o manifesto Expo/npm, configuração EAS, README de uso, código do app,
assets, configuração Metro/TypeScript, plugin `plugins/with-beeline-android.js`
e o módulo nativo Android local com seu código-fonte. O ID Android do app
exportado deve continuar `app.beeline.navigator`; não trocar o nome visível
BeeLine Navigator nem as integrações nativas necessárias para BLE, notificações
do Maps e GPS.

O ZIP não inclui:

- A raiz do monorepo, o firmware, backend, `server/`, scripts e configurações
  exclusivos do Replit.
- `node_modules/`, `.expo/`, a pasta Android gerada na raiz (`android/`) ou
  `ios/`. Preservar `modules/beeline-android/android/`, que é o código-fonte do
  módulo nativo e é necessário ao autolinking.
- `pnpm-lock.yaml`, `package-lock.json` ou `.npmrc`. Não incluir URLs privadas,
  referências `workspace:`/`catalog:` nem os nomes `REPLIT_*` na configuração
  exportada.

O arquivo `scripts/beeline-export/package-lock.json` é exclusivamente uma
referência interna para conferir dependências, versões e integridades durante a
geração. Ele não é copiado para o ZIP. O primeiro `npm install` do usuário cria
`package-lock.json` somente na cópia extraída; esse arquivo local deve ser
mantido no Git do próprio usuário.

## Manifesto independente e alterações de dependências

O exportador deriva `package.json` de `artifacts/beeline-navigator/package.json`,
remove os vínculos de workspace e scripts do Replit, coloca as bibliotecas de
runtime em `dependencies` e deixa apenas ferramentas de desenvolvimento em
`devDependencies`. React Query usa `^5.90.21`. O nome do pacote npm é
`beeline-apk`; isso não altera o ID Android escolhido. Não remover dependências
indiretas que o Expo ou o prebuild precisam, mesmo se não houver import direto.

Quando as dependências do app mudarem, audite os imports, plugins e peers, depois
regenere a referência interna e o ZIP:

```sh
python3 scripts/export-beeline.py --refresh-lock
```

Esse comando atualiza o lockfile de validação numa pasta temporária fora do
monorepo, normaliza os endereços para `https://registry.npmjs.org` e rejeita
qualquer URL privada ou dependência vinculada localmente. Revise o diff do lock
interno. `--normalize-lock` corrige apenas os URLs conhecidos do lock interno,
sem trocar versões nem integridades. Não use o lock do pnpm do monorepo.

O plugin Android precisa importar `expo/config-plugins`. A ponte nativa usa a API
pública de `expo`; `expo-modules-core` permanece infraestrutura transitiva, não
uma dependência direta. Preserve `modules/beeline-android/android/` e o
`expo-module.config.json` para autolinking.

Preserve a versão atual do Expo SDK; a exportação não é uma migração de SDK. As
correções de compatibilidade já confirmadas para React Native e
`@types/react-dom` ficam no exportador e na referência interna. Rode novamente
`npx expo install --check` quando atualizar dependências ou SDK. `expo-system-ui`
é necessário ao `userInterfaceStyle: automatic` e não deve ser removido por falta
de import explícito.

## Configuração EAS

O modelo `scripts/beeline-export/eas.json` deve manter:

- `cli.version` como `>= 5.9.0` e `cli.appVersionSource` como `local`.
- Build `preview` com `distribution: internal`, Node `22.13.0` e
  `android.buildType: apk`.
- `production: {}` vazio.

Não adicionar `EAS_PNPM_FILTER_WORKSPACE`. O perfil e o identificador
`app.beeline.navigator` são específicos da cópia Android exportada; não altere o
identificador nem remova permissões necessárias ao BLE, à localização em
segundo plano ou ao serviço de notificações. Não inclua `projectId` de outra
conta Expo nem credenciais no pacote.

## Validar o ZIP gerado

Execute os testes estruturais depois de cada exportação:

```sh
python3 scripts/test-beeline-export.py
```

Eles verificam que o conteúdo está na raiz, que os arquivos proibidos não foram
incluídos, que a identidade, as dependências e as configurações Expo/EAS estão
corretas e que os imports e arquivos do módulo nativo permanecem no pacote.

Para testar a primeira instalação de forma reproduzível, extraia para uma pasta
temporária **fora do monorepo**, com cache npm vazio e registry público. O preload
bloqueia acesso acidental a hosts privados; não é adicionado ao ZIP nem muda a
configuração de pacotes do workspace:

```sh
tmp="$(mktemp -d)"
unzip generated-artifacts/beeline-apk.zip -d "$tmp"
cd "$tmp"
NODE_OPTIONS="--require=/caminho/absoluto/scripts/beeline-export/deny-private-registry.cjs" \
  npm install --registry=https://registry.npmjs.org --userconfig=/dev/null \
  --cache="$tmp/npm-cache" --no-audit --no-fund
npx expo install --check
npm run typecheck
npx --yes expo-doctor@1.21.5
CI=1 npx expo prebuild --platform android --no-install
CI=1 npx expo export --platform android --output-dir bundle-check
```

`npm install` gera um lockfile nessa extração temporária, não no ZIP. Depois
desse primeiro install, `npm ci` pode ser usado ali. Na raiz do monorepo, os
checks adicionais do Android gerado podem ser feitos com
`BEELINE_VERIFY_DIR=/caminho/absoluto/do/projeto/extraido python3 scripts/test-beeline-export.py`.
Eles verificam o manifest gerado, o ID Android, o autolinking e o bundle Hermes.
Não equivalem a compilar/assinar um APK nem a testá-lo num telefone físico.

## Passo a passo do usuário: gerar o APK com EAS

1. Extraia o ZIP para uma pasta nova. Abra um terminal **na raiz que contém
   `package.json`**. Não crie um projeto vazio no Android Studio nem copie os
   arquivos para outro projeto.
2. Instale Node `22.13.0` e npm compatível. Rode
   `npm install --registry=https://registry.npmjs.org`; isso cria
   `package-lock.json` na cópia local, não no ZIP.
3. Confira a compatibilidade Expo com `npx expo install --check`. Também é
   recomendado executar `npm run typecheck` e `npx --yes expo-doctor@1.21.5`.
4. Inicialize Git com `git init`, use a sua própria conta com
   `npx eas-cli@latest login` e vincule/crie o projeto Expo com
   `npx eas-cli@latest init`.
5. O `eas init` pode atualizar `app.json` com o ID do projeto da sua conta.
   Configure a identidade Git se solicitada, adicione e faça commit da
   configuração e do lockfile gerado:
   `git add .` e `git commit -m "Initial BeeLine Android project"`.
6. Gere o APK de teste interno com
   `npx eas-cli@latest build -p android --profile preview`.
7. Baixe o APK indicado pelo EAS e instale no telefone, autorizando a origem do
   arquivo nas configurações do Android. A primeira instalação usa o novo ID
   `app.beeline.navigator` e não herda dados ou permissões da instalação antiga.

O EAS exige repositório Git inicializado e commit antes de iniciar a build. O
projeto Expo deve pertencer à própria conta do usuário. A build pode usar cota ou
gerar custos da conta; confira antes de executá-la. Este agente apenas prepara e
valida o ZIP e não faz login, vincula contas nem inicia a build EAS.

Para compilar localmente, Android Studio, Android SDK e JDK são necessários.
Depois da instalação inicial, rode `npx expo prebuild --platform android` e abra
a pasta `android/` gerada no Android Studio — não crie outro projeto. O README
incluído descreve os comandos Gradle para gerar um APK local de teste e distingue
essa build de uma assinatura de produção.
