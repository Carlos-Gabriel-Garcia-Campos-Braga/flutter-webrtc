# A régua do SupressorRnnoise, compilada fora do Flutter (ver teste_supressor_rnnoise.cc).
#
# Usa as MESMAS duas receitas do plugin — o download da libwebrtc e o rnnoise.cmake —, e
# por isso ela prova o que o build do app vai encontrar: se o MSVC aceita o RNNoise e se
# a classe compila sob /W4 /WX, que é como o app compila o plugin.
#
#   cmake -S windows/teste -B build -A x64
#   cmake --build build --config Release
#   build\Release\teste_supressor_rnnoise.exe
cmake_minimum_required(VERSION 3.15)
project(teste_supressor_rnnoise LANGUAGES C CXX)

if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
  cmake_policy(SET CMP0135 NEW)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/CMakeLists.txt")
include("${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/rnnoise.cmake")

add_executable(teste_supressor_rnnoise
  "teste_supressor_rnnoise.cc"
  "../supressor_rnnoise.cc"
)
target_include_directories(teste_supressor_rnnoise PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
  "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/libwebrtc/include"
)
target_compile_features(teste_supressor_rnnoise PRIVATE cxx_std_17)
# O mesmo que o `apply_standard_settings` do app põe no plugin.
target_compile_options(teste_supressor_rnnoise PRIVATE /W4 /WX /wd4100 /EHsc)
target_compile_definitions(teste_supressor_rnnoise PRIVATE LIB_WEBRTC_API_DLL "_HAS_EXCEPTIONS=0")
target_link_libraries(teste_supressor_rnnoise PRIVATE rnnoise)
