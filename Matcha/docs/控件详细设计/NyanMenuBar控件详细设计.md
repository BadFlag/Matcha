# NyanMenuBar 控件详细设计

## 功能

本文档描述 Matcha 原生 `NyanMenuBar` 菜单控件的职责边界、数据结构、绘制布局、事件处理、主题 Token、Action 绑定和与 `NyanMainTitleBar` 的集成方式。菜单系统由 `NyanMenuBar`、`NyanMenu` 和 `NyanMenuItemData` 共同构成，顶层菜单栏和弹出菜单项均采用数据驱动的集中绘制方案。

| 名称 | 说明 |
| --- | --- |
| 顶层菜单栏 | `NyanMenuBar` 负责横向一级菜单、hover、active、键盘入口和打开弹出菜单。 |
| 弹出菜单 | `NyanMenu` 负责顶层 popup 窗口、阴影、圆角、定位、动画、菜单项绘制和子菜单级联。 |
| 菜单项模型 | `NyanMenuItemData` 统一表达普通项、子菜单、分组、分隔线、勾选、禁用、危险项、快捷键和 QAction 绑定。 |
| 菜单项绘制 | 弹出菜单项由 `NyanMenu::PaintItem()` 集中绘制，不依赖每个菜单项单独创建 QWidget。 |
| 分隔线合并 | 分隔线由菜单项 `type == Divider` 统一表达和绘制，不作为弹出菜单中的独立 QWidget。 |
| QAction / 命令绑定 | 支持从 `QAction` 适配菜单项，并在触发时回调 `QAction::trigger()`，同时发出 Matcha 菜单信号。 |
| Matcha 主题 | 只使用 `Light.json` 已存在 Token key，通过 `ThemeAware`、`Theme().Color(...)`、`Theme().DimensionPx(...)`、`Theme().ResolveIcon(...)` 等机制读取，不新增不存在的 Token 字符串。 |
| NyanMainTitleBar 集成 | `NyanMenuBar` 保持作为 `NyanMainTitleBar` 上层左侧控件，不接管标题栏拖拽、窗口按钮、返回按钮和标题居中逻辑。 |

## 组件结构

### 文件和职责

| 文件 | 职责 |
| --- | --- |
| `Matcha\Include\Matcha\Widgets\Menu\NyanMenuBar.h` / `NyanMenuBar.cpp` | 横向菜单栏，维护顶层 `NyanMenuItemData` 列表、顶层项命中区域、hover/active 状态和 popup 打开关闭。 |
| `Matcha\Include\Matcha\Widgets\Menu\NyanMenu.h` / `NyanMenu.cpp` | `Qt::Popup` 下拉菜单容器，负责 popup 外壳、动画、屏幕边界、键盘导航、子菜单延迟、菜单项布局和集中绘制。 |
| `Matcha\Include\Matcha\Widgets\Menu\NyanMenuTypes.h` / `NyanMenuTypes.cpp` | 菜单数据模型，提供 `NyanMenuItemData`、`NyanMenuItemType`、`NyanPopupPlacement` 和 QAction 适配入口。 |
| `NyanMenuItem.h/.cpp` | 兼容保留类。数据化菜单路径不依赖它绘制弹出菜单项。 |
| `NyanMenuCheckItem.h/.cpp` | 兼容保留类。数据化菜单路径使用 `checkable / checked` 字段表达勾选状态。 |
| `NyanMenuSeparator.h/.cpp` | 兼容保留类。数据化菜单路径使用 `NyanMenuItemType::Divider` 表达分隔线。 |
| `MenuBarNode.h/.cpp` | UiNode 层菜单栏包装，负责把节点配置写入 `NyanMenuBar`。 |
| `MenuNode.h/.cpp` | UiNode 层菜单包装，负责组织菜单子项数据。 |
| `MenuItemNode.h/.cpp` | UiNode 层菜单项包装，负责保存 item key/text/enabled 等数据并触发菜单数据刷新。 |
| `ContextMenuComposer.h/.cpp` | Foundation 层上下文菜单组合器，后续可通过转换函数生成 `NyanMenuItemData`。 |

### 核心结构决策

