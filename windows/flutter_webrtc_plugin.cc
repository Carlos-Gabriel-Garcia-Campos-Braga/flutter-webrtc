#include "flutter_webrtc/flutter_web_r_t_c_plugin.h"

#include "flutter_common.h"
#include "flutter_webrtc.h"
#include "supressor_rnnoise.h"
#include "task_runner_windows.h"

#include <flutter/plugin_registrar_windows.h>

const char* kChannelName = "FlutterWebRTC.Method";
const char* kMetodoSupressaoRnnoise = "conversinhaSupressaoRnnoise";
static flutter_webrtc_plugin::FlutterWebRTC* g_shared_instance = nullptr;

namespace flutter_webrtc_plugin {

// A webrtc plugin for windows/linux.
class FlutterWebRTCPluginImpl : public FlutterWebRTCPlugin {
 public:
  static void RegisterWithRegistrar(PluginRegistrar* registrar) {
    auto channel = std::make_unique<MethodChannel>(
        registrar->messenger(), kChannelName,
        &flutter::StandardMethodCodec::GetInstance());

    auto* channel_pointer = channel.get();

    // Uses new instead of make_unique due to private constructor.
    std::unique_ptr<FlutterWebRTCPluginImpl> plugin(
        new FlutterWebRTCPluginImpl(registrar, std::move(channel)));
    channel_pointer->SetMethodCallHandler(
        [plugin_pointer = plugin.get()](const auto& call, auto result) {
          plugin_pointer->HandleMethodCall(call, std::move(result));
        });

    registrar->AddPlugin(std::move(plugin));
  }

  virtual ~FlutterWebRTCPluginImpl() {}

  BinaryMessenger* messenger() { return messenger_; }

  TextureRegistrar* textures() { return textures_; }

  TaskRunner* task_runner() { return task_runner_.get(); }

 private:
  // Creates a plugin that communicates on the given channel.
  FlutterWebRTCPluginImpl(PluginRegistrar* registrar,
                          std::unique_ptr<MethodChannel> channel)
      : channel_(std::move(channel)),
        messenger_(registrar->messenger()),
        textures_(registrar->texture_registrar()),
        task_runner_(std::make_unique<TaskRunnerWindows>()) {
    webrtc_ = std::make_unique<FlutterWebRTC>(this);
    g_shared_instance = webrtc_.get();
  }

  // Called when a method is called on |channel_|;
  void HandleMethodCall(const MethodCall& method_call,
                        std::unique_ptr<MethodResult> result) {
    if (method_call.method_name() == kMetodoSupressaoRnnoise) {
      AtenderSupressaoRnnoise(method_call, std::move(result));
      return;
    }
    // handle method call and forward to webrtc native sdk.
    auto method_call_proxy = MethodCallProxy::Create(method_call);
    webrtc_->HandleMethodCall(*method_call_proxy.get(),
                              MethodResultProxy::Create(std::move(result)));
  }

  // A supressão de ruído do Conversinha (supressor_rnnoise.h).
  //
  // Atendida aqui, e não no `HandleMethodCall` de `common/`, porque aquele é o mesmo do
  // Linux, onde o RNNoise não é compilado.
  //
  // `{ligado: bool}` liga ou desliga; sem `ligado`, só pergunta. Nos dois casos devolve o
  // retrato do supressor. A primeira chamada instala o processador, e ele nunca mais sai.
  void AtenderSupressaoRnnoise(const MethodCall& method_call,
                               std::unique_ptr<MethodResult> result) {
    auto processamento = webrtc_->audio_processing();
    if (!processamento.get()) {
      result->Error("rnnoise", "a libwebrtc nao entregou o processamento de audio");
      return;
    }
    if (supressor_ == nullptr) {
      supressor_ = new SupressorRnnoise();
      processamento->SetCapturePostProcessing(supressor_);
    }

    const auto* argumentos = method_call.arguments();
    if (argumentos && TypeIs<EncodableMap>(*argumentos)) {
      auto ligado =
          findEncodableValue(GetValue<EncodableMap>(*argumentos), "ligado");
      if (TypeIs<bool>(ligado)) {
        supressor_->Ligar(GetValue<bool>(ligado));
      }
    }

    EncodableMap retrato;
    retrato[EncodableValue("ligado")] = EncodableValue(supressor_->ligado());
    retrato[EncodableValue("taxa")] = EncodableValue(supressor_->taxa());
    retrato[EncodableValue("canais")] = EncodableValue(supressor_->canais());
    retrato[EncodableValue("quadros")] =
        EncodableValue(supressor_->quadros_filtrados());
    result->Success(EncodableValue(retrato));
  }

 private:
  std::unique_ptr<MethodChannel> channel_;
  std::unique_ptr<FlutterWebRTC> webrtc_;
  BinaryMessenger* messenger_;
  TextureRegistrar* textures_;
  std::unique_ptr<TaskRunner> task_runner_;
  // Nunca destruído, de propósito: a libwebrtc guarda este ponteiro e o usa no thread de
  // áudio até o fim do processo. Ver supressor_rnnoise.h.
  SupressorRnnoise* supressor_ = nullptr;
};

}  // namespace flutter_webrtc_plugin


void FlutterWebRTCPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  flutter_webrtc_plugin::FlutterWebRTCPluginImpl::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}

flutter_webrtc_plugin::FlutterWebRTC* FlutterWebRTCPluginSharedInstance() {
  return g_shared_instance;
} 