#include "piper_wrapper.h"
#include "piper.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <memory>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <stdio.h>
// 包含必要的头文件
#include <soxr.h>
#include <optional>
#include <cstring>
#include <functional>

// 定义内部上下文结构
struct PiperContext {
    piper::PiperConfig config;
    piper::Voice voice;
    bool initialized;
};

// 初始化 Piper
PiperContext* piper_wrapper_init(const char* espeak_data_path, const char* model_path,
                                 const char* config_path, int speaker_id, const char* language) {
    try {
        // 创建 Piper 上下文
        auto context = new PiperContext();
        context->initialized = false;

        // 设置 PiperConfig
        context->config.eSpeakDataPath = espeak_data_path;
        context->config.useESpeak = true;
        context->voice.synthesisConfig.sampleRate = 48000; // 设置默认采样率
        // 设置语言
        if (language && strlen(language) > 0) {
            std::string languageCopy(language);
            // 在eSpeak配置中设置语言
            context->voice.phonemizeConfig.eSpeak.voice = languageCopy;
            // 调整 eSpeak 的其他参数可能也有帮助
            std::cout << "Setting voice language to: " << languageCopy << std::endl;
        }
        spdlog::set_level(spdlog::level::debug);
        // 打印当前采样率
        std::cout << "Original model sample rate: " << context->voice.synthesisConfig.sampleRate << std::endl;

        // 准备加载模型
        std::string model_pathCopy(model_path);
        std::string modelPath = model_pathCopy.empty() ? "" : model_pathCopy;
        std::string config_pathCopy(config_path);
        std::string configPath = config_pathCopy.empty() ? "" : config_pathCopy;

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

// 文本转语音
int piper_wrapper_text_to_audio(PiperContext* context, const char* text,
                                short** audio_buffer, int* audio_length,int sampleRate,int channels) {
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
        context->voice.phonemizeConfig.eSpeak.voice = "cmn";
        // 执行文本到语音转换
        piper::textToAudio(
                context->config,
                context->voice,
                std::string(text),  // 使用std::string构造函数
                audioBuffer,
                result,
                nullptr
//                std::function<void()>([](){})
        );

        // 检查合成结果
        std::cout << "Synthesis completed. Buffer size: " << audioBuffer.size()
                  << ", audio duration: " << result.audioSeconds << "s" << std::endl;

        // 检查音频缓冲区大小
        if (audioBuffer.empty()) {
            std::cerr << "Warning: Synthesis produced empty audio buffer" << std::endl;
            *audio_length = 0;
            *audio_buffer = nullptr;  // 避免分配大小为0的数组
            return 0;  // 仍然返回成功，但音频长度为0
        }
        // 获取原始采样率
        int originalSampleRate = context->voice.synthesisConfig.sampleRate;
        std::cout << "Original sample rate: " << originalSampleRate << ", Target sample rate: " << sampleRate << std::endl;

        // 仅当目标采样率与原始采样率不同时执行重采样
        if (sampleRate != originalSampleRate) {
            std::cout << "Performing sample rate conversion from " << originalSampleRate << " to " << sampleRate << "..." << std::endl;

            // 1. 将short转换为float用于soxr处理（可选，取决于您的soxr配置）
            std::vector<float> floatInput(audioBuffer.size());
            for (size_t i = 0; i < audioBuffer.size(); i++) {
                floatInput[i] = audioBuffer[i] / 32768.0f;
            }

            // 2. 计算输出缓冲区大小
            size_t outputSize = static_cast<size_t>((static_cast<double>(audioBuffer.size()) * sampleRate) / originalSampleRate + 0.5);
            std::vector<float> floatOutput(outputSize);

            // 3. 配置soxr
            soxr_error_t error;
            soxr_io_spec_t io_spec = soxr_io_spec(SOXR_FLOAT32_I, SOXR_FLOAT32_I);
            soxr_quality_spec_t quality_spec = soxr_quality_spec(SOXR_VHQ, 0); // 使用最高质量

            // 4. 创建soxr重采样器
            soxr_t soxr = soxr_create(
                    originalSampleRate,    // 输入采样率
                    sampleRate,            // 输出采样率
                    channels,              // 声道数
                    &error,                // 错误处理
                    &io_spec,              // IO格式
                    &quality_spec,         // 质量设置
                    NULL                   // 运行时设置（使用默认值）
            );

            if (error) {
                std::cerr << "Error creating soxr: " << soxr_strerror(error) << std::endl;
                *audio_length = 0;
                *audio_buffer = nullptr;
                return -1;
            }

            // 5. 执行重采样
            size_t done;
            error = soxr_process(
                    soxr,
                    floatInput.data(), floatInput.size(), NULL,
                    floatOutput.data(), floatOutput.size(), &done
            );

            if (error) {
                std::cerr << "Error during resampling: " << soxr_strerror(error) << std::endl;
                soxr_delete(soxr);
                *audio_length = 0;
                *audio_buffer = nullptr;
                return -1;
            }

            std::cout << "Resampling completed. Output size: " << done << " samples" << std::endl;

            // 6. 将float转换回short
            std::vector<short> resampledBuffer(done);
            for (size_t i = 0; i < done; i++) {
                float sample = floatOutput[i];
                // 限制在[-1.0, 1.0]范围内防止截断失真
                if (sample > 1.0f) sample = 1.0f;
                if (sample < -1.0f) sample = -1.0f;
                resampledBuffer[i] = static_cast<short>(sample * 32767.0f);
            }

            // 7. 释放soxr资源
            soxr_delete(soxr);

            // 8. 更新audio_buffer
            *audio_length = static_cast<int>(resampledBuffer.size());
            *audio_buffer = new short[*audio_length];
            std::copy(resampledBuffer.begin(), resampledBuffer.end(), *audio_buffer);
        } else {
            // 如果采样率相同，直接使用原始数据
            *audio_length = static_cast<int>(audioBuffer.size());
            *audio_buffer = new short[*audio_length];
            std::copy(audioBuffer.begin(), audioBuffer.end(), *audio_buffer);
        }
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

// 释放 Piper 资源
void piper_wrapper_terminate(PiperContext* context) {
    if (context) {
        if (context->initialized) {
            piper::terminate(context->config);
        }
        delete context;
    }
}

// 释放音频缓冲区
void piper_wrapper_free_audio(short* audio_buffer) {
    if (audio_buffer) {
        delete[] audio_buffer;
    }
}