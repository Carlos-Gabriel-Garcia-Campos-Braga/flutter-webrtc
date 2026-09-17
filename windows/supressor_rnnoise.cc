#include "supressor_rnnoise.h"

#include "rnnoise.h"

namespace flutter_webrtc_plugin {

// `rnnoise_create(nullptr)` usa o modelo compilado dentro da biblioteca. Pode devolver
// nulo, e aí o `Process` só deixa o áudio passar: sem filtro é pior, sem voz é inaceitável.
SupressorRnnoise::SupressorRnnoise()
    : estado_(rnnoise_create(nullptr)),
      amostras_por_quadro_(rnnoise_get_frame_size()) {}

void SupressorRnnoise::Initialize(int sample_rate_hz, int num_channels) {
  taxa_.store(sample_rate_hz);
  canais_.store(num_channels);
  if (estado_ != nullptr) {
    rnnoise_init(estado_, nullptr);
  }
}

void SupressorRnnoise::Process(int num_bands, int num_frames, int buffer_size,
                               float* buffer) {
  // O RNNoise só sabe 48 kHz, e o quadro é o que decide: 480 amostras. O headset
  // Bluetooth em modo chamada (16 kHz) passa direto, sem reamostrar — reamostrar para cima
  // e para baixo a cada quadro custaria mais do que o filtro devolve num microfone desses.
  if (!ligado_.load(std::memory_order_relaxed) || estado_ == nullptr ||
      num_frames != amostras_por_quadro_) {
    return;
  }
  // Entrada e saída no mesmo buffer: o RNNoise copia a entrada para a análise antes de
  // escrever a saída, e o próprio exemplo dele chama assim. A escala já é a de int16 —
  // nada de multiplicar nem dividir por 32768.
  rnnoise_process_frame(estado_, buffer, buffer);
  quadros_filtrados_.fetch_add(1, std::memory_order_relaxed);
}

void SupressorRnnoise::Reset(int new_rate) {
  // A taxa mudou no meio da chamada — outro aparelho, outro formato. O estado do RNNoise
  // guarda janela e memória da rede do áudio anterior, e ele não serve para o novo.
  taxa_.store(new_rate);
  if (estado_ != nullptr) {
    rnnoise_init(estado_, nullptr);
  }
}

// Nada a soltar: ver o cabeçalho — este objeto vive o processo inteiro.
void SupressorRnnoise::Release() {}

}  // namespace flutter_webrtc_plugin
