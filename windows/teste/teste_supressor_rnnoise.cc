// A régua do SupressorRnnoise, sem Flutter e sem libwebrtc de pé: só a classe, o RNNoise
// compilado pela mesma receita do plugin e o cabeçalho da libwebrtc.
//
// Faz o papel do adaptador da libwebrtc — Initialize, Process em quadros de 10 ms, Reset —
// e cobra o que o app promete: desligado não toca em nada, ligado tira ruído sem comer a
// voz, e o que não é 48 kHz passa direto. Devolve 0 quando tudo vale.

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

#include "supressor_rnnoise.h"

namespace {

using flutter_webrtc_plugin::SupressorRnnoise;

const double kPi = 3.14159265358979323846;
const int kTaxa = 48000;
const int kQuadro = 480;
const int kQuadros = 600;  // 6 s

int falhas = 0;

void Conferir(bool condicao, const char* descricao) {
  std::printf("%s %s\n", condicao ? "ok   " : "FALHA", descricao);
  if (!condicao) ++falhas;
}

// Escala de int16, como o APM entrega: ruído branco na casa de -20 dBFS.
std::vector<float> Ruido(int amostras, unsigned semente) {
  std::mt19937 gerador(semente);
  std::normal_distribution<float> normal(0.0f, 3000.0f);
  std::vector<float> sinal(amostras);
  for (auto& amostra : sinal) amostra = normal(gerador);
  return sinal;
}

// Algo com cara de voz para o RNNoise: fundamental que desliza entre 120 e 180 Hz, doze
// harmônicos caindo, e sílabas de ~5 por segundo. Não é fala — é o bastante para a rede
// não tratar tudo como ruído.
std::vector<float> Vogais(int amostras) {
  std::vector<float> sinal(amostras);
  double fase = 0;
  for (int i = 0; i < amostras; ++i) {
    double t = static_cast<double>(i) / kTaxa;
    double f0 = 150 + 30 * std::sin(2 * kPi * 0.7 * t);
    fase += 2 * kPi * f0 / kTaxa;
    double silaba = 0.5 + 0.5 * std::sin(2 * kPi * 5 * t);
    double soma = 0;
    for (int h = 1; h <= 12; ++h) soma += std::sin(h * fase) / h;
    sinal[i] = static_cast<float>(4000 * silaba * soma);
  }
  return sinal;
}

double Rms(const std::vector<float>& sinal, int de, int ate) {
  double soma = 0;
  for (int i = de; i < ate; ++i) soma += static_cast<double>(sinal[i]) * sinal[i];
  return std::sqrt(soma / (ate - de));
}

double Db(double razao) { return 20 * std::log10(razao); }

// Passa `sinal` pelo supressor em quadros de `quadro` amostras, como o adaptador faz.
std::vector<float> Processar(SupressorRnnoise& supressor, std::vector<float> sinal,
                             int quadro) {
  for (size_t i = 0; i + quadro <= sinal.size(); i += quadro) {
    supressor.Process(1, quadro, quadro, &sinal[i]);
  }
  return sinal;
}

bool Finito(const std::vector<float>& sinal) {
  for (float amostra : sinal) {
    if (!std::isfinite(amostra)) return false;
  }
  return true;
}

}  // namespace

