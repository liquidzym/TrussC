#include "tcxsd/NativeRuntime.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifndef TCXSD_HAS_NATIVE
#define TCXSD_HAS_NATIVE 0
#endif

#ifndef TCXSD_ADDON_ROOT
#define TCXSD_ADDON_ROOT ""
#endif

#ifndef TCXSD_NATIVE_CLI
#define TCXSD_NATIVE_CLI ""
#endif

#if TCXSD_HAS_NATIVE
#include <stable-diffusion.h>
#endif

namespace tcx::sd {
namespace {

std::string pathString(const fs::path& path) {
    return path.empty() ? std::string() : path.string();
}

bool requireExistingPath(const fs::path& path, const char* label, std::string* error) {
    if (path.empty()) {
        return true;
    }
    if (fs::exists(path)) {
        return true;
    }
    if (error) {
        *error = std::string(label) + " does not exist: " + path.string();
    }
    return false;
}

fs::path absolutePath(const fs::path& path) {
    if (path.empty()) {
        return path;
    }

    std::error_code ec;
    fs::path absolute = fs::absolute(path, ec);
    return ec ? path : absolute;
}

std::string pathArg(const fs::path& path) {
    const fs::path absolute = absolutePath(path);
    auto u8 = absolute.generic_u8string();
    return std::string(u8.begin(), u8.end());
}

bool existingFile(const fs::path& path) {
    std::error_code ec;
    return !path.empty() && fs::is_regular_file(path, ec);
}

fs::path currentExecutableDir() {
#if defined(_WIN32)
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return {};
    }
    buffer.resize(length);
    return fs::path(buffer).parent_path();
#else
    return {};
#endif
}

fs::path bundledCliCandidate() {
#if defined(_WIN32)
    const char* name = "sd-cli.exe";
#else
    const char* name = "sd-cli";
#endif
    const fs::path exeDir = currentExecutableDir();
    return exeDir.empty() ? fs::path() : exeDir / name;
}

fs::path configuredCliCandidate() {
    const std::string configured = TCXSD_NATIVE_CLI;
    return configured.empty() ? fs::path() : fs::path(configured);
}

fs::path addonCliCandidate() {
#if defined(_WIN32)
    const char* name = "sd-cli.exe";
#else
    const char* name = "sd-cli";
#endif
    const std::string root = TCXSD_ADDON_ROOT;
    return root.empty() ? fs::path() : fs::path(root) / "libs" / "stable-diffusion" / "current" / "bin" / name;
}

fs::path resolveCliExecutable(const RuntimeSettings& settings) {
    if (!settings.cliExecutable.empty()) {
        return absolutePath(settings.cliExecutable);
    }

    for (const fs::path& candidate : {
             bundledCliCandidate(),
             configuredCliCandidate(),
             addonCliCandidate(),
         }) {
        if (existingFile(candidate)) {
            return absolutePath(candidate);
        }
    }

    const fs::path configured = configuredCliCandidate();
    return configured.empty() ? absolutePath(addonCliCandidate()) : absolutePath(configured);
}

bool autoPrefersCliProcess(const RuntimeSettings& settings) {
#if defined(_WIN32)
    return settings.backend == Backend::Cuda || settings.backend == Backend::Auto;
#else
    (void)settings;
    return false;
#endif
}

bool shouldUseCliProcess(const RuntimeSettings& settings, const fs::path& cliExecutable) {
    if (settings.executionMode == ExecutionMode::CliProcess) {
        return true;
    }
    if (settings.executionMode == ExecutionMode::InProcess) {
        return false;
    }
    return autoPrefersCliProcess(settings) && existingFile(cliExecutable);
}

const char* cliSamplerName(Sampler sampler) {
    switch (sampler) {
        case Sampler::Euler: return "euler";
        case Sampler::EulerA: return "euler_a";
        case Sampler::Dpmpp2M: return "dpm++2m";
        case Sampler::DdimTrailing: return "ddim_trailing";
        case Sampler::Auto: return "";
    }
    return "";
}

void sendProgress(
    const ProgressCallback& callback,
    JobId jobId,
    JobState state,
    std::string message,
    int step = 0,
    int totalSteps = 0,
    float seconds = 0.0f) {
    if (!callback) {
        return;
    }

    Progress progress;
    progress.jobId = jobId;
    progress.state = state;
    progress.step = step;
    progress.totalSteps = totalSteps;
    progress.seconds = seconds;
    progress.message = std::move(message);
    callback(progress);
}

double elapsedSecondsSince(std::chrono::steady_clock::time_point started) {
    const auto elapsed = std::chrono::steady_clock::now() - started;
    return std::chrono::duration<double>(elapsed).count();
}

std::string boolText(bool value) {
    return value ? "true" : "false";
}

void addPathMetadata(std::map<std::string, std::string>& metadata, const std::string& key, const fs::path& path) {
    if (!path.empty()) {
        metadata[key] = pathString(path);
    }
}

std::map<std::string, std::string> resultMetadata(
    const ImageRequest& request,
    ExecutionMode mode,
    const ModelPaths* paths = nullptr,
    const RuntimeSettings* settings = nullptr) {
    auto metadata = request.metadata;
    metadata["prompt"] = request.prompt;
    if (!request.negativePrompt.empty()) {
        metadata["negative_prompt"] = request.negativePrompt;
    }
    metadata["width"] = std::to_string(request.width);
    metadata["height"] = std::to_string(request.height);
    metadata["quality"] = toString(request.quality);
    metadata["sampler"] = toString(request.sampler);
    metadata["steps"] = std::to_string(request.steps);
    metadata["cfg_scale"] = std::to_string(request.cfgScale);
    metadata["strength"] = std::to_string(request.strength);
    metadata["seed"] = std::to_string(request.seed);
    metadata["batch_count"] = std::to_string(request.batchCount);
    metadata["execution_mode"] = toString(mode);
    if (settings) {
        metadata["backend"] = toString(settings->backend);
        metadata["runtime_execution_mode"] = toString(settings->executionMode);
        metadata["cpu_threads"] = std::to_string(settings->cpuThreads);
        metadata["keep_model_loaded"] = boolText(settings->keepModelLoaded);
        metadata["mmap"] = boolText(settings->mmap);
        metadata["flash_attention"] = boolText(settings->flashAttention);
        metadata["diffusion_flash_attention"] = boolText(settings->diffusionFlashAttention);
        metadata["diffusion_conv_direct"] = boolText(settings->diffusionConvDirect);
        metadata["vae_conv_direct"] = boolText(settings->vaeConvDirect);
        metadata["offload_params_to_cpu"] = boolText(settings->offloadParamsToCpu);
        metadata["keep_text_encoder_on_cpu"] = boolText(settings->keepTextEncoderOnCpu);
        metadata["keep_vae_on_cpu"] = boolText(settings->keepVaeOnCpu);
        metadata["keep_control_net_on_cpu"] = boolText(settings->keepControlNetOnCpu);
        metadata["max_vram_gib"] = std::to_string(settings->maxVramGiB);
        metadata["stream_layers"] = boolText(settings->streamLayers);
        if (!settings->backendAssignment.empty()) {
            metadata["backend_assignment"] = settings->backendAssignment;
        }
        if (!settings->paramsBackendAssignment.empty()) {
            metadata["params_backend_assignment"] = settings->paramsBackendAssignment;
        }
    }
    if (paths) {
        addPathMetadata(metadata, "model_path", paths->model);
        addPathMetadata(metadata, "diffusion_model_path", paths->diffusionModel);
        addPathMetadata(metadata, "high_noise_diffusion_model_path", paths->highNoiseDiffusionModel);
        addPathMetadata(metadata, "unconditional_diffusion_model_path", paths->unconditionalDiffusionModel);
        addPathMetadata(metadata, "clip_l_path", paths->clipL);
        addPathMetadata(metadata, "clip_g_path", paths->clipG);
        addPathMetadata(metadata, "clip_vision_path", paths->clipVision);
        addPathMetadata(metadata, "t5xxl_path", paths->t5xxl);
        addPathMetadata(metadata, "llm_path", paths->llm);
        addPathMetadata(metadata, "llm_vision_path", paths->llmVision);
        addPathMetadata(metadata, "vae_path", paths->vae);
        addPathMetadata(metadata, "audio_vae_path", paths->audioVae);
        addPathMetadata(metadata, "control_net_path", paths->controlNet);
        addPathMetadata(metadata, "photo_maker_path", paths->photoMaker);
    }
    return metadata;
}

std::string makeOutputStem(JobId jobId) {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return "tcxsd_job_" + std::to_string(jobId) + "_" + std::to_string(millis);
}

void addPathArgument(std::vector<std::string>& args, const char* key, const fs::path& path) {
    if (path.empty()) {
        return;
    }
    args.emplace_back(key);
    args.emplace_back(pathArg(path));
}

void addStringArgument(std::vector<std::string>& args, const char* key, const std::string& value) {
    if (value.empty()) {
        return;
    }
    args.emplace_back(key);
    args.emplace_back(value);
}

void addBoolArgument(std::vector<std::string>& args, const char* key, bool enabled) {
    if (enabled) {
        args.emplace_back(key);
    }
}

#if defined(_WIN32)
std::wstring utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) {
        return std::wstring(text.begin(), text.end());
    }

    std::wstring wide(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), size);
    return wide;
}

