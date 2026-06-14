# Workflow Web CEF Example Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Windows-ready `workflow-web-cef` example that opens as a local TrussC desktop app, presents a visual TypeScript workflow editor, and runs real tcxStableDiffusion jobs through the Node package without requiring end users to install Python, npm, CMake, or model files manually.

**Architecture:** The TrussC executable owns the native window, `tcxCEF::LocalAssetServer`, `tcxCEF::WebSocketBridge`, and CEF browser lifecycle. The web UI connects to the C++ bridge, C++ forwards JSONL messages to a bundled Node worker, and the worker calls `@trussc/tcx-stable-diffusion` with a persistent `sd-server` session. The release package bundles CEF runtime files, built web assets, the Node worker, a portable Node runtime, stable-diffusion.cpp binaries, and required model assets under `bin/data`.

**Tech Stack:** TrussC C++20, `tcxCEF`, `tcxStableDiffusion`, TypeScript plus Vite static build, portable Node 20 runtime, `@trussc/tcx-stable-diffusion`, stable-diffusion.cpp `sd-server`, Windows x64 CEF binary distribution.

---

## Product Constraints

- End users launch `workflow-web-cef.exe` directly from the packaged folder.
- End users do not run Python, npm, CMake, git, trusscli, model download commands, or CEF setup commands.
- Python remains allowed for developer setup, packaging, tests, and asset verification only.
- Node remains allowed as a bundled runtime inside the packaged app, not as a user-installed global dependency.
- All runtime network traffic stays local after setup: `http://127.0.0.1:<assetPort>/`, `ws://127.0.0.1:<bridgePort>/bridge`, and `http://127.0.0.1:<sdServerPort>/`.
- Every visible workflow node maps to a real backend request or returns a structured `BACKEND_UNSUPPORTED` error with remediation hints.
- Chinese prompts and visible Chinese UI labels must survive web UI, JSON workflow files, Node worker messages, sidecars, and generated metadata as UTF-8 text.
- The current ImGui example remains the native workbench and smoke harness. Do not move node canvas, gallery history, or batch workflow graph editing into the ImGui example.

## Current Evidence

- `addons/tcxCEF/libs/cef/current/cef_paths.cmake` was absent before Windows setup.
- `python addons/tcxCEF/tools/setup_cef.py --dry-run --config Release` resolves:

```text
platform: windows64
version:  144.0.27+g3fae261+chromium-144.0.7559.254
archive:  https://cef-builds.spotifycdn.com/cef_binary_144.0.27+g3fae261+chromium-144.0.7559.254_windows64.tar.bz2
```

- `tcxCEF` already exposes `Browser`, `LocalAssetServer`, `WebSocketBridge`, and `tcxcef_copy_runtime_files(<target>)`.
- `tcxMediaPipe` is the local reference for CEF setup, asset server usage, WebSocket bridge events, and post-build web asset copying.
- Windows wrapper build initially failed on MSVC warning `C4819` because upstream CEF enables `/WX` and the active code page was 936. `tcxCEF/tools/setup_cef.py` now appends `/utf-8` through the MSVC `CL` environment for Windows CEF wrapper builds.

## File Structure

Create:

- `examples/workflow-web-cef/addons.make`: declares `tcxCEF`, `tcxStableDiffusion`, `tcxTls`, and `tcxWebSocket`.
- `examples/workflow-web-cef/CMakeLists.txt`: builds the TrussC app, copies CEF runtime files, copies web and worker assets into `bin/`.
- `examples/workflow-web-cef/src/main.cpp`: TrussC entry point.
- `examples/workflow-web-cef/src/tcApp.h`: app state, CEF bridge, and worker process fields.
- `examples/workflow-web-cef/src/tcApp.cpp`: local asset server startup, bridge startup, browser startup, worker process lifecycle, bridge forwarding.
- `examples/workflow-web-cef/src/NodeWorkerProcess.h`: C++ interface for JSONL stdio worker messaging.
- `examples/workflow-web-cef/src/NodeWorkerProcess.cpp`: Windows process launch and pipe handling for bundled `node.exe`.
- `examples/workflow-web-cef/web/package.json`: Vite TypeScript build scripts.
- `examples/workflow-web-cef/web/index.html`: static CEF entry page.
- `examples/workflow-web-cef/web/src/main.ts`: UI bootstrap and bridge connection.
- `examples/workflow-web-cef/web/src/theme.css`: dark charcoal, warm gray, and earthy-gold UI theme.
- `examples/workflow-web-cef/web/src/workflow/schema.ts`: typed workflow graph schema.
- `examples/workflow-web-cef/web/src/workflow/examples.ts`: real sample workflow definitions.
- `examples/workflow-web-cef/web/src/workflow/validate.ts`: graph validation before run.
- `examples/workflow-web-cef/web/src/ui/GraphCanvas.ts`: node canvas, drag, select, connect, and viewport logic.
- `examples/workflow-web-cef/web/src/ui/Inspector.ts`: selected node editor.
- `examples/workflow-web-cef/web/src/ui/QueuePanel.ts`: job status, logs, cancel button, error remediation hints.
- `examples/workflow-web-cef/web/src/ui/Gallery.ts`: output thumbnails, sidecar open state, reuse seed controls.
- `examples/workflow-web-cef/worker/package.json`: worker build and test scripts.
- `examples/workflow-web-cef/worker/src/protocol.mjs`: JSONL message contract.
- `examples/workflow-web-cef/worker/src/workflow-executor.mjs`: converts graph nodes into Node package requests.
- `examples/workflow-web-cef/worker/src/worker.mjs`: stdin/stdout JSONL worker entry.
- `examples/workflow-web-cef/workflows/text-to-image.zh.json`: Chinese text-to-image sample.
- `examples/workflow-web-cef/workflows/controlnet-canny.json`: real SD15 ControlNet sample.
- `examples/workflow-web-cef/workflows/inpaint.json`: real inpaint sample.
- `examples/workflow-web-cef/workflows/lora-stack.json`: real LoRA stack sample with structured missing-LoRA error until a local LoRA is present.
- `examples/workflow-web-cef/bin/data/README.md`: explains bundled data layout.
- `tests/test_workflow_web_cef_plan.py`: static checks for scaffold, CMake runtime copy, no runtime Python dependency, and UTF-8 guardrails.
- `node/test/workflow-web-cef-protocol.test.mjs`: Node worker protocol and graph execution unit tests.
- `tools/prepare_workflow_web_cef_assets.py`: developer-only asset staging for CEF, portable Node, stable-diffusion runtime, and model assets.
- `tools/package_workflow_web_cef.py`: developer-only release folder builder.

