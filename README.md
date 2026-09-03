# DynamicTraverseDraw

DynamicTraverseDraw 是一个面向 Android NDK 的动态遍历与绘制框架。项目把“游戏数据处理”和“UI/渲染壳”拆开：游戏适配层只负责目标进程数据、对象遍历和更新，通用模块负责生命周期、分帧调度、对象池、投影和绘制辅助。

当前版本：**v3.0**

v3.0 相比旧版本是一次完整重构，接口、目录组织和接入方式均以当前代码为准。旧版本的实现和使用方式不再作为兼容目标。

## 特性

- `Game` + `GameRuntime`：统一游戏初始化、停止、切换和每帧调度。
- `Module` / `ImGuiDrawModule`：把绘图、自瞄、调试页面等功能挂到游戏实例上。
- `TrackedList<T>`：按对象地址去重，支持墓碑延迟回收、完整更新和增量更新。
- `FrameScanner`：把大型对象数组拆成多个批次，降低单帧遍历开销。
- `VecMath`：向量、距离、角度和世界坐标到屏幕坐标/包围盒投影。
- `DrawPrimitives`：通用屏幕包围盒和射线绘制辅助。
- 可注入多种 `diRW::baseRW` 读写后端，也可以由游戏自行创建默认后端。
- ImGui + OpenGL/Vulkan Android 悬浮窗壳，游戏页面由模块动态注册。

## 目录

```text
jni/
├── core/                    # 通用框架层
│   ├── Game.h               # Game 生命周期与 GameRuntime
│   ├── Module.h             # Module / ModuleHost
│   ├── ImGuiDrawModule.h    # 屏幕绘制与侧边栏页面接口
│   ├── draw/                # 通用绘制辅助
│   ├── math/                # VecMath
│   └── traverse/            # TrackedList / FrameScanner
├── games/                   # 游戏适配层
│   ├── Games.h              # 游戏注册表
│   ├── texun.hpp            # 完整接入实例，推荐从这里开始
│   ├── Example.hpp          # 教学接入示例
│   └── GameTemplate.hpp     # 空白接入骨架
├── diRW/                    # 进程读写抽象和后端
├── UI/                      # 主窗口、模块页面加载和帧循环粘合
├── src/                     # ImGui、图形和触摸实现
├── include/                 # 第三方及平台头文件
├── CMakeLists.txt           # CMake + Android NDK 构建脚本
├── Android.mk               # 旧 ndk-build 脚本（保留）
├── Application.mk           # 旧 ndk-build 配置（保留）
└── FRAMEWORK.md             # 框架接口和内部时序详解
```

## 构建

### 环境要求

- Android NDK，当前 CMake 配置默认使用 `C:/Sdk/ndk/30.0.15729638`。
- CMake 3.18 或更高版本。
- Android ABI：`arm64-v8a`。
- Android API：`android-26` 或更高。
- C++17 工具链。

也可以通过 `CMAKE_ANDROID_NDK` 或 `NDK_HOME` 指定 NDK 路径。

### 编译

在 `jni` 目录执行：

```bash
cmake -S . -B build \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26
cmake --build build --parallel
```

项目会依次读取 `CMAKE_ANDROID_NDK`、`NDK_HOME`，最后使用 CMake 文件中的默认 NDK 路径。也可以在配置时显式指定 `-DCMAKE_ANDROID_NDK=/path/to/ndk`。

如果 `build/` 已经配置完成，也可以直接执行：

```bash
cmake --build build --parallel
```

产物输出到 `bin/dynamic_draw`。`build/` 和 `bin/` 都是本地生成目录，不应提交到仓库。

## 快速了解代码

建议按下面顺序阅读：

1. [games/texun.hpp](games/texun.hpp)：一个完整的游戏接入实例。
2. [games/Example.hpp](games/Example.hpp)：使用占位偏移的教学版本。
3. [games/GameTemplate.hpp](games/GameTemplate.hpp)：新游戏的空白骨架。
4. [core/Game.h](core/Game.h)：游戏生命周期和运行时切换。
5. [core/traverse/TrackedList.h](core/traverse/TrackedList.h)：对象池语义和更新节奏。
6. [core/traverse/FrameScanner.h](core/traverse/FrameScanner.h)：分帧扫描流程。
7. [FRAMEWORK.md](FRAMEWORK.md)：完整架构、接口和扩展说明。

## 接入新游戏

新游戏通常只需要修改 `games/`，不需要改 `core/`、`diRW/` 或 UI 壳。

1. 复制 `games/texun.hpp`，修改命名空间、游戏类、包名、模块名、偏移和对象字段。
2. 定义继承 `TrackedObject` 的对象结构体。`address` 和 `findCount` 由对象池维护，不要重复定义或手动修改。
3. 在 `Init()` 中验证读写后端、获取模块基址并注册 `TrackedList` 更新回调。
4. 在 `Update()` 中读取对象数组头，调用 `FrameScanner::Next()`，再按目标游戏规则分类并调用 `actors.Add()`。
5. 一轮扫描完成后调用 `Sweep()` 和 `UpdateAll(true)`，每帧执行 `UpdateAll(false)`。
6. 在 `ImGuiDrawModule::DrawObject()` 中读取对象池数据，调用 `WorldToScreen()`/`WorldToBox()` 并绘制。
7. 在 `Games.h` 中 include 新文件，并把实例加入 `kGames` 数组。

最小结构如下：

```cpp
struct ActorData : TrackedObject {
    Vec3 location{};
};

class MyGame : public Game {
public:
    MyGame() : Game("我的游戏", "com.example.game"), draw_(*this) {
        Attach(&draw_);
    }

protected:
    bool Init(diRW::baseRW *rw) override;
    void Update() override;
    void Shutdown() override;

private:
    TrackedList<ActorData> actors;
    MyDraw draw_;
};
```

页面模块在构造函数中注册：

```cpp
AddPage("绘制设置", [this] { DrawSettings(); });
```

外部读写后端通过 `GameRuntime::Start(game, rw)` 注入；传入 `nullptr` 时，框架会调用游戏的 `CreateReader()`。

## 读写后端

`diRW/baseRW.hpp` 定义统一接口，当前仓库包含以下后端：

- `syscallRW`：Linux `process_vm_readv/process_vm_writev`。
- `TGodRW`、`RtRW`、`Qx11RW`、`TwTRW`、`copyRW`：可选驱动后端。

后端依赖目标设备、权限和对应驱动环境。仓库只提供接口和适配代码，不保证在任意设备上直接可用。

## 文档

- [FRAMEWORK.md](FRAMEWORK.md)：架构、生命周期、对象池、分帧遍历和扩展指南。
- [games/texun.hpp](games/texun.hpp)：完整实例和推荐使用方式。

## 更新日志

当前版本为 **v3.0**，完整更新内容见 [CHANGELOG.md](CHANGELOG.md)。

## 免责声明

本项目仅用于 Android NDK、进程读写接口、数据遍历和图形模块架构研究。请遵守目标软件的服务条款、设备权限要求和所在地法律法规，仅在获得授权的环境中使用。

## 许可证

当前仓库尚未声明开源许可证。发布前请根据你的授权方式补充 `LICENSE` 文件，并在此处更新许可证说明。