| 决策 | 说明 | 原因 |
| --- | --- | --- |
| 顶层菜单手绘 | `NyanMenuBar` 直接绘制顶层文字、hover 状态和 active 下划线。 | 顶层项状态、命中测试和 popup 切换由同一个控件统一管理。 |
| 下拉菜单项集中绘制 | `NyanMenu` 维护 visible item rect，并在 `PaintItem()` 中绘制分隔线、分组、图标、勾选、文本、快捷键和子菜单箭头。 | 菜单项布局、状态和绘制保持一致，避免每个 item QWidget 各自维护视觉状态。 |
| 数据化菜单树 | `NyanMenuItemData` 保存 key、text、icon、shortcut、extraText、type、children、enabled、visible、checkable、checked、danger 和 action。 | 同一数据结构覆盖顶层菜单、弹出菜单、子菜单和命令绑定。 |
| popup 外壳内聚 | `NyanMenu` 同时承担弹出窗口外壳和菜单内容绘制职责。 | 对外 API 保持简单，内部通过 `ContentRect()`、`PopupSize()`、`PaintPopupFrame()`、`PaintItem()` 分离职责。 |
| 分隔线和勾选项数据化 | 分隔线使用 `NyanMenuItemType::Divider`，勾选使用 `checkable / checked` 字段。 | 菜单项类型统一进入命中测试、键盘导航和绘制流程。 |
| QAction 适配 | `NyanMenuItemData::FromAction()` 从 QAction 生成菜单项，触发时调用 `action->trigger()`。 | 兼容 Qt 命令模型，同时向 Matcha 层发出菜单信号。 |
| 弹出动画 | 使用 `QParallelAnimationGroup` 同时驱动 `geometry` 和 `windowOpacity`。 | popup 打开时具备明确的展开方向和淡入过程，关闭时统一收敛到 `CloseAll()`。 |
| 不拆出 public popup 类 | 当前设计不新增 public `NyanPopupMenu`。 | `NyanMenu` 已能覆盖下拉菜单和级联子菜单；后续只有在复杂度明显上升时再拆分内部 helper。 |

## Token 取值设计

设计原则：

1. 只使用 `D:\CodeData\Matcha\Matcha\Resources\Themes\Light.json` 已存在 key。
2. 不新增 `menu.xxx`、`popup_shadow`、`motion_duration_ms` 等不存在 JSON key。
3. 不把 Token 值缓存为成员变量，绘制和布局时直接读取。
4. 动画时长、popup offset、最小宽度等若 `Light.json` 无对应 key，作为类内 constexpr 或局部 fallback，不伪装成主题 Token。

| 语义 | Matcha key | 当前 Light.json 值 | 用途 |
| --- | --- | --- | --- |
| 菜单栏背景 | `colorBgContainer` | `#FFFFFFFF` | `NyanMenuBar` 背景、popup 内容背景。 |
| 次级背景 | `colorBgContainerSecondary` | `#FAFAFC` | 可用于 group 行或弱背景。 |
| hover 背景 | `colorFillHover` | `#0D000000` | 顶层项和菜单项 hover。 |
| pressed 背景 | `colorFillSecondaryHover` | `#1A000000` | 菜单项 pressed。 |
| selected 背景 | `colorPrimaryBg` | `#F0F7FF` | active/checked 可选背景。 |
| 主色 | `colorPrimary` | `#0066FF` | 顶层 active 下划线、selected 文本、checkmark。 |
| 正文 | `colorText` | `#E0000000` | 普通菜单项文本。 |
| 次级文本 | `colorTextSecondary` | `#A6000000` | shortcut、group 文本。 |
| 禁用文本 | `colorTextTertiary` | `#66000000` | disabled item 文本和图标。 |
| 分隔线 | `colorDivider` | `#E3E3E6` | divider。 |
| 边框 | `colorBorder` | `#D8D8DA` | popup border。 |
| 危险色 | `colorError` | `#CC1423` | danger 文本。 |
| 菜单栏高度 | `controlHeightSM` | `24` | `NyanMenuBar` height。 |
| 菜单项高度 | `controlHeightMD` | `32` | popup item height。 |
| 小间距 | `spaceXXS` | `4` | popup padding、divider margin。 |
| 常规间距 | `spaceXS` | `8` | icon gap、菜单栏 item gap。 |
| 中间距 | `spaceSM` | `12` | item horizontal padding。 |
| 默认圆角 | `radiusDefault` | `3` | item hover 背景圆角。 |
| 大圆角 | `radiusLarge` | `6` | popup 圆角。 |
| 细线 | `lineWidthMS` | `1` | popup border、divider。 |
| 下划线 | `lineWidthMD` | `2` | 顶层 active underline。 |
| 图标尺寸 | `iconSizeSM` | `16` | 菜单项 icon/checkmark。 |
| 阴影 | `shadowMS` | JSON shadows 数组 | popup shadow 绘制输入。 |

## 资源图标取值设计

| 场景 | 设计 |
| --- | --- |
| 调用方传入图标 | `NyanMenuItemData::icon` 保存 `QIcon`，绘制时按 enabled/disabled 选择 `QIcon::Mode`。 |
| 框架内置图标 | 调用方通过 `fw::icons` 和 `Theme().ResolveIcon(...)` 生成 `QIcon` 后传入菜单项。菜单系统不硬编码资源路径。 |
| checkmark | 不强依赖资源图标，使用 `QPainter::drawLine()` 绘制勾选。 |
| submenu arrow | 不强依赖资源图标，使用两条线绘制右箭头。 |
| 图标颜色 | 若 `ResolveIcon(...)` 能生成主题色图标，则由调用方生成；菜单绘制层不发明额外 icon token。 |

## 绘制和布局设计

### NyanMenuBar

