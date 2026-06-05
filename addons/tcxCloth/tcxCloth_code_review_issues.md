# tcxCloth 代码审查问题清单

> 基于 2026-06-01 全面代码审查，仅覆盖 macOS / Windows 平台。

---

## 🔴 P0 — 正确性（应尽快修复）

### 1. 约束求解重复执行

**文件**：`src/Cloth.cpp` — `stepCpu()` / `integrateCpu()`

**现状**：`stepCpu()` 调用顺序为：

```
solveConstraintsCpu() → accumulateForcesCpu() → integrateCpu() → collideCpu()
```

但 `integrateCpu()` 内部又调用了一次 `solveConstraintsCpu()`，导致每子步约束求解执行 **2×** 而非预期的 1×。

**影响**：
- 实际迭代次数 = `constraintIterations × 2`，小刚度值下布料行为偏硬
- 第一次求解基于上一帧过期位置，工作基本浪费
- GPU 路径（`cloth.glsl`）同样"约束→积分→约束"双次求解，行为一致但同样有误

**建议**：重构为标准 Verlet 循环：

```
accumulateForces → integrate → solveConstraints → collide
```

将约束求解从 `integrateCpu()` 中移除，仅在积分后执行一次。GPU 着色器同步调整。

---

### 2. 阻尼与帧率耦合

**文件**：`src/Cloth.cpp` — `integrateCpu()`；`shaders/cloth.glsl` — `timing.y`

**现状**：阻尼实现为 Verlet 速度乘以 `(1.0 - damping)`，每个子步应用指数衰减：

```cpp
velocity = (pos - prevPos) * (1 - damping)
```

**影响**：`damping = 0.5` 在 8 个子步时，有效帧阻尼为 `(0.5)^8 = 0.004`，远比预期激进。不同子步数下布料行为截然不同。

**建议**：
- 使用时间相关阻尼：`float dampingFactor = std::pow(1.0f - settings_.damping, dt)`
- 或近似：`1.0f - settings_.damping * dt`
- GPU 着色器 `timing.y` 同步修改
- 文档化阻尼单位为"每秒 [0,1]"

---

## 🟡 P1 — 重要修复

### 3. 风力分布物理错误

**文件**：`src/Cloth.cpp` — `addWindForcesTriangleCpu()`

**现状**：风力乘以 `particle.inverseMass`，将力转换为了加速度，意味着更重的粒子受到的风力更小。

**影响**：三角形面上的风力应与质量无关（风压力 ≠ 加速度），当前实现物理不正确。

**建议**：移除 `* particle.inverseMass`，或将风力均分给 3 个顶点后由积分器通过 `inverseMass` 正确转换。

---

### 4. `pinParticle` / `setParticlePosition` 无边界检查

**文件**：`src/tcxCloth/Cloth.h:55`

**现状**：`pinParticle(int x, int y)` 和 `setParticlePosition(int x, int y, ...)` 使用原始 `int` 网格坐标，无边界检查。越界值通过 `index()` 导致：
- Debug：断言触发
- Release：静默越界，未定义行为

**建议**：
- 至少添加前置条件断言（Debug 守护）
- Release 模式下对越界输入做 clamp 或 early-return
- 文档化参数范围契约

---

### 5. `ClothConstraint::a/b` 用 `int` + `-1` 哨兵

**文件**：`src/tcxCloth/ClothTypes.h:12`

**现状**：约束端点索引使用 `int`，以 `-1` 作为哨兵值，与 `ClothTopologyInfo` 中的 `std::size_t` 计数类型不一致。

**影响**：负哨兵无法与无符号类型安全互转，有符号/无符号比较产生编译器警告。

**建议**：统一为 `std::size_t`，哨兵值改为 `SIZE_MAX` 或使用 `std::optional<size_t>`。

---

### 6. `ClothSettings::columns/rows` 无验证

**文件**：`src/tcxCloth/ClothSettings.h:15`

**现状**：`columns`/`rows` 为 `int`（默认 64），`validateAndStoreSettings()` 未校验正值和上界。

**影响**：负值或零值无意义；极大值导致粒子/约束数指数级爆炸，分配失败或崩溃。

**建议**：在 `validateAndStoreSettings()` 中添加：
```cpp
settings_.columns = std::clamp(settings_.columns, 2, 256);
settings_.rows    = std::clamp(settings_.rows,    2, 256);
```
上界可酌情调整，但必须有保护。

