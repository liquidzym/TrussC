#include "tcxsd/NativeRuntime.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "stb/stb_image.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#include <winhttp.h>
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

#ifndef TCXSD_NATIVE_SERVER
#define TCXSD_NATIVE_SERVER ""
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

fs::path configuredServerCandidate() {
    const std::string configured = TCXSD_NATIVE_SERVER;
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

fs::path addonServerCandidate() {
#if defined(_WIN32)
    const char* name = "sd-server.exe";
#else
    const char* name = "sd-server";
#endif
    const std::string root = TCXSD_ADDON_ROOT;
    return root.empty() ? fs::path() : fs::path(root) / "libs" / "stable-diffusion" / "current" / "bin" / name;
}

fs::path defaultOutputDirectory() {
    std::error_code ec;
    fs::path base = fs::current_path(ec);
    if (ec || base.empty()) {
        base = fs::path(".");
    }
    return base / "tcxStableDiffusionOutputs";
}

fs::path outputDirectoryForSettings(const RuntimeSettings& settings) {
    return settings.outputDirectory.empty() ? defaultOutputDirectory() : settings.outputDirectory;
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

fs::path resolveServerExecutable(const RuntimeSettings& settings) {
    if (!settings.serverExecutable.empty()) {
        return absolutePath(settings.serverExecutable);
    }

    for (const fs::path& candidate : {
             configuredServerCandidate(),
             addonServerCandidate(),
         }) {
        if (existingFile(candidate)) {
            return absolutePath(candidate);
        }
    }

    const fs::path configured = configuredServerCandidate();
    return configured.empty() ? absolutePath(addonServerCandidate()) : absolutePath(configured);
}

bool autoPrefersCliProcess(const RuntimeSettings& settings) {
#if defined(_WIN32)
    return settings.backend == Backend::Cuda || settings.backend == Backend::Auto;
#else
    (void)settings;
    return false;
#endif
}

bool autoPrefersPersistentServer(const RuntimeSettings& settings) {
#if defined(_WIN32)
    return settings.keepModelLoaded && (settings.backend == Backend::Cuda || settings.backend == Backend::Auto);
#else
    (void)settings;
    return false;
#endif
}

bool shouldUsePersistentServer(const RuntimeSettings& settings, const fs::path& serverExecutable) {
    if (settings.executionMode == ExecutionMode::PersistentServer) {
        return true;
    }
    if (settings.executionMode == ExecutionMode::CliProcess || settings.executionMode == ExecutionMode::InProcess) {
        return false;
    }
    return autoPrefersPersistentServer(settings) && existingFile(serverExecutable);
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

std::string lowerText(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

std::string errorCodeForText(const std::string& message) {
    const std::string text = lowerText(message);
    if (text.find("out of memory") != std::string::npos || text.find("cuda oom") != std::string::npos) {
        return "CUDA_OOM";
    }
    if (text.find("does not exist") != std::string::npos || text.find("was not found") != std::string::npos || text.find("missing model") != std::string::npos) {
        return "MODEL_ASSET_MISSING";
    }
    if (text.find("sd-server did not become ready") != std::string::npos || text.find("failed to start sd-server") != std::string::npos || text.find("not reachable") != std::string::npos) {
        return "SERVER_START_FAILED";
    }
    if (text.find("backend_unsupported") != std::string::npos || text.find("unsupported") != std::string::npos || text.find("not supported") != std::string::npos) {
        return "BACKEND_UNSUPPORTED";
    }
    if (text.find("may not interrupt") != std::string::npos || (text.find("cancel") != std::string::npos && text.find("interrupt") != std::string::npos)) {
        return "CANCEL_NOT_INTERRUPTIBLE";
    }
    if (text.find("no image payload") != std::string::npos || text.find("returned no images") != std::string::npos || text.find("did not create") != std::string::npos) {
        return "OUTPUT_MISSING";
    }
    if (text.find("timed out") != std::string::npos || text.find("timeout") != std::string::npos) {
        return "TIMEOUT";
    }
    return "UNKNOWN";
}

std::string remediationForCode(const std::string& code) {
    if (code == "CUDA_OOM") {
        return "Use RuntimeSettings::lowVramCuda() or ModelProfile::runtime(RuntimePreset::LowVram); reduce width, height, steps, or batchCount; close other GPU-heavy apps.";
    }
    if (code == "MODEL_ASSET_MISSING") {
        return "Run python tools/setup_sd.py download-model --model <model-id>; check modelDir points at every required asset.";
    }
    if (code == "SERVER_START_FAILED") {
        return "Confirm sd-server.exe exists, check server_log metadata, or choose another serverPort.";
    }
    if (code == "BACKEND_UNSUPPORTED") {
        return "Switch to ExecutionMode::PersistentServer for this request, or remove the unsupported field.";
    }
    if (code == "CANCEL_NOT_INTERRUPTIBLE") {
        return "Cancel was requested, but active upstream generation may finish its current step; use shorter processTimeoutSeconds or restart the managed server.";
    }
    if (code == "OUTPUT_MISSING") {
        return "Open the backend log recorded in metadata and check outputDirectory permissions and disk space.";
    }
    if (code == "TIMEOUT") {
        return "Increase processTimeoutSeconds or use a smaller draft request first.";
    }
    return "Check backend logs and inspect resolved runtime/model paths.";
}

void annotateError(ImageResult& result) {
    if (result.error.empty()) {
        return;
    }
    const std::string code = errorCodeForText(result.error);
    result.metadata["error_code"] = code;
    result.metadata["remediation_hint"] = remediationForCode(code);
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
    metadata["request_mode"] = toString(request.mode);
    metadata["quality"] = toString(request.quality);
    metadata["sampler"] = toString(request.sampler);
    metadata["steps"] = std::to_string(request.steps);
    metadata["cfg_scale"] = std::to_string(request.cfgScale);
    metadata["strength"] = std::to_string(request.strength);
    metadata["control_strength"] = std::to_string(request.controlStrength);
    metadata["upscale_factor"] = std::to_string(request.upscaleFactor);
    metadata["seed"] = std::to_string(request.seed);
    metadata["batch_count"] = std::to_string(request.batchCount);
    metadata["execution_mode"] = toString(mode);
    addPathMetadata(metadata, "init_image", request.initImage);
    addPathMetadata(metadata, "mask_image", request.maskImage);
    addPathMetadata(metadata, "control_image", request.controlImage);
    addPathMetadata(metadata, "refine_source_image", request.refineSourceImage);
    if (!request.loras.empty()) {
        metadata["lora_count"] = std::to_string(request.loras.size());
    }
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
        if (!settings->serverHost.empty()) {
            metadata["server_host"] = settings->serverHost;
        }
        addPathMetadata(metadata, "lora_model_directory", settings->loraModelDirectory);
        addPathMetadata(metadata, "hires_upscalers_directory", settings->hiresUpscalersDirectory);
        addPathMetadata(metadata, "output_root", settings->outputDirectory);
        addPathMetadata(metadata, "temp_root", settings->tempDirectory);
        addPathMetadata(metadata, "cache_root", settings->cacheDirectory);
        metadata["server_port"] = std::to_string(settings->serverPort);
        metadata["server_startup_timeout_seconds"] = std::to_string(settings->serverStartupTimeoutSeconds);
        metadata["server_poll_interval_ms"] = std::to_string(settings->serverPollIntervalMs);
        metadata["server_reuse_existing"] = boolText(settings->serverReuseExisting);
        metadata["keep_server_running"] = boolText(settings->keepServerRunning);
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

#if defined(_WIN32)
struct PersistentServerProcess {
    PROCESS_INFORMATION process{};
    bool started = false;
    fs::path logPath;
};

struct HttpResponse {
    bool ok = false;
    int status = 0;
    std::string body;
    std::string error;
};

std::string jsonEscape(const std::string& text) {
    std::ostringstream out;
    for (unsigned char c : text) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) {
                    out << "\\u"
                        << std::uppercase << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(c)
                        << std::nouppercase << std::dec;
                } else {
                    out << static_cast<char>(c);
                }
                break;
        }
    }
    return out.str();
}

std::string jsonString(const std::string& value) {
    return "\"" + jsonEscape(value) + "\"";
}

std::string serverUrl(const RuntimeSettings& settings) {
    return "http://" + settings.serverHost + ":" + std::to_string(settings.serverPort);
}

std::string mimeTypeForPath(const fs::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (ext == ".jpg" || ext == ".jpeg") {
        return "image/jpeg";
    }
    if (ext == ".webp") {
        return "image/webp";
    }
    return "image/png";
}

bool readFileBytes(const fs::path& path, std::vector<unsigned char>& bytes, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) {
            *error = "Could not read file: " + pathArg(path);
        }
        return false;
    }
    bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

bool writeFileBytes(const fs::path& path, const std::vector<unsigned char>& bytes, std::string* error) {
    std::error_code ec;
    const fs::path parent = path.parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent, ec);
        if (ec) {
            if (error) {
                *error = "Could not create output directory: " + parent.string() + " (" + ec.message() + ")";
            }
            return false;
        }
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        if (error) {
            *error = "Could not write file: " + pathArg(path);
        }
        return false;
    }
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(out);
}