std::wstring quoteWindowsArg(const std::wstring& arg) {
    if (arg.empty()) {
        return L"\"\"";
    }

    bool needsQuotes = false;
    for (wchar_t ch : arg) {
        if (ch == L' ' || ch == L'\t' || ch == L'\n' || ch == L'"') {
            needsQuotes = true;
            break;
        }
    }
    if (!needsQuotes) {
        return arg;
    }

    std::wstring quoted = L"\"";
    int backslashes = 0;
    for (wchar_t ch : arg) {
        if (ch == L'\\') {
            ++backslashes;
            continue;
        }
        if (ch == L'"') {
            quoted.append(static_cast<size_t>(backslashes * 2 + 1), L'\\');
            quoted.push_back(ch);
            backslashes = 0;
            continue;
        }
        quoted.append(static_cast<size_t>(backslashes), L'\\');
        backslashes = 0;
        quoted.push_back(ch);
    }
    quoted.append(static_cast<size_t>(backslashes * 2), L'\\');
    quoted.push_back(L'"');
    return quoted;
}

std::string windowsErrorMessage(DWORD code) {
    LPWSTR message = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&message),
        0,
        nullptr);

    std::string result = "Windows error " + std::to_string(code);
    if (length > 0 && message) {
        const std::wstring wide(message, length);
        const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
        if (size > 0) {
            std::string utf8(size, '\0');
            WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), size, nullptr, nullptr);
            result += ": " + utf8;
        }
    }
    if (message) {
        LocalFree(message);
    }
    return result;
}

