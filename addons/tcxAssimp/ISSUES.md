# tcxAssimp — 开发日志 & 已知问题

## 已解决问题

### 1. Assimp v5.4.3 / v5.4.1 / master — Clang 21 编译错误
- **现象**: `::aiGetMaterialTexture` 等全局命名空间查找失败
- **原因**: Clang 21 (Xcode 26) 对 `::ai*` C-linkage 函数的全局命名空间查找更严格
- **修复**: CMake `FetchContent_MakeAvailable` 后用 `file(READ/WRITE)` + `string(REGEX REPLACE)` 全局替换 `::aiGet` → `aiGet`、`::aiReturn` → `aiReturn` 等前缀

### 2. Assimp 动态库链接失败 — `libassimp.5.dylib` not found
- **现象**: dyld crash: `Library not loaded: @rpath/libassimp.5.dylib`
- **原因**: FetchContent 默认编译 Assimp 为动态库，macOS .app bundle 未包含
- **修复**: `set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)` 强制静态链接（须在 `FetchContent_Declare` 之前设置）

### 3. SceneData.h 双重 `namespace tcx::assimp {`
- **现象**: `Model.cpp:171: error: expected '}'` / `Importer.cpp:202: error: expected '}'`
- **原因**: 编辑时误加了第二个 `namespace tcx::assimp {`
- **修复**: 删除重复的 namespace 声明

### 4. 模型路径加载失败
- **现象**: `getDataPath("Fox/glTF/Fox.gltf")` 返回无效路径
- **原因**:
  1. Fox 模型只有 `.fbx` 格式，无 `.gltf` 子目录
  2. 数据在 `bin/data/`，从 exe 路径需 `../../../data/`（3 层上至 bin，进入 data）
- **修复**:
  - 路径: `setDataPathRoot("../../../data/")`
  - 文件名: `getDataPath("Fox/Fox_05.fbx")`

### 5. 模型缩放归一化无效
- **现象**: `setScaleNormalize(true)` 调用后模型大小不变
- **原因**: `setScaleNormalize(true)` 在 `load()` **之后**调用，但 `normalizeScale()` 在 `load()` 内部执行，此时 `scaleNormalize_` 仍为 false
- **修复**: `setScaleNormalize(true)` 必须在 `load()` **之前**调用

### 6. 模型不显示（仅 placeholder 立方体）
- **现象**: 模型加载成功但画面只有旋转立方体
- **原因**: TrussC 3D PBR 管线需要 `addLight()` + `setMaterial()` + `setCameraPosition()` 才能渲染
- **修复**: 在 draw 中添加光照和材质设置

### 7. 线框（Wireframe）坐标系镜像
- **现象**: W 键线框出现在模型下方，呈镜像倒影
- **原因**: Assimp 模型 Y-up，TrussC 屏幕坐标 Y-down。PBR 管线（`mesh.draw()`）内部处理 Y 翻转，但 `mesh.drawWireframe()` 走 sokol_gl 直绘，无 Y 翻转
- **修复**: `drawWireframe()` 内添加 `tc::scale(1.0f, -1.0f, 1.0f)` 手动翻转 Y 轴

### 8. Quaternion API
- TrussC `tc::Quaternion` 无 `fromEuler()` 静态方法
- 需手动用欧拉→四元数公式计算
- `tc::Quaternion::slerp(a, b, t)` 存在且可用
- `tc::Quaternion::toMatrix()` 返回 `tc::Mat4`

### 9. Mesh API
- `tc::Mesh` 自带 `drawWireframe()` — 不要手写 `drawLine` 循环
- `tc::Mesh` 有 `addVertex/addNormal/addTexCoord/addColor/addIndex`
- `tc::Mesh::draw()` 通过 PBR 管线，需要灯光材质

## 注意事项

- TrussC 3D 坐标系 Y-down（屏幕坐标），Assimp 默认 Y-up
- TrussC `getDataPath()` 从 exe 所在目录解析相对路径
- TrussC PBR 管线必须先 `addLight()` → `setMaterial()` → `setCameraPosition()` → 再画 mesh
- FetchContent 的 CACHE 变量必须在 `FetchContent_Declare` 之前设置才能生效

## 未解决问题

