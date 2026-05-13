# tcxTraerPhysics 更新日志

## 2026-05-12

### 移植对照

- 对照原始 Traer Physics 3.0 Java/Processing 源码与文档，确认默认积分器、`tick()` 默认时间步、`gravity` 语义、`removeParticle()` 语义和 spring/attraction force 方向。
- 保留 Traer 3.0 的核心兼容行为：`gravity` 是全局 force vector，不是按质量缩放的物理加速度；`removeParticle()` 只从粒子列表移除粒子，不自动删除连接到该粒子的 spring/attraction。
- 新增更适合 C++ ownership 的安全路径 `removeParticleAndForces()` 和 `removeForcesConnectedTo()`，用于同时删除粒子和关联内置 force。

### 行为修复

- 修复 `locked` 粒子在默认 RK4 积分器中仍会移动的问题。
- `Spring` 和 `Attraction` 现在统一使用 `isFree()` / `isFixed()` 判断，内置二体 force 会尊重 `locked` 状态。
- `makeFixed()` 和 `lock()` 会清空速度，避免固定粒子恢复自由后残留旧速度。
- 固定/锁定粒子仍会随 `tick()` 增加 age，保持和原始 Traer 的固定粒子 age 行为一致。
- `setMinimumDistance()` 与 `setRestLength()` 会 clamp 到非负值，避免无效输入造成异常 force magnitude。

### API 质量

- 新增 `ParticleSystem::IntegratorType`，支持 `RungeKutta`、`ModifiedEuler`、`Euler` 三种现代枚举选择。
- 保留 Traer 风格整数 selector：`RUNGE_KUTTA`、`MODIFIED_EULER`、`EULER`。
- `setIntegrator(std::unique_ptr<Integrator>)` 现在拒绝空指针并抛出 `std::invalid_argument`。
- `ParticleSystem` 禁止 copy/move，避免 integrator 内部保存的 `ParticleSystem*` 失效。
- `getParticle()` / `getSpring()` / `getAttraction()` / `getCustomForce()` 和 index remove API 增加边界检查，越界时抛出 `std::out_of_range`。
- index remove API 现在返回被移除对象，方便调用方继续管理生命周期或调试。
- pointer remove API 现在返回 `bool`，明确表示是否真的删除了对象。
- 新增 `numberOfParticles()`、`numberOfSprings()`、`numberOfAttractions()`、`numberOfCustomForces()` Traer 兼容别名。
- 新增只读容器访问器 `getParticles()`、`getSprings()`、`getAttractions()`、`getCustomForces()`，便于渲染和批处理遍历。

### 解算效率

- 新增 `reserve()`、`reserveParticles()`、`reserveSprings()`、`reserveAttractions()`、`reserveCustomForces()`，减少初始化大量粒子和 force 时的 vector realloc。
- `applyForces()` 增加零 gravity / 零 drag 快路径；当二者都为 0 时跳过全局 force pass。
- 全局 gravity/drag pass 会跳过 fixed/locked 粒子，减少无效 force 写入。
- `clearForces()` 使用一次 Vec3 赋值清零，减少重复字段写入代码。
- RK4 仍保留为默认高精度积分器；大量弹簧网格场景建议使用 `ModifiedEuler`，可显著减少每帧 force evaluation 次数。

### 测试

- `tests/physics_test.cpp` 不再复制物理实现，而是直接 include 真实 `tcxTraerPhysics.h`。
- 新增 `tests/stubs/TrussC.h`，用于 standalone 测试时提供最小 `tc::Vec3`。
- 覆盖 gravity force 语义、fixed/locked 行为、spring force、attraction force、min distance clamp、remove semantics、安全 API、reserve/read-only view 和 RK4 spring period。
- 当前验证命令：

```bash
clang++ -std=c++17 -Wall -Wextra -pedantic -Itests/stubs -Isrc -o /tmp/tcxTraerPhysics_test tests/physics_test.cpp
/tmp/tcxTraerPhysics_test
```

### 示例

- `example-basic` 增加 `reserve(2, 1)`，并改用 `ParticleSystem::IntegratorType` 切换积分器。
- `example-attract` 增加粒子和 attraction 预分配；清空 planets 时使用 `removeParticleAndForces()`，避免留下关联 attraction。
- `example-attract` 修正初始轨道速度和点击新增粒子的速度：按 Traer attraction 公式 `v = sqrt(strength * sunMass / radius)` 计算切向速度，移除导致粒子快速逃逸的额外倍率和随机速度。
- `example-cloth` 增加完整粒子/弹簧预分配；默认改用 `ModifiedEuler`，更适合大量弹簧的实时布料示例；仍可按 `1` 切回 RK4。
- 三个示例都改为固定 60Hz update，减少不同平台和不同刷新率下的模拟速度差异。
- 示例 header 移除全局 `using namespace`，避免污染包含方；TrussC namespace 使用限制在 `.cpp`。

### 构建与集成

- 根 `.gitignore` 增加 `!addons/tcxTraerPhysics`，使该 addon 可被 Git 跟踪。
- 保持 header-only CMake 设计：`tcxTraerPhysics` 是 `INTERFACE` library，并链接 `TrussC`。
- 验证过的示例构建：

```bash
cmake --build /tmp/tcxTraerPhysics-example-basic-build -j4
cmake --build /tmp/tcxTraerPhysics-example-attract-build -j4
cmake --build /tmp/tcxTraerPhysics-example-cloth-build -j4
```

### 已知非阻塞项

- 示例构建时仍可能出现 TrussC core 侧的 duplicate `libTrussC.a` linker warning；该 warning 不是由 tcxTraerPhysics 代码引入。
- 尚未在 Windows、Linux、Web/Emscripten 上完成实际构建验证；本次改动保持标准 C++17/header-only 设计，未引入平台专属 API。
