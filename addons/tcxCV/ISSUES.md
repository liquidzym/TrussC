# tcxCV 代码审阅 — 发现的问题

> 审阅日期: 2026-05-31
> 审阅范围: `src/` 全部 4086 行源码 + `src/cld/` 子库 + CMakeLists.txt + 示例
> 总体评价: **良好** — ofxCv 的忠实移植，功能完备，API 设计合理。主要短板是零测试和少数安全性隐患。

---

## #1 — 完全缺少自动化测试

**严重性**: 🔴 高 | **类型**: 质量保障

16 个示例都是交互式可视化 demo，没有任何可自动验证的单元测试。

- `toCv()`/`toOf()` 类型转换的正确性（RGB vs BGR 通道顺序是 CV 类项目中最常见的 bug 源头）
- `ContourFinder` 面积过滤/颜色跟踪/孔洞检测
- `Calibration` 标定→去畸变 round-trip
- `KalmanPosition` 状态预测→校正一致性
- `RunningBackground` 学习率衰减→前景提取
- `estimateAffine3D` 仿射矩阵正确性

**建议**: 至少为 Utilities、Wrappers、ContourFinder 添加 catch2//gtest 测试。

---

## #2 — `estimateAffine3D` 对 `tc::Vec3` 内存布局做不安全假设

**严重性**: 🟡 中 | **类型**: 安全性 | **文件**: `tcxCvWrappers.cpp:159-160`

```cpp
Mat fromMat(1, (int)from.size(), CV_32FC3, &from[0]);
Mat toMat(1, (int)to.size(), CV_32FC3, &to[0]);
```

直接用 `&from[0]` 构造 `CV_32FC3` Mat，假设 `tc::Vec3` 是 3 个连续 float（12 字节），无 padding。
同理 `Calibration::undistort(vector<tc::Vec2>&)` 也用 `&src[0].x` 构造 `CV_32FC2` Mat。

大多数平台上 `tc::Vec3` = `{float x, y, z}` = 12 字节没有 padding，但 C++ 标准不保证。如果 TrussC 未来给 Vec3 加了 `__attribute__((aligned(16)))` 或虚函数，数据会被静默破坏，极难调试。

**建议**: 添加 `static_assert(sizeof(tc::Vec3) == 3 * sizeof(float))` 和 `static_assert(sizeof(tc::Vec2) == 2 * sizeof(float))`；或改为逐点拷贝到临时 `vector<Point3f>`。

---

## #3 — `toCv(const tc::Image&)` 使用 `const_cast` 指向 const 内存

**严重性**: 🟡 中 | **类型**: 代码安全 | **文件**: `tcxCvUtilities.h:96-100`

```cpp
inline cv::Mat toCv(const tc::Image& img) {
    if (!img.isAllocated()) return cv::Mat();
    cv::Mat rgba(..., const_cast<unsigned char*>(img.getPixelsData()));
    return rgba.clone();  // 立即克隆，当前安全
}
```

`const_cast` 创建了指向 const 内存的 Mat header（语义上允许写入），然后立刻 `clone()` 做深拷贝。
当前逻辑安全因为 clone 不写原数据。但如果后续有人移除 `.clone()` 或在 clone 之前意外修改 Mat，就会产生 **未定义行为**（写入 const 内存）。

**建议**: 先 `memcpy` 到临时缓冲区再构造 Mat，或使用 `Mat::zeros + copyTo` 明确表达只读意图。

---

## #4 — `toCv(cv::Mat&)` 返回共享引用而非深拷贝，修改风险

**严重性**: 💡 低 | **类型**: API 设计 | **文件**: `tcxCvUtilities.h:31-33`

```cpp
inline cv::Mat toCv(cv::Mat& mat) {
    return mat;  // 返回引用，不拷贝
}
```

`toCv(cv::Mat&)` 返回 Mat 的浅拷贝（共享数据），而 `toCv(const cv::Mat&)` 返回 `.clone()` 深拷贝。两版本语义不一致——调用者可能不知道通过返回的 Mat 修改数据会影响原始 Mat。

在 wrapThree 宏生成的函数中：
```cpp
cv::Mat xMat = toCv(x), yMat = toCv(y);
// ... 如果 x 和 y 是同一对象的两个引用，修改 xMat 可能影响 yMat
```

