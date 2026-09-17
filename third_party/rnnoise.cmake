# RNNoise 0.2 (https://github.com/xiph/rnnoise), a supressão de ruído do Conversinha.
#
# `rnnoise/` é o tarball oficial da v0.2 sem o que é de autotools — COPYING, AUTHORS,
# README, `include/` e `src/` copiados como vieram:
#
#   rnnoise-0.2.tar.gz  sha256 90fce4b00b9ff24c08dbfe31b82ffd43bae383d85c5535676d28b0a2b11c0d37
#
# O modelo vem compilado dentro (`src/rnnoise_data.c`), então não há arquivo de pesos para
# achar em execução.
#
# Biblioteca estática À PARTE, e não fontes na DLL do plugin: o `apply_standard_settings`
# do app compila o plugin com /W4 /WX, e o `rnnoise_data.c` sozinho são dezenas de milhares
# de literais double em arrays float — um C4305 cada, que com /WX é erro.
#
# Só MSVC. É a tradução do `configure.ac` e do `Makefile.am` para o cl.exe.

set(RNNOISE_DIR "${CMAKE_CURRENT_LIST_DIR}/rnnoise")

add_library(rnnoise STATIC
  "${RNNOISE_DIR}/src/denoise.c"
  "${RNNOISE_DIR}/src/rnn.c"
  "${RNNOISE_DIR}/src/pitch.c"
  "${RNNOISE_DIR}/src/kiss_fft.c"
  "${RNNOISE_DIR}/src/celt_lpc.c"
  "${RNNOISE_DIR}/src/nnet.c"
  "${RNNOISE_DIR}/src/nnet_default.c"
  "${RNNOISE_DIR}/src/parse_lpcnet_weights.c"
  "${RNNOISE_DIR}/src/rnnoise_data.c"
  "${RNNOISE_DIR}/src/rnnoise_tables.c"
  # A escolha de SIMD em execução: o `x86cpu.c` pergunta ao processador (o ramo `_MSC_VER`
  # dele já usa `__cpuid`) e a tabela do `x86_dnn_map.c` aponta para a versão certa.
  # Compilar tudo com /arch:AVX2 derrubaria PC anterior a 2013; compilar sem a escolha
  # deixa o RNNoise escalar, 3x mais lento.
  "${RNNOISE_DIR}/src/x86/x86_dnn_map.c"
  "${RNNOISE_DIR}/src/x86/x86cpu.c"
  "${RNNOISE_DIR}/src/x86/nnet_sse4_1.c"
  "${RNNOISE_DIR}/src/x86/nnet_avx2.c"
)

target_include_directories(rnnoise
  PUBLIC "${RNNOISE_DIR}/include"
  PRIVATE "${RNNOISE_DIR}/src"
)

# `__SSE__` e `__SSE2__` à mão, e é o ponto mais fácil de errar aqui. O gcc os define
# sempre em x64; o MSVC nunca. Sem eles o `vec.h` cai no caminho escalar, que inclui um
# `os_support.h` que NÃO vem no tarball — o mesmo buraco que impede o 0.2 de compilar em
# ARM. O jeito do Opus seria `OPUS_X86_MAY_HAVE_SSE2`, mas com ele o `x86cpu.h` passa a
# incluir um `opus_defines.h`, que também não vem.
target_compile_definitions(rnnoise PRIVATE
  RNN_ENABLE_X86_RTCD
  DISABLE_DEBUG_FLOAT
  __SSE__
  __SSE2__
  _CRT_SECURE_NO_WARNINGS
)

# C4305 e C4244 são double → float, nos pesos e nas contas: dezenas de milhares, todos de
# propósito, e afogariam o log do runner. Os outros avisos continuam aparecendo.
target_compile_options(rnnoise PRIVATE /wd4305 /wd4244)

# Os macros que `-msse4.1` e `-mavx -mfma -mavx2` definem no gcc. No MSVC os intrínsecos
# existem sem bandeira nenhuma; o que falta é o macro que o código consulta — e sem
# `__SSE4_1__` o `nnet_sse4_1.c` se recusa a compilar.
set_source_files_properties("${RNNOISE_DIR}/src/x86/nnet_sse4_1.c"
  PROPERTIES COMPILE_DEFINITIONS "__SSE3__;__SSSE3__;__SSE4_1__"
)

# /arch:AVX2 define `__AVX__` e `__AVX2__`, e só neste arquivo: ele só roda quando o
# processador tem AVX2. `__FMA__` vem à mão e é seguro pelo mesmo motivo — o `x86cpu.c`
# exige AVX2 E FMA antes de escolher este caminho.
set_source_files_properties("${RNNOISE_DIR}/src/x86/nnet_avx2.c"
  PROPERTIES
    COMPILE_OPTIONS "/arch:AVX2"
    COMPILE_DEFINITIONS "__FMA__"
)

# A licença BSD-3 exige o aviso junto do binário. Vai ao lado do .exe pela mesma porta das
# DLLs (`flutter_webrtc_bundled_libraries`), e com nome próprio: "COPYING" solto na pasta
# do app não diria de quem é.
configure_file("${RNNOISE_DIR}/COPYING"
  "${CMAKE_CURRENT_BINARY_DIR}/RNNoise-COPYING.txt" COPYONLY)
set(RNNOISE_AVISO_DE_LICENCA "${CMAKE_CURRENT_BINARY_DIR}/RNNoise-COPYING.txt")