bool runCliProcess(
    const fs::path& executable,
    const std::vector<std::string>& args,
    const fs::path& workDir,
    const fs::path& logPath,
    int timeoutSeconds,
    const std::atomic<bool>& cancelRequested,
    int* exitCode,
    std::string* error) {
    std::wstring command = quoteWindowsArg(executable.wstring());
    for (const std::string& arg : args) {
        command.push_back(L' ');
        command += quoteWindowsArg(utf8ToWide(arg));
    }

    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;

    HANDLE logHandle = CreateFileW(
        logPath.wstring().c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        &security,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (logHandle == INVALID_HANDLE_VALUE) {
        if (error) {
            *error = "Failed to create sd-cli log file: " + pathArg(logPath) + " (" + windowsErrorMessage(GetLastError()) + ")";
        }
        return false;
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = logHandle;
    startup.hStdError = logHandle;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION process{};
    std::wstring mutableCommand = command;
    const std::wstring work = workDir.empty() ? std::wstring() : workDir.wstring();

    const BOOL created = CreateProcessW(
        nullptr,
        mutableCommand.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        work.empty() ? nullptr : work.c_str(),
        &startup,
        &process);
    CloseHandle(logHandle);

    if (!created) {
        if (error) {
            *error = "Failed to start sd-cli: " + windowsErrorMessage(GetLastError());
        }
        return false;
    }

    const auto started = std::chrono::steady_clock::now();
    bool cancelled = false;
    bool timedOut = false;
    for (;;) {
        const DWORD wait = WaitForSingleObject(process.hProcess, 200);
        if (wait == WAIT_OBJECT_0) {
            break;
        }
        if (wait != WAIT_TIMEOUT) {
            if (error) {
                *error = "WaitForSingleObject failed for sd-cli: " + windowsErrorMessage(GetLastError());
            }
            TerminateProcess(process.hProcess, 1);
            break;
        }

        if (cancelRequested.load()) {
            cancelled = true;
            TerminateProcess(process.hProcess, 130);
            break;
        }

        if (timeoutSeconds > 0) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - started).count();
            if (elapsed >= timeoutSeconds) {
                timedOut = true;
                TerminateProcess(process.hProcess, 124);
                break;
            }
        }
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);

    if (exitCode) {
        *exitCode = static_cast<int>(code);
    }
    if (cancelled) {
        if (error) {
            *error = "sd-cli job was cancelled.";
        }
        return false;
    }
    if (timedOut) {
        if (error) {
            *error = "sd-cli timed out after " + std::to_string(timeoutSeconds) + " seconds.";
        }
        return false;
    }
    return true;
}
#else
bool runCliProcess(
    const fs::path&,
    const std::vector<std::string>&,
    const fs::path&,
    const fs::path&,
    int,
    const std::atomic<bool>&,
    int*,
    std::string* error) {
    if (error) {
        *error = "CLI process execution is currently implemented for Windows only.";
    }
    return false;
}
#endif