bool loadPngPixelsFromFile(const fs::path& path, trussc::Pixels& pixels, std::string* error) {
    std::vector<unsigned char> bytes;
    if (!readFileBytes(path, bytes, error)) {
        return false;
    }
    if (bytes.empty()) {
        if (error) {
            *error = "PNG file is empty: " + pathArg(path);
        }
        return false;
    }
    if (bytes.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        if (error) {
            *error = "PNG file is too large to decode safely: " + pathArg(path);
        }
        return false;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* decoded = stbi_load_from_memory(
        bytes.data(),
        static_cast<int>(bytes.size()),
        &width,
        &height,
        &channels,
        4);
    if (!decoded) {
        const char* reason = stbi_failure_reason();
        if (error) {
            *error = "Generated PNG could not be decoded: " + pathArg(path);
            if (reason && *reason) {
                *error += " (" + std::string(reason) + ")";
            }
        }
        return false;
    }

    pixels.setFromPixels(decoded, width, height, 4);
    stbi_image_free(decoded);
    return true;
}

bool readPngDimensionsFromFile(const fs::path& path, int& width, int& height) {
    std::vector<unsigned char> bytes;
    std::string error;
    if (!readFileBytes(path, bytes, &error) || bytes.size() < 24) {
        return false;
    }
    static constexpr unsigned char kPngSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    if (!std::equal(std::begin(kPngSignature), std::end(kPngSignature), bytes.begin())) {
        return false;
    }
    width = (static_cast<int>(bytes[16]) << 24) |
            (static_cast<int>(bytes[17]) << 16) |
            (static_cast<int>(bytes[18]) << 8) |
            static_cast<int>(bytes[19]);
    height = (static_cast<int>(bytes[20]) << 24) |
             (static_cast<int>(bytes[21]) << 16) |
             (static_cast<int>(bytes[22]) << 8) |
             static_cast<int>(bytes[23]);
    return width > 0 && height > 0;
}

bool binaryToBase64(const std::vector<unsigned char>& bytes, std::string& encoded, std::string* error) {
    DWORD size = 0;
    if (!CryptBinaryToStringA(
            bytes.empty() ? nullptr : bytes.data(),
            static_cast<DWORD>(bytes.size()),
            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
            nullptr,
            &size)) {
        if (error) {
            *error = "CryptBinaryToStringA failed: " + windowsErrorMessage(GetLastError());
        }
        return false;
    }

    encoded.assign(size, '\0');
    if (!CryptBinaryToStringA(
            bytes.empty() ? nullptr : bytes.data(),
            static_cast<DWORD>(bytes.size()),
            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
            encoded.data(),
            &size)) {
        if (error) {
            *error = "CryptBinaryToStringA failed: " + windowsErrorMessage(GetLastError());
        }
        return false;
    }
    if (!encoded.empty() && encoded.back() == '\0') {
        encoded.pop_back();
    }
    return true;
}

bool base64ToBinary(const std::string& encoded, std::vector<unsigned char>& bytes, std::string* error) {
    DWORD size = 0;
    if (!CryptStringToBinaryA(
            encoded.c_str(),
            static_cast<DWORD>(encoded.size()),
            CRYPT_STRING_BASE64,
            nullptr,
            &size,
            nullptr,
            nullptr)) {
        if (error) {
            *error = "CryptStringToBinaryA failed: " + windowsErrorMessage(GetLastError());
        }
        return false;
    }

    bytes.assign(size, 0);
    if (!CryptStringToBinaryA(
            encoded.c_str(),
            static_cast<DWORD>(encoded.size()),
            CRYPT_STRING_BASE64,
            bytes.data(),
            &size,
            nullptr,
            nullptr)) {
        if (error) {
            *error = "CryptStringToBinaryA failed: " + windowsErrorMessage(GetLastError());
        }
        return false;
    }
    bytes.resize(size);
    return true;
}

std::string fileToDataUrl(const fs::path& path, std::string* error) {
    std::vector<unsigned char> bytes;
    if (!readFileBytes(path, bytes, error)) {
        return {};
    }
    std::string encoded;
    if (!binaryToBase64(bytes, encoded, error)) {
        return {};
    }
    return "data:" + mimeTypeForPath(path) + ";base64," + encoded;
}

bool parseJsonStringAt(const std::string& json, size_t quote, std::string& value) {
    if (quote >= json.size() || json[quote] != '"') {
        return false;
    }
    value.clear();
    for (size_t i = quote + 1; i < json.size(); ++i) {
        const char ch = json[i];
        if (ch == '"') {
            return true;
        }
        if (ch != '\\') {
            value.push_back(ch);
            continue;
        }
        if (++i >= json.size()) {
            return false;
        }
        const char escaped = json[i];
        switch (escaped) {
            case '"': value.push_back('"'); break;
            case '\\': value.push_back('\\'); break;
            case '/': value.push_back('/'); break;
            case 'b': value.push_back('\b'); break;
            case 'f': value.push_back('\f'); break;
            case 'n': value.push_back('\n'); break;
            case 'r': value.push_back('\r'); break;
            case 't': value.push_back('\t'); break;
            case 'u':
                value.push_back('?');
                i += std::min<size_t>(4, json.size() - i - 1);
                break;
            default:
                value.push_back(escaped);
                break;
        }
    }
    return false;
}

std::string jsonStringValue(const std::string& json, const std::string& key, size_t start = 0) {
    const std::string needle = "\"" + key + "\"";
    size_t keyPos = json.find(needle, start);
    if (keyPos == std::string::npos) {
        return {};
    }
    size_t colon = json.find(':', keyPos + needle.size());
    if (colon == std::string::npos) {
        return {};
    }
    size_t quote = json.find('"', colon + 1);
    if (quote == std::string::npos) {
        return {};
    }
    std::string value;
    return parseJsonStringAt(json, quote, value) ? value : std::string();
}

bool serverStatusIsTerminal(const std::string& status) {
    return status == "completed" || status == "failed" || status == "cancelled";
}

HttpResponse httpRequest(
    const std::string& method,
    const std::string& host,
    int port,
    const std::string& path,
    const std::string& body,
    int timeoutSeconds) {
    HttpResponse response;
    HINTERNET session = WinHttpOpen(
        L"tcxStableDiffusion/0.1",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!session) {
        response.error = "WinHttpOpen failed: " + windowsErrorMessage(GetLastError());
        return response;
    }

    const int timeoutMs = timeoutSeconds > 0 ? timeoutSeconds * 1000 : 30000;
    WinHttpSetTimeouts(session, 5000, 5000, timeoutMs, timeoutMs);

    const std::wstring wideHost = utf8ToWide(host);
    HINTERNET connect = WinHttpConnect(session, wideHost.c_str(), static_cast<INTERNET_PORT>(port), 0);
    if (!connect) {
        response.error = "WinHttpConnect failed: " + windowsErrorMessage(GetLastError());
        WinHttpCloseHandle(session);
        return response;
    }

    const std::wstring wideMethod = utf8ToWide(method);
    const std::wstring widePath = utf8ToWide(path);
    HINTERNET request = WinHttpOpenRequest(
        connect,
        wideMethod.c_str(),
        widePath.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        0);
    if (!request) {
        response.error = "WinHttpOpenRequest failed: " + windowsErrorMessage(GetLastError());
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return response;
    }

    std::wstring headers;
    if (!body.empty()) {
        headers = L"Content-Type: application/json\r\nAccept: application/json\r\n";
    }
    const BOOL sent = WinHttpSendRequest(
        request,
        headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
        headers.empty() ? 0 : static_cast<DWORD>(headers.size()),
        body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()),
        static_cast<DWORD>(body.size()),
        static_cast<DWORD>(body.size()),
        0);
    if (!sent || !WinHttpReceiveResponse(request, nullptr)) {
        response.error = "WinHTTP request failed: " + windowsErrorMessage(GetLastError());
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return response;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(
        request,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &status,
        &statusSize,
        WINHTTP_NO_HEADER_INDEX);
    response.status = static_cast<int>(status);

    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) {
            response.error = "WinHttpQueryDataAvailable failed: " + windowsErrorMessage(GetLastError());
            break;
        }
        if (available == 0) {
            break;
        }
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request, chunk.data(), available, &read)) {
            response.error = "WinHttpReadData failed: " + windowsErrorMessage(GetLastError());
            break;
        }
        chunk.resize(read);
        response.body += chunk;
    }

    response.ok = response.error.empty() && response.status >= 200 && response.status < 300;
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return response;
}