这不是 bug（当前用法中没有出现这种问题），但 API 的隐含语义容易被误用。

**建议**: 在文档中明确说明 `toCv(Mat&)` 返回共享 Mat，修改会影响原始数据。

---

## #5 — `warpPerspective`/`unwarpPerspective` 参数应为 `const&`

**严重性**: 💡 低 | **类型**: API 设计 | **文件**: `tcxCvWrappers.h:324,337`

```cpp
void warpPerspective(const S& src, D& dst, std::vector<cv::Point2f>& dstPoints, ...)
void unwarpPerspective(const S& src, D& dst, std::vector<cv::Point2f>& srcPoints, ...)
```

`dstPoints`/`srcPoints` 只是作为输入被 `getPerspectiveTransform` 读取，函数不修改它们，但签名是非 const 引用。这阻止了传入临时量或 const vector。

**建议**: 改为 `const std::vector<cv::Point2f>&`。

---

## #6 — CLD 像素拷贝循环效率低

**严重性**: 💡 低 | **类型**: 性能 | **文件**: `tcxCvWrappers.h:246-256`

```cpp
// cv::Mat → imatrix: 逐像素嵌套循环
for (int y = 0; y < height; ++y) {
    const unsigned char* dstPtr = dstMat.ptr<unsigned char>(y);
    for (int x = 0; x < width; ++x) {
        img[y][x] = dstPtr[x];
    }
}
// ... ETF + FDoG 处理 ...
// imatrix → cv::Mat: 又一次逐像素循环
```

输入/输出各一次全图像素级嵌套循环。1920×1080 = 2M 像素 × 2 = 4M 次逐元素迭代。CLD 本身已 CPU 密集（ETF Smooth 是 O(N × halfW × M × 2)），额外的拷贝不是瓶颈但也不必要。

**建议**: 使用 `std::memcpy`（如果 Mat 行连续且 imatrix 行对齐）或 `cv::Mat::forEach`。

---

## #7 — `ContourFinder`: `autoThreshold_` 与 `useTargetColor` 语义重叠

**严重性**: 💡 低 | **类型**: 运行时效率 | **文件**: `tcxCvContourFinder.cpp:27-37`

```cpp
if (useTargetColor) {
    inRange(img, base - offset, base + offset, thresh);  // → 二值图 (0/255)
}
if (autoThreshold_) {
    threshold(thresh, thresholdValue_, invert_);  // 对二值图再做 Otsu？冗余
}
```

当 `useTargetColor=true` + `autoThreshold_=true` 时，`inRange` 已经产生 0/255 二值掩码，再对它做 `threshold()` 是无意义的 CPU 浪费。Otsu 阈值（THRESH_OTSU）对只有 0/255 两级值的图计算直方图毫无用处。

**建议**: 在 `useTargetColor` 为 true 时跳过 `autoThreshold_` 逻辑，或自动禁用。

---

## #8 — `Calibration::undistort` 对 `tc::Vec2` 的不安全内存假设

**严重性**: 🟡 中 | **类型**: 安全性 | **文件**: `tcxCvCalibration.cpp:169-177`

```cpp
tc::Vec2 Calibration::undistort(tc::Vec2& src) const {
    tc::Vec2 dst;
    Mat matSrc = Mat(1, 1, CV_32FC2, &src.x);  // 假设 Vec2.x 和 Vec2.y 连续
    Mat matDst = Mat(1, 1, CV_32FC2, &dst.x);
    undistortPoints(matSrc, matDst, ...);
    return dst;
}

void Calibration::undistort(vector<tc::Vec2>& src, vector<tc::Vec2>& dst) const {
    Mat matSrc = Mat(n, 1, CV_32FC2, &src[0].x);  // 假设 Vec2 数组紧密排列
    Mat matDst = Mat(n, 1, CV_32FC2, &dst[0].x);
    ...
}
```

与 #2 同类问题。`CV_32FC2` 要求每元素 2 个连续 float，如果 `tc::Vec2` 有 padding 或非标准布局，数据会错位。

**建议**: 同 #2，添加 `static_assert` 或改用逐点拷贝。

---

## #9 — `Calibration` 的 `imagePoints` 是 public 成员