| 项 | 设计 |
| --- | --- |
| 数据 | `QList<NyanMenuItemData> _items`，只显示 `visible == true` 的顶层项。 |
| 几何 | `QList<QRect> _itemRects`，每次布局或绘制前根据 font metrics 重建。 |
| 高度 | `Theme().DimensionPx("controlHeightSM").value_or(24)`。 |
| 宽度 | 文本宽度 + `spaceXS * 2`，项间距使用 `spaceXXS` 或 `spaceXS`。 |
| 背景 | 填充 `colorBgContainer` 或由父标题栏透出；设计首选填充 `colorBgContainer`。 |
| hover | 文本保持 `colorText`，底部可绘制 hover/active 下划线；hover 背景如需启用使用 `colorFillHover`。 |
| active | popup visible 且当前 index active 时，底部绘制 `colorPrimary` 2px 下划线。 |

### NyanMenu

`NyanMenu` 内部按两部分职责组织：

| 职责 | 设计 |
| --- | --- |
| Popup 外壳 | 顶层 `Qt::Popup`，绘制阴影、圆角背景、边框，负责定位、动画、关闭、子菜单链。 |
| Menu 内容 | 维护 visible items、item rect、hovered/pressed/active、scroll offset，集中绘制菜单项。 |
| 弹出菜单宽度 | `PopupSize()` 遍历 visible item 计算自然宽度。主文本必须先通过 `MnemonicState::Parse(item.text).displayText` 去掉助记符标记后再测量；右侧尾列按实际内容动态计算：子菜单只预留 `iconSizeXS` 箭头宽度，快捷键/extraText 预留其真实 `fontMetrics().horizontalAdvance(...)`，没有尾部内容则不预留尾列。禁止使用固定 80px/90px 尾列挤压主文本。 |

菜单项绘制顺序：

1. 如果 `Divider`，绘制分隔线并返回。
2. 如果 interactive 且 hover/active/pressed，绘制圆角背景。
3. 如果 `Group`，绘制 group 文本并返回。
4. 绘制 checkmark 或 icon，占用固定 icon 列。
5. 使用 `MnemonicState::Parse(...)` 得到显示文本，避免把 `&File`、`&Open` 之类助记符原文绘制成 `&...`。
6. 根据子菜单箭头或快捷键真实宽度计算右侧尾列，得到主文本绘制区域。
7. 绘制主文本，仅在真实超宽时 elide。
8. 如果有 children，绘制 submenu arrow。
9. 否则绘制 shortcut 或 extraText。

## 事件处理设计

| 事件 | NyanMenuBar | NyanMenu |
| --- | --- | --- |
| mouse move | 更新 hovered index；popup 已打开时 hover 到其他顶层项立即切换。 | 更新 hovered/active item；hover 子菜单项后延迟打开。 |
| mouse press | 记录 pressed 或直接打开顶层菜单。 | 记录 pressed item，disabled/group/divider 不可 pressed。 |
| mouse release | 顶层点击打开/关闭/切换。 | 同一 item release 才触发；普通 item 触发后 close all；submenu item 打开子菜单。 |
| leave | 清理 hover；popup 打开时不立即关闭顶层 active。 | 可清理 hover；若移动到子菜单安全区则不关闭父菜单。 |
| key press | Alt/F10 激活；Left/Right 切换；Down/Enter/Space 打开；Esc 关闭。 | Up/Down 移动 active；Right 打开子菜单；Left 关闭当前子菜单；Enter/Space 触发；Esc closeRequested。 |
| wheel | 不处理。 | 调整 `_scrollOffset`，长菜单滚动。 |
| focus/window deactivate | 退出菜单模式。 | popup 外部点击或窗口失活时 close all。 |

## 菜单数据结构和 API 设计

### 数据结构

```c++
enum class NyanMenuItemType {
    Item,
    SubMenu,
    Group,
    Divider
};

enum class NyanPopupPlacement {
    Below,
    Context,
    Right,
    Left
};

struct NyanMenuItemData {
    QString key;
    QString text;
    QIcon icon;
    QKeySequence shortcut;
    QString extraText;
    NyanMenuItemType type = NyanMenuItemType::Item;
    QList<NyanMenuItemData> children;
    bool enabled = true;
    bool visible = true;
    bool checkable = false;
    bool checked = false;
    bool danger = false;
    QPointer<QAction> action;

    static auto ActionItem(const QString& key,
                           const QString& text,
                           const QKeySequence& shortcut = {},
                           bool danger = false) -> NyanMenuItemData;
    static auto SubMenu(const QString& key,
                        const QString& text,
                        const QList<NyanMenuItemData>& children) -> NyanMenuItemData;
    static auto Group(const QString& text) -> NyanMenuItemData;
    static auto Divider() -> NyanMenuItemData;
    static auto FromAction(QAction* action) -> NyanMenuItemData;
};
```

### API 设计