### 10. 动画播放时模型变形/放大
- **现象**: 静态模型正常，按 1-9 播放动画后模型变得巨大且 mesh 混乱扭曲
- **原因**:
  1. 每个 `aiMesh` 的本地 bone index 被直接写进顶点权重，播放时却按全局 skeleton bone index 读取，多个 mesh/重复骨骼时索引错位
  2. skinning 只使用单个骨骼节点的局部动画矩阵，没有沿 Assimp node tree 累乘 parent transform
  3. 导入阶段混入 Y 翻转，顶点、节点、骨骼 offset、动画关键帧不在同一个坐标空间
  4. `Model::draw*()` 没有应用 `Model` transform 和 mesh 所属 node transform
- **修复**:
  - 导入顺序改为 node → skeleton → mesh，使 mesh 权重写入全局 bone index
  - CPU skinning 使用 animated node global transform，并按 mesh node inverse 转回 mesh local
  - 删除导入时的 Y 翻转尝试，保持 Assimp 顶点/节点/骨骼/动画在一致坐标系
  - `drawFaces` / `drawWireframe` / `drawBoundingBox` / `drawSkeleton` 统一应用 model transform
  - 全局包围盒改为 scene-space 计算，自动居中/适配会包含 node transform 和 mesh instance
  - `setScaleNormalize(true)` 支持 load 前后调用，归一化时自动把模型中心移到本地原点并按 200 world units 适配示例视图
  - 示例 viewer 默认改为正视、不自动旋转；按 `A` 开关自动旋转，左右键手动调整角度
  - 动画按键状态显示 `当前/总数` 和 duration；当源文件只导入 1 个 Assimp clip 时，按 2-9 会明确提示无对应 clip
- **状态**: ✅ 已修复，`examples/staticModelExample` 已编译通过

### 11. 多个例子重复下载 Assimp
- **现象**: 每次新建 tcxAssimp 例子都重新 clone+编译 Assimp（~10分钟）
- **原因**: TrussC `use_addon.cmake` 会把 addon 添加到每个 app 的 `build-*/addons/tcxAssimp`，默认 FetchContent 因此落到每个 app 自己的 `build-*/_deps/assimp-*`
- **修复**: `tcxAssimp/CMakeLists.txt` 显式设置 Assimp `SOURCE_DIR` / `BINARY_DIR` / `SUBBUILD_DIR` 到 addon 内共享目录：
  - source: `addons/tcxAssimp/_fcache/assimp-src`
  - build: `addons/tcxAssimp/build-<platform>/assimp-build`
  - subbuild: `addons/tcxAssimp/build-<platform>/assimp-subbuild`
  - 并启用 `UPDATE_DISCONNECTED TRUE`，已有 checkout 时不再每次 configure 联网检查
- **状态**: ✅ 已修复

### 13. 材质贴图未真正进入 TrussC PBR Material
- **现象**: Assimp 能读到 texture path / embedded texture，但 runtime draw 只设置 diffuse color，外部贴图和 GLB embedded texture 不显示
- **修复**:
  - Material import 增加 base color / normal / metallic-roughness / emissive / occlusion texture 类型
  - glTF metallic-roughness 改用 `aiTextureType_GLTF_METALLIC_ROUGHNESS`，不再用 `aiTextureType_UNKNOWN` 误兜底
  - 导入 glTF alpha mode / alpha cutoff metadata，并把 base alpha / opacity 传给 TrussC material
  - 新增独立 `TextureResolver`：外部相对路径、Blender `//texture.png`、大小写文件名 fallback、GLB `*0` embedded texture
  - Runtime 为每个 material 构建 `tc::Material`，并绑定 base color / normal / metallic-roughness / emissive / occlusion texture
- **状态**: ✅ 已实现

### 14. 动画 bind pose 缺省值、暂停姿态与 CPU 开销
- **现象**:
  1. 动画 channel 缺 position / rotation / scale track 时，会用零位移 / identity / 1 倍 scale 替代，导致部分模型姿态偏移
  2. `pause()` 或非循环动画结束后，draw 可能退回静态 node transform
  3. 每个 mesh draw 都重新计算整棵 node global，skinning 内也重复计算