**严重性**: 💡 低 | **类型**: 封装 | **文件**: `tcxCvCalibration.h:113`

```cpp
std::vector<std::vector<cv::Point2f>> imagePoints;  // public!
```

`imagePoints` 是标定的核心数据，`add()` 和 `clean()` 方法负责维护它与 `objectPoints` 的一致性。但它是 public 的，用户可以直接 `imagePoints.push_back(...)` 而不更新 `objectPoints`，导致 `calibrate()` 时数据不一致。

**建议**: 改为 `protected`，提供 `size()` 和只读访问器。

---

## #10 — `RunningBackground::setLearningTime` 的学习率计算可能让用户困惑

**严重性**: 💡 低 | **类型**: API 语义 | **文件**: `tcxCvRunningBackground.cpp:89-92`

```cpp
void RunningBackground::setLearningTime(double learningTime_) {
    this->learningTime = 1.0 / learningTime_;  // 存储 1/learningTime
    useLearningTime = true;
}
```

`setLearningTime(900)` 意味着"900 帧后背景收敛"，但内部存储的是 `1.0/900 = 0.00111`，然后在 `update()` 中：
```cpp
addWeighted(accumulator, learningTime, frame, 1.0, 0.0, backgroundUpdate);
// = accumulator * (1/900) + frame * 1.0
```

这不是标准指数移动平均公式。标准 EMA 是 `accumulator = α * frame + (1-α) * accumulator`，其中 `α = 1/N`。但这里写的是 `accumulator * (1/N) + frame * 1.0`，结果完全不同——accumulator 贡献被极度缩小，frame 权重固定为 1.0，学习率语义不对。

当 `useLearningTime=false` 时用 `accumulateWeighted(frame, accumulator, learningRate)` 才是正确的 EMA。

**建议**: 修正 `useLearningTime` 分支为 `addWeighted(accumulator, 1.0 - learningTime, frame, learningTime, 0.0, accumulator)`。

---

## #11 — `RunningBackground` 的 `ignoreForeground` 模式下 accumulator 和 background 不同步

**严重性**: 🟡 中 | **类型**: 逻辑错误 | **文件**: `tcxCvRunningBackground.cpp:28-32`

```cpp
if (ignoreForeground) {
    Mat notForeground;
    bitwise_not(thresholded, notForeground);
    Mat backgroundUpdate;
    if (useLearningTime) {
        addWeighted(accumulator, learningTime, frame, 1.0, 0.0, backgroundUpdate);
    } else {
        addWeighted(accumulator, 1.0 - learningRate, frame, learningRate, 0.0, backgroundUpdate);
    }
    backgroundUpdate.copyTo(accumulator, notForeground);  // 只在非前景区域更新
}
accumulator.copyTo(background);  // background = 整个 accumulator
```

`ignoreForeground` 模式下，`backgroundUpdate` 计算了全图的加权平均，但只拷贝到 accumulator 的非前景区域。然后 `background = accumulator`——这意味着前景区域保留了**上一次**的 accumulator 值。这本身是正确的设计意图（前景区域不更新背景模型），但配合 #10 中 `useLearningTime` 的错误公式，前景区域的背景会越来越偏离实际。

**建议**: 修正 #10 的学习率公式后此问题自然解决。

---

## #12 — `FinderFlow::calcFlow` 中 FlowPyrLK 在第一帧时不检查 prevPyramid 是否有效

**严重性**: 💡 低 | **类型**: 边界条件 | **文件**: `tcxCvFlow.cpp:78-82`

```cpp
void FlowPyrLK::calcFlow(Mat prev, Mat next) {
    if (!nextPts.empty() || calcFeaturesNextFrame) {
        if (calcFeaturesNextFrame) {
            calcFeaturesToTrack(prevPts, next);
            if (prevPts.empty()) {       // ← 早期返回
                nextPts.clear();
                return;                  // 但 prevPyramid 没有被初始化
            }
            calcFeaturesNextFrame = false;
        } else {
            swap(prevPts, nextPts);
        }
        nextPts.clear();

        if (prevPyramid.empty()) {
            buildOpticalFlowPyramid(prev, prevPyramid, ...);
        }
```