| API | 设计 |
| --- | --- |
| `NyanMenuBar::SetItems(...)` | 一次性设置顶层菜单数据。 |
| `NyanMenuBar::AddMenu(...)` | 添加顶层 submenu 数据。 |
| `NyanMenuBar::Clear()` | 清空所有顶层菜单。 |
| `NyanMenu::SetItems(...)` | 设置当前 popup 展示的菜单项数据。 |
| `NyanMenu::PopupAt(...)` | 在全局位置弹出。 |
| `NyanMenu::PopupBelow(...)` | 菜单栏下方弹出。 |
| `NyanMenu::PopupBeside(...)` | 子菜单侧边弹出。 |
| `NyanMenu::CloseAll()` | 关闭当前菜单和所有子菜单。 |
| `NyanMenu::AddItem(...)` 等兼容 API | 保留为适配层，内部构造 `NyanMenuItemData`，不再创建 item QWidget。 |

## 信号槽 / Action / 命令绑定设计

| 信号 | 触发时机 |
| --- | --- |
| `NyanMenuBar::ItemsChanged()` | 顶层 items 改变。 |
| `NyanMenuBar::ItemTriggered(QString key)` | 顶层或下拉普通项触发后转发。 |
| `NyanMenuBar::ActionTriggered(QAction*)` | 绑定 QAction 的菜单项触发后转发。 |
| `NyanMenu::ItemTriggered(QString key)` | 当前 popup 内普通项触发。 |
| `NyanMenu::ActionTriggered(QAction*)` | 当前 popup 内 QAction 触发。 |
| `NyanMenu::SubmenuRequested(QString key, QRect itemRect)` | hover 或键盘打开子菜单。 |
| `NyanMenu::CloseRequested()` | Esc 或普通项触发后请求关闭整个菜单链。 |

QAction 规则：

1. `FromAction()` 读取 text、icon、shortcut、enabled、visible、checkable、checked。
2. `QAction::menu()` 若存在，可作为后续递归适配来源；当前设计保留接口，不强制实现 Qt `QMenu` 全量转换。
3. 点击绑定 QAction 的 item 时，先判断 enabled，再调用 `action->trigger()`，然后发出 `ActionTriggered(action)` 和 `ItemTriggered(key)`。
4. 用 `QPointer<QAction>` 避免悬空；每次绘制前可同步 QAction 状态。

## 与 NyanMainTitleBar 的集成关系

| 边界 | 设计 |
| --- | --- |
| 所属区域 | `NyanMenuBar` 仍由 `NyanMainTitleBar` 持有，位于上层左侧。 |
| 高度 | 与标题栏上层 `controlHeightSM` 对齐。 |
| 背景 | 默认使用 `colorBgContainer`，不使用 `colorPrimaryBg`，避免菜单栏和标题栏割裂。 |
| 拖拽 | 标题栏拖拽命中必须排除菜单栏区域。 |
| 标题居中 | 标题栏计算左右占位时继续把菜单栏宽度纳入。 |
| UiNode | `MainTitleBarNode` 继续绑定已有 `NyanMenuBar`，`MenuBarNode/MenuNode/MenuItemNode` 以数据模型作为菜单配置来源。 |

## 实现代码

以下代码展示关键接口和关键实现组织。

### NyanMenuTypes.h

```c++
#pragma once

#include <Matcha/Core/Macros.h>

#include <QIcon>
#include <QKeySequence>
#include <QList>
#include <QPointer>

class QAction;

namespace matcha::gui {

enum class NyanMenuItemType {
    Item,
    SubMenu,
    Group,
    Divider
};

enum class NyanPopupPlacement {
    Below,
    Context,
    Right,
    Left
};

struct MATCHA_EXPORT NyanMenuItemData {
    QString key;
    QString text;
    QIcon icon;
    QKeySequence shortcut;
    QString extraText;
    NyanMenuItemType type = NyanMenuItemType::Item;
    QList<NyanMenuItemData> children;
    bool enabled = true;
    bool visible = true;
    bool checkable = false;
    bool checked = false;
    bool danger = false;
    QPointer<QAction> action;

    [[nodiscard]] static auto ActionItem(
        const QString& key,
        const QString& text,
        const QKeySequence& shortcut = {},
        bool danger = false) -> NyanMenuItemData;

    [[nodiscard]] static auto SubMenu(
        const QString& key,
        const QString& text,
        const QList<NyanMenuItemData>& children) -> NyanMenuItemData;

    [[nodiscard]] static auto Group(const QString& text) -> NyanMenuItemData;
    [[nodiscard]] static auto Divider() -> NyanMenuItemData;
    [[nodiscard]] static auto FromAction(QAction* action) -> NyanMenuItemData;
};

} // namespace matcha::gui
```

### NyanMenuBar.h

