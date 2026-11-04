# Checklist de release — proposta para execução quando solicitada

Não executado nesta análise. Itens manuais exigem Android/ESP32 reais.

- [ ] Executar o futuro comparador de fontes; revisar Markdown/JSON, inclusive fontes inconclusivas.
- [ ] Aprovar/rejeitar cada diff; atualizar versão/data da referência e changelog somente com aprovação.
- [ ] Registrar versões separadas da referência, catálogo, protocolo, app e firmware, hashes e compatibilidade.
- [ ] Regenerar tabelas Android/ESP32 do catálogo único e verificar ausência de divergência.
- [ ] Executar testes de parser, classificação conservadora, protocolo, CRC/UTF-8, invalidação/TTL, sequência/sessão, reconexão e fila limitada.
- [ ] Exercitar corpus real pt-BR, separado de sintéticos, anotando Maps/Android e modo carro/moto. Captura e anotação são **validação manual**.
- [ ] Conferir títulos, textos, subtextos e hash de ícone quando disponível; registrar ausência sem simular hash.
- [ ] Conferir quatro combinações app1/fw1, app1/fw2, app2/fw1 e app2/fw2; testar negociação de versão e MTU. **Validação manual**.
- [ ] Verificar nenhuma seta falsa para chegada/rotatória/faixa/unknown e nenhuma seta antiga após perda de fonte.
- [ ] Conferir desenho atual reaproveitado em cada categoria compatível; na fase de novos ícones, amostra de cada família no CYD, ambos os sentidos e lados. **Validação manual**.
- [ ] Conferir régua, cardeais em inglês, bússola GPS, pinagem/ST7789/BGR, fonte/acentos e corte de texto. **Validação manual**.
- [ ] Testar segundo plano, tela bloqueada, perda de BLE/GPS, permissão revogada e morte de processo. **Validação manual**.
- [ ] Medir latência p50/p95, perdas/timeouts, memória e consumo; anotar cenário e incerteza dos relógios. **Validação manual**, sem substituir por timings host.
- [ ] Preparar pacotes com exportadores documentados, compilar e verificar na bancada antes do uso.
- [ ] Registrar consentimento/sanitização do corpus e apagar capturas privadas conforme política aprovada.
