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