Modify:

- `docs/ROADMAP_RUNTIME_WORKFLOWS.md`: link this plan and mark the ImGui workbench as frozen except bug fixes and smoke coverage.
- `README.md`: add a short pointer to the new CEF workflow example after the example exists.

Do not modify:

- `examples/ideogram4-basic` for graph UI features. Only touch it for regressions found while running shared tests.

---

### Task 1: Prepare Windows CEF Runtime

**Files:**
- Generated outside this repo: `G:/TrussC/addons/tcxCEF/libs/cef/current/cef_paths.cmake`
- Generated outside this repo: `G:/TrussC/addons/tcxCEF/libs/cef/current/build_manifest.json`

- [ ] **Step 1: Confirm the Windows CEF archive selection**

Run from `G:/TrussC`:

```powershell
python addons/tcxCEF/tools/setup_cef.py --dry-run --config Release
```

Expected output includes:

```text
CEF setup dry run
platform: windows64
archive:  https://cef-builds.spotifycdn.com/cef_binary_144.0.27+g3fae261+chromium-144.0.7559.254_windows64.tar.bz2
```

- [ ] **Step 2: Download CEF and build only `libcef_dll_wrapper`**

Run from `G:/TrussC`:

```powershell
python addons/tcxCEF/tools/setup_cef.py --config Release
```

Expected output includes:

```text
CEF setup complete
```

This command downloads a CEF binary distribution and compiles `libcef_dll_wrapper`. It must not build Chromium or CEF from source.

On Windows, `setup_cef.py` must keep the `CL=/utf-8` build environment behavior. Without it, MSVC can emit `C4819` for UTF-8 CEF headers and fail because the CEF wrapper project treats warnings as errors.

- [ ] **Step 3: Verify generated CEF paths**

Run from `G:/TrussC`:

```powershell
python addons/tcxCEF/tools/verify_cef.py
```

Expected output:

```text
tcxCEF setup verified
```

- [ ] **Step 4: Assert Windows runtime files exist**

Run from `G:/TrussC`:

```powershell
$manifest = Get-Content addons/tcxCEF/libs/cef/current/build_manifest.json -Raw | ConvertFrom-Json
@(
  $manifest.wrapper_library,
  $manifest.libcef_library,
  (Join-Path $manifest.release_dir "libcef.dll"),
  (Join-Path $manifest.resource_dir "icudtl.dat")
) | ForEach-Object {
  if (-not (Test-Path $_)) { throw "Missing CEF runtime file: $_" }
}
"CEF Windows runtime files present"
```

Expected output:

```text
CEF Windows runtime files present
```

- [ ] **Step 5: Keep generated CEF artifacts out of the tcxStableDiffusion commit**

Run from `G:/TrussC/addons/tcxStableDiffusion`:

```powershell
git status --short
```

Expected: no changes from CEF generated files inside this repo.

---

### Task 2: Add Static Tests For The New Example Contract

**Files:**
- Create: `tests/test_workflow_web_cef_plan.py`

- [ ] **Step 1: Write the failing static test**

Create `tests/test_workflow_web_cef_plan.py`:

