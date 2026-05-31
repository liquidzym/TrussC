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

---

## 2026-05-31 代码质量审查

> 审查范围：全部公共头文件 + 实现 (.cpp) + 示例，仅关注代码质量，不含 git 相关问题。
> 整体评价：**优秀** — 架构层次分明，Assimp 依赖与运行时完全隔离

### 🔴 高优先级

#### 20. `applySkinning()` 重复计算 boneMatrix — 每顶点每帧
- **现象**: `Model::applySkinning()` 中，每个顶点的每个 bone weight 都执行 `meshNodeInv * skeleton.bones[bi].finalMatrix`，同一个 bone 的矩阵被不同顶点重复计算
- **影响**: 10K 顶点 × 4 weights = 40K 次矩阵乘法；实际只需 boneCount 次（如 50 次）
- **位置**: `src/Model.cpp` — `applySkinning()` 方法
- **建议**: 在顶点循环前预计算 `std::vector<tc::Mat4> boneMatrices(boneCount)`，循环内直接查表

```cpp
// 当前：每个顶点重复计算
for (size_t vi = 0; vi < mesh.vertices.size(); vi++) {
    for (int s = 0; s < 4; s++) {
        tc::Mat4 bm = meshNodeInv * sceneData_.skeleton.bones[bi].finalMatrix; // 重复！
    }
}

// 建议：预计算一次
std::vector<tc::Mat4> boneMats(boneCount);
for (int i = 0; i < boneCount; i++)
    boneMats[i] = meshNodeInv * sceneData_.skeleton.bones[i].finalMatrix;
for (size_t vi = 0; vi < mesh.vertices.size(); vi++) {
    // 直接使用 boneMats[bd.indices[s]]
}
```

### 🟡 中优先级

#### 21. `Skeleton::findBone()` O(n) 线性搜索
- **现象**: `findBone()` 逐字符串比较遍历 `bones` 向量
- **影响**: `importMesh()` 中每个 bone weight 都调用一次，O(bones × vertices)；`importSkeleton()` 去重也依赖它
- **位置**: `include/tcx/assimp/Bone.h` — `Skeleton::findBone()`
- **建议**: 添加 `std::unordered_map<std::string, int> boneIndex` 字段，load 时构建一次

#### 22. `computeNodeGlobalTransforms()` 代码重复
- **现象**: `Importer.cpp` 和 `Model.cpp` 各有一份几乎相同的递归节点遍历逻辑（根节点查找、递归结构、遍历顺序完全一致）
- **影响**: 如果节点遍历逻辑有 bug，需修两处
- **位置**: `src/Importer.cpp` — `computeNodeGlobals()` 和 `src/Model.cpp` — `computeNodeGlobalTransforms()`
- **建议**: 提取共享的 `computeNodeGlobals(scene, locals)` 函数，两处都调用

#### 23. `Bone::finalMatrix` 运行时状态混在 `SceneData` 中
- **现象**: `SceneData` 设计意图是"CPU 侧静态资产数据"，但 `Bone::finalMatrix` 是每帧由动画系统写入的运行时状态
- **影响**: 违反数据/状态分离原则，`const SceneData&` 在动画激活时无法保持 const 正确性
- **位置**: `include/tcx/assimp/Bone.h` — `Bone::finalMatrix`
- **建议**: 将 `finalMatrix` 移到 Model 的运行时数据中（如 `std::vector<tc::Mat4> boneFinalMatrices_`）

#### 24. `GpuSkinnedRenderer` 深度依赖 `tc::internal` 命名空间
- **现象**: 直接访问 `tc::internal::inFboPass`、`tc::internal::currentFboColorFormat`、`tc::internal::currentFboSampleCount`、`tc::internal::currentProjectionMatrix` 等全局变量和函数
- **影响**: TrussC 内部 API 变化时会静默失效或崩溃
- **位置**: `src/GpuSkinnedRenderer.cpp` — `draw()` 方法
- **建议**: 优先使用 TrussC 公开渲染上下文查询 API（如存在），否则在注释中标注依赖的内部 API 版本

#### 25. CMake `CACHE FORCE` 污染全局配置
- **现象**: `BUILD_SHARED_LIBS OFF CACHE FORCE`、`ASSIMP_BUILD_ZLIB OFF CACHE FORCE` 等影响整个 CMake 构建树
- **影响**: 可能影响同项目中其他依赖共享库的 target
- **位置**: `CMakeLists.txt`
- **建议**: 用 `set()` 而非 `CACHE FORCE`，或限定 scope