```c++
#pragma once

#include <Matcha/Theming/ThemeAware.h>
#include <Matcha/Widgets/Menu/NyanMenuTypes.h>

#include <QElapsedTimer>
#include <QPointer>
#include <QWidget>

namespace matcha::gui {

class NyanMenu;

class MATCHA_EXPORT NyanMenuBar : public QWidget, public ThemeAware {
    Q_OBJECT

public:
    explicit NyanMenuBar(QWidget* parent = nullptr);
    ~NyanMenuBar() override;

    void SetItems(const QList<NyanMenuItemData>& items);
    [[nodiscard]] auto Items() const -> QList<NyanMenuItemData>;
    void AddMenu(const NyanMenuItemData& menu);
    void AddMenu(const QString& key, const QString& text, const QList<NyanMenuItemData>& children);
    void Clear();

    // 兼容接口：内部只创建数据，不再创建顶层 QPushButton。
    auto AddMenu(const QString& title) -> NyanMenu*;
    [[nodiscard]] auto MenuCount() const -> int;

    [[nodiscard]] auto sizeHint() const -> QSize override;
    [[nodiscard]] auto minimumSizeHint() const -> QSize override;

Q_SIGNALS:
    void ItemsChanged();
    void ItemTriggered(const QString& key);
    void ActionTriggered(QAction* action);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    auto eventFilter(QObject* watched, QEvent* event) -> bool override;
    void OnThemeChanged() override;

private:
    [[nodiscard]] auto BarHeight() const -> int;
    [[nodiscard]] auto IndexAt(const QPoint& pos) const -> int;
    [[nodiscard]] auto FirstActivatableIndex() const -> int;
    [[nodiscard]] auto NextActivatableIndex(int from, int delta) const -> int;
    [[nodiscard]] auto CanActivate(const NyanMenuItemData& item) const -> bool;

    void RebuildRects();
    void OpenIndex(int index);
    void ClosePopup();
    void PaintItem(QPainter& painter, int index);
    void SyncActionStates();

    QList<NyanMenuItemData> _items;
    QList<QRect> _itemRects;
    QPointer<NyanMenu> _popup;
    int _hoveredIndex = -1;
    int _activeIndex = -1;
    bool _popupOpen = false;
    bool _altPressedAlone = false;
    QElapsedTimer _dismissTimer;
    int _dismissedIndex = -1;
};

} // namespace matcha::gui
```

### NyanMenu.h

```c++
#pragma once

#include <Matcha/Theming/ThemeAware.h>
#include <Matcha/Widgets/Menu/NyanMenuTypes.h>

#include <QPointer>
#include <QWidget>

class QParallelAnimationGroup;
class QTimer;

namespace matcha::gui {

class MATCHA_EXPORT NyanMenu : public QWidget, public ThemeAware {
    Q_OBJECT

public:
    explicit NyanMenu(QWidget* parent = nullptr);
    ~NyanMenu() override;

    void SetItems(const QList<NyanMenuItemData>& items);
    [[nodiscard]] auto Items() const -> QList<NyanMenuItemData>;
    void AddItem(const NyanMenuItemData& item);
    void Clear();

    void PopupAt(const QPoint& globalPos, NyanPopupPlacement placement = NyanPopupPlacement::Context);
    void PopupBelow(const QPoint& globalPos, int anchorWidth = 0);
    void PopupBeside(const QPoint& globalPos, int anchorHeight = 0);
    void CloseAll();

    [[nodiscard]] auto IsOpen() const -> bool;
    [[nodiscard]] auto sizeHint() const -> QSize override;
    [[nodiscard]] auto minimumSizeHint() const -> QSize override;

Q_SIGNALS:
    void ItemsChanged();
    void ItemTriggered(const QString& key);
    void ActionTriggered(QAction* action);
    void SubmenuRequested(const QString& key, const QRect& itemRect);
    void CloseRequested();

protected:
    auto event(QEvent* event) -> bool override;
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void OnThemeChanged() override;

private:
    [[nodiscard]] auto PopupSize() const -> QSize;
    [[nodiscard]] auto ContentRect() const -> QRect;
    [[nodiscard]] auto AvailableGeometry(const QPoint& pos) const -> QRect;
    [[nodiscard]] auto AdjustedGeometry(const QPoint& globalPos, NyanPopupPlacement placement) const -> QRect;
    [[nodiscard]] auto CollapsedGeometry(const QRect& target, const QPoint& globalPos, NyanPopupPlacement placement) const -> QRect;
    [[nodiscard]] auto IndexAt(const QPoint& pos) const -> int;
    [[nodiscard]] auto CanActivate(const NyanMenuItemData& item) const -> bool;

    void StartPopupAnimation(const QRect& target, const QPoint& globalPos, NyanPopupPlacement placement);
    void StopPopupAnimation();
    void RebuildRects();
    void PaintPopupFrame(QPainter& painter);
    void PaintItem(QPainter& painter, const NyanMenuItemData& item, const QRect& rect, int index);
    void TriggerItem(int index);
    void OpenSubmenu(int index);
    void CloseChildPopup();
    void MoveActive(int delta);
    void SyncActionStates();

    QList<NyanMenuItemData> _items;
    QList<QRect> _itemRects;
    QPointer<NyanMenu> _childPopup;
    QPointer<QParallelAnimationGroup> _popupAnimation;
    QTimer* _submenuTimer = nullptr;
    int _hoveredIndex = -1;
    int _pressedIndex = -1;
    int _activeIndex = -1;
    int _scrollOffset = 0;
    int _pendingSubmenuIndex = -1;
};

} // namespace matcha::gui
```