```python
import json
import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
EXAMPLE = ROOT / "examples" / "workflow-web-cef"


class WorkflowWebCefPlanTest(unittest.TestCase):
    def test_scaffold_files_exist(self):
        required = [
            EXAMPLE / "addons.make",
            EXAMPLE / "CMakeLists.txt",
            EXAMPLE / "src" / "main.cpp",
            EXAMPLE / "src" / "tcApp.h",
            EXAMPLE / "src" / "tcApp.cpp",
            EXAMPLE / "src" / "NodeWorkerProcess.h",
            EXAMPLE / "src" / "NodeWorkerProcess.cpp",
            EXAMPLE / "web" / "package.json",
            EXAMPLE / "web" / "index.html",
            EXAMPLE / "web" / "src" / "main.ts",
            EXAMPLE / "web" / "src" / "workflow" / "schema.ts",
            EXAMPLE / "worker" / "package.json",
            EXAMPLE / "worker" / "src" / "worker.mjs",
            EXAMPLE / "workflows" / "text-to-image.zh.json",
            EXAMPLE / "workflows" / "controlnet-canny.json",
        ]
        missing = [path for path in required if not path.exists()]
        self.assertEqual([], missing)

    def test_cmake_copies_cef_web_and_worker_assets(self):
        cmake = (EXAMPLE / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("tcxcef_copy_runtime_files(workflow-web-cef)", cmake)
        self.assertIn("workflow-web-cef/web", cmake)
        self.assertIn("workflow-web-cef/worker", cmake)
        self.assertIn("runtime/node", cmake)

    def test_addons_make_declares_cef_and_sd(self):
        addons = (EXAMPLE / "addons.make").read_text(encoding="utf-8")
        for name in ["tcxCEF", "tcxStableDiffusion", "tcxTls", "tcxWebSocket"]:
            self.assertRegex(addons, rf"(?m)^{re.escape(name)}$")

    def test_no_runtime_python_dependency_in_cpp_or_worker(self):
        files = list((EXAMPLE / "src").glob("*.cpp")) + list((EXAMPLE / "src").glob("*.h"))
        files += list((EXAMPLE / "worker" / "src").glob("*.mjs"))
        joined = "\n".join(path.read_text(encoding="utf-8") for path in files)
        self.assertNotIn("python ", joined.lower())
        self.assertNotIn("python.exe", joined.lower())
        self.assertNotIn("tools/setup_", joined)

    def test_chinese_workflow_is_utf8_and_not_garbled(self):
        workflow = json.loads((EXAMPLE / "workflows" / "text-to-image.zh.json").read_text(encoding="utf-8"))
        text = json.dumps(workflow, ensure_ascii=False)
        self.assertIn("本地生成", text)
        self.assertNotRegex(text, r"[涓锛绛妯]")
```

- [ ] **Step 2: Run the test and verify it fails before scaffold**

Run:

```powershell
python -m unittest tests.test_workflow_web_cef_plan -v
```

Expected: FAIL because `examples/workflow-web-cef` does not exist yet.

---

### Task 3: Scaffold The TrussC CEF Host

**Files:**
- Create: `examples/workflow-web-cef/addons.make`
- Create: `examples/workflow-web-cef/CMakeLists.txt`
- Create: `examples/workflow-web-cef/src/main.cpp`
- Create: `examples/workflow-web-cef/src/tcApp.h`
- Create: `examples/workflow-web-cef/src/tcApp.cpp`

- [ ] **Step 1: Create `addons.make`**

Use this content:

```text
# TrussC addons - one addon per line
tcxCEF
tcxStableDiffusion
tcxTls
tcxWebSocket
```

- [ ] **Step 2: Create `CMakeLists.txt` with runtime asset copy**

Use this content:

```cmake
cmake_minimum_required(VERSION 3.20)
project(workflow-web-cef)

if(NOT DEFINED TRUSSC_DIR)
    set(TRUSSC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../core")
endif()

include(${TRUSSC_DIR}/cmake/trussc_app.cmake)

trussc_app()
tcxcef_copy_runtime_files(workflow-web-cef)

set(WORKFLOW_WEB_CEF_WEB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/web")
set(WORKFLOW_WEB_CEF_WORKER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/worker")
set(WORKFLOW_WEB_CEF_RUNTIME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/runtime")

if(NOT EXISTS "${WORKFLOW_WEB_CEF_WEB_ROOT}/dist/index.html")
    message(FATAL_ERROR
        "[workflow-web-cef] web assets are missing. Run from examples/workflow-web-cef/web:\n"
        "  npm install\n"
        "  npm run build")
endif()

if(NOT EXISTS "${WORKFLOW_WEB_CEF_WORKER_ROOT}/dist/worker.mjs")
    message(FATAL_ERROR
        "[workflow-web-cef] worker bundle is missing. Run from examples/workflow-web-cef/worker:\n"
        "  npm install\n"
        "  npm run build")
endif()

if(WIN32 AND NOT EXISTS "${WORKFLOW_WEB_CEF_RUNTIME_ROOT}/node/node.exe")
    message(FATAL_ERROR
        "[workflow-web-cef] portable Node runtime is missing. Run:\n"
        "  python ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/prepare_workflow_web_cef_assets.py --node-runtime")
endif()

add_custom_target(workflow_web_cef_assets ALL
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        "$<TARGET_FILE_DIR:workflow-web-cef>/workflow-web-cef"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:workflow-web-cef>/workflow-web-cef/web"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:workflow-web-cef>/workflow-web-cef/worker"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:workflow-web-cef>/runtime"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${WORKFLOW_WEB_CEF_WEB_ROOT}/dist"
        "$<TARGET_FILE_DIR:workflow-web-cef>/workflow-web-cef/web/dist"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${WORKFLOW_WEB_CEF_WORKER_ROOT}/dist"
        "$<TARGET_FILE_DIR:workflow-web-cef>/workflow-web-cef/worker/dist"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${WORKFLOW_WEB_CEF_RUNTIME_ROOT}"
        "$<TARGET_FILE_DIR:workflow-web-cef>/runtime"
    COMMENT "[workflow-web-cef] Copying web, worker, and portable runtime assets"
    VERBATIM)
add_dependencies(workflow_web_cef_assets workflow-web-cef)
```

- [ ] **Step 3: Create `main.cpp`**

Use this content:

```cpp
#include "TrussC.h"
#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.title = "tcxStableDiffusion Workflow Studio";
    settings.width = 1440;
    settings.height = 920;
    settings.highDpi = false;
    return TC_RUN_APP(tcApp, settings);
}
```

