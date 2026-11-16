# RVOXX — C++ / JUCE / Windows x64

VST3 de dinâmica vocal com versões mono/estéreo e aplicativo standalone. Implementação independente orientada pelo manual RVox e XMLs fornecidos, sem código ou DSP da Waves. JUCE 8.0.12 está fixado no commit `29396c22c93392d6738e021b83196283d6e4d850`.

## Compilar no Windows

Instale Visual Studio 2022 com **Desktop development with C++**, Windows SDK, Git e CMake 3.22 ou superior. No terminal, na raiz deste repositório:

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

O CMake baixa o JUCE na primeira configuração. Também aceita `-DJUCE_PATH=C:/deps/JUCE` ou uma instalação via `-DJUCE_DIR=...`. Os artefatos ficam em `build/RVOXX_artefacts/Release/VST3/RVOXX.vst3` e `build/RVOXX_artefacts/Release/Standalone/RVOXX.exe`. Copie a **pasta inteira** `RVOXX.vst3` para `C:/Program Files/Common Files/VST3/` e faça a DAW procurar plugins novamente. A ação `.github/workflows/windows.yml` compila e testa com MSVC em Windows x64, gera ZIPs com SHA-256 e, em tags `v*`, publica um prerelease. Sua presença não significa que já rodou. JUCE 8.0.12 não suporta MinGW; use o fluxo MSVC.

## Funcionamento

- Gate: −80 a 0 dB em passos de 0,1 dB. −80 representa desligado/−infinito. Expansão suave antes da compressão.
- Compression: −36 a 0 dB. Reduzir o valor aumenta a razão, a redução e a compensação de ganho.
- Gain: −36 a 0 dB; teto máximo do limitador de saída.
- Gate Release: 10 a 250 ms, padrão 130 ms, acessível pela lista de parâmetros da DAW. O XML declara o parâmetro, mas não um controle no painel.
- Medidor central: soma da redução do gate e do compressor, sem a compensação de ganho. O número inferior guarda o pico; clique para zerar.
- Saída mono/estéreo com pico independente por canal e reset por clique. Processamento estéreo vinculado preserva o balanço.
- Automação, edição numérica (valores positivos viram negativos), reset com duplo clique, estado da sessão, presets próprios `.rvoxx`, comparação A/B e undo/redo.
- Link: vincula os movimentos de Compression e Gain durante o arrasto, limitado às faixas de cada parâmetro. Também pode ser ativado arrastando uma seleção sobre os dois controles a partir do fundo do painel.
- Light/Dark/Original: interface vetorial clara com painel cinza, medidores azuis e cursores laranja. A aparência é redimensionável e não depende de um bitmap. O valor de Comp fica dentro do cursor; Gate em OFF e níveis silenciosos mostram −Inf.

Comece pela compressão, ajuste o teto de saída e refine o gate por último, conforme a sequência do manual.

Na versão 0.1.1, OFF do gate usa diretamente o comando de bypass e mantém uma rampa de recuperação de 2 ms. Isso corrige uma falha de precisão em 192 kHz que podia manter a expansão ativa após desabilitá-la. Os testes também verificam reativação e independência do tamanho de bloco.

Na versão 0.1.2, o medidor deixa de publicar redução para silêncio ou sinais abaixo da faixa de entrada de −80 dB. Isso corrige a faixa vermelha falsa, sem desativar o processamento do expansor e sem esconder redução de ruído dentro da faixa mensurável. Os valores de parâmetros salvos pela DAW são preservados; instâncias novas iniciam com Gate em −Inf, Comp em 0,0 dB e Gain em 0,0 dB.

## Fidelidade e limites

O PDF contém um erro tipográfico nas faixas de Compression e Gain (−36 a −36); os XMLs especificam −36 a 0. Os XMLs também identificam mono/estéreo e taxa máxima de 192 kHz. A implementação é exercitada de 44,1 até 192 kHz.

O manual não publica curvas, razões, ataque/release do compressor, detecção ou limitador. Aqui são usados detector de pico (1/80 ms), compressor de razão variável 1:1–8:1 com knee de 6 dB e ataque/release de 3/160 ms, compensação derivada do threshold, expansor de razão 1,75:1 com knee de 6 dB e limitador vinculado de ataque instantâneo/release de 60 ms, sem lookahead e com latência zero. Essas escolhas são aproximações explícitas: não há alegação de equivalência sonora com o original. O limitador garante picos de amostra, não true peak entre amostras.

A imagem gerada na conversa ainda não foi disponibilizada como arquivo; portanto não está embutida e não foi alterada. O botão **Imagem** abre um PNG/JPG local, mostrando a arte completa sem recortes e sem pintar sobre ela; controles e leituras ao vivo ficam abaixo da imagem. Para incorporá-la ao binário, coloque o arquivo original em `Assets/imagem.png` ou forneça `-DRVOXX_ARTWORK=C:/arte/imagem.png`. O arquivo é embutido sem regravar seus bytes. Não há upscale automático nem alegação de resolução 8K; uma fonte de 8K deve ser fornecida nessa resolução.

A interface vetorial de fallback é provisória, não uma reprodução exata da imagem ou dos skins proprietários. Os presets de fábrica e recursos da Waves não são incluídos. A/B e a imagem carregada por arquivo são temporários por janela; o estado ativo e o tema são salvos na sessão. Não há padrão global de skin entre instâncias.

## Testes sem JUCE

Para validar somente o DSP com um compilador C++17, sem dependências gráficas:

```sh
cmake -S . -B build-dsp -DRVOXX_BUILD_PLUGIN=OFF -DBUILD_TESTING=ON
cmake --build build-dsp --parallel 2
ctest --test-dir build-dsp --output-on-failure
```

Os testes exercitam bypass de dinâmica, gate, compensação, teto em transientes, silêncio, estéreo vinculado, diferentes blocos e taxas e entradas não finitas. A suíte JUCE adicional cobre automação, estado, layouts e processamento. Antes de distribuir, compile e valide o VST3 na DAW Windows escolhida e verifique os termos de licença do JUCE aplicáveis ao seu produto.
