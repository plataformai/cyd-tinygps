# Mensageria BeeLine — diagnóstico e plano

Análise de 04/11/2026. **Somente documentação: nenhuma funcionalidade foi implantada.**

## Leitura recomendada
1. [Diagnóstico](diagnostico.md): comportamento comprovado pelo código e riscos.
2. [Cobertura](matriz-cobertura.md): 66 manobras, estados, eventos e dados complementares.
3. [Inventário visual](matriz-visuais.md): Android e ESP32 separados, reaproveitamento e faltantes.
4. [Arquitetura proposta](arquitetura-proposta.md): catálogo, texto, protocolo e validade.
5. [Etapas de implantação](plano-implantacao.md).
6. [Verificação das fontes](verificacao-fontes.md) e [perguntas](perguntas-e-lacunas.md).
7. [Checklist futuro](../CHECKLIST_RELEASE.md).

Complementos: [visuais de estados e eventos](visuais-complementares.csv), [experimentos sintéticos](experimentos-parser.json), [proposta de correção da referência](proposta-diff-referencia.patch) e [validação desta entrega](validacao.md).

As matrizes também estão em CSV, para filtrar por família, cobertura, fonte ou ícone faltante. Os nomes camelCase expandem a referência recebida; **não são nomes Android verificados nesta análise**. Números de linha das citações correspondem ao código inspecionado nesta entrega.

## Conclusão
O projeto possui seis saídas de manobra, mas isso não corresponde a seis categorias Google reconhecidas fielmente: manter e leve são confundidos, e retorno não preserva sentido. Há componentes úteis a conservar: BLE serializado, parser defensivo no ESP32, telemetria independente, bússola por deslocamento GPS e desenho vetorial sem bitmaps.

Prioridade: impedir classificação falsa e seta antiga, depois ampliar a mensagem textual. Preservar os seis desenhos atuais **quando a semântica permitir**; nos outros casos mostrar texto com estado neutro, sem inventar seta. A família nova de imagens permanece posterior.

O catálogo pode representar todas as 66 entradas; **a fonte atual não garante dados para reconhecer todas elas**. SDK/API exigiriam outro fornecedor de navegação, não simplesmente permissão adicional para ler a rota do Maps.

Não houve consulta às páginas externas nesta análise. As alegações externas mantêm o nível e as ressalvas do material recebido. O registro de fontes usa resultado `nao_verificado`, data e hash nulos. A conferência ao vivo e o script estão planejados, não executados.
