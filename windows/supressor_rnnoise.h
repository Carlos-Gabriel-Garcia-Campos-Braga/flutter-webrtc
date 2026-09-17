#ifndef CONVERSINHA_SUPRESSOR_RNNOISE_H_
#define CONVERSINHA_SUPRESSOR_RNNOISE_H_

#include <atomic>
#include <cstdint>

#include "rtc_audio_processing.h"

struct DenoiseState;

namespace flutter_webrtc_plugin {

// A supressão de ruído do Conversinha: o RNNoise no fim do processamento do microfone.
//
// Entra pelo `SetCapturePostProcessing` da libwebrtc, que roda DEPOIS do cancelador de
// eco — um filtro não linear antes dele estragaria o cancelamento — e entrega o formato
// exato do RNNoise: um canal, quadros de 10 ms, float na escala de int16. Não há conversão
// nem buffer extra.
//
// **Instalado uma vez e nunca removido.** Desligar passando nulo derruba o app: depois de
// inicializado, o adaptador da libwebrtc chama `Initialize` no ponteiro novo sem checar,
// no thread de áudio. Ligar e desligar é a flag atômica daqui de dentro, e pelo mesmo
// motivo o objeto nunca é destruído — é um por instância do plugin, que na prática é uma
// por processo, e vazá-lo não custa nada.
class SupressorRnnoise
    : public libwebrtc::RTCAudioProcessing::CustomProcessing {
 public:
  SupressorRnnoise();

  // Do thread da plataforma. Vale no quadro seguinte, sem re-capturar o microfone.
  void Ligar(bool ligado) { ligado_.store(ligado); }

  // O retrato que o app escreve no log: é a única prova de que o gancho rodou numa
  // chamada de verdade, e de que a taxa chegou a 48 kHz.
  bool ligado() const { return ligado_.load(); }
  int taxa() const { return taxa_.load(); }
  int canais() const { return canais_.load(); }
  int64_t quadros_filtrados() const { return quadros_filtrados_.load(); }

  // Do thread de áudio, sempre sob o mutex do adaptador da libwebrtc — por isso o estado
  // do RNNoise não precisa de trava própria.
  void Initialize(int sample_rate_hz, int num_channels) override;
  void Process(int num_bands, int num_frames, int buffer_size,
               float* buffer) override;
  void Reset(int new_rate) override;
  void Release() override;

 private:
  DenoiseState* const estado_;
  const int amostras_por_quadro_;
  std::atomic<bool> ligado_{false};
  std::atomic<int> taxa_{0};
  std::atomic<int> canais_{0};
  std::atomic<int64_t> quadros_filtrados_{0};
};

}  // namespace flutter_webrtc_plugin

#endif  // CONVERSINHA_SUPRESSOR_RNNOISE_H_