当 `calcFeaturesNextFrame=true` 且 `prevPts` 为空（例如图像太暗，`goodFeaturesToTrack` 找不到角点），函数早期返回，但 `prevPyramid` 从未构建。下次调用时 `calcFeaturesNextFrame=false`，`nextPts` 可能为空也跳过了特征检测分支，但 `prevPyramid.empty()` 判断还在。逻辑链脆弱，在极端情况下可能导致不正确的光流计算。

**建议**: 在 `prevPts.empty()` 的早期返回中同时重置 `prevPyramid` 和设置 `calcFeaturesNextFrame = true`。

---

## #13 — `FlowFarneback::resetFlow()` 中 `flow.setTo(0)` 可能在空 Mat 上操作

**严重性**: 💡 低 | **类型**: 边界条件 | **文件**: `tcxCvFlow.cpp:111`

```cpp
void FlowFarneback::resetFlow() {
    Flow::resetFlow();  // last = Mat(); curr = Mat(); hasFlow = false;
    flow.setTo(0);      // flow 可能从未被 calcFlow 初始化！
}
```

如果 `resetFlow()` 在任何 `calcFlow()` 调用之前被调用，`flow` 是默认构造的空 Mat，`flow.setTo(0)` 是空操作（不崩溃），但语义不清晰。

**建议**: 改为 `flow = Mat();` 或 `if (!flow.empty()) flow.setTo(0);`。

---

## #14 — CLD 子库的 `round()` 和 `ABS()` 宏与标准库冲突

**严重性**: 🟡 中 | **类型**: 可移植性 | **文件**: `src/cld/fdog.cpp:10-13`

```cpp
#ifndef ABS
    #define ABS(x) ( ((x)>0) ? (x) : (-(x)) )
#endif
#define round(x) ((int) ((x) + 0.5))
```

- `round(x)` 宏与 C99/C++11 的 `std::round()` 冲突。如果其他头文件（包括 OpenCV）引入了 `using namespace std` 或直接调用 `round()`，此宏会破坏它们。
- `ABS` 宏与 `<cstdlib>` 中的宏在某些平台上冲突。

`#define round(x)` 是无条件的（没有 `#ifndef round` 保护），最危险。

**建议**: 使用 `cv::saturate_cast<int>` 或 `static_cast<int>(std::round(x))` 替代 `round` 宏；用 `std::abs` 替代 `ABS` 宏。

---

## #15 — ETF 子库使用原始指针数组 `p[]` 虽已重构为 `vector<Vect>` 但 `rebuildRows()` 有无效化风险

**严重性**: 💡 低 | **类型**: 健壮性 | **文件**: `src/cld/ETF.h:20-25`

```cpp
std::vector<Vect> data_{{1.0, 0.0, 1.0}};
std::vector<Vect*> p{data_.data()};  // 指向 data_ 内部的原始指针

void rebuildRows() {
    p.resize(static_cast<size_t>(Nr));
    for (int i = 0; i < Nr; ++i) {
        p[static_cast<size_t>(i)] = data_.data() + static_cast<size_t>(i) * Nc;
    }
}
```

`p` 存储 `data_.data()` 的偏移指针。如果 `data_` 发生 reallocation（例如 `push_back`、`resize`），所有 `p` 中的指针立即悬空。当前代码只在 `init()` 和 `copy()` 中修改 `data_`，随后立刻 `rebuildRows()`，所以目前安全。但 `p` 实际上已经不再需要——`operator[]` 直接用 `data_.data() + i * Nc` 计算，`p` 是遗留的冗余设计。

**建议**: 删除 `p` 成员和 `rebuildRows()`，`operator[]` 已经用 `data_.data()` 计算偏移，不需要指针数组。

---

## #16 — `findFirst`/`findLast` 未找到时返回 0 而非 -1

**严重性**: 💡 低 | **类型**: API 语义 | **文件**: `tcxCvHelpers.cpp:38-49`

```cpp
int findFirst(const Mat& arr, unsigned char target) {
    for (int i = 0; i < arr.rows; i++) {
        if (arr.at<unsigned char>(i) == target) return i;
    }
    return 0;  // 未找到也返回 0？
}
```

未找到时返回 0（即第一个元素的索引），调用者无法区分"找到了第一个位置"和"没找到"。应返回 -1 或提供 `bool` 返回。

**建议**: 返回 -1 表示未找到。