ImageResult generateImageWithCli(
    const ModelPaths& paths,
    const RuntimeSettings& settings,
    const fs::path& cliExecutable,
    const fs::path& cliWorkDir,
    JobId jobId,
    const ImageRequest& request,
    const ProgressCallback& progress,
    const std::atomic<bool>& cancelRequested) {
    ImageResult result;
    result.jobId = jobId;
    result.metadata = resultMetadata(request, ExecutionMode::CliProcess, &paths, &settings);
    result.metadata["cli_executable"] = pathString(cliExecutable);
    const auto started = std::chrono::steady_clock::now();
    auto finish = [&](ImageResult&& value) {
        value.durationSeconds = elapsedSecondsSince(started);
        value.metadata["duration_seconds"] = std::to_string(value.durationSeconds);
        return std::move(value);
    };

    if (cancelRequested.load()) {
        result.state = JobState::Cancelled;
        result.error = "Job was cancelled before it started.";
        return finish(std::move(result));
    }

    if (!request.initImage.empty() || !request.maskImage.empty() || !request.controlImage.empty()) {
        result.error = "Image-to-image, inpainting, and ControlNet request inputs are reserved in the API but not wired in this first implementation.";
        return finish(std::move(result));
    }
    if (!request.loras.empty()) {
        result.error = "LoRA request inputs are reserved in the API but not wired in this first implementation.";
        return finish(std::move(result));
    }
    if (request.batchCount != 1) {
        result.error = "The CLI process backend currently supports batchCount == 1. Batch output naming will be wired in a later pass.";
        return finish(std::move(result));
    }

    std::error_code ec;
    fs::path outputDir = settings.outputDirectory.empty()
        ? fs::temp_directory_path(ec) / "tcxStableDiffusion"
        : settings.outputDirectory;
    if (ec) {
        outputDir = fs::path("tcxStableDiffusionOutputs");
    }
    fs::create_directories(outputDir, ec);
    if (ec) {
        result.error = "Failed to create output directory: " + outputDir.string() + " (" + ec.message() + ")";
        return finish(std::move(result));
    }

    const std::string stem = makeOutputStem(jobId);
    const fs::path outputPath = outputDir / (stem + ".png");
    const fs::path logPath = outputDir / (stem + ".log");
    result.outputPath = outputPath;
    result.metadata["native_output_path"] = pathArg(outputPath);
    result.metadata["cli_log"] = pathArg(logPath);

    std::vector<std::string> args;
    addPathArgument(args, "-m", paths.model);
    addPathArgument(args, "--diffusion-model", paths.diffusionModel);
    addPathArgument(args, "--high-noise-diffusion-model", paths.highNoiseDiffusionModel);
    addPathArgument(args, "--uncond-diffusion-model", paths.unconditionalDiffusionModel);
    addPathArgument(args, "--clip_l", paths.clipL);
    addPathArgument(args, "--clip_g", paths.clipG);
    addPathArgument(args, "--clip_vision", paths.clipVision);
    addPathArgument(args, "--t5xxl", paths.t5xxl);
    addPathArgument(args, "--llm", paths.llm);
    addPathArgument(args, "--llm_vision", paths.llmVision);
    addPathArgument(args, "--vae", paths.vae);
    addPathArgument(args, "--audio-vae", paths.audioVae);
    addPathArgument(args, "--control-net", paths.controlNet);
    addPathArgument(args, "--photo-maker", paths.photoMaker);

    addStringArgument(args, "-p", request.prompt);
    addStringArgument(args, "-n", request.negativePrompt);
    args.emplace_back("--cfg-scale");
    args.emplace_back(std::to_string(request.cfgScale));
    args.emplace_back("--steps");
    args.emplace_back(std::to_string(request.steps));
    args.emplace_back("-W");
    args.emplace_back(std::to_string(request.width));
    args.emplace_back("-H");
    args.emplace_back(std::to_string(request.height));
    if (request.seed >= 0) {
        args.emplace_back("--seed");
        args.emplace_back(std::to_string(request.seed));
    }
    const char* sampler = cliSamplerName(request.sampler);
    if (sampler && *sampler) {
        args.emplace_back("--sampling-method");
        args.emplace_back(sampler);
    }
    if (!settings.backendAssignment.empty()) {
        args.emplace_back("--backend");
        args.emplace_back(settings.backendAssignment);
    }
    if (!settings.paramsBackendAssignment.empty()) {
        args.emplace_back("--params-backend");
        args.emplace_back(settings.paramsBackendAssignment);
    }
    if (settings.cpuThreads > 0) {
        args.emplace_back("--threads");
        args.emplace_back(std::to_string(settings.cpuThreads));
    }
    if (settings.maxVramGiB != 0.0f) {
        args.emplace_back("--max-vram");
        args.emplace_back(std::to_string(settings.maxVramGiB));
    }
    addBoolArgument(args, "--mmap", settings.mmap);
    addBoolArgument(args, "--offload-to-cpu", settings.offloadParamsToCpu);
    addBoolArgument(args, "--clip-on-cpu", settings.keepTextEncoderOnCpu);
    addBoolArgument(args, "--vae-on-cpu", settings.keepVaeOnCpu);
    addBoolArgument(args, "--control-net-cpu", settings.keepControlNetOnCpu);
    addBoolArgument(args, "--stream-layers", settings.streamLayers);
    addBoolArgument(args, "--fa", settings.flashAttention);
    addBoolArgument(args, "--diffusion-fa", settings.diffusionFlashAttention);
    addBoolArgument(args, "--diffusion-conv-direct", settings.diffusionConvDirect);
    addBoolArgument(args, "--vae-conv-direct", settings.vaeConvDirect);
    args.emplace_back("-v");
    args.emplace_back("-o");
    args.emplace_back(pathArg(outputPath));

    sendProgress(progress, jobId, JobState::Running, "sd-cli process started: " + pathArg(logPath));

    int exitCode = 1;
    std::string processError;
    const bool processOk = runCliProcess(
        cliExecutable,
        args,
        cliWorkDir.empty() ? cliExecutable.parent_path() : cliWorkDir,
        logPath,
        settings.processTimeoutSeconds,
        cancelRequested,
        &exitCode,
        &processError);

    if (cancelRequested.load()) {
        result.state = JobState::Cancelled;
        result.error = processError.empty() ? "Job was cancelled." : processError;
        return finish(std::move(result));
    }

    if (!processOk || exitCode != 0) {
        result.error = processError.empty()
            ? "sd-cli failed with exit code " + std::to_string(exitCode)
            : processError + " Exit code: " + std::to_string(exitCode);
        result.error += ". Log: " + pathArg(logPath);
        result.metadata["cli_exit_code"] = std::to_string(exitCode);
        return finish(std::move(result));
    }
    result.metadata["cli_exit_code"] = std::to_string(exitCode);

    if (!fs::exists(outputPath)) {
        result.error = "sd-cli finished but did not create the expected output image: " + pathArg(outputPath) + ". Log: " + pathArg(logPath);
        return finish(std::move(result));
    }

    if (!result.pixels.load(outputPath)) {
        result.error = "Generated image exists but could not be loaded: " + pathArg(outputPath);
        return finish(std::move(result));
    }

    result.ok = true;
    result.state = JobState::Complete;
    sendProgress(progress, jobId, JobState::Complete, "sd-cli process completed: " + pathArg(outputPath));
    return finish(std::move(result));
}

