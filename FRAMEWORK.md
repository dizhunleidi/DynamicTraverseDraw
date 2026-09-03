# DynamicTraverseDraw 框架文档

> 本项目是 TGOD_pubg 的框架重构版：只保留"数据处理框架"本身，游戏的具体功能
> （绘制、自瞄、名称表等）全部在游戏适配层，由原项目按需接入。
> ImGui 主框架、渲染后端（OpenGL/Vulkan）、触摸注入这套壳与原项目完全一致，未做改动。

---

## 目录

1. [总体架构](#1-总体架构)
2. [目录结构](#2-目录结构)
3. [构建系统](#3-构建系统)
4. [程序启动与主循环](#4-程序启动与主循环)
5. [core 框架层详解](#5-core-框架层详解)
6. [games 游戏适配层详解](#6-games-游戏适配层详解)
7. [diRW 读写层详解](#7-dirw-读写层详解)
8. [UI 壳详解](#8-ui-壳详解)
9. [一帧的完整数据流](#9-一帧的完整数据流)
10. [扩展指南](#10-扩展指南)
11. [与原项目的模块映射及当前 TODO](#11-与原项目的模块映射及当前-todo)

---

## 1. 总体架构

整个工程分五层，依赖方向自上而下：

```
┌─────────────────────────────────────────────────────┐
│  UI 壳（UI/）                                        │
│  主窗口 = 模块加载器：侧边栏默认只有"主页"，          │
│  动态加载游戏模块里的 ImGuiDrawModule 页面           │
├─────────────────────────────────────────────────────┤
│  core 框架层（core/）                                │
│  Module/ModuleHost 模块机制                          │
│  Game 基类（纯数据处理生命周期）+ GameRuntime        │
│  TrackedList 对象池 + FrameScanner 分帧调度          │
│  VecMath 向量/投影                                   │
│  ※ 不包含窗口壳；绘制原语仅提供轻量 ImGui 绘制辅助    │
├─────────────────────────────────────────────────────┤
│  games 游戏适配层（games/）                           │
│  每个游戏一个继承 Game 的实例类：                     │
│  偏移表 / 寻址 / 遍历分类 / 更新回调 / 数据结构体     │
│  绘图、自瞄、调试都是 Module，构造时捕获游戏引用     │
├─────────────────────────────────────────────────────┤
│  diRW 读写层（diRW/）                                │
│  baseRW 抽象接口 + 多种读写后端                      │
├─────────────────────────────────────────────────────┤
│  ImGui 壳（src/ + include/）—— 与原项目一致，不动     │
│  ImGui、Vulkan/OpenGL 渲染、触摸注入、悬浮窗创建     │
└─────────────────────────────────────────────────────┘
```

核心设计决策：

| 决策 | 说明 |
| --- | --- |
| 无命名空间 | 所有框架类型都是全局的（`Module`、`Game`、`TrackedList`…），与原项目风格一致 |
| **游戏功能皆模块** | ESP 绘制、自瞄、调试窗口……全部是挂在游戏上的 `Module` 子类，跟游戏生命周期。带界面的模块继承 `ImGuiDrawModule`：`DrawObject` 画屏幕，`AddPage` 声明窗口页面进主窗口侧边栏 |
| core 只管数据 | `core/` 不包含主窗口和窗口布局；`Module`/`ImGuiDrawModule` 只保存模块接口、页面回调和屏幕尺寸，通用绘制原语单独提供 ImGui 绘制辅助 |
| 游戏即实例 | 每个游戏是一个继承 `Game` 的类的实例，数据全是成员；模块构造时捕获游戏引用（`g_`），直接读写，无 static_cast |
| 模板化对象池 | 每种对象类型一个 `TrackedList<T>`，游戏传自己的结构体，不做"一个大类装五种池" |

---

## 2. 目录结构

```
DynamicTraverseDraw/jni/
├── CMakeLists.txt            # 构建脚本（arm64-v8a，产物 bin/dynamic_draw）
├── Android.mk / Application.mk  # 旧 ndk-build 脚本（已不用，保留）
├── FRAMEWORK.md              # 本文档
│
├── core/                     # ★ 框架层（本次重构的核心，纯数据处理）
│   ├── Module.h              #   Module 基类 + ModuleHost 宿主
│   ├── ImGuiDrawModule.h     #   界面模块（DrawObject 屏幕绘制 + AddPage 窗口页）
│   ├── Game.h                #   Game 基类（数据处理生命周期）+ GameRuntime
│   ├── math/
│   │   ├── VecMath.h/.cpp    #   Vec2/3/4、Rotation、世界→屏幕投影
│   └── traverse/
│       ├── TrackedList.h     #   对象池模板（Add/Sweep/UpdateAll）
│       └── FrameScanner.h    #   分帧遍历调度
│
├── games/                    # ★ 游戏适配层
│   ├── Games.h               #   游戏清单（接新游戏唯一要动的地方）
│   ├── Example.hpp           #   教学接入模板（占位偏移，展示完整流程）
│   ├── GameTemplate.hpp      #   空白接入骨架（不自动注册）
│   └── texun.hpp             #   完整接入实例（推荐对照）
│
├── diRW/                     # ★ 读写层
│   ├── baseRW.hpp            #   抽象基类（接口 + 通用读取函数）
│   ├── syscallRW.hpp         #   process_vm_readv 直读后端（PUBG 目前用的）
│   ├── TGodRW.hpp            #   TGod 驱动后端
│   ├── RtRW.hpp / Qx11RW.hpp #   Rt / Qx11 驱动后端
│   ├── TwTRW.hpp             #   TwT 驱动后端（带触摸/陀螺仪等扩展）
│   ├── copyRW.hpp            #   copy 驱动后端
│   └── tool.h                #   getPID(包名) 等小工具
│
├── UI/                       # ★ UI 壳（主窗口 = 侧边栏模块加载器）
│   ├── MyUI.h / MyUI.cpp     #   DrawMainMenu（侧边栏+页面）/DrawFpsText + ShellTick
│   └── draw_Gui.cpp          #   字体加载、帧循环粘合、ShellTick 调用
│
├── src/ + include/           # ImGui 壳：ImGui 源码、Vulkan/OpenGL 后端、
│   └── ...                   #   触摸注入（TouchHelperA）、悬浮窗（ANativeWindowCreator）
│                             #   与原项目完全一致，不要动
└── bin/dynamic_draw          # 编译产物（Android arm64 可执行文件）
```

---

## 3. 构建系统

CMake（NDK toolchain），要点：

- **平台**：仅 `arm64-v8a`，`android-26`，`c++_static`，C++17
- **NDK 路径**：优先 `CMAKE_ANDROID_NDK` / 环境变量 `NDK_HOME`，默认回退 `C:/Sdk/ndk/30.0.15729638`
- **产物**：`jni/bin/dynamic_draw`（可执行文件，不是 .so）
- **源文件组织**：
  - `src/` 显式列出（ImGui、渲染后端、触摸）
  - `UI/*.cpp` 自动 glob（`CONFIGURE_DEPENDS`，加文件不用改 CMake）
  - `core/` 只有 `VecMath.cpp` 需要编译，其余全是头文件
- **预编译库**：`src/ImGui/misc/git_freetype/<abi>/libfreetype.a`（ImGui freetype 字体引擎用）
- **链接**：log、android、EGL、GLESv3、dl、m、atomic、c、z
- **编译选项**：`-O2 -fvisibility=hidden -fexceptions -frtti`；宏 `VK_USE_PLATFORM_ANDROID_KHR`、`IMGUI_IMPL_VULKAN_NO_PROTOTYPES`、`IMGUI_ENABLE_FREETYPE` 等

编译命令：

```bash
cd DynamicTraverseDraw/jni
cmake --build build        # build/ 目录已配置好，增量编译
```

---

## 4. 程序启动与主循环

`src/main.cpp`（ImGui 壳，与原项目一致）：

```
main()
 ├─ GraphicsManager::getGraphicsInterface(VULKAN)   # 选渲染后端
 ├─ screen_config()                                  # 读屏幕分辨率/方向
 ├─ ANativeWindowCreator::Create("Surface", ...)     # 创建悬浮窗
 ├─ graphics->Init_Render(window, w, h)              # 初始化 Vulkan
 ├─ Touch::Init(...) + setOrientation                # 触摸注入初始化
 ├─ init_My_drawdata()                               # 字体/样式（draw_Gui.cpp）
 └─ while (UIIsRunning())
     ├─ drawBegin()                                  # 屏幕旋转/悬浮窗重建处理
     ├─ ANativeWindowCreator::ProcessMirrorDisplay() # 屏幕镜像（未穿透时）
     ├─ graphics->NewFrame()
     ├─ Layout_tick_UI(nullptr)                      # ← 每帧核心，见下
     └─ graphics->EndFrame()
 └─ graphics->Shutdown() + Destroy(window)
```

`Layout_tick_UI()`（`UI/draw_Gui.cpp`）每帧只做限帧 + 调 `ShellTick()`：

```cpp
Time_fps.AotuFPS(); SetFps(fps);   // 帧率控制（默认 fps=95）
ShellTick();                       // ← 壳与框架的唯一交点，见 §8
```

---

## 5. core 框架层详解

### 5.1 Module.h / ImGuiDrawModule.h —— 模块机制（游戏功能的统一形态）

两个头文件，按需引用：`Module.h` 是纯模块机制，`ImGuiDrawModule.h`
是界面模块扩展（引用它即自动包含 Module.h）。

```cpp
// core/Module.h —— 纯模块机制
class Module {
public:
    Module(const char *name);       // name 用于 UI 显示（"绘图"/"自瞄"…）
    const char *name;
    virtual void Tick() = 0;        // 每帧调用
    virtual void Shutdown() {}      // 停止时清理模块资源
};

class ModuleHost {                  // 模块宿主：收集 + 驱动（Game 继承它）
    void Attach(Module *m);         // 挂载（不持有所有权）
    const std::vector<Module*> &Modules();
    void TickModules();             // 按挂载顺序逐个 Tick
    void ShutdownModules();         // 按反向挂载顺序逐个 Shutdown
};

// core/ImGuiDrawModule.h —— 界面模块：两部分
class ImGuiDrawModule : public Module {
    // ---- 第 1 部分：屏幕绘制（随 Tick 运行） ----
    virtual void DrawObject() {}    // 画屏幕前景层（ESP 方框/骨骼…），不画留空
    void Tick() override { if (screenInitialized_) DrawObject(); }
                                             // 未初始化屏幕时不会绘制

    // ---- 第 2 部分：窗口页面（动态加载进主窗口侧边栏） ----
    struct Page {
        const char *title;             // 侧边栏标题
        std::function<void()> content; // 选中该页时调用的内容函数
    };
    const std::vector<Page> &Pages();
protected:
    void AddPage(const char *title, std::function<void()> content);  // 构造时声明

    // ---- 屏幕中心（渲染数据，界面模块自管，不进游戏数据层） ----
    void InitializeScreen(float screenX, float screenY); // 主窗口启动游戏前写入宽高
    bool IsScreenInitialized() const;
    float ScreenX() const;          float ScreenY() const;
    float ScreenCenterX() const;    float ScreenCenterY() const; // 由宽高自动计算
    // screenInitialized_ 默认 false；未初始化时 Tick 不调用 DrawObject
};
```

注意 `Shutdown()` 在当前实现里是带空默认实现的虚函数（不再是纯虚）。
正常停止时，`Game::Stop()` 会先按反向挂载顺序调用各模块的 `Shutdown()`，
再调用游戏的 `Shutdown()`，最后释放 `rw_`。基类析构阶段派生类成员已经销毁，
因此基类析构只释放 `rw_`；模块还应在自身析构函数中保证资源最终释放。

**设计要点：**

- **界面模块分两部分**（原项目 DrawObject 关系的抽象化）：
  - **屏幕绘制 `DrawObject`**：随 Tick 每帧运行，直接画屏幕前景层，与
    主窗口无关——勾不勾侧边栏都在跑；
  - **窗口页面 `AddPage`**：一个标题对应一个内容函数，主窗口动态加载，
    选中即调用。设置、调试、雷达面板……要几页加几页，一个模块多页也行。
- **主窗口 = 模块加载器**：主窗口不认识任何具体模块，只认 `ImGuiDrawModule`
  接口——侧边栏默认只有"主页"，把游戏模块声明的所有页面动态加进侧边栏。
  新增带界面的模块零 UI 改动。
- **模块只属于游戏**：ESP 绘制、自瞄、调试窗口……全部挂在 `Game`
  （游戏实例）上，跟随该游戏生命周期，每帧在数据处理之后调用。
  主窗口、FPS 文字这类与具体游戏无关的界面是普通 ImGui 绘制，不套模块。
- **模块怎么拿数据**：构造时捕获游戏引用。游戏模块由游戏类构造（见 §6），
  持有 `GameXxx &g_`，`Tick()` 里直接 `g_.players` 全量访问，无 static_cast、
  无全局查找。
- core 的模块接口不直接调用 ImGui；主窗口和具体控件由 UI/游戏模块负责，
  `DrawPrimitives` 仅提供可选的通用绘制辅助。
- **模块间协作**：模块之间不互相认识；要共享数据/配置就放在游戏实例的成员上
  （页面控件改成员，绘制模块读同一份，参考 Example.hpp）。

### 5.2 Game.h —— 游戏基类（纯数据处理）+ 运行时

```cpp
class Game : public ModuleHost {
public:
    Game(const char *name);
    const char *name;                  // UI 下拉框显示
    const char *packageName;            // 目标进程包名
    bool Start(baseRW *rw);            // 生命周期 ↓
    void Stop();
    void Frame();                      // Update() + TickModules()
    bool Inited();
    virtual diRW::baseRW *CreateReader(); // rw == nullptr 时的内置后端，可选
protected:
    virtual bool Init(diRW::baseRW *rw) = 0;                      // 注册回调等
    virtual void Update() = 0;                                    // 每帧数据处理
    virtual void Shutdown() {}                                    // 游戏数据清理
    diRW::baseRW *rw_ = nullptr;
};
```

子类至少实现 `Init(rw)` 和 `Update()` 两个纯虚函数，其余全由基类管：

| 纯虚函数 | 职责 | 对应原项目 |
| --- | --- | --- |
| `CreateReader()` | 可选的内置后端创建逻辑；通常使用 `packageName` 获取 PID | 原"初始化绘制"的建库逻辑 |
| `Init(rw)` | 建模块基址、注册更新回调（屏幕中心等渲染数据不经过这里，界面模块自管） | 原 `init_esp` |
| `Update()` | 每帧：寻址→遍历→更新，**不画图** | 原 `draw_esp` 的数据部分 |
| `Shutdown()` | 清空对象池、重置地址和游戏侧缓存；模块资源由各模块的 `Shutdown()` 清理 | 原 `deinit_esp` |

**每帧时序**：`Frame()` = `Update()`（数据处理）→ `TickModules()`（本游戏挂载的
绘图/自瞄/调试模块）。模块拿到的数据永远是"本帧刚更新完"的。

**rw 所有权约定**：`Start()` 传入的 `rw` 归游戏所有；`Init` 失败由 `Start` 内部
`delete` 并置空；成功后由 `Stop()`/析构负责释放。不会泄漏或双删。

**`GameRuntime`** —— 运行时切换：

```cpp
namespace GameRuntime {
    inline Game *current;   // 当前选中的游戏实例
    bool Start(Game &game, diRW::baseRW *rw);     // 停旧的 → 注入/回退 rw → game.Start
    void Stop();                                  // 调 current->Stop()
    bool IsRunning();                             // current && current->Inited()
    void Frame();                                 // IsRunning() 才调 game.Frame()
}
```

`Stop()` 之后当前运行状态会关闭；UI 重新勾"初始化"时可对同一个游戏注入新的
读写后端并重开。传入非空 `rw` 时使用外部后端，传入 `nullptr` 时回退到该游戏的
`CreateReader()`。切换下拉框 + 勾选则会 Start 新的（内部先 Stop 旧的）。

### 5.3 TrackedList.h —— 对象池（框架的心脏）

```cpp
template <typename T>
class TrackedList;
```

**对 T 的约定**：业务数据继承 `TrackedObject`，框架元数据集中放在父结构体中，
由编译器强制检查继承关系，业务结构体不需要重复声明这两个字段：

```cpp
struct TrackedObject {
    uintptr_t address = 0; // 由 TrackedList 维护
    int findCount = 0;     // 由 TrackedList 维护
};

struct AnyData : TrackedObject {
    // ... 其余字段游戏随便定义
};
```

`TrackedList<T>` 要求 `T` 公有继承 `TrackedObject`。如果忘记继承，实例化
`TrackedList<T>` 时会直接触发编译错误；`address` 和 `findCount` 的值由对象池
维护，游戏侧不要直接修改。

**生命周期时序**（游戏侧 `Update()` 的标准写法）：

```
分帧遍历中           list.Add(addr)      → 已存在？findCount++ 并返回已有对象
                                          （若是墓碑则原地复活 + 完整更新）
                                          新对象？入池、findCount=1、立即跑一次
                                          update_(obj, true) 完整更新
一整轮扫完（SweepEnd）list.Sweep()       → 消失的对象打墓碑（findCount=-1，不立即删除），
                                          其余 findCount 清零等下一轮；
                     list.UpdateAll(true) → 全池完整更新（慢数据，跳过墓碑）
每帧末尾             list.UpdateAll(false)→ 全池增量更新（快数据，跳过墓碑）
```

**墓碑延迟回收**：`findCount == -1` 即墓碑。Sweep 只标记不擦除；墓碑数量达到
池的 1/4（且至少 16 个）才一次性物理压缩 + 重建索引——把高频少量删除触发的
O(n) 重建摊薄掉（物资/容器这类高换手的池收益最大）。墓碑期间 `UpdateAll`/`Get`
跳过它（不浪费 rw 读取，语义上等于已删除）；`All()` 里仍可见（`findCount<0`
可辨识），绘制侧想"立即隐藏"就按 `findCount >= 0` 过滤，想多显示几帧就不管。

**"慢数据 / 快数据"分频**是性能关键：名字、队伍、血量上限这类基本不变的字段
只在 `full=true` 时读；坐标、血量、朝向这类每帧变的全量读。都在游戏侧的更新
回调里用 `bool full` 参数自己控制。

**完整接口**：

| 接口 | 说明 |
| --- | --- |
| `SetUpdateFunc(fn)` | 注册更新回调 `void(T&, bool full)`；未注册时对象仍可入池，但不会自动更新字段 |
| `SetFilter(fn)` | 可选过滤 `bool(uintptr_t addr)`，返回 false 不入池（如人机过滤） |
| `Add(addr, args...)` | 遍历中遇到地址；额外参数转发给 T 构造函数（如直接填 className）；返回池内指针 |
| `Remove(addr)` | 立即物理移除（低频操作；高频回收交给 Sweep 的墓碑机制） |
| `Sweep()` | 一轮结束：给消失对象打墓碑，达到阈值统一压缩 |
| `UpdateAll(full)` | 全池跑更新回调（自动跳过墓碑） |
| `Get(addr)` | 按地址查（墓碑视为不存在，返回 nullptr） |
| `GetAt(index)` | 按下标取 |
| `All()` | **返回内部 deque 引用，不加锁**；墓碑 `findCount<0` 可自行过滤 |
| `Size()` / `Clear()` | 总数（含墓碑） / 清空 |

**线程模型**：`Add/Sweep/UpdateAll/Get/Size` 各自持锁；但按"遍历与绘制同线程"
设计（当前也是同线程）。`All()` 无锁返回引用——如果以后把骨骼读取等拆到独立
线程，那些线程只允许通过 update 回调写数据，不允许直接持有 `All()` 的引用跨线程用。

**存储与索引**：元素存在 `std::deque` 里（push_back 不使已有元素失效，vector 会）；
内部并行维护一份 `address → 对象指针` 的哈希索引，**`Add`/`Get` 平均 O(1)**，
池再大查重也不掉帧。Sweep 平时只打墓碑不擦除，索引保持有效；只有物理压缩
（墓碑到阈值）或 `Remove` 才重建索引（O(n)，被摊薄到每 1/4 换手率一次）。因此
`address` 字段必须由框架维护，游戏侧不要手改，否则索引失配。

### 5.4 FrameScanner.h —— 分帧遍历调度

把原项目 MaxCount/NowCount 的轮转节奏收进框架。FrameScanner 只维护游标，
不读内存、不做对象分类、不执行回调或 Sweep。游戏侧拿到一批连续槽位后，
可以用一次 readv 读取整批指针，再自行分类并加入对象池：

#### 接口

```cpp
class FrameScanner {
public:
    struct Batch {
        uintptr_t address; // 本批第一个槽位的地址
        int count;         // 本批连续槽位的数量
    };

    explicit FrameScanner(int maxPerFrame = 30);
    int maxPerFrame;       // 每帧最多处理的槽位数

    Batch Next(uintptr_t arrayAddress, int totalSlots);
    bool IsRoundComplete(int totalSlots) const;
    void Reset();
    int Cursor() const;
};
```

参数含义：

- `arrayAddress`：当前目标数组第 0 个元素的地址，不是数组中读出来的第一个对象地址。
- `totalSlots`：当前数组的槽位总数，由游戏侧读取并传入；不能传缓冲区容量。
- `maxPerFrame`：本次最多处理多少个连续槽位。小于等于 0 时内部按 1 处理。
- `Batch.address`：本批起始槽位地址，计算方式是
  `arrayAddress + cursor * sizeof(uintptr_t)`。
- `Batch.count`：本批实际槽位数，末批可能小于 `maxPerFrame`。

`FrameScanner` 的游标由 `Next()` 自动向后移动。每次 `Next()` 只返回一批，
下一帧继续调用即可；当 `IsRoundComplete(totalSlots)` 返回 `true` 时，说明
这一轮已经覆盖了 `[0, totalSlots)` 的全部槽位。完成本轮的 `Sweep()` 等操作
后调用 `Reset()`，下一次从第 0 个槽位重新开始。

#### 标准使用流程

游戏侧每帧按以下顺序处理：

1. 读取当前数组的首地址和数量。
2. 调用 `scanner.Next(arrayAddress, totalSlots)` 获取本批地址和数量。
3. 准备至少能容纳 `batch.count` 个指针的缓冲区。
4. 用一次 `readv()` 读取本批连续槽位。
5. 读取成功后遍历缓冲区，做有效性判断、分类并调用对象池的 `Add()`。
6. 调用 `IsRoundComplete(totalSlots)`；完成时执行 `Sweep()`、完整更新和 `Reset()`。
7. 在每帧末尾执行对象池的增量更新。

```cpp
FrameScanner scanner{30};
std::array<uintptr_t, 30> pointerBuffer{};

void UpdateActors() {
    uintptr_t actorArray = 0;
    int actorCount = 0;

    // 这里由游戏侧读取当前 Actors 数组头，得到数组首地址和槽位数量。
    if (!ReadActorArray(&actorArray, &actorCount)) {
        scanner.Reset();
        return;
    }

    // 空数组也代表本轮没有对象，需要结束本轮并清理消失对象。
    if (!actorArray || actorCount <= 0) {
        actors.Sweep();
        actors.UpdateAll(true);
        scanner.Reset();
        return;
    }

    const FrameScanner::Batch batch =
        scanner.Next(actorArray, actorCount);

    if (batch.count <= 0 ||
        !rw->readv(batch.address, pointerBuffer.data(),
            static_cast<size_t>(batch.count) * sizeof(uintptr_t))) {
        // 本批没有读取成功：不要 Add，也不要 Sweep。
        // 当前实现已经推进游标；需要重扫时从本轮起点 Reset。
        return;
    }

    for (int i = 0; i < batch.count; ++i) {
        const uintptr_t actor =
            pointerBuffer[static_cast<size_t>(i)];
        if (actor == 0)
            continue;

        // 按当前游戏的对象类型、阵营、类名等规则分类。
        actors.Add(actor);
    }

    if (scanner.IsRoundComplete(actorCount)) {
        actors.Sweep();
        actors.UpdateAll(true);
        scanner.Reset();
    }

    actors.UpdateAll(false);
}
```

读取失败时，`Next()` 已经移动了游标。`texun.hpp` 和 `Example.hpp` 采用
`scanner.Reset()`，下一帧从本轮第 0 个槽位重新开始；如果你的游戏允许跳过失败批次，
也可以保留游标继续处理下一批。当前接口只提供整轮重置，不提供回退到刚才那一批的操作。
无论采用哪种策略，失败批次都不能调用 `Add()` 或 `Sweep()`。

需要注意，`arrayAddress` 和 `totalSlots` 应来自同一帧读取的数组头。数组在游戏
运行中可能发生变化；游戏侧每帧重新读取数组头，扫描器只根据本次传入的数量计算
范围，不会缓存数组地址或数量。

内部逻辑：

- Next(arrayAddress, totalSlots) 每帧最多返回 maxPerFrame 个连续槽位；
  Batch.address 是本批起始地址，Batch.count 是本批数量；指针步长使用
  sizeof(uintptr_t)，不会读取 totalSlots 之后的槽位；
- 当前批次读取成功后，游戏侧调用 IsRoundComplete(totalSlots)。为 true 时，
  才执行 Sweep、UpdateAll(true) 等整轮操作，然后调用 Reset() 开始下一轮；
- 空数组返回 count == 0 且视为本轮完成；数组地址和数量由游戏侧按当前游戏逻辑提供；
- 批量 readv 失败时，本批次不要分类、Add 或 Sweep；是否重置游标由游戏侧决定；
- addr < kMinValidAddress (0xFFFFFF) 视为无效槽位跳过（与原版判断一致），分类策略
  完全由游戏侧在一次批量读取后的普通循环中完成。

buffer 的容量必须至少为 batch.count（通常按 scanner.maxPerFrame 预留）。这样一帧只需
一次读取最多 30 个槽 + 增量更新，Actor 数组几千个对象也不会卡帧；
相比每槽回调，热路径少了 std::function 间接调用和重复读请求。代价是一个对象
从入池到被 Sweep 之间可能滞后几帧，对绘制场景无影响。

### 5.5 VecMath —— 数学工具

`core/math/VecMath.h/.cpp`，全局命名空间：

| 类型/函数 | 说明 |
| --- | --- |
| `Vec2/Vec3/Vec4` | 基础向量；`Vec3` 带运算符和 `IsValid()`（排 NaN/Inf/非规格化/超大量级/任一近零分量） |
| `Rotation` | pitch/yaw/roll（度） |
| `WorldToScreen(m, pos, cx, cy)` | 世界→屏幕。矩阵约定：`w = m[3]x + m[7]y + m[11]z + m[15]`；`w <= 0` 的点视为无效，返回 `(0,0)` |
| `WorldToBox(m, pos, cx, cy, lowerOffset, upperOffset, ratio)` | 世界坐标→屏幕包围盒。返回 `Vec4{x=左边, y=顶边, z=半宽, w=高度}`；下端/上端偏移沿 Z 轴，宽高比由调用方按对象类型提供 |
| `CalcDistance(a, b)` | 两点欧氏距离，按当前坐标单位取整；不会自动换算厘米/米 |
| `CalcScreenRadius(p, cx, cy)` | 屏幕点到准星距离 |
| `CalcRotation(from, to)` | 按当前坐标和角度约定由两点求朝向，pitch/yaw 返回度数，roll 为 0 |
| `MinAngleDiff(target, current)` | 最小角差，归一到 [-180,180] |

---

## 6. games 游戏适配层详解

### 6.1 Games.h —— 游戏清单

```cpp
#include "core/Game.h"
#include "games/Example.hpp"

inline Game *const kGames[] = {
    &example::game, // 教学示例
};
inline const int kGameCount = ...;
```

UI 下拉框直接遍历这个数组。

### 6.2 texun.hpp / Example.hpp —— 接入实例与教学模板

`games/texun.hpp` 是当前项目的完整接入实例，展示真实的对象结构、模块、偏移、
对象池和调试绘制流程。`games/Example.hpp` 是与该实例保持相同节奏的教学模板，
模块名、包名、so 名称和偏移使用占位值，需要接入具体游戏时由游戏侧填写。模板完整展示
读写后端接口 → 对象池 →
分帧遍历 → 更新回调 → DrawObject 屏幕绘制 → 主窗口侧边栏页面 的流程。
文件从上到下五段，接真游戏照这个骨架抄：

1. **数据结构体**：继承 `TrackedObject`（其中的 `address`/`findCount` 由
   `TrackedList` 维护），其余字段完全按游戏需要定，**框架对业务字段零知识**。
2. **读写后端**：模板不内置模拟后端。真游戏可以由窗口或外部配置选择
   `syscallRW` 或驱动后端，再通过 `GameRuntime::Start(game, rw)` 注入；如果传入
   `nullptr`，则回退到游戏自己的 `CreateReader()`（见 §7）。
3. **界面模块**：继承 `ImGuiDrawModule`——`DrawObject()` 画屏幕
    （示例按 `texun.hpp` 绘制包围框和射线），构造函数 `AddPage` 声明页面，
    成员函数体定义在游戏类之后（完整类型）。
4. **游戏实例类**：继承 `Game`，对象池/矩阵/配置全是成员；实现
    `Init`/`Update`/`Shutdown` 生命周期函数；读写后端由外部传入；构造函数
   `Attach` 模块；文件末尾 `inline ExampleGame game;`。
5. **模块实现 + 实例**：`DrawObject`/各页内容函数直接用构造捕获的 `g_`。

关键细节（示例代码里有注释）：

- **真实对象链**：模板按 `GWorld → PersistentLevel → Actors` 的顺序读取，
  具体链路和偏移由目标游戏填写。
- **分帧遍历的批量读取**：调用 scanner.Next() 得到连续的 Batch.address 和 Batch.count，由游戏侧保证缓冲区容量足够；实例使用一次 readv 读取批次后再分类。
- **设置页改的是游戏实例的成员**（`drawBox`/`drawRay`），
  `DrawObject` 读同一份，改动立即生效——这就是模块间共享数据的标准做法。

---

## 7. diRW 读写层详解

### 7.1 baseRW.hpp —— 抽象基类

```cpp
class baseRW {
public:
    enum class PidMode { Global, Private };

    // 三个纯虚接口（后端唯一要实现的东西）
    virtual bool readv(uintptr_t addr, void *buf, size_t size) = 0;
    virtual bool writev(uintptr_t addr, void *buf, size_t size) = 0;
    virtual uintptr_t get_module_base(const char *name) = 0;

    // 通用读取辅助
    float getFloat(addr);  int getDword(addr);  bool getBool(addr);
    char *getUTF8(addr);            // UTF-16 → UTF-8（游戏内名字符串）
    uintptr_t getPtr64(addr);       // 读 8 字节并 & 0xFFFFFFFFFF
    uintptr_t getPtr32(addr);       // 读 4 字节并 & 0xFFFFFFFFFF
    bool writeFloat(addr, float);

    // 变参多级跳转：jumpPoint(base, off1, off2, off3)
    //   逐级 readv(base+off) 取下一级指针；偏移必须是 int
    template <typename... Args>
    uintptr_t jumpPoint(uintptr_t addr, Args... args);

    bool isConnected();             // 后端就绪状态（驱动对接失败不崩，由调用方查询）

    // PID 管理
    enum PidMode;                   // Global：所有实例共享一个静态 pid（atomic）
                                    // Private：实例各自持有一个 pid
    static getGlobalPid()/setGlobalPid(tpid);
};
```

要点：

- **`jumpPoint` 的地址掩码 `& 0xFFFFFFFFFF`**（40 位）：用户态地址空间裁剪，
  与原版判断一致，读垃圾值时把它洗成 0 附近的无效地址。
- **`connected` 标志**：后端构造失败不抛异常不退出，置 false，由调用方
  `isConnected()` 决定是否继续。
- **Global/Private 两种 pid 模式**：默认用 Global（整个进程一个目标 pid，
  多线程读写时用 atomic 保证可见性）。
- **已知小坑**：`getUTF8` 里 `while (pTempUTF16 < pTempUTF16 + 28)` 恒真，
  实际靠循环体内的编码分支 break 退出，只处理前 14 个 UTF-16 字符。
  修复它会改变名字读取长度，暂保持原样（与原版行为一致）。

### 7.2 后端一览

| 后端 | 文件 | 原理 | 状态 |
| --- | --- | --- | --- |
| `syscallRW` | syscallRW.hpp | `process_vm_readv/process_vm_writev` 直读 | ★ PUBG 默认使用 |
| `TGodRW` | TGodRW.hpp | TGod 驱动（ioctl，带 ReadMode 选择） | 可选 |
| `RtRW` | RtRW.hpp | Rt 驱动 | 可选 |
| `Qx11RW` | Qx11RW.hpp | Qx11 驱动 | 可选 |
| `TwTRW` | TwTRW.hpp | TwT 驱动（还带触摸注入/陀螺仪等扩展接口） | 可选 |
| `copyRW` | copyRW.hpp | copy 驱动 | 可选 |

`diRW/tool.h` 提供 `getPID(包名)`（遍历 /proc 找进程）。

后端可以在窗口或外部配置中选择并创建，然后通过
`GameRuntime::Start(game, rw)` 注入游戏；这样无需修改游戏类即可动态切换
`syscallRW`、驱动后端或其他实现。若传入 `nullptr`，框架会调用游戏自己的
`CreateReader()` 作为默认后端。游戏的 `packageName` 可直接用于默认后端获取 PID。

---

## 8. UI 壳详解

### 8.1 MyUI.h / MyUI.cpp —— 主窗口（侧边栏模块加载）+ 每帧调度

主窗口是一个**模块加载器**：侧边栏默认只有"主页"，把当前游戏各模块
（`ImGuiDrawModule`，`dynamic_cast` 识别）用 `AddPage` 声明的页面
（标题 → 内容函数）动态加进侧边栏，选中即调用对应内容函数。
屏幕绘制（`DrawObject`）不在主窗口——它随游戏帧的 `TickModules()` 运行。
壳的构成：

1. **普通绘制函数**（直接调用，不是模块）：
   - `DrawMainMenu()`：主窗口——侧边栏（主页 + 动态模块页）+ 页面区域；
   - `DrawFpsText()`：屏幕上方的 FPS 文字。
2. **模块加载辅助**（static）：
   - `CollectPages()`：遍历游戏模块，收集所有 `ImGuiDrawModule::Page`
     （上限 64）；
   - `DrawHomePage()`：主页内容——游戏下拉框、初始化开关、运行状态、退出按钮。
3. **`ShellTick()`** —— 每帧的调度中枢：

```cpp
void ShellTick() {
    DrawFpsText();        // ① FPS 文字
    DrawMainMenu();       // ② 主窗口（侧边栏页面，选中页调 page->content()）
    GameRuntime::Frame(); // ③ 当前游戏：数据处理 + TickModules
                          //    （含界面模块的 DrawObject 屏幕绘制）
}
```

主窗口长这样（侧边栏按模块声明的页面自动生成，新模块零改动出现）：

```
┌ DynamicTraverseDraw ───────────────────────────┐
│ ┌──────────┐  主页                             │
│ │ 主页     │  游戏 [ 示例 ▼]                    │
│ │ 示例设置 │  [x] 初始化                       │
│ │ 示例调试 │  ────────────────                 │
│ │          │  运行状态:运行中                   │
│ │          │  [ 退出 ]                          │
│ └──────────┘                                   │
└────────────────────────────────────────────────┘
```

- 主页里勾"初始化"：先把屏幕宽高喂给该游戏的界面模块（`InitializeScreen`，
  屏幕中心由模块内部自动计算；只有宽高均为正时才允许绘制；渲染数据归
  ImGuiDrawModule 自管，不进数据层），再由窗口根据读写配置创建 `rw`，调用
  `GameRuntime::Start(*kGames[selected], rw)`；如果没有外部后端则传 `nullptr`，
  框架自动回退到游戏的 `CreateReader()`；取消则 `Stop()`。
- 游戏未启动时侧边栏只显示"主页"（模块页随游戏生命周期出现/消失，
  选中值自动回落）。
- 一个模块可以声明多页（示例模块就有"示例设置"+"示例调试"两页），
  标题可以随意取，与模块名无关。

### 8.2 draw_Gui.cpp —— 帧循环粘合（ImGui 壳的一部分）

- 全局：`graphics`（渲染接口）、`window`（悬浮窗）、`displayInfo`、
  `abs_ScreenX/Y`、`fps=95`、字体指针。
- `init_My_drawdata()`：亮色主题 + 系统中文字体（28px）+ FontAwesome 图标
  合并 + `ScaleAllSizes(3.5f)`（高分屏缩放）。
- `drawBegin()`：处理 `permeate_record`（穿透模式切换时重建悬浮窗）和屏幕
  旋转（转屏时同步 `Touch::setOrientation`）。
- `Layout_tick_UI()`：限帧 → `ShellTick()`。

---

## 9. 一帧的完整数据流

```
ImGui 帧开始
  │
  ├─ ShellTick()                     ─────── 壳（MyUI.cpp）───────
  │   ├─ DrawFpsText()               FPS 文字
  │   ├─ DrawMainMenu()              主窗口（侧边栏：主页+动态模块页；
  │   │                               选中页调 page->content() 画控件）
  │   └─ GameRuntime::Frame()        （仅 current && inited 时）
  │       ├─ current->Frame()
  │       │   ├─ Update()                     游戏数据处理
  │       │   │   ├─ 寻址 / 遍历（scanner.Next + readv 分帧）/ 更新回调
  │       │   │   └─ 各池 UpdateAll(false)：每帧增量更新
  │       │   └─ TickModules()                本游戏的模块（按挂载顺序）
  │       │       ├─ 绘图模块 Tick → DrawObject()   ← 屏幕前景层（ESP…）
  │       │       └─ 自瞄模块 Tick                   ← 纯功能模块
  │
  └─ EndFrame 呈现
```

性能特征：单帧成本 = 一次最多 30 槽的批量读取 + 全池一次增量读 + 模块绘制，与场上对象总数无关（完整更新摊到了一整轮的分帧里）。

---

## 10. 扩展指南

### 10.1 接入一个新游戏（checklist）

> **完整接入实例：[games/texun.hpp](games/texun.hpp)** —— 推荐先按该文件理解
> 模块、对象池、分帧遍历、更新回调和绘制页面的完整流程，再把占位内容替换为目标游戏布局。
> `games/Example.hpp` 是同一写法的教学版本，`games/GameTemplate.hpp` 是空白骨架。

1. **建 `games/Your.hpp`**，照 texun.hpp 的结构写：
   - 定义数据结构体并继承 `TrackedObject`（`address`/`findCount` 不要重复定义）；
   - 写偏移表 `struct Offsets` + `constexpr Offsets kOff{}`；
   - 声明并实现游戏模块（构造捕获 `YourGame &g`；屏幕绘制/窗口页按需）；
   - 写 `class YourGame : public Game`，对象池、矩阵等全部做成成员，构造函数
     里 `Attach` 模块；文件末尾 `inline YourGame game;`。
2. **`games/Games.h`** 加两行：`#include "games/Your.hpp"` + 数组加
   `&your::game,`。
3. 完成。UI 会自动出现新游戏和它的模块页面；core、diRW、UI **一行都不用改**。

骨架：

```cpp
#pragma once
#include "core/Game.h"
#include "core/ImGuiDrawModule.h"   // 界面模块（含 Module.h）
#include "core/math/VecMath.h"
#include "core/traverse/FrameScanner.h"
#include "core/traverse/TrackedList.h"
#include "diRW/syscallRW.hpp"   // 按需换后端
#include "diRW/tool.h"

namespace yourgame {

struct ActorData : TrackedObject {
    // 你的字段
};

class YourGame; // 前置声明

// 界面模块：DrawObject 画屏幕 + AddPage 声明窗口页
class YourDraw : public ImGuiDrawModule {
public:
    YourDraw(YourGame &g) : ImGuiDrawModule("绘制"), g_(g) {
        AddPage("绘制设置", [this] { DrawSettings(); });
    }
    void DrawObject() override;          // 屏幕前景层，随 Tick 每帧跑
private:
    void DrawSettings();                 // "绘制设置"页内容
    YourGame &g_;
};

class YourGame : public Game {
public:
    YourGame() : Game("YourGame"), draw_(*this) { Attach(&draw_); }

    TrackedList<ActorData> actors;
    float matrix[16] = {};

protected:
    bool Init(diRW::baseRW *rw) override {
        lib_ = rw->get_module_base("libxxx.so");
        actors.SetUpdateFunc([this](ActorData &a, bool full) { UpdateActor(a, full); });
        return true;
    }
    void Update() override {
        // 见 §5.4：scanner.Next + 一次 readv；成功后分类/Add；
        // scanner.IsRoundComplete(actorCount) 为 true 后再 Sweep。
    }
    void Shutdown() override { actors.Clear(); }

private:
    void UpdateActor(ActorData &a, bool full) { /* rw_ 读写 */ }
    YourDraw draw_;
    FrameScanner scanner_{30};
    uintptr_t lib_ = 0;
};

inline YourGame game;

inline void YourDraw::DrawObject() {
    // 屏幕绘制：直接用 g_.actors / g_.matrix 投影后画
}

inline void YourDraw::DrawSettings() {
    // 页面控件：改 g_ 的成员，DrawObject 读同一份
}

} // namespace yourgame
```

### 10.2 写一个新模块（绘图/自瞄/其他）

```cpp
// 纯功能模块：只要 Tick
class YourAim : public Module {
public:
    YourAim(YourGame &g) : Module("自瞄"), g_(g) {}
    void Tick() override { /* 每帧逻辑，读 g_ */ }
private:
    YourGame &g_;
};

// 界面模块：继承 ImGuiDrawModule，两部分按需取用
class YourDraw : public ImGuiDrawModule {
public:
    YourDraw(YourGame &g) : ImGuiDrawModule("绘制"), g_(g) {
        AddPage("绘制设置", [this] { DrawSettings(); });  // 窗口页：几页都行
        AddPage("绘制调试", [this] { DrawDebug(); });
    }
    void DrawObject() override { /* 屏幕绘制，随 Tick 每帧跑 */ }
private:
    void DrawSettings() { /* 控件改 g_ 的成员 */ }
    void DrawDebug()    { /* 调试信息 */ }
    YourGame &g_;
};
// 挂载：YourGame 构造函数里加成员 + Attach，或外部 game.Attach(&某模块)
```

不需要屏幕绘制的界面模块：不重写 `DrawObject`（默认空）即可，
`Tick()` 默认就是 `DrawObject()`，等于什么都不跑，页面照常加载。
模块之间不互相认识；要共享数据/配置就放在游戏实例的成员上。
不依赖游戏数据的界面（如新的悬浮工具窗）不套模块，直接在 MyUI.cpp 写
普通绘制函数并在 `ShellTick()` 里调用即可。

### 10.3 换/加读写后端

- 加后端：继承 `baseRW`，实现 `readv/writev/get_module_base`，构造成功置
  `connected = true`。
- 换后端：窗口/外部配置创建新的 `rw`，重新调用
  `GameRuntime::Start(game, rw)`；旧后端会在启动新实例前由 `Stop()` 释放。
  传入 `nullptr` 则使用游戏内置的 `CreateReader()`。

---

## 11. 与原项目的模块映射及当前 TODO

> PUBG 适配层（原 games/Pubg.hpp）已按框架重构前的版本删除；下表是
> 重写时的对照清单，重写后接回 Games.h 即可。

| 原项目（TGOD_pubg/jni/hack） | 重写后归属 | 状态 |
| --- | --- | --- |
| main_hack.cpp 初始化/主循环拆解 | `Game` 基类生命周期 + `GameRuntime` | ✅ 框架已就位 |
| ObjectManage/DataStruct.h | `core/math/VecMath.h` + 游戏内结构体 | ✅ |
| ObjectManage/ObjectDataManage.cpp 五池管理 | `TrackedList<T>` × N（模板化，逐类型） | ✅ |
| 分帧遍历（MaxCount/NowCount） | `FrameScanner` | ✅ |
| DrawObject / 绘图.cpp | 绘图模块 `DrawObject()`（屏幕绘制） | ⬜ 重写游戏时接 |
| TouchAim | 自瞄模块 `Tick()` | ⬜ 重写游戏时接 |
| 自瞄窗口 / 调试窗口 | 绘图/自瞄模块的 `AddPage` 页面（主窗口侧边栏） | ✅ 机制就位，内容随功能接 |
| Other/BoneGet.hpp 骨骼读取 | 更新回调里做（可先缓存骨骼地址） | ⬜ 重写游戏时接 |
| Other/classname.hpp 类名识别 | 批量读取后的游戏侧分类循环 + 名称表 | ⬜ 重写游戏时接 |
| Other/武器.cpp、ContainerItemInfo.h 名称表 | 待接（接后可在对应模块页面提供切换） | ⬜ 重写游戏时接 |
| diRW/kernelRW 驱动后端 | 窗口/外部配置创建后通过 `GameRuntime::Start(game, rw)` 注入 | ⬜ 按需 |

框架层（core/diRW/UI）已全部就位并可运行；接一个真游戏 = 照 §10.1 的
骨架写一个 games/Xxx.hpp，完成 `Init`/`Update`/`Shutdown` 和模块内容；可以实现
`CreateReader()` 提供默认后端，也可以完全由窗口或其他外部配置层创建后注入。