HttpResponse serverGet(const RuntimeSettings& settings, const std::string& path, int timeoutSeconds = 10) {
    return httpRequest("GET", settings.serverHost, settings.serverPort, path, {}, timeoutSeconds);
}

HttpResponse serverPost(const RuntimeSettings& settings, const std::string& path, const std::string& body, int timeoutSeconds = 30) {
    return httpRequest("POST", settings.serverHost, settings.serverPort, path, body, timeoutSeconds);
}

std::vector<std::string> buildServerArguments(const ModelPaths& paths, const RuntimeSettings& settings) {
    std::vector<std::string> args;
    addStringArgument(args, "--listen-ip", settings.serverHost.empty() ? "127.0.0.1" : settings.serverHost);
    args.emplace_back("--listen-port");
    args.emplace_back(std::to_string(settings.serverPort > 0 ? settings.serverPort : 1234));
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
    addPathArgument(args, "--lora-model-dir", settings.loraModelDirectory);
    addPathArgument(args, "--hires-upscalers-dir", settings.hiresUpscalersDirectory);
    addStringArgument(args, "--backend", settings.backendAssignment);
    addStringArgument(args, "--params-backend", settings.paramsBackendAssignment);
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
    return args;
}

std::string serverLoraPath(const fs::path& path, const RuntimeSettings& settings) {
    if (path.empty()) {
        return {};
    }
    if (path.is_relative()) {
        return path.generic_string();
    }
    if (!settings.loraModelDirectory.empty()) {
        std::error_code ec;
        const fs::path base = fs::absolute(settings.loraModelDirectory, ec);
        const fs::path item = fs::absolute(path, ec);
        const fs::path relative = fs::relative(item, base, ec);
        if (!ec && !relative.empty()) {
            const std::string generic = relative.generic_string();
            if (generic != "." && generic.rfind("..", 0) != 0) {
                return generic;
            }
        }
    }
    return path.generic_string();
}