---

## 🟢 P2 — 质量提升

### 7. 碰撞无摩擦

**文件**：`src/Cloth.cpp` — `collideCpu()`

**现状**：碰撞仅投影位置到表面，不调整 `previousPosition`，下一 Verlet 步速度方向不变，布料在碰撞体上无摩擦滑动。

**建议**：碰撞时调整 `previousPosition` 移除朝向碰撞体的速度分量，或添加可配置摩擦系数。

---

### 8. 累加器时间漂移

**文件**：`src/Cloth.cpp` — `stepCpu()`

**现状**：固定时间步累加器有最大 8 步保护防止死亡螺旋，但超出时剩余时间被丢弃，低帧率期间模拟时钟持续漂移，产生不易察觉的慢动作。

**建议**：循环后钳制：
```cpp
if (accumulator_ > fixedStep) accumulator_ = fixedStep;
```

---

### 9. 约束刚度复利效应

**文件**：`src/Cloth.cpp` — `solveConstraintsCpu()`

**现状**：刚度作为修正缩放因子在每个子步和每次迭代中重复应用。`N` 子步 × `M` 次迭代时，有效刚度为 `~1-(1-stiffness)^(N*M)`，小刚度值在多次迭代下变得非常刚硬，调参困难。

**建议**：采用 XPBD 风格的后步形状保持混合，或按迭代数归一化：
```cpp
float effectiveStiffness = 1.0f - std::pow(1.0f - stiffness, 1.0f / iterations);
```
至少文档化此复利行为。

---

### 10. 索引类型混用

**文件**：`Cloth.h`, `ClothTypes.h`, `Cloth.cpp`

**现状**：
- `columns()`/`rows()` → `int`
- `triangleIndexCount()`/`wireIndexCount()`/`constraintCount()` → `std::size_t`
- `triangleIndices_`/`wireIndices_` → `unsigned int`
- `ClothConstraint::a/b` → `int`

**影响**：调用者频繁遭遇有符号/无符号比较警告和窄化转换。

**建议**：统一为一种索引类型（建议 `std::size_t`），或定义类型别名 `using Index = std::size_t;`。

---

### 11. 移动语义未完整处理

**文件**：`src/tcxCloth/Cloth.h:30`

**现状**：移动构造/赋值可能浅移动 `unique_ptr<GpuResources>`，但 `accumulator_`、碰撞器向量等成员未显式处理，移动源对象可能处于意外状态。

**建议**：显式实现移动操作，确保移动源对象处于有效的"空"状态（`accumulator_ = 0`，清空碰撞器等），或使用 `= default` 并审查所有成员的移动语义。

---

### 12. 碰撞器仅整体替换

**文件**：`src/tcxCloth/Cloth.h:92`

**现状**：碰撞器通过 `setSphereColliders()`/`setPlaneColliders()` 整体替换，静默丢弃先前数据，无追加/删除单个碰撞器机制。

**建议**：添加 `addSphereCollider()`/`removeSphereCollider()` 等增量 API，或文档化整体替换语义。

---

### 13. 着色器 `options.w` 命名与行为不匹配

**文件**：`shaders/cloth.glsl`

**现状**：`options.w` 注释为"max grid step"，实际作为全局修正缩放因子：`corrected = pos + correction * options.w`。>1.0 值可能导致约束过冲不稳定。

**建议**：重命名为 `constraintScale` 或 `relaxationFactor`，添加范围说明（推荐 [0, 1]）。

---

### 14. 着色器死代码

**文件**：`shaders/cloth.glsl`

**现状**：`hash()` 函数已声明但从未调用。

**建议**：移除，或如果预留给未来功能则添加注释说明。

---

## 🔵 P3 — 优化与打磨

### 15. 每帧全量 GPU→CPU 回读

**文件**：`src/Cloth.cpp` — `syncParticlesFromGpu()`

**现状**：每帧读取整个位置纹理，GPU→CPU 回读管线代价高昂，造成 GPU 停顿。

**建议**：
- 异步回读 + 1 帧延迟
- 仅在 CPU 侧粒子数据实际需要时回读
- 长期：实现纯 GPU 渲染路径，直接从纹理读取位置（GPU_NOTES.md 已列为待办）

---

### 16. 每帧重建网格

**文件**：`src/Cloth.cpp` — `rebuildMeshes()`