#if TCXSD_HAS_NATIVE
sample_method_t toNativeSampler(Sampler sampler, const sd_ctx_t* ctx) {
    if (sampler == Sampler::Auto && ctx) {
        return sd_get_default_sample_method(ctx);
    }

    switch (sampler) {
        case Sampler::Euler: return EULER_SAMPLE_METHOD;
        case Sampler::EulerA: return EULER_A_SAMPLE_METHOD;
        case Sampler::Dpmpp2M: return DPMPP2M_SAMPLE_METHOD;
        case Sampler::DdimTrailing: return DDIM_TRAILING_SAMPLE_METHOD;
        case Sampler::Auto: return EULER_SAMPLE_METHOD;
    }
    return EULER_SAMPLE_METHOD;
}

const char* fallbackBackend(Backend backend) {
    switch (backend) {
        case Backend::Cuda: return "cuda0";
        case Backend::Metal: return "metal";
        case Backend::Cpu: return "cpu";
        case Backend::Auto: return "auto";
    }
    return "auto";
}

struct ProgressBridge {
    JobId jobId = 0;
    ProgressCallback callback;
};

void progressCallback(int step, int steps, float seconds, void* data) {
    auto* bridge = static_cast<ProgressBridge*>(data);
    if (!bridge || !bridge->callback) {
        return;
    }

    Progress progress;
    progress.jobId = bridge->jobId;
    progress.state = JobState::Running;
    progress.step = step;
    progress.totalSteps = steps;
    progress.seconds = seconds;
    bridge->callback(progress);
}