- [ ] **Step 4: Create `tcApp.h`**

Use this content:

```cpp
#pragma once

#include "NodeWorkerProcess.h"

#include <TrussC.h>
#include <tcxCEF.h>

#include <filesystem>
#include <string>

class tcApp : public tc::App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void cleanup() override;

private:
    std::filesystem::path executableDir() const;
    std::filesystem::path webRoot() const;
    std::filesystem::path nodeExecutable() const;
    std::filesystem::path workerScript() const;
    void startWorker();
    void handleBridgeMessage(tcxCEF::WebSocketBridgeMessage& message);
    void sendStatus(const std::string& stage, const std::string& detail);

    tcxCEF::LocalAssetServer assetServer_;
    tcxCEF::WebSocketBridge bridge_;
    tcxCEF::Browser browser_;
    tc::EventListener bridgeMessageListener_;
    NodeWorkerProcess worker_;
    std::string status_ = "Starting";
    std::string lastError_;
};
```

- [ ] **Step 5: Create `tcApp.cpp`**

Use this content:

```cpp
#include "tcApp.h"

#include "nlohmann/json.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
using Json = nlohmann::json;

std::filesystem::path currentExecutablePath() {
#ifdef _WIN32
    std::array<wchar_t, 4096> buffer{};
    const DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (size > 0 && size < buffer.size()) {
        return std::filesystem::path(buffer.data());
    }
#endif
    return {};
}
}

void tcApp::setup() {
    bridgeMessageListener_ = bridge_.onMessage.listen(this, &tcApp::handleBridgeMessage);

    const auto root = webRoot();
    if (!std::filesystem::is_regular_file(root / "dist" / "index.html")) {
        lastError_ = "Web UI is missing: " + (root / "dist" / "index.html").string();
        return;
    }

    tcxCEF::LocalAssetServerSettings assetSettings;
    assetSettings.root = root;
    if (!assetServer_.start(assetSettings)) {
        lastError_ = "Failed to start local asset server at " + root.string();
        return;
    }

    tcxCEF::WebSocketBridgeSettings bridgeSettings;
    bridgeSettings.path = "/bridge";
    if (!bridge_.start(bridgeSettings)) {
        lastError_ = "Failed to start WebSocket bridge";
        assetServer_.stop();
        return;
    }

    startWorker();
    if (!lastError_.empty()) {
        bridge_.stop();
        assetServer_.stop();
        return;
    }

    tcxCEF::BrowserSettings browserSettings;
    browserSettings.url = assetServer_.url("/dist/index.html") + "?bridgePort=" + std::to_string(bridge_.port());
    browserSettings.showWindow = true;
    browserSettings.openDevTools = false;
    browserSettings.width = 1440;
    browserSettings.height = 920;
    if (!browser_.setup(browserSettings)) {
        lastError_ = browser_.lastError();
        worker_.stop();
        bridge_.stop();
        assetServer_.stop();
        return;
    }

    status_ = "Ready";
}

void tcApp::update() {
    browser_.update();
    worker_.drainMessages([this](const std::string& text) {
        bridge_.broadcast(text);
    });
    if (!browser_.lastError().empty()) {
        lastError_ = browser_.lastError();
    }
}

void tcApp::draw() {
    tc::setColor(0.05f, 0.045f, 0.035f, 1.0f);
    tc::drawRectangle(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
    if (!lastError_.empty()) {
        tc::setColor(0.92f, 0.74f, 0.38f, 1.0f);
        tc::drawBitmapString("workflow-web-cef: " + lastError_, 24, 42);
    } else {
        tc::setColor(0.78f, 0.62f, 0.27f, 1.0f);
        tc::drawBitmapString("workflow-web-cef: " + status_, 24, 42);
    }
}

void tcApp::cleanup() {
    browser_.shutdown();
    worker_.stop();
    bridge_.stop();
    assetServer_.stop();
}

std::filesystem::path tcApp::executableDir() const {
    const auto exe = currentExecutablePath();
    if (!exe.empty()) {
        return exe.parent_path();
    }
    return std::filesystem::current_path();
}

std::filesystem::path tcApp::webRoot() const {
    return executableDir() / "workflow-web-cef" / "web";
}

std::filesystem::path tcApp::nodeExecutable() const {
#ifdef _WIN32
    return executableDir() / "runtime" / "node" / "node.exe";
#else
    return executableDir() / "runtime" / "node" / "bin" / "node";
#endif
}

std::filesystem::path tcApp::workerScript() const {
    return executableDir() / "workflow-web-cef" / "worker" / "dist" / "worker.mjs";
}

void tcApp::startWorker() {
    NodeWorkerSettings settings;
    settings.nodeExecutable = nodeExecutable();
    settings.workerScript = workerScript();
    settings.cwd = executableDir();
    if (!worker_.start(settings)) {
        lastError_ = worker_.lastError();
        return;
    }
    sendStatus("native-host", "Worker started");
}

void tcApp::handleBridgeMessage(tcxCEF::WebSocketBridgeMessage& message) {
    if (!worker_.send(message.text)) {
        Json error;
        error["type"] = "error";
        error["id"] = "";
        error["error"] = {
            {"code", "WORKER_UNAVAILABLE"},
            {"message", worker_.lastError()},
            {"remediation_hints", Json::array({"Restart the app.", "Check the bundled runtime/node/node.exe and worker/dist/worker.mjs files."})}
        };
        bridge_.send(message.clientId, error.dump());
    }
}

void tcApp::sendStatus(const std::string& stage, const std::string& detail) {
    Json message;
    message["type"] = "hostStatus";
    message["stage"] = stage;
    message["detail"] = detail;
    bridge_.broadcast(message.dump());
}
```

