RVOXX — prévia para Windows x64, em C++17/JUCE 8.0.12.

- Interface clara restaurada: painel cinza, medidores azuis e cursores laranja, conforme a referência original. Sem madeira ou elementos vintage.
- Corrigida a falsa redução vermelha em silêncio: o DSP continua expandindo normalmente, mas o medidor não interpreta ganho aplicado a silêncio como redução de um sinal mensurável.
- Gate inicia em −Inf; entrada e saída silenciosas exibem −Inf. O valor de Comp fica dentro do cursor e acompanha automação, sem duplicação de números.
- Mantida a correção de OFF em 192 kHz. Inclui testes para silêncio, ruído real, automação, valores iniciais e renderização da interface em repouso e com áudio.

- VST3 mono/estéreo, automação, estado da sessão, gate, compressor, limitador, medidores e presets.
- Baixe `RVOXX-Windows-x64.zip`, extraia e copie a pasta inteira `RVOXX.vst3` para `C:\Program Files\Common Files\VST3\`.
- `RVOXX-Standalone-Windows-x64.zip` contém o aplicativo para execução fora da DAW.
- O processamento é independente e aproximado; não usa o algoritmo da Waves nem promete equivalência sonora.
- A imagem aprovada ainda depende do envio do arquivo original. O botão `Imagem` permite abrir um PNG/JPG local sem modificar o arquivo; a imagem completa é mostrada acima dos controles ao vivo. Para embutir na compilação, forneça `Assets/imagem.png` ou configure `RVOXX_ARTWORK`.
- Este release é de teste. A validação real em uma DAW Windows ainda deve ser realizada.
