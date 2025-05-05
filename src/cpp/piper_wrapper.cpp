#include "piper_wrapper.h"
#include "piper.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <memory>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
// 定义内部上下文结构
struct PiperContext {
    piper::PiperConfig config;
    piper::Voice voice;
    bool initialized;
};

// 初始化 Piper
PiperContext* piper_wrapper_init(const char* espeak_data_path, const char* model_path,
                                 const char* config_path, int speaker_id) {
    try {
        // 创建 Piper 上下文
        auto context = new PiperContext();
        context->initialized = false;

        // 设置 PiperConfig
        context->config.eSpeakDataPath = espeak_data_path;
        context->config.useESpeak = true;
        spdlog::set_level(spdlog::level::debug);

        // 准备加载模型
        std::basic_string<char, std::char_traits<char>, std::allocator<char>> modelPath(model_path);
        std::basic_string<char, std::char_traits<char>, std::allocator<char>> configPath =
                (config_path) ? std::basic_string<char, std::char_traits<char>, std::allocator<char>>(config_path) :
                std::basic_string<char, std::char_traits<char>, std::allocator<char>>("");

        // 设置 speaker ID
        std::optional<long long> speakerId;
        if (speaker_id >= 0) {
            speakerId = static_cast<long long>(speaker_id);
        }

        // 加载语音模型
        try {
            piper::loadVoice(
                    context->config,
                    modelPath,
                    configPath,
                    context->voice,
                    speakerId,
                    false // 不使用 CUDA
            );
            // 初始化 Piper
            piper::initialize(context->config);

            context->initialized = true;
            return context;
        } catch (const std::exception& e) {
            std::cerr << "Error loading voice: " << e.what() << std::endl;
            delete context;
            return nullptr;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error initializing piper: " << e.what() << std::endl;
        return nullptr;
    }
}

// 释放 Piper 资源
void piper_wrapper_terminate(PiperContext* context) {
    if (context) {
        if (context->initialized) {
            piper::terminate(context->config);
        }
        delete context;
    }
}

// 文本转语音
int piper_wrapper_text_to_audio(PiperContext* context, const char* text,
                                short** audio_buffer, int* audio_length) {
    if (!context || !context->initialized || !text || !audio_buffer || !audio_length) {
        std::cerr << "Invalid arguments passed to piper_wrapper_text_to_audio" << std::endl;
        return -1;
    }

    try {
        // 准备合成所需的参数
        std::vector<short> audioBuffer;
        piper::SynthesisResult result;

        // 打印调试信息
        std::cout << "Starting synthesis for text: " << text << std::endl;

        // 执行文本到语音转换
        piper::textToAudio(
                context->config,
                context->voice,
                std::string(text),  // 使用std::string构造函数
                audioBuffer,
                result,
                std::function<void()>([](){})
        );

        // 检查合成结果
        std::cout << "Synthesis completed. Buffer size: " << audioBuffer.size()
                  << ", phoneme count: " << result.phonemeCount
                  << ", audio duration: " << result.audioSeconds << "s" << std::endl;

        // 检查音频缓冲区大小
        if (audioBuffer.empty()) {
            std::cerr << "Warning: Synthesis produced empty audio buffer" << std::endl;
            *audio_length = 0;
            *audio_buffer = nullptr;  // 避免分配大小为0的数组
            return 0;  // 仍然返回成功，但音频长度为0
        }

        // 将结果复制到输出缓冲区
        *audio_length = static_cast<int>(audioBuffer.size());
        *audio_buffer = new short[*audio_length];
        std::copy(audioBuffer.begin(), audioBuffer.end(), *audio_buffer);

        std::cout << "Successfully copied " << *audio_length << " audio frames to output buffer" << std::endl;
        return 0; // 成功
    } catch (const std::exception& e) {
        std::cerr << "Error synthesizing speech: " << e.what() << std::endl;
        *audio_length = 0;
        *audio_buffer = nullptr;
        return -1;
    } catch (...) {
        std::cerr << "Unknown error occurred during speech synthesis" << std::endl;
        *audio_length = 0;
        *audio_buffer = nullptr;
        return -1;
    }
}
// 释放音频缓冲区
void piper_wrapper_free_audio(short* audio_buffer) {
    if (audio_buffer) {
        delete[] audio_buffer;
    }
}