- [ ] **Step 6: Run scaffold test**

Run:

```powershell
python -m unittest tests.test_workflow_web_cef_plan -v
```

Expected: tests still fail until `NodeWorkerProcess`, web, worker, and workflows are added.

---

### Task 4: Add Windows Node Worker Process Bridge

**Files:**
- Create: `examples/workflow-web-cef/src/NodeWorkerProcess.h`
- Create: `examples/workflow-web-cef/src/NodeWorkerProcess.cpp`

- [ ] **Step 1: Create the public interface**

Use this content for `NodeWorkerProcess.h`:

```cpp
#pragma once

#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

struct NodeWorkerSettings {
    std::filesystem::path nodeExecutable;
    std::filesystem::path workerScript;
    std::filesystem::path cwd;
};

class NodeWorkerProcess {
public:
    NodeWorkerProcess() = default;
    ~NodeWorkerProcess();

    NodeWorkerProcess(const NodeWorkerProcess&) = delete;
    NodeWorkerProcess& operator=(const NodeWorkerProcess&) = delete;

    bool start(const NodeWorkerSettings& settings);
    void stop();
    bool send(const std::string& jsonLine);
    void drainMessages(const std::function<void(const std::string&)>& emit);
    bool isRunning() const;
    std::string lastError() const;

private:
    void readerLoop();
    void pushMessage(std::string message);
    void setError(std::string error);

    NodeWorkerSettings settings_;
    mutable std::mutex mutex_;
    std::vector<std::string> messages_;
    std::string lastError_;
    std::thread readerThread_;
    bool running_ = false;

#ifdef _WIN32
    PROCESS_INFORMATION processInfo_{};
    HANDLE childStdinWrite_ = nullptr;
    HANDLE childStdoutRead_ = nullptr;
#endif
};
```

- [ ] **Step 2: Implement Windows process launch and JSONL pipe forwarding**

Use `CreateProcessW` with redirected stdin and stdout. The implementation must:

- reject missing `node.exe`
- reject missing `worker.mjs`
- launch with command line `"node.exe" "worker.mjs"`
- write each outbound message with exactly one trailing newline
- read stdout line by line and enqueue complete JSON strings
- kill the worker during `stop()`
- return `WORKER_UNAVAILABLE` through `lastError()` when pipes fail

Add a unit-style smoke in `tests/test_workflow_web_cef_plan.py` after this class exists:

```python
    def test_node_worker_process_uses_createprocess_on_windows(self):
        source = (EXAMPLE / "src" / "NodeWorkerProcess.cpp").read_text(encoding="utf-8")
        self.assertIn("CreateProcessW", source)
        self.assertIn("STARTUPINFOW", source)
        self.assertIn("WriteFile", source)
        self.assertIn("ReadFile", source)
        self.assertIn("WORKER_UNAVAILABLE", source)
```

- [ ] **Step 3: Run static tests**

Run:

```powershell
python -m unittest tests.test_workflow_web_cef_plan -v
```

Expected: process bridge checks pass; remaining web and worker files still fail.

---

### Task 5: Define The Workflow Graph Schema

**Files:**
- Create: `examples/workflow-web-cef/web/package.json`
- Create: `examples/workflow-web-cef/web/index.html`
- Create: `examples/workflow-web-cef/web/src/workflow/schema.ts`
- Create: `examples/workflow-web-cef/web/src/workflow/examples.ts`
- Create: `examples/workflow-web-cef/web/src/workflow/validate.ts`

- [ ] **Step 1: Create web package**

Use this content:

```json
{
  "name": "workflow-web-cef-ui",
  "private": true,
  "type": "module",
  "scripts": {
    "build": "vite build",
    "test": "tsc --noEmit"
  },
  "devDependencies": {
    "typescript": "^5.8.0",
    "vite": "^6.0.0"
  }
}
```

- [ ] **Step 2: Define graph node and edge types**

`schema.ts` must export these exact node kind strings:

```ts
export type WorkflowNodeKind =
  | "ModelProfile"
  | "RuntimePreset"
  | "Prompt"
  | "NegativePrompt"
  | "SourceImage"
  | "MaskImage"
  | "ControlNet"
  | "LoRAStack"
  | "Generate"
  | "QualityCheck"
  | "SaveArtifact"
  | "BatchSeeds";

export type WorkflowEdgeKind =
  | "prompt"
  | "negativePrompt"
  | "image"
  | "mask"
  | "control"
  | "lora"
  | "settings"
  | "artifact";

export interface WorkflowNode<T = Record<string, unknown>> {
  id: string;
  kind: WorkflowNodeKind;
  label: string;
  x: number;
  y: number;
  data: T;
}

export interface WorkflowEdge {
  id: string;
  from: string;
  to: string;
  kind: WorkflowEdgeKind;
}

export interface TxcSdWorkflow {
  schema: "tcxsd.workflow.v1";
  id: string;
  title: string;
  language: "en" | "zh";
  nodes: WorkflowNode[];
  edges: WorkflowEdge[];
}
```