### NyanMenu.cpp 关键实现草案

```c++
auto NyanMenu::AdjustedGeometry(const QPoint& globalPos, NyanPopupPlacement placement) const -> QRect
{
    const QSize popupSize = PopupSize();
    QRect target(globalPos, popupSize);

    constexpr int kPopupOffset = 2; // Light.json 中没有 motion/popup offset token，使用实现常量。
    if (placement == NyanPopupPlacement::Below) {
        target.moveTop(globalPos.y() + kPopupOffset);
    } else if (placement == NyanPopupPlacement::Right) {
        target.moveLeft(globalPos.x() + kPopupOffset);
    } else if (placement == NyanPopupPlacement::Left) {
        target.moveRight(globalPos.x() - kPopupOffset);
    }

    const QRect available = AvailableGeometry(globalPos);
    if (target.right() > available.right()) {
        if (placement == NyanPopupPlacement::Right) {
            target.moveRight(globalPos.x() - kPopupOffset);
        } else {
            target.moveRight(available.right());
        }
    }
    if (target.bottom() > available.bottom()) {
        target.moveBottom(available.bottom());
    }
    if (target.left() < available.left()) {
        target.moveLeft(available.left());
    }
    if (target.top() < available.top()) {
        target.moveTop(available.top());
    }
    return target;
}

auto NyanMenu::CollapsedGeometry(const QRect& target,
                                 const QPoint& globalPos,
                                 NyanPopupPlacement placement) const -> QRect
{
    QRect start = target;
    constexpr int kMinimumExtent = 1;

    if (placement == NyanPopupPlacement::Right || placement == NyanPopupPlacement::Left) {
        start.setWidth(kMinimumExtent);
        const bool opensLeft = placement == NyanPopupPlacement::Left || target.left() < globalPos.x();
        if (opensLeft) {
            start.moveRight(target.right());
        }
        return start;
    }

    start.setHeight(kMinimumExtent);
    const bool opensUp = target.top() < globalPos.y();
    if (opensUp) {
        start.moveBottom(target.bottom());
    }
    return start;
}

void NyanMenu::StartPopupAnimation(const QRect& target,
                                   const QPoint& globalPos,
                                   NyanPopupPlacement placement)
{
    StopPopupAnimation();

    constexpr int kMotionDurationMs = 120; // 不写入 Light.json，不伪造 token。
    const QRect start = CollapsedGeometry(target, globalPos, placement);

    setWindowOpacity(0.0);
    setGeometry(start);

    auto* group = new QParallelAnimationGroup(this);
    auto* geometryAnimation = new QPropertyAnimation(this, "geometry", group);
    geometryAnimation->setDuration(kMotionDurationMs);
    geometryAnimation->setEasingCurve(QEasingCurve::OutCubic);
    geometryAnimation->setStartValue(start);
    geometryAnimation->setEndValue(target);

    auto* opacityAnimation = new QPropertyAnimation(this, "windowOpacity", group);
    opacityAnimation->setDuration(kMotionDurationMs);
    opacityAnimation->setEasingCurve(QEasingCurve::OutCubic);
    opacityAnimation->setStartValue(0.0);
    opacityAnimation->setEndValue(1.0);

    connect(group, &QParallelAnimationGroup::finished, this, [this, target]() {
        setGeometry(target);
        setWindowOpacity(1.0);
        _popupAnimation.clear();
    });

    _popupAnimation = group;
    group->start(QAbstractAnimation::DeleteWhenStopped);
}
```

