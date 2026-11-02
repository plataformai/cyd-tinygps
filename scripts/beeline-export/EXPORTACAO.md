# Manter a exportação independente

A fonte do app continua em `artifacts/beeline-navigator`. O processo de exportação
está em `scripts/export-beeline.py`; o ZIP não é uma segunda cópia editada à mão.

```sh
python3 scripts/export-beeline.py
```

Saída: `generated-artifacts/beeline-apk.zip`, ignorada por Git e deploy.
Inclui somente os diretórios de código/assets necessários, configurações Expo,
módulo Android local e documentação deste diretório. Não inclui o firmware.

O manifesto independente é derivado do app e filtrado pelo exportador. O npm lock
deste diretório é gerado em pasta temporária fora do monorepo, sem configurações
Linux-only da raiz. Se mudar as dependências do app:

```sh
python3 scripts/export-beeline.py --refresh-lock
```

Esse comando consulta o registry, regenera o npm lock e exporta novamente.
Revise o diff do lock e valide **o ZIP extraído em pasta fora do monorepo** com
npm ci, TypeScript, Expo install --check, Expo Doctor, prebuild e autolinking.
Não substitua o npm lock pelo pnpm lock da raiz.

O registry do ambiente também pode aparecer nos campos resolved do npm lock,
mesmo gerando o arquivo fora do monorepo. O exportador normaliza o proxy conhecido
para https://registry.npmjs.org, sem alterar versões nem integridades, e rejeita
qualquer URL privada ou registry inesperado antes de empacotar. Para corrigir
somente URLs de um lock já existente: `python3 scripts/export-beeline.py --normalize-lock`.

Verificar a portabilidade dos downloads em uma extração nova com cache npm vazio,
`--registry=https://registry.npmjs.org` e
`NODE_OPTIONS=--require=/caminho/absoluto/scripts/beeline-export/deny-private-registry.cjs`.
O preload de teste bloqueia hosts privados, não é incluído no ZIP e não altera o
registry nem as políticas de instalação do workspace original.

Checks estruturais rápidos do ZIP:

```sh
python3 scripts/test-beeline-export.py
```

Na pasta isolada, após npm ci, rode os checks do README, depois:

```sh
CI=1 npx expo prebuild --platform android --no-install
CI=1 npx expo export --platform android --output-dir bundle-check
```

Na raiz original, execute os checks adicionais com
`BEELINE_VERIFY_DIR=/caminho/absoluto/do/projeto/extraido python3 scripts/test-beeline-export.py`.
Eles inspecionam o AndroidManifest gerado, identificador, autolinking local e bundle
Hermes. As regras tools:node=remove são bloqueios de permissões para o merger do
Gradle, não solicitações ativas de permissão. A inspeção do manifest gerado não
substitui verificar o manifest final do APK compilado.

Foram auditados imports no app, componentes, hooks, tarefas de localização,
ponte Android e plugins, além de peers do Expo Router. Os pacotes removidos
apareciam apenas no manifesto; expo-symbols é peer opcional do Router e o app usa
Stack, não NativeTabs. Expo Modules Core continua como infraestrutura transitiva,
não como dependência direta do app independente.

Exceção confirmada no prebuild: expo-system-ui é necessário para
userInterfaceStyle=automatic no Android, apesar de não ter import direto. Foi
mantido em dependencies para preservar o comportamento configurado.

Preservar as versões do SDK atual. Não usar este exportador como migração de Expo
nem alterar os outros serviços do workspace. As correções públicas dos imports do
plugin/ponte são compartilhadas na fonte; scripts e dependências específicos do
Replit só são excluídos na exportação.

O npm lock resolveu Expo 58.0.22 dentro do intervalo existente. Seu diagnóstico
exige React Native 0.88.1 e @types/react-dom ~19.3.0; o exportador aplica esses dois
ajustes de patch na exportação, sem mudar a versão principal do SDK ou o preview
original. Ao atualizar o lock, conferir novamente expo install --check.