void logCallback(sd_log_level_t level, const char* text, void* data) {
    auto* bridge = static_cast<ProgressBridge*>(data);
    if (!bridge || !bridge->callback || !text) {
        return;
    }

    Progress progress;
    progress.jobId = bridge->jobId;
    progress.state = level == SD_LOG_ERROR ? JobState::Failed : JobState::Running;
    progress.message = std::string("stable-diffusion.cpp: ") + text;
    bridge->callback(progress);
}
#endif

} // namespace

struct NativeRuntime::Impl {
    ModelPaths paths;
    RuntimeSettings settings;
    bool cliMode = false;
    fs::path cliExecutable;
    fs::path cliWorkDir;

#if TCXSD_HAS_NATIVE
    sd_ctx_t* ctx = nullptr;
    mutable std::mutex mutex;
#endif
};

NativeRuntime::NativeRuntime()
    : impl_(std::make_unique<Impl>()) {
}

NativeRuntime::~NativeRuntime() {
    shutdown();
}

bool NativeRuntime::available() {
    return TCXSD_HAS_NATIVE != 0;
}

std::string NativeRuntime::systemInfo() {
#if TCXSD_HAS_NATIVE
    const char* info = sd_get_system_info();
    return info ? info : std::string();
#else
    return "stable-diffusion.cpp native runtime is not installed.";
#endif
}