```c++
namespace {

[[nodiscard]] auto displayTextOf(const QString& rawText) -> QString
{
    return MnemonicState::Parse(rawText).displayText;
}

} // namespace

auto NyanMenu::PopupSize() const -> QSize
{
    QFont resolvedFont = font();
    if (const auto spec = Theme().Font("fontSM")) {
        resolvedFont.setPointSize(spec->sizeInPt);
    } else {
        resolvedFont.setPixelSize(12);
    }

    const QFontMetrics metrics(resolvedFont);
    const int itemPadding = Theme().DimensionPx("spaceSM").value_or(12);
    const int iconSize = Theme().DimensionPx("iconSizeSM").value_or(16);
    const int iconGap = Theme().DimensionPx("spaceXS").value_or(8);
    const int trailingGap = Theme().DimensionPx("spaceLG").value_or(24);
    int width = kPopupMinWidth;
    int height = Theme().DimensionPx("spaceXXS").value_or(4) * 2;

    for (const auto& runtime : _items) {
        const auto& item = runtime.data;
        if (!item.visible) {
            continue;
        }

        height += ItemHeight(item);

        // 宽度按最终显示文本测量，不把助记符 & 计入菜单项正文。
        const QString displayText = displayTextOf(item.text);
        const QString trailing = !item.extraText.isEmpty()
            ? item.extraText
            : item.shortcut.toString(QKeySequence::NativeText);
        const bool hasSubmenu = item.type == NyanMenuItemType::SubMenu || !item.children.isEmpty();
        const int submenuTrailingWidth = hasSubmenu ? Theme().DimensionPx("iconSizeXS").value_or(12) : 0;
        const int shortcutTrailingWidth = trailing.isEmpty() ? 0 : metrics.horizontalAdvance(trailing);
        const int trailingWidth = std::max(submenuTrailingWidth, shortcutTrailingWidth);
        const int rowWidth = itemPadding * 2
            + iconSize + iconGap
            + metrics.horizontalAdvance(displayText)
            + (trailingWidth > 0 ? trailingGap + trailingWidth : 0);
        width = std::max(width, rowWidth);
    }

    height = std::min(height, kPopupMaxHeight);
    return {width + kPopupShadowMargin * 2, height + kPopupShadowMargin * 2};
}

void NyanMenu::PaintItem(QPainter& painter,
                         const NyanMenuItemData& item,
                         const QRect& rect,
                         int index)
{
    if (!rect.intersects(this->rect())) {
        return;
    }

    const int itemPadding = Theme().DimensionPx("spaceSM").value_or(12);
    const int iconSize = Theme().DimensionPx("iconSizeSM").value_or(16);
    const int iconGap = Theme().DimensionPx("spaceXS").value_or(8);
    const int radius = Theme().DimensionPx("radiusDefault").value_or(3);
    const QColor divider = Theme().Color("colorDivider").value_or(QColor("#E3E3E6"));
    const QColor text = Theme().Color("colorText").value_or(QColor("#E0000000"));
    const QColor secondaryText = Theme().Color("colorTextSecondary").value_or(QColor("#A6000000"));
    const QColor disabledText = Theme().Color("colorTextTertiary").value_or(QColor("#66000000"));
    const QColor hoverBg = Theme().Color("colorFillHover").value_or(QColor("#0D000000"));
    const QColor pressedBg = Theme().Color("colorFillSecondaryHover").value_or(QColor("#1A000000"));
    const QColor selected = Theme().Color("colorPrimary").value_or(QColor("#0066FF"));
    const QColor selectedBg = Theme().Color("colorPrimaryBg").value_or(QColor("#F0F7FF"));
    const QColor danger = Theme().Color("colorError").value_or(QColor("#CC1423"));

    if (item.type == NyanMenuItemType::Divider) {
        const int y = rect.center().y();
        painter.setPen(QPen(divider, Theme().DimensionPx("lineWidthMS").value_or(1)));
        painter.drawLine(rect.left() + itemPadding, y, rect.right() - itemPadding, y);
        return;
    }

    const bool hovered = index == _hoveredIndex;
    const bool pressed = index == _pressedIndex;
    const bool active = index == _activeIndex;
    const bool interactive = CanActivate(item);

    if (interactive && (hovered || pressed || active)) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(pressed ? pressedBg : (active ? selectedBg : hoverBg));
        painter.drawRoundedRect(rect.adjusted(1, 1, -1, -1), radius, radius);
    }

    if (item.type == NyanMenuItemType::Group) {
        painter.setPen(secondaryText);
        painter.drawText(rect.adjusted(itemPadding, 0, -itemPadding, 0),
                         Qt::AlignVCenter | Qt::AlignLeft,
                         displayTextOf(item.text));
        return;
    }

    const QColor contentColor = !item.enabled ? disabledText
        : item.danger ? danger
        : active ? selected
        : text;

    int left = rect.left() + itemPadding;
    const int centerY = rect.center().y();
    const QRect iconRect(left, centerY - iconSize / 2, iconSize, iconSize);

    if (item.checkable) {
        painter.setPen(QPen(contentColor, 2));
        if (item.checked) {
            painter.drawLine(iconRect.left() + 3, iconRect.center().y(),
                             iconRect.left() + 7, iconRect.bottom() - 4);
            painter.drawLine(iconRect.left() + 7, iconRect.bottom() - 4,
                             iconRect.right() - 2, iconRect.top() + 4);
        }
    } else if (!item.icon.isNull()) {
        const QIcon::Mode mode = item.enabled ? QIcon::Normal : QIcon::Disabled;
        painter.drawPixmap(iconRect, item.icon.pixmap(iconRect.size(), mode));
    }
    left += iconSize + iconGap;

    const int right = rect.right() - itemPadding;
    const int trailingGap = Theme().DimensionPx("spaceLG").value_or(24);
    const int arrowSize = Theme().DimensionPx("iconSizeXS").value_or(12);
    const bool hasSubmenu = item.type == NyanMenuItemType::SubMenu || !item.children.isEmpty();
    const QString shortcut = !item.extraText.isEmpty()
        ? item.extraText
        : item.shortcut.toString(QKeySequence::NativeText);
    const int trailingWidth = hasSubmenu
        ? arrowSize
        : (shortcut.isEmpty() ? 0 : painter.fontMetrics().horizontalAdvance(shortcut));
    const int textRight = right - (trailingWidth > 0 ? trailingGap + trailingWidth : 0);
    const QRect textRect(left, rect.top(), std::max(0, textRight - left), rect.height());
    const QString displayText = displayTextOf(item.text);

    painter.setPen(contentColor);
    painter.drawText(textRect,
                     Qt::AlignVCenter | Qt::AlignLeft,
                     painter.fontMetrics().elidedText(displayText, Qt::ElideRight, textRect.width()));

    if (hasSubmenu) {
        const QRect arrowRect(right - arrowSize + 1, centerY - arrowSize / 2, arrowSize, arrowSize);
        painter.setPen(QPen(contentColor, 1.5));
        painter.drawLine(arrowRect.left() + 3, arrowRect.top() + 2,
                         arrowRect.right() - 3, arrowRect.center().y());
        painter.drawLine(arrowRect.right() - 3, arrowRect.center().y(),
                         arrowRect.left() + 3, arrowRect.bottom() - 2);
    } else {
        if (!shortcut.isEmpty()) {
            painter.setPen(item.enabled ? secondaryText : disabledText);
            painter.drawText(QRect(right - trailingWidth, rect.top(), trailingWidth, rect.height()),
                             Qt::AlignVCenter | Qt::AlignRight,
                             shortcut);
        }
    }
}
```