**现状**：每帧从零重建填充和线框网格，包括重新上传所有顶点数据，静态布料时为不必要工作。

**建议**：添加脏标记，仅在位置实际变化时重建。

---

### 17. `CpuParticle` 缓存密度低

**文件**：`src/tcxCloth/ClothTypes.h`

**现状**：约 106-120 字节/粒子（8×Vec3 + 1×Vec2 + 1×float + 1×bool），`bool pinned` 导致 `inverseMass` 后填充浪费。

**建议**：
- 用 `inverseMass == 0` 替代 `pinned` 布尔字段（固定粒子已设 `inverseMass` 为 0）
- 消除冗余字段，减少缓存行占用

---

### 18. 着色器 Uniform 魔术索引

**文件**：`src/Cloth.cpp` — `ClothStepUniforms`

**现状**：使用魔术索引如 `uniforms.options[3] = 0.30f`，含义不透明。

**建议**：添加命名常量或命名访问器方法。

---

### 19. 生成文件写入源码树

**文件**：`CMakeLists.txt`

**现状**：着色器输出写入 `CMAKE_CURRENT_SOURCE_DIR/shaders/generated/`，违反非源码树构建约定，造成 git 噪声和只读源码树（CI）问题。

**建议**：改为 `CMAKE_CURRENT_BINARY_DIR/shaders/generated/`。

---

### 20. sokol 工具路径推导脆弱

**文件**：`CMakeLists.txt`

**现状**：通过提取 TrussC 目标的 `INTERFACE_INCLUDE_DIRECTORIES` 第一个元素再取 dirname 推导路径，顺序变化即静默失败。

**建议**：使用显式 CMake 变量或 `find_program()` 定位 sokol-shdc。

---

### 21. 测试覆盖缺口

**文件**：`tests/`

**现状**：以下关键场景未覆盖：

| 缺口 |
|------|
| `PlaneCollider` 完全未测试 |
| `pinCorners()`、`isPinned()`、`setParticlePosition()` 从未调用 |
| 移动语义 `Cloth(Cloth&&)` 未测试 |
| `release()` 未测试 |
| 退化输入（1×1、0×0 网格，越界坐标，dt=0，负 dt） |
| `damping`/`substeps`/`fixedTimeStep` 效果未独立验证 |
| 法线方向/幅度从未验证 |
| 默认 `ClothSettings` 可运行性未测试 |

**建议**：按优先级逐步补充，优先 P0/P1 相关参数和路径。

---

### 22. 测试框架改进

**文件**：`tests/test_cpu_reference.cpp`

**现状**：
- 极简自定义框架，`require()` 失败即终止整个二进制
- 单体 `main()` ~170 行顺序断言无逻辑分组
- 状态在多个逻辑测试段间累积，违反测试隔离
- 无 CTest 集成

**建议**：
- 添加 CTest 集成：`enable_testing()` + `add_test()`
- 拆分为独立测试用例，避免状态累积
- 考虑采用轻量框架（如 Catch2）或至少添加测试发现/汇总

---

### 23. `setup()`/`release()` 生命周期

**文件**：`src/tcxCloth/Cloth.h`

**现状**：手动生命周期管理，与析构函数语义重复。`release()` 后继续使用或 `setup()` 重复调用可能导致状态不一致。

**建议**：至少文档化前置条件和状态机（如 `release()` 后仅允许 `setup()`），或考虑 RAII 化使 `setup()` 幂等。

---

### 24. 状态归属不一致

**文件**：`src/tcxCloth/Cloth.h`

**现状**：`settings_` 存储模拟参数副本，但 `gravity_`/`windDirection_`/`windStrength_` 作为独立成员游离在外。修改 `settings_` 不影响重力/风力，反之亦然。

**建议**：将重力/风力参数纳入 `ClothSettings`，或显式文档化分离理由。

---

## 里程碑建议

| 阶段 | 范围 | 预估工作量 |
|------|------|-----------|
| **阶段一** | #1, #2, #3（物理正确性） | 2-3 天 |
| **阶段二** | #4, #5, #6（API 安全与验证） | 1 天 |
| **阶段三** | #7–#14（质量提升） | 3-5 天 |
| **阶段四** | #15–#24（优化与打磨） | 持续迭代 |

> 修复 #1 和 #2 后建议重新审视所有示例的视觉效果，确认布料行为符合预期。