- **修复**:
  - Node import 保存 bind pose TRS；动画缺失 track 时回退到 node 的 bind pose
  - Animator 增加 `hasActiveClip()` / `isAdvancing()`，pause 和非循环结束仍保持当前 pose，`stop()` 才清空当前 clip
  - AnimationClip 建立 node channel lookup，避免每次按字符串线性扫描所有 channel
  - drawFaces / drawWireframe / drawSkeleton 每次 draw 只计算一次 node globals，并复用到每个 mesh
  - skinning 每帧只计算一次 node globals，再更新 bone final matrices
  - `Model::setTransform()` 改为分解 TRS，不再只取 position
- **状态**: ✅ 已修复

### 15. 缺少可自动验证的范例程序
- **现象**: 之前主要靠 viewer 肉眼确认，矩阵转换、动画状态和材质导入没有可重复检查的程序
- **修复**:
  - 新增 `examples/verifyModelExample`
  - 内置 `data/simple.obj`，固定验证 OBJ 导入和 scene bounds
  - 验证 Assimp matrix conversion、Animation bind-pose fallback、pause active pose、`Model::setTransform`
  - 如果本机存在 `staticModelExample/bin/data/Fox/Fox_05.fbx`，额外验证动画 clip、骨骼导入和 bone override API
  - 如果本机存在 `FlightHelmet.gltf`，额外验证 base-color / metallic-roughness / occlusion texture ref 导入
- **状态**: ✅ 已实现，运行结果 `tcxAssimp verification PASS`

### 16. 手动骨骼覆盖 API
- **现象**: 无法从用户代码指定某根骨骼姿态，后续做交互控制或 IK 难以接入
- **修复**:
  - `Model::findBone()` / `getBoneName()`
  - `setBoneGlobalTransform(index/name, matrix)`
  - `clearBoneOverride(index/name)` / `clearBoneOverrides()`
  - CPU skinning 和 skeleton debug draw 会使用 override 后的 model-space bone global matrix
- **状态**: ✅ 已实现基础版本

### 17. addon 内独立 GPU skinned renderer
- **现象**: CPU skinning 每帧改写 mesh 顶点，动画模型越复杂开销越高；直接改 TrussC core 的 Mesh/PBR pipeline 又会扩大影响面
- **修复**:
  - 新增 `GpuSkinnedRenderer`，在 tcxAssimp addon 内维护独立 skinned vertex buffer、index buffer、sokol shader 和 pipeline cache
  - 新增 `src/shaders/tcxAssimpSkinned.glsl`，通过已有 `core/tools/sokol-shdc` 生成 shader header
  - `Model::setGpuSkinningEnabled(true)` 启用 GPU 路径；`drawFaces()` 在可用时走 GPU skinning，否则回落 CPU skinning
  - GPU 路径只更新 bone final matrices，不再每帧 CPU 改写顶点
  - GPU 路径支持 base color texture 和 alpha mask cutoff
  - `staticModelExample` 默认请求 GPU skinning，并增加 `G` 键切换 CPU / GPU 路径
- **限制**:
  - 当前 GPU shader 支持最多 128 bones
  - 当前 GPU 路径使用简单 directional lighting；normal / metallic-roughness / emissive / occlusion 的完整 PBR parity 后续应接 TrussC renderer/pipeline
- **状态**: ✅ 已实现基础版本

### 18. 任务书示例工程和公开检查 API
- **修复**:
  - 新增 `Node` / `Mesh` view API
  - `Model` 增加 `getMesh()` / `getNode()` / `findNode()` / `getSceneMin()` / `getSceneMax()`
  - 新增 `gltfMaterialExample`
  - 新增 `fbxModelExample`
  - 新增 `fbxAnimationExample`
  - 新增 `boneControlExample`
  - 新增 `debugSceneHierarchyExample`
  - verifyModelExample 增加 Assimp matrix identity/scale、quaternion、Node parent-child transform、Mesh/Node view API 验证
- **状态**: ✅ 已实现并在 macOS 编译通过

### 19. Assimp 版本固定
- **修复**:
  - `FetchContent_Declare(assimp)` 从 `GIT_TAG master` 固定为 `v5.4.3`
  - 保留 macOS Clang 21 的 `material.inl` patch 和 SDK zlib 设置
- **状态**: ✅ 已实现

### 12. fetchContent 缓存跨项目共享
- **现象**: 不同 addon（如 tcxOpenCV, tcxAssimp）各自独立下载依赖，新建任意例子都重下载
- **建议**: TrussC 框架层面统一 `FETCHCONTENT_BASE_DIR` 到 `~/.trussc/fc_cache`