### 🟢 低优先级

#### 26. `importNode()` 递归深度风险
- **现象**: 某些 DCC 工具导出的 FBX 有嵌套 100+ 层的节点，递归遍历可能导致栈溢出
- **位置**: `src/Importer.cpp` — `importNode()`；`src/Model.cpp` — `computeNodeGlobalTransforms()`
- **建议**: 改用显式栈的迭代遍历

#### 27. `const Mesh Model::getMesh(size_t i) const` 返回值 + `const_cast`
- **现象**: 返回 `const Mesh`（非引用）且内部 `const_cast<SceneData*>`，语义不清晰
- **影响**: `const` 修饰返回值对右值无实际效果；`const_cast` 绕过了 const 安全
- **位置**: `src/Model.cpp` — `getMesh()` / `getNode()` const 重载
- **建议**: 提供 `ConstMesh` 类持有 `const SceneData*`，或将 Mesh 的修改方法标记为需要非 const 访问

#### 28. `TextureResolver::resolvePath()` 每次纹理加载遍历目录
- **现象**: 大小写不敏感回退逻辑在每次纹理加载时执行 `fs::directory_iterator`
- **影响**: 同一目录下多个纹理时重复 I/O
- **位置**: `src/TextureResolver.cpp` — `resolvePath()`
- **建议**: 缓存目录列表，或只在首次失败时遍历

#### 29. `Node::getGlobalTransform()` O(depth) 递归
- **现象**: 每次调用递归到根节点，循环遍历所有节点时总复杂度 O(n²)
- **位置**: `src/Node.cpp` — `getGlobalTransform()`
- **建议**: 文档中提示用户使用 `Model::computeNodeGlobalTransforms()` 批量获取

#### 30. `Animator` 持有裸指针 `const std::vector<AnimationClip>* clips_`
- **现象**: `clips_` 指向 `Model::sceneData_.animations`，若 Model 被 move/clear 则悬空
- **影响**: 当前 `Model` 禁止拷贝/移动，实际安全，但依赖隐式约束
- **位置**: `include/tcx/assimp/Animation.h` — `Animator::clips_`
- **建议**: 在文档注释中明确 "clips_ 必须在 Animator 整个生命周期内有效"

#### 31. `Mesh` / `Node` 句柄类持有裸指针 `SceneData*`
- **现象**: 如果 `Model` 被 `clear()` 或析构，所有 `Mesh`/`Node` 引用立即悬空
- **影响**: 3D 引擎常见权衡（与 Assimp 的 `aiMesh*` 类似），但应在文档中警告
- **位置**: `include/tcx/assimp/Mesh.h`、`include/tcx/assimp/Node.h`
- **建议**: 在类文档注释中说明生命周期依赖

---

### 审查亮点（做得好的地方）

| 方面 | 评价 |
|------|------|
| Assimp 隔离 | `AssimpConvert.h` 是唯一同时依赖 Assimp 和 TrussC 的文件，运行时零 Assimp 依赖 |
| Importer PImpl | `struct Impl` 隐藏 `Assimp::Importer`，公共头无需 include Assimp |
| SceneData 设计 | 纯数据结构，清晰表达场景图/骨骼/动画/材质/纹理 |
| 双渲染路径 | CPU 蒙皮 + GPU 蒙皮可选，GpuSkinnedRenderer 自包含 |
| GPU 蒙皮自包含 | 自有 sokol pipeline/buffer/shader，不侵入 TrussC 核心 |
| 材质双模型 | 同时支持 Phong 和 PBR |
| 嵌入式纹理 | `*0` 引用格式 + `loadFromMemory` 正确处理 |
| 大小写不敏感纹理 | 跨平台文件系统的实用回退 |
| 骨骼覆盖 | `setBoneGlobalTransform()` 支持手动控制，动画和手动可叠加 |
| 导入标志 | `defaultFlags()` 精选实用后处理 |
| Assimp Clang 21 补丁 | CMake 自动 patch `material.inl` 的 `::` 前缀问题 |
| 示例覆盖 | 6 个示例覆盖静态/材质/FBX 动画/骨骼控制/调试层级/验证 |
| 文件拖放 | `filesDropped` 支持运行时替换模型 |