fs::path serverLogPathForSettings(const RuntimeSettings& settings) {
    std::error_code ec;
    fs::path outputDir = outputDirectoryForSettings(settings);
    fs::create_directories(outputDir, ec);
    return outputDir / ("tcxsd_server_" + makeOutputStem(0) + ".log");
}

void stopPersistentServerProcess(PersistentServerProcess& server, bool keepRunning);

bool startPersistentServerProcess(
    const ModelPaths& paths,
    const RuntimeSettings& settings,
    const fs::path& executable,
    const fs::path& workDir,
    PersistentServerProcess& server,
    std::string* error) {
    if (settings.serverReuseExisting) {
        const HttpResponse health = serverGet(settings, "/sdcpp/v1/capabilities", 5);
        if (health.ok) {
            return true;
        }
        if (error) {
            *error = "Existing sd-server is not reachable at " + serverUrl(settings) + ": " + health.error;
        }
        return false;
    }

    std::wstring command = quoteWindowsArg(executable.wstring());
    for (const std::string& arg : buildServerArguments(paths, settings)) {
        command.push_back(L' ');
        command += quoteWindowsArg(utf8ToWide(arg));
    }

    server.logPath = serverLogPathForSettings(settings);

    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;

    HANDLE logHandle = CreateFileW(
        server.logPath.wstring().c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        &security,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (logHandle == INVALID_HANDLE_VALUE) {
        if (error) {
            *error = "Failed to create sd-server log file: " + pathArg(server.logPath) + " (" + windowsErrorMessage(GetLastError()) + ")";
        }
        return false;
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = logHandle;
    startup.hStdError = logHandle;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    std::wstring mutableCommand = command;
    const std::wstring work = workDir.empty() ? executable.parent_path().wstring() : workDir.wstring();
    PROCESS_INFORMATION process{};
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
            *error = "Failed to start sd-server: " + windowsErrorMessage(GetLastError());
        }
        return false;
    }

    server.process = process;
    server.started = true;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(settings.serverStartupTimeoutSeconds > 0 ? settings.serverStartupTimeoutSeconds : 120);
    while (std::chrono::steady_clock::now() < deadline) {
        DWORD exitCode = STILL_ACTIVE;
        if (GetExitCodeProcess(server.process.hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
            if (error) {
                *error = "sd-server exited during startup with code " + std::to_string(static_cast<int>(exitCode)) + ". Log: " + pathArg(server.logPath);
            }
            stopPersistentServerProcess(server, false);
            return false;
        }

        const HttpResponse health = serverGet(settings, "/sdcpp/v1/capabilities", 3);
        if (health.ok) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    if (error) {
        *error = "sd-server did not become ready at " + serverUrl(settings) + ". Log: " + pathArg(server.logPath);
    }
    stopPersistentServerProcess(server, false);
    return false;
}

void stopPersistentServerProcess(PersistentServerProcess& server, bool keepRunning) {
    if (!server.started) {
        return;
    }
    if (!keepRunning) {
        DWORD exitCode = STILL_ACTIVE;
        if (GetExitCodeProcess(server.process.hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
            TerminateProcess(server.process.hProcess, 0);
            WaitForSingleObject(server.process.hProcess, 5000);
        }
    }
    CloseHandle(server.process.hThread);
    CloseHandle(server.process.hProcess);
    server.process = {};
    server.started = false;
}

std::string buildSdcppImageRequest(const ImageRequest& request, const RuntimeSettings& settings, std::string* error) {
    std::ostringstream out;
    out << "{";
    out << "\"prompt\":" << jsonString(request.prompt) << ",";
    out << "\"negative_prompt\":" << jsonString(request.negativePrompt) << ",";
    out << "\"width\":" << request.width << ",";
    out << "\"height\":" << request.height << ",";
    out << "\"strength\":" << request.strength << ",";
    out << "\"seed\":" << request.seed << ",";
    out << "\"batch_count\":" << (request.batchCount > 0 ? request.batchCount : 1) << ",";
    out << "\"control_strength\":" << request.controlStrength << ",";
    out << "\"sample_params\":{";
    const char* sampler = cliSamplerName(request.sampler);
    if (sampler && *sampler) {
        out << "\"sample_method\":" << jsonString(sampler) << ",";
    }
    out << "\"sample_steps\":" << request.steps << ",";
    out << "\"guidance\":{\"txt_cfg\":" << request.cfgScale << "}},";

    if (!request.initImage.empty()) {
        const std::string dataUrl = fileToDataUrl(request.initImage, error);
        if (dataUrl.empty()) {
            return {};
        }
        out << "\"init_image\":" << jsonString(dataUrl) << ",";
    }
    if (!request.maskImage.empty()) {
        const std::string dataUrl = fileToDataUrl(request.maskImage, error);
        if (dataUrl.empty()) {
            return {};
        }
        out << "\"mask_image\":" << jsonString(dataUrl) << ",";
    }
    if (!request.controlImage.empty()) {
        const std::string dataUrl = fileToDataUrl(request.controlImage, error);
        if (dataUrl.empty()) {
            return {};
        }
        out << "\"control_image\":" << jsonString(dataUrl) << ",";
    }

    out << "\"lora\":[";
    for (size_t i = 0; i < request.loras.size(); ++i) {
        if (i > 0) {
            out << ",";
        }
        out << "{\"path\":" << jsonString(serverLoraPath(request.loras[i].path, settings))
            << ",\"multiplier\":" << request.loras[i].weight
            << ",\"is_high_noise\":false}";
    }
    out << "],";
    out << "\"output_format\":\"png\",";
    out << "\"output_compression\":100";
    out << "}";
    return out.str();
}

ImageResult generateImageWithPersistentServer(
    const ModelPaths& paths,
    const RuntimeSettings& settings,
    const fs::path& serverExecutable,
    const fs::path& serverLogPath,
    JobId jobId,
    const ImageRequest& request,
    const ProgressCallback& progress,
    const std::atomic<bool>& cancelRequested) {
    ImageResult result;
    result.jobId = jobId;
    result.metadata = resultMetadata(request, ExecutionMode::PersistentServer, &paths, &settings);
    result.metadata["server_url"] = serverUrl(settings);
    result.metadata["server_executable"] = pathString(serverExecutable);
    if (!serverLogPath.empty()) {
        result.metadata["server_log"] = pathArg(serverLogPath);
    }

    const auto started = std::chrono::steady_clock::now();
    auto finish = [&](ImageResult&& value) {
        value.durationSeconds = elapsedSecondsSince(started);
        value.metadata["duration_seconds"] = std::to_string(value.durationSeconds);
        annotateError(value);
        return std::move(value);
    };

    if (cancelRequested.load()) {
        result.state = JobState::Cancelled;
        result.error = "Job was cancelled before it started.";
        return finish(std::move(result));
    }

    const auto capabilities = BackendCapabilities::forRuntime(paths, settings, ExecutionMode::PersistentServer);
    if (!capabilities.supports(request)) {
        result.error = capabilities.unsupportedReason(request, ExecutionMode::PersistentServer);
        result.metadata["capability_check"] = "failed";
        return finish(std::move(result));
    }

    if (request.batchCount != 1) {
        result.error = "The persistent server backend currently returns the first image only. Use batchCount == 1 until batch result plumbing is added.";
        return finish(std::move(result));
    }

    std::error_code ec;
    fs::path outputDir = outputDirectoryForSettings(settings);
    fs::create_directories(outputDir, ec);
    if (ec) {
        result.error = "Failed to create output directory: " + outputDir.string() + " (" + ec.message() + ")";
        return finish(std::move(result));
    }

    const std::string stem = makeOutputStem(jobId);
    const fs::path outputPath = outputDir / (stem + ".png");
    result.outputPath = outputPath;
    result.metadata["native_output_path"] = pathArg(outputPath);

    const HttpResponse health = serverGet(settings, "/sdcpp/v1/capabilities", 10);
    if (!health.ok) {
        result.error = "sd-server is not reachable at " + serverUrl(settings) + ": " + (health.error.empty() ? health.body : health.error);
        return finish(std::move(result));
    }

    std::string requestError;
    const std::string body = buildSdcppImageRequest(request, settings, &requestError);
    if (body.empty()) {
        result.error = requestError.empty() ? "Failed to build sd-server request body." : requestError;
        return finish(std::move(result));
    }
    result.metadata["server_request_api"] = "/sdcpp/v1/img_gen";

    sendProgress(progress, jobId, JobState::Running, "sd-server job submit: " + serverUrl(settings));
    const HttpResponse submit = serverPost(settings, "/sdcpp/v1/img_gen", body, 30);
    if (!submit.ok) {
        result.error = "sd-server submit failed with HTTP " + std::to_string(submit.status) + ": " + (submit.error.empty() ? submit.body : submit.error);
        return finish(std::move(result));
    }

    std::string serverJobId = jsonStringValue(submit.body, "id");
    std::string pollUrl = jsonStringValue(submit.body, "poll_url");
    std::string status = jsonStringValue(submit.body, "status");
    if (pollUrl.empty() && !serverJobId.empty()) {
        pollUrl = "/sdcpp/v1/jobs/" + serverJobId;
    }
    if (pollUrl.empty()) {
        result.error = "sd-server submit response did not include poll_url. Response: " + submit.body;
        return finish(std::move(result));
    }
    result.metadata["server_job_id"] = serverJobId;

    const auto deadline = settings.processTimeoutSeconds > 0
        ? std::chrono::steady_clock::now() + std::chrono::seconds(settings.processTimeoutSeconds)
        : std::chrono::steady_clock::time_point::max();
    const int pollIntervalMs = settings.serverPollIntervalMs > 0 ? settings.serverPollIntervalMs : 500;
    std::string pollBody = submit.body;

    while (!serverStatusIsTerminal(status)) {
        if (cancelRequested.load()) {
            const HttpResponse cancel = serverPost(settings, pollUrl + "/cancel", "{}", 10);
            result.state = JobState::Cancelled;
            result.error = cancel.ok
                ? "sd-server job was cancelled."
                : "Cancellation requested. sd-server may not interrupt active generation yet: " + (cancel.error.empty() ? cancel.body : cancel.error);
            result.metadata["server_cancel_response_status"] = std::to_string(cancel.status);
            return finish(std::move(result));
        }

        if (std::chrono::steady_clock::now() >= deadline) {
            const HttpResponse cancel = serverPost(settings, pollUrl + "/cancel", "{}", 10);
            result.error = "sd-server job timed out after " + std::to_string(settings.processTimeoutSeconds) + " seconds.";
            result.metadata["server_cancel_response_status"] = std::to_string(cancel.status);
            return finish(std::move(result));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
        const HttpResponse poll = serverGet(settings, pollUrl, 30);
        if (!poll.ok) {
            result.error = "sd-server poll failed with HTTP " + std::to_string(poll.status) + ": " + (poll.error.empty() ? poll.body : poll.error);
            return finish(std::move(result));
        }
        pollBody = poll.body;
        status = jsonStringValue(pollBody, "status");
        sendProgress(progress, jobId, JobState::Running, "sd-server job status: " + status);
        if (status.empty()) {
            result.error = "sd-server poll response did not include status. Response: " + pollBody;
            return finish(std::move(result));
        }
    }

    result.metadata["server_status"] = status;
    if (status == "cancelled") {
        result.state = JobState::Cancelled;
        result.error = jsonStringValue(pollBody, "message");
        if (result.error.empty()) {
            result.error = "sd-server job was cancelled.";
        }
        return finish(std::move(result));
    }
    if (status == "failed") {
        result.error = jsonStringValue(pollBody, "message");
        if (result.error.empty()) {
            result.error = "sd-server job failed. Response: " + pollBody;
        }
        return finish(std::move(result));
    }

    const std::string encoded = jsonStringValue(pollBody, "b64_json");
    if (encoded.empty()) {
        result.error = "sd-server completed but returned no image payload.";
        return finish(std::move(result));
    }

    std::vector<unsigned char> pngBytes;
    std::string decodeError;
    if (!base64ToBinary(encoded, pngBytes, &decodeError)) {
        result.error = decodeError;
        return finish(std::move(result));
    }
    if (!writeFileBytes(outputPath, pngBytes, &decodeError)) {
        result.error = decodeError;
        return finish(std::move(result));
    }
    if (!loadPngPixelsFromFile(outputPath, result.pixels, &decodeError)) {
        result.error = decodeError.empty()
            ? "Generated image exists but could not be loaded: " + pathArg(outputPath)
            : decodeError;
        return finish(std::move(result));
    }

    result.ok = true;
    result.state = JobState::Complete;
    sendProgress(progress, jobId, JobState::Complete, "sd-server job completed: " + pathArg(outputPath));
    return finish(std::move(result));
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
        annotateError(value);
        return std::move(value);
    };

    if (cancelRequested.load()) {
        result.state = JobState::Cancelled;
        result.error = "Job was cancelled before it started.";
        return finish(std::move(result));
    }

    const auto capabilities = BackendCapabilities::forRuntime(paths, settings, ExecutionMode::CliProcess);
    if (!capabilities.supports(request)) {
        result.error = capabilities.unsupportedReason(request, ExecutionMode::CliProcess);
        result.metadata["capability_check"] = "failed";
        return finish(std::move(result));
    }

    if (!request.loras.empty()) {
        result.error = "BACKEND_UNSUPPORTED: per-request LoRA stacks are not supported by CliProcess. Use PersistentServer for LoRA requests.";
        return finish(std::move(result));
    }
    if (request.batchCount != 1) {
        result.error = "The CLI process backend currently supports batchCount == 1. Batch output naming will be wired in a later pass.";
        return finish(std::move(result));
    }

    std::error_code ec;
    fs::path outputDir = outputDirectoryForSettings(settings);
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
    addPathArgument(args, "--lora-model-dir", settings.loraModelDirectory);

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
    addPathArgument(args, "--init-img", request.initImage);
    addPathArgument(args, "--mask", request.maskImage);
    addPathArgument(args, "--control-image", request.controlImage);
    if (!request.initImage.empty()) {
        args.emplace_back("--strength");
        args.emplace_back(std::to_string(request.strength));
    }
    if (!request.controlImage.empty()) {
        args.emplace_back("--control-strength");
        args.emplace_back(std::to_string(request.controlStrength));
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

    int outputWidth = 0;
    int outputHeight = 0;
    if (readPngDimensionsFromFile(outputPath, outputWidth, outputHeight)) {
        result.metadata["image_width"] = std::to_string(outputWidth);
        result.metadata["image_height"] = std::to_string(outputHeight);
    }
    result.metadata["pixel_decode"] = "skipped_for_cli_process";

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
    bool serverMode = false;
    fs::path cliExecutable;
    fs::path cliWorkDir;
    fs::path serverExecutable;
    fs::path serverWorkDir;

#if defined(_WIN32)
    PersistentServerProcess serverProcess;
#endif

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
    const fs::path serverExecutable = resolveServerExecutable(settings);
    if (shouldUsePersistentServer(settings, serverExecutable)) {
        if (!existingFile(serverExecutable)) {
            if (settings.executionMode == ExecutionMode::PersistentServer) {
                if (error) {
                    *error = "sd-server executable does not exist: " + serverExecutable.string();
                }
                return false;
            }
        } else {
#if defined(_WIN32)
            PersistentServerProcess serverProcess;
            std::string serverError;
            const fs::path serverWorkDir = settings.serverWorkDir.empty()
                ? serverExecutable.parent_path()
                : absolutePath(settings.serverWorkDir);
            if (startPersistentServerProcess(paths, settings, serverExecutable, serverWorkDir, serverProcess, &serverError)) {
                impl_->serverMode = true;
                impl_->cliMode = false;
                impl_->serverExecutable = serverExecutable;
                impl_->serverWorkDir = serverWorkDir;
                impl_->serverProcess = serverProcess;
                impl_->cliExecutable.clear();
                impl_->cliWorkDir.clear();
                return true;
            }
            stopPersistentServerProcess(serverProcess, false);
            if (settings.executionMode == ExecutionMode::PersistentServer) {
                if (error) {
                    *error = serverError;
                }
                return false;
            }
#else
            if (settings.executionMode == ExecutionMode::PersistentServer) {
                if (error) {
                    *error = "Persistent sd-server execution is currently implemented for Windows only.";
                }
                return false;
            }
#endif
        }
    }

    if (shouldUseCliProcess(settings, cliExecutable)) {
        if (!existingFile(cliExecutable)) {
            if (error) {
                *error = "sd-cli executable does not exist: " + cliExecutable.string();
            }
            return false;
        }
        impl_->cliMode = true;
        impl_->serverMode = false;
        impl_->cliExecutable = cliExecutable;
        impl_->cliWorkDir = settings.cliWorkDir.empty() ? cliExecutable.parent_path() : absolutePath(settings.cliWorkDir);
        impl_->serverExecutable.clear();
        impl_->serverWorkDir.clear();
        return true;
    }

    impl_->cliMode = false;
    impl_->serverMode = false;
    impl_->cliExecutable.clear();
    impl_->cliWorkDir.clear();
    impl_->serverExecutable.clear();
    impl_->serverWorkDir.clear();

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
#if defined(_WIN32)
    stopPersistentServerProcess(impl_->serverProcess, impl_->settings.keepServerRunning);
#endif
    if (impl_->ctx) {
        free_sd_ctx(impl_->ctx);
        impl_->ctx = nullptr;
    }
    impl_->cliMode = false;
    impl_->serverMode = false;
    impl_->cliExecutable.clear();
    impl_->cliWorkDir.clear();
    impl_->serverExecutable.clear();
    impl_->serverWorkDir.clear();
#endif
}

bool NativeRuntime::isLoaded() const {
#if TCXSD_HAS_NATIVE
    return impl_->ctx != nullptr || impl_->cliMode || impl_->serverMode;
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
    if (impl_->serverMode) {
#if defined(_WIN32)
        return generateImageWithPersistentServer(
            impl_->paths,
            impl_->settings,
            impl_->serverExecutable,
            impl_->serverProcess.logPath,
            jobId,
            request,
            progress,
            cancelRequested);
#else
        result.error = "Persistent sd-server execution is currently implemented for Windows only.";
        return result;
#endif
    }

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
        annotateError(value);
        return std::move(value);
    };

    if (!impl_->ctx) {
        result.error = "Native runtime is not loaded.";
        return finish(std::move(result));
    }

    const auto capabilities = BackendCapabilities::forRuntime(impl_->paths, impl_->settings, ExecutionMode::InProcess);
    if (!capabilities.supports(request)) {
        result.error = capabilities.unsupportedReason(request, ExecutionMode::InProcess);
        result.metadata["capability_check"] = "failed";
        return finish(std::move(result));
    }

    if (!request.initImage.empty() || !request.maskImage.empty() || !request.controlImage.empty()) {
        result.error = "BACKEND_UNSUPPORTED: image-to-image, inpainting, and ControlNet inputs are not supported by InProcess. Use PersistentServer or CliProcess for this request.";
        return finish(std::move(result));
    }
    if (!request.loras.empty()) {
        result.error = "BACKEND_UNSUPPORTED: per-request LoRA stacks are not supported by InProcess. Use PersistentServer for LoRA requests.";
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
    sendProgress(progress, jobId, JobState::Running, "direct in-process generate_image enter");
    sd_image_t* images = generate_image(impl_->ctx, &params);
    sendProgress(progress, jobId, JobState::Running, "direct in-process generate_image returned");
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