- [ ] **Step 3: Add validation rules**

`validate.ts` must reject:

- duplicate node IDs
- edges with missing endpoints
- workflows without exactly one `Generate` node
- `ControlNet` nodes without a connected control image
- `Inpaint` workflows without both `SourceImage` and `MaskImage`
- `LoRAStack` nodes with an empty `loras` array

Validation returns:

```ts
export interface WorkflowValidation {
  ok: boolean;
  errors: Array<{ code: string; message: string; nodeId?: string }>;
  warnings: Array<{ code: string; message: string; nodeId?: string }>;
}
```

- [ ] **Step 4: Add real sample workflows**

`examples.ts` must export:

- `textToImageZhWorkflow`
- `controlNetCannyWorkflow`
- `inpaintWorkflow`
- `loraStackWorkflow`

The Chinese sample must include the exact visible text:

```text
本地生成
```

- [ ] **Step 5: Build the web TypeScript project**

Run from `examples/workflow-web-cef/web`:

```powershell
npm install
npm run test
npm run build
```

Expected:

```text
vite v...
dist/index.html
```

---

### Task 6: Build The Visual Web UI

**Files:**
- Create: `examples/workflow-web-cef/web/src/main.ts`
- Create: `examples/workflow-web-cef/web/src/theme.css`
- Create: `examples/workflow-web-cef/web/src/ui/GraphCanvas.ts`
- Create: `examples/workflow-web-cef/web/src/ui/Inspector.ts`
- Create: `examples/workflow-web-cef/web/src/ui/QueuePanel.ts`
- Create: `examples/workflow-web-cef/web/src/ui/Gallery.ts`

- [ ] **Step 1: Implement the CEF bridge client**

`main.ts` must parse `bridgePort` from `location.search`, connect to:

