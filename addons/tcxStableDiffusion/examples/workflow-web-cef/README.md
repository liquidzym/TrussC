# workflow-web-cef

这是 tcxStableDiffusion 的 Windows 桌面工作流示例：前端是中文网页界面，宿主使用 `tcxCEF` 打开本地页面，后端通过内置 Node runtime 调用 `sd-server.exe` 和 Node 包真实生成图片。

用户拿到打包目录后只需要双击：

```text
workflow-web-cef.exe
```

运行时不依赖 Python，也不要求用户安装 Node、npm 或 CEF。Python 只用于开发期准备资产、下载/校验模型和生成发布目录。

## 开发期准备

在 addon 根目录执行：

```powershell
python tools\prepare_workflow_web_cef_assets.py --all
```

这个命令会准备：

- Windows CEF runtime 和 wrapper。
- 便携 Node runtime。
- `sd-server.exe`、`sd-cli.exe`、`stable-diffusion.dll`。
- Ideogram4、FLUX.2-klein、Z-Image、SD 1.5 ControlNet Canny 模型目录。
- ControlNet、图生图、inpaint 所需的本地输入图。

## 构建前端和 worker

```powershell
cd examples\workflow-web-cef\web
npm install
npm run build

cd ..\worker
npm install
npm run build
```

## 构建和打包

先用 TrussC/CMake 构建 `workflow-web-cef.exe`，再回到 addon 根目录执行：

```powershell
python tools\package_workflow_web_cef.py
```

发布目录默认生成到：

```text
dist/workflow-web-cef-windows-x64
```

这个目录包含 exe、CEF runtime、网页产物、worker、便携 Node、Node 包、native SD runtime、模型、输入图和输出目录，适合直接交给用户打开。

## 已落地功能

- 中文工作流节点界面。
- 深色配土黄金视觉风格。
- 文生图、ControlNet Canny、inpaint、LoRA 栈四类示例。
- 真实 `sd-server.exe` 调用，不走 mock。
- 队列、取消、侧车 JSON 查看、seed 复用、LoRA 扫描。
- 结构化错误码和中文 remediation hints。
