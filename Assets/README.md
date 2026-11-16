# Imagem aprovada

Coloque aqui o arquivo original aprovado com o nome `imagem.png`. O CMake o incorpora ao plugin sem regravar nem alterar seus bytes. Também é possível configurar `-DRVOXX_ARTWORK=/caminho/arquivo.png`.

Sem esse arquivo, a compilação usa a interface vetorial e oferece o botão **Imagem** para abrir uma imagem local. O conteúdo completo da imagem é preservado, com ajuste proporcional apenas para exibição. Os controles e os medidores ao vivo aparecem abaixo, fora da imagem.

O carregamento não faz upscale e não promete 8K. Para uma fonte de 8K, forneça o próprio arquivo nessa resolução. Não substitua a imagem aprovada por uma regeneração.