int main() {
  const int amostras = kQuadros * kQuadro;
  // A primeira metade é aquecimento: o RNNoise tem memória, e medir desde o primeiro
  // quadro mediria a rede ainda acordando.
  const int metade = amostras / 2;

  {
    auto* supressor = new SupressorRnnoise();
    supressor->Initialize(kTaxa, 1);
    const auto ruido = Ruido(amostras, 1);
    const auto saida = Processar(*supressor, ruido, kQuadro);
    Conferir(std::memcmp(saida.data(), ruido.data(), ruido.size() * sizeof(float)) == 0,
             "desligado, o áudio passa bit a bit");
    Conferir(supressor->quadros_filtrados() == 0, "desligado, nenhum quadro é contado");
    Conferir(supressor->taxa() == kTaxa && supressor->canais() == 1,
             "o Initialize fica no retrato: taxa e canais");
  }

  {
    auto* supressor = new SupressorRnnoise();
    supressor->Initialize(kTaxa, 1);
    supressor->Ligar(true);
    const auto ruido = Ruido(amostras, 2);
    const auto saida = Processar(*supressor, ruido, kQuadro);
    const double queda = Db(Rms(saida, metade, amostras) / Rms(ruido, metade, amostras));
    std::printf("      ruído puro: %.1f dB\n", queda);
    Conferir(Finito(saida), "ligado, nenhuma amostra vira NaN ou infinito");
    Conferir(queda < -20, "ligado, o ruído puro cai mais de 20 dB");
    Conferir(supressor->quadros_filtrados() == kQuadros, "ligado, cada quadro é contado");
  }

  {
    auto* supressor = new SupressorRnnoise();
    supressor->Initialize(kTaxa, 1);
    supressor->Ligar(true);
    const auto vogais = Vogais(amostras);
    const auto ruido = Ruido(amostras, 3);
    std::vector<float> mistura(amostras);
    for (int i = 0; i < amostras; ++i) mistura[i] = vogais[i] + ruido[i] * 0.3f;
    const auto saida = Processar(*supressor, mistura, kQuadro);
    const double preservada = Db(Rms(saida, metade, amostras) / Rms(vogais, metade, amostras));
    std::printf("      vogais com ruído, saída contra as vogais limpas: %.1f dB\n", preservada);
    Conferir(preservada > -6, "ligado, a voz não é comida junto com o ruído");
  }

  {
    auto* supressor = new SupressorRnnoise();
    supressor->Initialize(kTaxa, 1);
    supressor->Ligar(true);
    supressor->Reset(16000);
    const auto ruido = Ruido(160 * 100, 4);
    const auto saida = Processar(*supressor, ruido, 160);
    Conferir(std::memcmp(saida.data(), ruido.data(), ruido.size() * sizeof(float)) == 0,
             "a 16 kHz (Bluetooth em modo chamada) o áudio passa direto");
    Conferir(supressor->taxa() == 16000 && supressor->quadros_filtrados() == 0,
             "o Reset fica no retrato, e nada é contado como filtrado");
  }

  {
    auto* supressor = new SupressorRnnoise();
    supressor->Initialize(kTaxa, 1);
    supressor->Ligar(true);
    const auto ruido = Ruido(amostras, 5);
    auto saida = ruido;
    for (int q = 0; q < kQuadros; ++q) {
      // Desliga no meio, como o interruptor com a chamada de pé.
      if (q == kQuadros / 2) supressor->Ligar(false);
      supressor->Process(1, kQuadro, kQuadro, &saida[q * kQuadro]);
    }
    Conferir(std::memcmp(&saida[metade], &ruido[metade], metade * sizeof(float)) == 0,
             "desligar vale no quadro seguinte");
    Conferir(supressor->quadros_filtrados() == kQuadros / 2,
             "só os quadros ligados foram contados");
  }

  {
    // **Quanto custa por quadro.** O filtro roda dentro do processamento de captura, na
    // linha de tempo do áudio: um quadro de 10 ms que demore 10 ms para ser filtrado é
    // uma chamada picotada. Aqui ele processa 30 s de áudio e diz a fatia do orçamento
    // que gastou.
    //
    // O teto é FROUXO de propósito (20% do tempo real) — o runner é compartilhado e uma
    // medida apertada viraria falha por vizinho barulhento. Ele não persegue microssegundo:
    // pega o dia em que o caminho vetorizado deixar de entrar e o custo virar outra ordem
    // de grandeza, que é a regressão que importa.
    auto* supressor = new SupressorRnnoise();
    supressor->Initialize(kTaxa, 1);
    supressor->Ligar(true);
    const int quadros = 3000;  // 30 s
    auto sinal = Ruido(quadros * kQuadro, 6);
    const auto comeco = std::chrono::steady_clock::now();
    for (int q = 0; q < quadros; ++q) {
      supressor->Process(1, kQuadro, kQuadro, &sinal[q * kQuadro]);
    }
    const double us =
        std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - comeco)
            .count();
    const double por_quadro = us / quadros;
    std::printf("      %.0f us por quadro de 10 ms — %.2f%% do tempo real\n", por_quadro,
                por_quadro / 100.0);
    Conferir(por_quadro < 2000, "o filtro cabe folgado no orçamento do quadro");
  }

  std::printf("%s\n", falhas == 0 ? "tudo certo" : "houve falhas");
  return falhas == 0 ? 0 : 1;
}
