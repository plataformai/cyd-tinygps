---
name: Distribuição Android BeeLine
description: Decisões do usuário sobre exportação independente e identidade do Android.
---

O usuário escolheu exportar um projeto Android independente para download, sem retirar o app atual do monorepo ou perder o preview. O identificador confirmado para a distribuição independente é `app.beeline.navigator`.

**Why:** O objetivo é facilitar builds externos sem depender dos catálogos e pacotes do workspace; a escolha explícita foi exportar, não desvincular o app existente.

**How to apply:** Preservar o monorepo e seus serviços ao preparar entregas externas. Não interpretar o identificador antigo no preview como autorização para renomear instalações existentes; o novo identificador representa outro aplicativo no Android.

## Portabilidade dos downloads

O registry de pacotes do ambiente pode continuar sendo herdado mesmo ao instalar fora do monorepo. Uma instalação local bem-sucedida não comprova que os endereços de download de um lockfile sejam acessíveis em máquinas externas.

**Why:** Um export independente passou nos checks locais, mas ainda carregava URLs de tarballs de um registry privado do ambiente.

**How to apply:** Verificar também os destinos de download e a instalação com cache vazio e hosts privados indisponíveis. Não mudar as políticas de registry do workspace para resolver a portabilidade da entrega.

## Contrato do ZIP para EAS

O ZIP do BeeLine deve abrir diretamente na raiz do projeto Expo, sem uma pasta envoltória nem a raiz do monorepo. Não incluir `node_modules/`, `.expo/`, `android/`, `ios/`, `pnpm-lock.yaml`, `package-lock.json` ou `.npmrc`; o usuário cria o lockfile localmente ao executar `npm install`, antes dos checks Expo e da build EAS. Preservar o app original, o identificador `app.beeline.navigator` e a configuração de preview Android APK. Os passos completos e o comando gerador devem permanecer documentados junto ao exportador em `scripts/beeline-export/EXPORTACAO.md`.

**Why:** o usuário pediu uma exportação EAS autocontida sem lockfiles, com instalação inicial via npm, e pediu que essas instruções permaneçam para futuras gerações do ZIP.

**How to apply:** tratar o conteúdo de `scripts/beeline-export/EXPORTACAO.md` como o guia operacional completo; manter o teste estrutural do ZIP alinhado a este contrato. O lockfile interno de validação, se mantido, nunca deve ser copiado para o ZIP.
