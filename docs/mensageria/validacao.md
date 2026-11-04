# Validação desta entrega documental

Data: 04/11/2026.

- Matrizes: 66 manobras únicas nas duas versões (Markdown/CSV), mesmos nomes e ordem; 14 eventos e22 estados/dados adicionais, total102 linhas de cobertura. Complemento visual com36 itens.
- Registro de fontes: 15 URLs da seção13, todas sem consulta externa e com data/hash nulos. Um domínio externo (Pub.dev) reservado para revisão manual.
- Referência copiada byte a byte; comparação binária e SHA-256 confrontados com o anexo.
- Código Android/Expo, firmware e exportador não alterados; verificação de diff nessas áreas.
- Executado `node --experimental-strip-types --test artifacts/beeline-navigator/tests/navigation.test.mjs artifacts/beeline-navigator/tests/heading.test.mjs`: **9 testes passaram, 0 falharam**.
- Node emitiu aviso `MODULE_TYPELESS_PACKAGE_JSON`: fez releitura dos arquivos TypeScript como ES modules; não houve falha. Não alterar package.json em uma tarefa documental.
- Executadas12 frases sintéticas diretamente no parser atual, com saída em experimentos-parser.json. Exemplo observado: “Seu destino está à direita” retorna right; “Vire levemente à direita” também retorna right; distância ausente retorna0. São diagnósticos de lacunas, não testes de uma correção já implantada.
- Proposta de diff da referência separada e validada como patch aplicável, **sem aplicação**. Trata chegada ambígua, precedência de mudança de nome e exemplo que presume circulação/confiança sem evidência.
- Nenhum build APK/ESP32, fonte externa, screenshot de interface, teste BLE físico ou medição de latência foi executado. Não houve alteração de interface que justificasse iniciar/reiniciar workflows.

Aceites físicos permanecem no checklist futuro. O inventário de 66 entradas não constitui alegação de 66 manobras reconhecidas.