---

## #17 — `editDistance` 中 `mostRepresentative` 的溢出保护逻辑有误

**严重性**: 🟡 中 | **类型**: 逻辑错误 | **文件**: `tcxCvDistance.cpp:40-44`

```cpp
for (int j = 0; j < n; j++) {
    int curEdit = editDistance(strs[i], strs[j]);
    if (curEdit < curScore) {  // ← 应该是 curEdit > curScore 或溢出检测？
        curScore = numeric_limits<int>::max() / 2;  // overflow guard
    }
    curScore += curEdit;
}
```

注释说"overflow guard"，但条件 `curEdit < curScore` 意味着：**当当前编辑距离小于累积分数时**就截断。这是错误的——正常情况下 `curEdit` 总是远小于 `curScore`（因为 curScore 是累加和），所以这个条件几乎总是为真，导致 `curScore` 被过早截断为 `INT_MAX/2`，使得所有字符串的得分几乎相同，`mostRepresentative` 的选择基本退化为第一个字符串。

正确的溢出检测应该是 `curScore > numeric_limits<int>::max() - curEdit`（加法前检测）。

**建议**: 修正为 `if (curScore > numeric_limits<int>::max() - curEdit) break;` 或使用 `size_t`/`int64_t`。

---

## #18 — `convexityDefects` 对缺陷点的编码方式改变，与 OpenCV 语义不兼容

**严重性**: 💡 低 | **类型**: API 设计 | **文件**: `tcxCvWrappers.cpp:85-97`

```cpp
// OpenCV 的 Vec4i: [startIdx, endIdx, farIdx, depth]
// tcxCV 的 Vec4i:  [farX, farY, midX, midY]
defect[0] = contour[farIdx].x;
defect[1] = contour[farIdx].y;
defect[2] = (contour[startIdx].x + contour[endIdx].x) / 2;
defect[3] = (contour[startIdx].y + contour[endIdx].y) / 2;
```

OpenCV 原生 `convexityDefects` 返回索引+深度，tcxCV 把它重新编码为坐标。这使得：
1. 丢失了深度信息（凸缺陷的严重程度）
2. 用户如果查 OpenCV 文档会完全困惑
3. `startIdx`/`endIdx` 用中点代替，而不是真正的弦端点

**建议**: 保留 OpenCV 原始格式，或提供两种返回方式并明确文档说明。

---

## #19 — 构建产物泄漏到 addon 目录（~100MB）

**严重性**: 💡 低 | **类型**: 仓库卫生

```
examples/example-morphology/build-macos/   16MB
examples/example-verify/build-macos/       69MB
examples/example-difference/build-macos/  3.8MB
examples/example-contours-basic/build-macos/ 13MB
```

总计约 102MB 构建产物留在 addon 目录中。虽然全局 `.gitignore` 排除了 `build-*/`，但 `du -sh` 显示整个 tcxCV 目录占 113MB，其中 90% 是构建产物。

**建议**: 添加 `tcxCV/.gitignore` 排除 `build-*/`，或清理已有构建目录。

---

## #20 — `wrapThree` 宏在头文件中 define/undef

**严重性**: 💡 低 | **类型**: 代码风格 | **文件**: `tcxCvWrappers.h:48-58`

```cpp
#define wrapThree(name) \
template <class X, class Y, class Result> \
void name(X& x, Y& y, Result& result) { ... }

wrapThree(max)
wrapThree(min)
// ... 10 个操作
#undef wrapThree
```

用宏生成 10 个函数模板。虽然 `#undef` 限制了作用域，但：
1. IDE 和代码导航工具无法跳转到宏生成的函数
2. `doxygen` 等文档工具可能无法正确解析
3. C++20 可以用 `concept` + 变参模板替代

**建议**: 考虑用可变参数模板或简单的代码生成脚本替代。

---

## #21 — `ContourFinder::draw()` 使用 `tc::drawLine` 逐段绘制，效率低

**严重性**: 💡 低 | **类型**: 性能 | **文件**: `tcxCvContourFinder.cpp:177-190`