## UiNode 集成设计

| 节点 | 集成方式 |
| --- | --- |
| `MenuBarNode` | 作为 `NyanMenuBar` 的 UiNode 包装，向控件写入顶层 `NyanMenuItemData::SubMenu()`。 |
| `MenuNode` | 维护 `QList<NyanMenuItemData>` 子树，作为弹出菜单的数据来源。 |
| `MenuItemNode` | 保存 item key/text/enabled 等数据，并通知父 `MenuNode` 重建菜单数据。 |
| `ContextMenuComposer` | 可提供转换函数，把 `fw::ContextMenuItem` 转为 `gui::NyanMenuItemData`。 |

`AddMenu(QString)`、`AddItem(QString)` 等兼容 API 保留为数据模型适配入口，内部不再创建 `NyanMenuItem` / `NyanMenuSeparator` QWidget。

## 验证清单

| 验证项 | 预期 |
| --- | --- |
| 范围约束 | 设计范围限定在 `NyanMenuBar`、`NyanMenu`、菜单项数据模型和相关 UiNode 集成，不扩展到非菜单控件。 |
| 组件边界 | 明确 `NyanMenuBar` 负责顶层横向菜单，`NyanMenu` 负责 popup 外壳和弹出菜单项，`NyanMenuItemData` 负责菜单数据。 |
| 绘制方式 | 顶层菜单栏和下拉菜单项均为集中手绘，不依赖 QPushButton 或每项 QWidget。 |
| popup 动画 | 文档包含 `AdjustedGeometry`、`CollapsedGeometry`、`StartPopupAnimation` 的 geometry + opacity 设计。 |
| 分隔线表达 | 分隔线由 `NyanMenuItemType::Divider` 表达。 |
| 勾选项表达 | 勾选项由 `checkable / checked` 字段表达。 |
| Token key | 所有字符串 Token key 均来自 `Light.json`。 |
| QAction | 支持 QAction 状态同步和 trigger 回传。 |
| keyboard | 覆盖 Alt/F10、方向键、Enter/Space、Esc。 |
| NyanMainTitleBar | 明确菜单栏不接管标题栏拖拽和窗口按钮。 |
| 弹出菜单宽度 | 下拉菜单宽度按助记符解析后的显示文本、真实快捷键宽度和子菜单箭头宽度计算，避免 `&...`、`O...` 这类非必要省略。 |

## 后续注意事项

| 项 | 说明 |
| --- | --- |
| 保持数据化入口 | 后续新增菜单能力应优先扩展 `NyanMenuItemData`，再由 `NyanMenuBar` 和 `NyanMenu` 消费数据。 |
| 保持集中绘制 | 弹出菜单项状态和视觉应继续由 `NyanMenu::PaintItem()` 管理，避免重新引入每项 QWidget 绘制路径。 |
| 谨慎处理兼容 API | `AddMenu(QString)`、`AddItem(QString)` 等兼容 API 应保持为数据模型适配入口。 |
| 不新增假 Token | popup offset、动画时长、最小宽度如果没有 Light.json key，先用 constexpr；后续若主题系统正式增加 motion token，再统一迁移。 |
| NyanPopupMenu 是否拆出 | 当前不新增 public `NyanPopupMenu`。如果 `NyanMenu` 后续实现过大，可把 popup 外壳拆成 private helper，最后再决定是否公开。 |
| 与上下文菜单统一 | `ContextMenuComposer::ContextMenuItem` 已有无 Qt 数据模型，后续应提供转换到 `NyanMenuItemData` 的桥接函数。 |
| 测试优先级 | 先验证菜单栏打开、hover 切换、popup 动画、普通 item 触发、disabled 不触发、divider/group 跳过、子菜单定位，再验证 QAction 和长菜单滚动。 |
