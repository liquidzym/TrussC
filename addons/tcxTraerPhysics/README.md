# tcxTraerPhysics

`tcxTraerPhysics` 是 TrussC 的 header-only 粒子物理 addon，基于 Traer Physics 的 Processing/Java 设计移植并现代化。它提供粒子、弹簧、吸引/排斥力，以及 Euler、Modified Euler、RK4 三种积分器。

## 添加到项目

在示例或项目的 `addons.make` 中加入：

```text
tcxTraerPhysics
```

然后在代码中包含：

```cpp
#include <tcxTraerPhysics.h>
```

## 最小用法

```cpp
tcx::ParticleSystem::Ptr ps = tcx::ParticleSystem::create(0.5f, 0.001f);
ps->reserve(2, 1);

auto anchor = ps->makeParticle(1.0f, 320, 100, 0);
anchor->makeFixed();

auto bob = ps->makeParticle(3.0f, 320, 300, 0);
ps->makeSpring(anchor, bob, 0.2f, 0.01f, 150.0f);

void update() {
    ps->tick();
}

void draw() {
    drawLine(anchor->position.x, anchor->position.y,
             bob->position.x, bob->position.y);
    drawCircle(bob->position.x, bob->position.y, 12);
}
```

## 常用 API

- `makeParticle(mass, x, y, z)` 创建粒子。
- `makeSpring(a, b, stiffness, damping, restLength)` 创建弹簧。
- `makeAttraction(a, b, strength, minDistance)` 创建吸引或排斥力；`strength > 0` 为吸引，`strength < 0` 为排斥。
- `tick()` 使用 Traer 默认时间步 `1.0` 推进一步。
- `tick(dt)` 使用自定义时间步。
- `setIntegrator(tcx::ParticleSystem::IntegratorType::ModifiedEuler)` 切换积分器。
- `reserve(particles, springs, attractions, customForces)` 在创建大量对象前预分配容量。
- `getSprings()` / `getParticles()` 获取只读容器，适合绘制遍历。

## 积分器选择

```cpp
ps->setIntegrator(tcx::ParticleSystem::IntegratorType::RungeKutta);
ps->setIntegrator(tcx::ParticleSystem::IntegratorType::ModifiedEuler);
ps->setIntegrator(tcx::ParticleSystem::IntegratorType::Euler);
```

- `RungeKutta`：默认值，精度高，每次 `tick()` 会计算 4 次 force。
- `ModifiedEuler`：实时弹簧网格、布料、交互系统优先使用，速度更好。
- `Euler`：最简单，适合测试或阻尼很强的系统。

## 删除粒子

为了兼容 Traer 3.0，`removeParticle(p)` 只会从 particle list 删除粒子，不会自动删除连接到它的 spring 或 attraction。

如果需要一起删除关联内置 force，使用：

```cpp
ps->removeParticleAndForces(p);
```

或者只清理连接关系：

```cpp
ps->removeForcesConnectedTo(p);
```

## 固定和锁定

```cpp
p->makeFixed();  // 固定粒子，并清空速度
p->makeFree();   // 解除 fixed
p->lock();       // 锁定粒子，并清空速度
p->unlock();     // 解除 locked
```

`locked` 是 C++ 版本增加的更强固定状态。内置积分器、spring 和 attraction 都会把 locked 粒子当作 fixed 处理。

## 性能建议

- 大量创建粒子和弹簧前先调用 `reserve()`。
- 布料、软体、网格类示例优先用 `ModifiedEuler`。
- `gravity = 0` 且 `drag = 0` 时，全局 force pass 会自动跳过。
- 渲染大量 spring 时使用 `for (const auto& s : ps->getSprings())`，不要每帧重复 `getSpring(i)`。
- 在 TrussC 中建议使用固定 update rate，例如 `setIndependentFps(60.0f, 0.0f)`，减少不同平台刷新率造成的模拟差异。

## 示例

- `examples/example-basic`：弹簧摆。
- `examples/example-attract`：中心吸引和轨道粒子。
- `examples/example-cloth`：弹簧布料，默认使用 `ModifiedEuler`。

## 测试

standalone 测试不依赖完整 TrussC runtime，只使用 `tests/stubs/TrussC.h` 提供最小 `tc::Vec3`：

```bash
clang++ -std=c++17 -Wall -Wextra -pedantic -Itests/stubs -Isrc -o /tmp/tcxTraerPhysics_test tests/physics_test.cpp
/tmp/tcxTraerPhysics_test
```