```ts
const socket = new WebSocket(`ws://127.0.0.1:${bridgePort}/bridge`);
```

It must send workflow commands with this envelope:

```ts
interface BridgeCommand {
  id: string;
  type: "validateWorkflow" | "runWorkflow" | "cancelJob" | "listModels" | "listLoras" | "openSidecar";
  workflow?: TxcSdWorkflow;
  jobId?: string;
  payload?: Record<string, unknown>;
}
```

- [ ] **Step 2: Implement the first-screen layout**

The first screen must contain:

- left palette with node types and model profiles
- center graph canvas
- right inspector
- bottom queue and log panel
- output gallery

No landing page, hero copy, marketing cards, or instructional feature text.

- [ ] **Step 3: Apply dark plus earthy-gold theme**

`theme.css` must define these tokens:

```css
:root {
  color-scheme: dark;
  --bg: #0d0b08;
  --panel: #17130e;
  --panel-2: #211b12;
  --line: #4d3b1c;
  --gold: #c59a42;
  --gold-2: #e1c16a;
  --text: #f4e6c1;
  --muted: #a99368;
  --danger: #f87171;
  --ok: #70d398;
}
```

Every button, node, tab, inspector field, queue row, and gallery tile must use these tokens or neutral alpha variants derived from them.

- [ ] **Step 4: Add interaction expectations**

The UI must support:

- drag nodes
- select nodes
- connect nodes by clicking one output and one input
- edit selected node fields
- load one of the bundled sample workflows
- validate before run
- run a real workflow
- cancel active job
- show structured remediation hints from backend errors
- show gallery thumbnail after a generated PNG exists
- open sidecar JSON in an inspector pane

- [ ] **Step 5: Run web tests and build**

Run:

```powershell
cd examples/workflow-web-cef/web
npm run test
npm run build
```

Expected: TypeScript passes and `dist/index.html` exists.

---

### Task 7: Implement The Node Worker Protocol

**Files:**
- Create: `examples/workflow-web-cef/worker/package.json`
- Create: `examples/workflow-web-cef/worker/src/protocol.mjs`
- Create: `examples/workflow-web-cef/worker/src/workflow-executor.mjs`
- Create: `examples/workflow-web-cef/worker/src/worker.mjs`
- Create: `node/test/workflow-web-cef-protocol.test.mjs`

- [ ] **Step 1: Create worker package**

Use this content:

```json
{
  "name": "workflow-web-cef-worker",
  "private": true,
  "type": "module",
  "scripts": {
    "build": "node scripts/build.mjs",
    "test": "node --test"
  }
}
```

Add `worker/scripts/build.mjs` that copies `src/*.mjs` into `dist/` and rewrites imports to stay relative.

- [ ] **Step 2: Define protocol helpers**

`protocol.mjs` must export:

```js
export function ok(id, type, payload = {}) {
  return { id, type, ok: true, ...payload };
}

export function fail(id, error) {
  return {
    id,
    type: "error",
    ok: false,
    error: {
      code: error?.code || "UNKNOWN",
      message: error?.message || String(error),
      remediation_hints: error?.remediationHints || error?.remediation_hints || []
    }
  };
}
```

- [ ] **Step 3: Convert graph nodes to real Node package requests**

`workflow-executor.mjs` must import from `../../../node/src/index.mjs` during development and from `@trussc/tcx-stable-diffusion` in the packaged worker build.

It must support:

- text-to-image using `createTextToImageRequest`
- image-to-image using `createImageToImageRequest`
- inpaint using `createInpaintRequest`
- ControlNet using `createControlNetRequest`
- LoRA using `createLoraStackRequest`
- batch seed sweeps using `createBatchJob`
- persistent server reuse through `createGenerationSession`
- output, temp, cache, and log roots under `bin/data/workflows`

- [ ] **Step 4: Implement stdin/stdout JSONL entry**

`worker.mjs` must:

- read one JSON object per stdin line
- handle `validateWorkflow`
- handle `runWorkflow`
- handle `cancelJob`
- handle `listModels`
- handle `listLoras`
- emit `progress`, `result`, and `error` JSON objects
- keep one active persistent session per model/runtime preset
- call `close()` on process exit

- [ ] **Step 5: Write Node protocol tests**

`node/test/workflow-web-cef-protocol.test.mjs` must assert:

- valid Chinese workflow keeps `本地生成`
- missing `Generate` node returns `WORKFLOW_INVALID`
- ControlNet without control image returns `WORKFLOW_INVALID`
- missing LoRA returns `MODEL_ASSET_MISSING`
- backend unsupported returns `BACKEND_UNSUPPORTED` with remediation hints

Run from `node`:

```powershell
npm test
```

Expected: all Node tests pass.

---

### Task 8: Add Real Workflow JSON Files And Asset Roots

**Files:**
- Create: `examples/workflow-web-cef/workflows/text-to-image.zh.json`
- Create: `examples/workflow-web-cef/workflows/controlnet-canny.json`
- Create: `examples/workflow-web-cef/workflows/inpaint.json`
- Create: `examples/workflow-web-cef/workflows/lora-stack.json`
- Create: `examples/workflow-web-cef/bin/data/README.md`

- [ ] **Step 1: Add Chinese text-to-image workflow**

The workflow must include:

```json
{
  "schema": "tcxsd.workflow.v1",
  "id": "text-to-image.zh",
  "title": "Chinese poster smoke",
  "language": "zh"
}
```

It must include a `Prompt` node with readable Chinese text containing `本地生成`.

- [ ] **Step 2: Add ControlNet Canny workflow**

The workflow must use:

```json
{
  "model": "sd15-controlnet-canny",
  "runtimePreset": "default",
  "quality": "draft",
  "controlStrength": 1.0
}
```

It must reference a local control image under `bin/data/inputs/control/canny-guide.png`.

- [ ] **Step 3: Add inpaint workflow**

The workflow must reference:

```text
bin/data/inputs/inpaint/source.png
bin/data/inputs/inpaint/mask.png
```

The worker must return `MODEL_ASSET_MISSING` with remediation hints if either file is absent.

- [ ] **Step 4: Add LoRA workflow**

The workflow must include a `LoRAStack` node with a relative LoRA path under `bin/data/models/loras`.

If no LoRA asset is bundled, the sample must intentionally return:

```json
{
  "code": "MODEL_ASSET_MISSING",
  "remediation_hints": [
    "Place a .safetensors LoRA under bin/data/models/loras.",
    "Run listLoras from the workflow UI and select the discovered relative path."
  ]
}
```

- [ ] **Step 5: Document `bin/data` lifecycle**

`bin/data/README.md` must describe:

- `models/`
- `inputs/`
- `outputs/`
- `tmp/`
- `cache/`
- `logs/`
- cleanup through the Node package cleanup API

---

### Task 9: Prepare Runtime Assets For Direct Launch

**Files:**
- Create: `tools/prepare_workflow_web_cef_assets.py`
- Create or update generated local files under: `examples/workflow-web-cef/runtime/`
- Create or update generated local files under: `examples/workflow-web-cef/bin/data/`

- [ ] **Step 1: Add developer-only asset staging tool**

The tool must provide these flags:

```text
--cef
--node-runtime
--native-sd
--models
--all
--verify-only
```

It must run developer setup commands and copy verified assets into the new example. It must never be called by C++ or the Node worker at runtime.

- [ ] **Step 2: Stage portable Node runtime**

On Windows, place:

```text
examples/workflow-web-cef/runtime/node/node.exe
examples/workflow-web-cef/runtime/node/LICENSE
examples/workflow-web-cef/runtime/node/node_modules/
```

The runtime can come from an official Node Windows x64 archive or a locally configured bundled runtime. The staged folder must be self-contained for the worker.

- [ ] **Step 3: Stage stable-diffusion native runtime**

Copy from `libs/stable-diffusion/current/bin/` into:

```text
examples/workflow-web-cef/bin/
  sd-server.exe
  sd-cli.exe
  stable-diffusion.dll
```

Verify:

```powershell
Test-Path examples/workflow-web-cef/bin/sd-server.exe
Test-Path examples/workflow-web-cef/bin/stable-diffusion.dll
```

Both return `True`.

- [ ] **Step 4: Stage model assets**

Copy or hardlink from the verified model source into:

```text
examples/workflow-web-cef/bin/data/models/
  ideogram4-q4_0/
  flux2-klein-4b-q4_0/
  z-image-turbo-q3_k/
  sd15-controlnet-canny/
    v1-5-pruned-emaonly.safetensors
    control_v11p_sd15_canny_fp16.safetensors
```

Run:

```powershell
python tools/verify_sd.py --model-root examples/workflow-web-cef/bin/data/models --model ideogram4-q4_0
python tools/verify_sd.py --model-root examples/workflow-web-cef/bin/data/models --model flux2-klein-4b-q4_0
python tools/verify_sd.py --model-root examples/workflow-web-cef/bin/data/models --model z-image-turbo-q3_k
python tools/verify_sd.py --model-root examples/workflow-web-cef/bin/data/models --model sd15-controlnet-canny
```

Expected: every command exits 0.

- [ ] **Step 5: Stage deterministic input images**

Create or copy:

```text
examples/workflow-web-cef/bin/data/inputs/control/canny-guide.png
examples/workflow-web-cef/bin/data/inputs/inpaint/source.png
examples/workflow-web-cef/bin/data/inputs/inpaint/mask.png
```

These images are test assets. They must be small, local, and redistributable.

---

### Task 10: Package The Example For End Users

**Files:**
- Create: `tools/package_workflow_web_cef.py`
- Modify: `README.md`

- [ ] **Step 1: Add release packager**

The packager must produce:

```text
dist/workflow-web-cef-windows-x64/
  workflow-web-cef.exe
  libcef.dll
  chrome_elf.dll
  icudtl.dat
  *.pak
  locales/
  workflow-web-cef/web/dist/
  workflow-web-cef/worker/dist/
  runtime/node/node.exe
  sd-server.exe
  sd-cli.exe
  stable-diffusion.dll
  data/models/
  data/inputs/
  data/outputs/
  data/tmp/
  data/cache/
  data/logs/
```

- [ ] **Step 2: Verify no external runtime tools are required**

Run with a constrained PATH from the package folder:

```powershell
$oldPath = $env:PATH
$env:PATH = "$PWD"
try {
  .\workflow-web-cef.exe
} finally {
  $env:PATH = $oldPath
}
```

Expected: app opens, web UI connects to bridge, and worker reports ready without using global `node`, `python`, `npm`, `cmake`, or `trusscli`.

- [ ] **Step 3: Add README usage text**

Document:

```text
Open examples/workflow-web-cef/bin/workflow-web-cef.exe for local development.
Open dist/workflow-web-cef-windows-x64/workflow-web-cef.exe for the packaged build.
The packaged build includes its web UI, worker, CEF runtime, Node runtime, native sd-server runtime, and model data.
```

---

### Task 11: Full Verification Chain

**Files:**
- Modify only files required by failures found during verification.

- [ ] **Step 1: Run Python tests**

```powershell
python -m unittest discover -s tests -p "test_*.py"
```

Expected: all tests pass.

- [ ] **Step 2: Run Node package tests**

```powershell
cd node
npm test
```

Expected: all tests pass.

- [ ] **Step 3: Run web and worker tests**

```powershell
cd examples/workflow-web-cef/web
npm run test
npm run build
cd ..\worker
npm test
npm run build
```

Expected: TypeScript and Node worker tests pass, `web/dist/index.html` and `worker/dist/worker.mjs` exist.

- [ ] **Step 4: Verify CEF setup**

Run from `G:/TrussC`:

```powershell
python addons/tcxCEF/tools/verify_cef.py
```

Expected:

```text
tcxCEF setup verified
```

- [ ] **Step 5: Build the new example**

Run from `G:/TrussC/addons/tcxStableDiffusion/examples/workflow-web-cef`:

```powershell
trusscli update
trusscli build
```

Expected: build succeeds and `bin/workflow-web-cef.exe` exists.

- [ ] **Step 6: Run real text-to-image smoke**

From the UI, load `text-to-image.zh.json` and run draft generation.

Expected sidecar fields:

```json
{
  "ok": true,
  "metadata": {
    "workflow_id": "text-to-image.zh",
    "language": "zh"
  }
}
```

The output PNG exists and the sidecar JSON contains `本地生成`.

- [ ] **Step 7: Run real ControlNet smoke**

From the UI, load `controlnet-canny.json` and run draft generation.

Expected:

- model is `sd15-controlnet-canny`
- request includes `controlImage`
- sidecar records `control_strength`
- output PNG exists
- no `BACKEND_UNSUPPORTED`

- [ ] **Step 8: Review code twice before commit**

First review:

```powershell
git diff -- examples/workflow-web-cef node tests tools docs README.md
```

Check API correctness, message protocol, and runtime roots.

Second review:

```powershell
git diff --check
git status --short
```

Check whitespace, generated assets, packaging boundaries, UI text, Chinese UTF-8, and untracked files.

- [ ] **Step 9: Commit after verification**

```powershell
git add docs README.md tests node examples/workflow-web-cef tools
git commit -m "Add workflow Web CEF example plan and implementation"
```

Commit generated model files only if the repo policy explicitly allows large local assets. Otherwise keep generated runtime/model assets ignored and document the packaging step.

---

## Self-Review

- Spec coverage: The plan covers direct launch packaging, Windows CEF setup, CEF runtime copy, web UI, C++ bridge, Node worker, real graph execution, ControlNet, image workflows, LoRA, Chinese UTF-8, sidecars, cleanup roots, packaging, and verification.
- Placeholder scan: The plan contains concrete file paths, commands, expected outputs, and message contracts. It avoids unspecified task entries.
- Type consistency: Workflow schema uses `TxcSdWorkflow` consistently in web commands. Backend message envelopes use `id`, `type`, `ok`, `payload`, and `error` consistently.
- Runtime boundary: Python is present only in developer setup, packaging, and tests. The packaged runtime uses C++, CEF, bundled Node, and local native/model assets.

Plan complete and saved to `docs/superpowers/plans/2026-06-12-workflow-web-cef-example.md`. Recommended execution option: Subagent-Driven implementation task by task, with review after each task group.
