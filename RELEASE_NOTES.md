RVOXX — prévia para Windows x64, em C++17/JUCE 8.0.12.

- Gate corrigido: OFF agora desativa a expansão também em 192 kHz, com rampa suave e retorno ao ganho unitário. Inclui regressões de automação em quatro taxas e três tamanhos de bloco.
- Visual vintage com madeira escura, painel de metal envelhecido, controles em latão, medidores âmbar e indicador de estado do gate.

- VST3 mono/estéreo, automação, estado da sessão, gate, compressor, limitador, medidores e presets.
- Baixe `RVOXX-Windows-x64.zip`, extraia e copie a pasta inteira `RVOXX.vst3` para `C:\Program Files\Common Files\VST3\`.
- `RVOXX-Standalone-Windows-x64.zip` contém o aplicativo para execução fora da DAW.
- O processamento é independente e aproximado; não usa o algoritmo da Waves nem promete equivalência sonora.
- A imagem aprovada ainda depende do envio do arquivo original. O botão `Imagem` permite abrir um PNG/JPG local sem modificar o arquivo; a imagem completa é mostrada acima dos controles ao vivo. Para embutir na compilação, forneça `Assets/imagem.png` ou configure `RVOXX_ARTWORK`.
- Este release é de teste. A validação real em uma DAW Windows ainda deve ser realizada.