```cpp
void ContourFinder::draw() const {
    for (size_t i = 0; i < polylines.size(); i++) {
        const tc::Path& poly = polylines[i];
        for (int j = 0; j < poly.size() - 1; j++) {
            tc::drawLine(poly[j].x, poly[j].y, poly[j + 1].x, poly[j + 1].y);
        }
        // Close the contour
        tc::drawLine(poly[poly.size() - 1].x, ...);
    }
}
```

每条线段一次 `drawLine` 调用。对于复杂轮廓（数百到数千点），这会产生大量 OpenGL draw call。应使用 `tc::Path::draw()` 或批量绘制。

**建议**: 使用 `tc::Path` 的原生绘制方法（如果支持）或构建顶点缓冲区一次提交。

---

## #22 — `KalmanEuler::update` 中欧拉角回绕逻辑可能累积漂移

**严重性**: 💡 低 | **类型**: 数值精度 | **文件**: `tcxCvKalman.h:139-152`

```cpp
template <class T>
void KalmanEuler_<T>::update(const tc::Quaternion& q) {
    tc::Vec3 euler = q.toEuler();
    for (int i = 0; i < 3; i++) {
        float* vals = &euler.x;
        float* prevVals = &eulerPrev.x;
        float rev = floorf((prevVals[i] + 180.0f) / 360.0f) * 360.0f;
        vals[i] += rev;
        if (vals[i] < -90.0f + rev && prevVals[i] > 90.0f + rev) vals[i] += 360.0f;
        else if (vals[i] > 90.0f + rev && prevVals[i] < -90.0f + rev) vals[i] -= 360.0f;
    }
    KalmanPosition_<T>::update(euler);
    eulerPrev = euler;
}
```

回绕逻辑通过在当前值上加 `rev`（前值的整圈偏移）来保持连续性。但如果万向节锁发生（pitch = ±90°），roll 和 yaw 会突变，此逻辑无法正确处理。另外 `eulerPrev` 累积了加上 `rev` 后的值，长时间运行后可能漂移到极大数值，降低 Kalman 滤波器的浮点精度。

**建议**: 考虑在每次 update 后对 `eulerPrev` 做归一化（限制在 [-180, 180] 范围），或使用四元数直接做 Kalman 滤波（避免欧拉角表示）。

---

## 总结优先级表

| # | 严重性 | 问题 | 类型 |
|---|--------|------|------|
| 1 | 🔴 高 | 完全缺少自动化测试 | 质量保障 |
| 2 | 🟡 中 | estimateAffine3D Vec3 内存布局假设 | 安全性 |
| 3 | 🟡 中 | toCv(const Image&) 的 const_cast | 代码安全 |
| 8 | 🟡 中 | Calibration::undistort Vec2 内存布局假设 | 安全性 |
| 10 | 🟡 中 | RunningBackground 学习率公式错误 | 逻辑错误 |
| 11 | 🟡 中 | RunningBackground ignoreForeground 模式与 #10 联动 | 逻辑错误 |
| 14 | 🟡 中 | CLD round/ABS 宏与标准库冲突 | 可移植性 |
| 17 | 🟡 中 | mostRepresentative 溢出保护逻辑错误 | 逻辑错误 |
| 4 | 💡 低 | toCv(Mat&) 返回共享引用 | API 设计 |
| 5 | 💡 低 | warpPerspective 参数应为 const& | API 设计 |
| 6 | 💡 低 | CLD 像素拷贝循环 | 性能 |
| 7 | 💡 低 | ContourFinder autoThreshold + inRange 冗余 | 效率 |
| 9 | 💡 低 | Calibration imagePoints public | 封装 |
| 12 | 💡 低 | FlowPyrLK 首帧边界条件 | 健壮性 |
| 13 | 💡 低 | FlowFarneback resetFlow 空 Mat | 边界条件 |
| 15 | 💡 低 | ETF p[] 冗余指针数组 | 健壮性 |
| 16 | 💡 低 | findFirst/findLast 返回值语义 | API |
| 18 | 💡 低 | convexityDefects 编码方式 | API 兼容性 |
| 19 | 💡 低 | 构建产物泄漏 ~102MB | 仓库卫生 |
| 20 | 💡 低 | wrapThree 宏 | 代码风格 |
| 21 | 💡 低 | ContourFinder::draw 逐线段绘制 | 性能 |
| 22 | 💡 低 | KalmanEuler 欧拉角漂移 | 数值精度 |