bool NativeRuntime::setup(const ModelPaths& paths, const RuntimeSettings& settings, std::string* error) {
    shutdown();
    impl_->paths = paths;
    impl_->settings = settings;

    if (!paths.hasImagePipeline()) {
        if (error) {
            *error = "No image model path was provided.";
        }
        return false;
    }

    if (!requireExistingPath(paths.model, "model", error) ||
        !requireExistingPath(paths.diffusionModel, "diffusion model", error) ||
        !requireExistingPath(paths.unconditionalDiffusionModel, "unconditional diffusion model", error) ||
        !requireExistingPath(paths.llm, "llm", error) ||
        !requireExistingPath(paths.vae, "vae", error) ||
        !requireExistingPath(paths.controlNet, "control net", error)) {
        return false;
    }

#if TCXSD_HAS_NATIVE
    std::lock_guard<std::mutex> lock(impl_->mutex);
    const fs::path cliExecutable = resolveCliExecutable(settings);
    if (shouldUseCliProcess(settings, cliExecutable)) {
        if (!existingFile(cliExecutable)) {
            if (error) {
                *error = "sd-cli executable does not exist: " + cliExecutable.string();
            }
            return false;
        }
        impl_->cliMode = true;
        impl_->cliExecutable = cliExecutable;
        impl_->cliWorkDir = settings.cliWorkDir.empty() ? cliExecutable.parent_path() : absolutePath(settings.cliWorkDir);
        return true;
    }

    impl_->cliMode = false;
    impl_->cliExecutable.clear();
    impl_->cliWorkDir.clear();

    const std::string model = pathString(paths.model);
    const std::string diffusion = pathString(paths.diffusionModel);
    const std::string highNoiseDiffusion = pathString(paths.highNoiseDiffusionModel);
    const std::string uncondDiffusion = pathString(paths.unconditionalDiffusionModel);
    const std::string clipL = pathString(paths.clipL);
    const std::string clipG = pathString(paths.clipG);
    const std::string clipVision = pathString(paths.clipVision);
    const std::string t5xxl = pathString(paths.t5xxl);
    const std::string llm = pathString(paths.llm);
    const std::string llmVision = pathString(paths.llmVision);
    const std::string vae = pathString(paths.vae);
    const std::string audioVae = pathString(paths.audioVae);
    const std::string controlNet = pathString(paths.controlNet);
    const std::string photoMaker = pathString(paths.photoMaker);

    std::string backend = settings.backendAssignment.empty()
        ? fallbackBackend(settings.backend)
        : settings.backendAssignment;
    std::string paramsBackend = settings.paramsBackendAssignment;
    if (paramsBackend.empty() && settings.offloadParamsToCpu) {
        paramsBackend = "cpu";
    }

    sd_ctx_params_t params;
    sd_ctx_params_init(&params);
    params.model_path = model.empty() ? nullptr : model.c_str();
    params.diffusion_model_path = diffusion.empty() ? nullptr : diffusion.c_str();
    params.high_noise_diffusion_model_path = highNoiseDiffusion.empty() ? nullptr : highNoiseDiffusion.c_str();
    params.uncond_diffusion_model_path = uncondDiffusion.empty() ? nullptr : uncondDiffusion.c_str();
    params.clip_l_path = clipL.empty() ? nullptr : clipL.c_str();
    params.clip_g_path = clipG.empty() ? nullptr : clipG.c_str();
    params.clip_vision_path = clipVision.empty() ? nullptr : clipVision.c_str();
    params.t5xxl_path = t5xxl.empty() ? nullptr : t5xxl.c_str();
    params.llm_path = llm.empty() ? nullptr : llm.c_str();
    params.llm_vision_path = llmVision.empty() ? nullptr : llmVision.c_str();
    params.vae_path = vae.empty() ? nullptr : vae.c_str();
    params.audio_vae_path = audioVae.empty() ? nullptr : audioVae.c_str();
    params.control_net_path = controlNet.empty() ? nullptr : controlNet.c_str();
    params.photo_maker_path = photoMaker.empty() ? nullptr : photoMaker.c_str();
    params.n_threads = settings.cpuThreads > 0 ? settings.cpuThreads : sd_get_num_physical_cores();
    params.rng_type = settings.backend == Backend::Cuda ? CUDA_RNG : STD_DEFAULT_RNG;
    params.sampler_rng_type = RNG_TYPE_COUNT;
    params.enable_mmap = settings.mmap;
    params.flash_attn = settings.flashAttention;
    params.diffusion_flash_attn = settings.diffusionFlashAttention;
    params.diffusion_conv_direct = settings.diffusionConvDirect;
    params.vae_conv_direct = settings.vaeConvDirect;
    params.offload_params_to_cpu = settings.offloadParamsToCpu;
    params.keep_clip_on_cpu = settings.keepTextEncoderOnCpu;
    params.keep_vae_on_cpu = settings.keepVaeOnCpu;
    params.keep_control_net_on_cpu = settings.keepControlNetOnCpu;
    params.max_vram = settings.maxVramGiB;
    params.stream_layers = settings.streamLayers;
    params.backend = backend.empty() ? nullptr : backend.c_str();
    params.params_backend = paramsBackend.empty() ? nullptr : paramsBackend.c_str();

    impl_->ctx = new_sd_ctx(&params);
    if (!impl_->ctx) {
        if (error) {
            *error = "stable-diffusion.cpp failed to create a context. Check model paths, VRAM, CUDA driver, and backend flags.";
        }
        return false;
    }

    return true;
#else
    if (error) {
        *error = "stable-diffusion.cpp native runtime is not installed. Run: python tools/setup_sd.py build-native --profile windows-cuda";
    }
    return false;
#endif
}

void NativeRuntime::shutdown() {
#if TCXSD_HAS_NATIVE
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->ctx) {
        free_sd_ctx(impl_->ctx);
        impl_->ctx = nullptr;
    }
    impl_->cliMode = false;
    impl_->cliExecutable.clear();
    impl_->cliWorkDir.clear();
#endif
}

bool NativeRuntime::isLoaded() const {
#if TCXSD_HAS_NATIVE
    return impl_->ctx != nullptr || impl_->cliMode;
#else
    return false;
#endif
}

ImageResult NativeRuntime::generateImage(
    JobId jobId,
    const ImageRequest& request,
    const ProgressCallback& progress,
    const std::atomic<bool>& cancelRequested) {
    ImageResult result;
    result.jobId = jobId;

    if (cancelRequested.load()) {
        result.state = JobState::Cancelled;
        result.error = "Job was cancelled before it started.";
        return result;
    }

#if TCXSD_HAS_NATIVE
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->cliMode) {
        return generateImageWithCli(
            impl_->paths,
            impl_->settings,
            impl_->cliExecutable,
            impl_->cliWorkDir,
            jobId,
            request,
            progress,
            cancelRequested);
    }

    result.metadata = resultMetadata(request, ExecutionMode::InProcess, &impl_->paths, &impl_->settings);
    const auto started = std::chrono::steady_clock::now();
    auto finish = [&](ImageResult&& value) {
        value.durationSeconds = elapsedSecondsSince(started);
        value.metadata["duration_seconds"] = std::to_string(value.durationSeconds);
        return std::move(value);
    };

    if (!impl_->ctx) {
        result.error = "Native runtime is not loaded.";
        return finish(std::move(result));
    }

    if (!request.initImage.empty() || !request.maskImage.empty() || !request.controlImage.empty()) {
        result.error = "Image-to-image, inpainting, and ControlNet request inputs are reserved in the API but not wired in this first implementation.";
        return finish(std::move(result));
    }
    if (!request.loras.empty()) {
        result.error = "LoRA request inputs are reserved in the API but not wired in this first implementation.";
        return finish(std::move(result));
    }

    std::string prompt = request.prompt;
    std::string negativePrompt = request.negativePrompt;

    sd_img_gen_params_t params;
    sd_img_gen_params_init(&params);
    params.prompt = prompt.c_str();
    params.negative_prompt = negativePrompt.empty() ? nullptr : negativePrompt.c_str();
    params.width = request.width;
    params.height = request.height;
    params.sample_params.sample_steps = request.steps;
    params.sample_params.sample_method = toNativeSampler(request.sampler, impl_->ctx);
    params.sample_params.scheduler = sd_get_default_scheduler(impl_->ctx, params.sample_params.sample_method);
    params.sample_params.guidance.txt_cfg = request.cfgScale;
    params.strength = request.strength;
    params.seed = request.seed;
    params.batch_count = request.batchCount > 0 ? request.batchCount : 1;

    ProgressBridge bridge{jobId, progress};
    sd_set_log_callback(logCallback, &bridge);
    sd_set_progress_callback(progressCallback, &bridge);
    sd_image_t* images = generate_image(impl_->ctx, &params);
    sd_set_progress_callback(nullptr, nullptr);
    sd_set_log_callback(nullptr, nullptr);

    if (cancelRequested.load()) {
        result.state = JobState::Cancelled;
        result.error = "Job was cancelled.";
    }

    if (!images) {
        if (result.error.empty()) {
            result.error = "stable-diffusion.cpp returned no images.";
        }
        return finish(std::move(result));
    }

    const int count = params.batch_count;
    sd_image_t first = images[0];
    if (first.data && first.width > 0 && first.height > 0 && first.channel > 0) {
        result.pixels.setFromPixels(first.data, static_cast<int>(first.width), static_cast<int>(first.height), static_cast<int>(first.channel));
        result.ok = result.state != JobState::Cancelled;
        result.state = result.ok ? JobState::Complete : JobState::Cancelled;
    } else if (result.error.empty()) {
        result.error = "Generated image buffer was empty.";
    }

    for (int i = 0; i < count; ++i) {
        std::free(images[i].data);
        images[i].data = nullptr;
    }
    std::free(images);

    return finish(std::move(result));
#else
    (void)request;
    (void)progress;
    result.error = "stable-diffusion.cpp native runtime is not installed. Run: python tools/setup_sd.py build-native --profile windows-cuda";
    return result;
#endif
}

} // namespace tcx::sd
