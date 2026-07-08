# NyanMainTitleBar控件详细设计

## 功能

本设计用于指导 `NyanMainTitleBar` 控件重建。`NyanMainTitleBar` 是 Matcha 原生主窗口标题栏控件：构造时绑定宿主窗口，标题栏采用上下两层结构，上层承载返回主页区域、菜单、图片标题和窗口按钮，下层承载主题色区域或自定义客户区，并通过宿主窗口事件完成移动、缩放和窗口状态同步。

`NyanMainTitleBar` 只负责标题栏自身能力，不接管父窗体背景、阴影、透明背景和父窗体边距。父窗体是否无边框、是否有阴影、是否预留外边距，由窗口壳或 `WindowNode` 一类装配层统一决定。

### 功能表

| 名称 | 说明 |
| --- | --- |
| 强制父窗口 | 构造函数必须传入 `QWidget* parent`，`parent == nullptr` 直接断言或运行时失败。标题栏后续通过该父窗口执行移动、缩放、最小化、最大化和关闭。 |
| 上下两层标题栏 | 上层是白色标题区，放置返回区域、菜单、图片标题和窗口按钮；下层是主题色区域，默认显示主题色背景，也允许替换为自定义客户区。 |
| 只绘制自身 | `paintEvent()` 只绘制 `NyanMainTitleBar` 自己的上下两层背景和分割线，不绘制父窗体背景、阴影或圆角。 |
| 背景单一来源 | 不再通过 `_upperBar` / `_defaultLowerBar` 或样式表重复绘制上下背景；这些视觉容器已移除，避免与 `paintEvent()` 形成两套背景来源。 |
| 直接读取 Token | 不定义标题栏高度、按钮宽度、按钮高度、图标尺寸、resize 命中宽度等 Token 缓存成员。需要值时直接调用 `Theme().Color(key)`、`Theme().DimensionPx(key)`。 |
| Token 来源受控 | 字符串 Token key 必须来自 `Matcha\Resources\Themes\Light.json`，不得在设计和代码中写入不存在的标题栏专用 key。 |
| 返回区域手绘 | 不创建真实返回按钮控件，而是在标题栏左侧保留 `RouteBackRect()` 区域，由 `paintEvent()` 手绘背景和图标。 |
| 返回区域状态 | 提供可用、图标、形态控制：`SetRouteBackEnabled()`、`SetRouteBackIcon()`、`SetRouteBackButtonShape()`。返回区域始终存在，默认可用、圆形。 |
| 返回区域事件 | 鼠标按下命中返回区域时进入 pressed 状态；移动时刷新 hover 状态；释放时仍命中才发出 `RouteBackRequested()`。是否切换页面由业务层处理。 |
| 保留菜单入口 | 上层左侧继续创建 `NyanMenuBar`，通过 `MenuBar()` 返回，菜单内容后续再单独调整。 |
| 图片标题 | 不提供字符串标题接口，不执行文字标题绘制。标题只接收一张 `QPixmap` 图片，并在上层标题区按比例居中绘制。 |
| 自定义客户区 | 提供 `SetCustomCentral(QWidget* widget, bool autoDelete = false)`。传入非空 widget 时替换下层默认主题色区域；传入 `nullptr` 时恢复默认下层背景。自定义客户区挂入后需要安装事件过滤器，移除时同步卸载。 |
| 窗口按钮 | 上层右侧创建最小化、最大化/还原、关闭按钮，按钮点击时先操作父窗口，再发出原有信号。 |
| 事件过滤器 | 安装自身、父窗口以及 `qApp` 应用级事件过滤器，监听 `WindowStateChange`、父窗口内所有 QWidget 的鼠标移动、鼠标按下、鼠标释放等事件，用于状态同步、返回区域状态、移动窗口和拖拽 resize。自定义下层客户区仍单独安装事件过滤器，便于挂载和移除时保持生命周期对称。 |
| 标题栏拖动 | 在上层空白拖拽区按下并移动时，优先使用 `QWindow::startSystemMove()`；不可用时使用 QWidget geometry 兜底移动。返回区域、菜单、窗口按钮、自定义下层客户区不作为拖拽区域。 |
| 边缘缩放 | 鼠标位于父窗口边缘时命中 resize 区域，优先使用 `QWindow::startSystemResize()`；不可用时手动调整父窗口 geometry。命中判断基于全局鼠标坐标换算到父窗口，不依赖父窗口边缘是否裸露，因此 central widget 填满窗口时仍可拖拽缩放。 |
| 双击最大化 | 双击上层空白拖拽区时切换父窗口最大化/还原。 |
| 状态同步 | 父窗口最大化/还原后刷新最大化按钮图标。 |
| 不依赖 `MainTitleBarNode` | 标题栏能力沉到 `NyanMainTitleBar` 自身，`MainTitleBarNode` 只承担装配职责；后续即使移除 Node 也不影响控件职责。 |

## Token取值设计

### 可使用的真实 Token

以下 key 均来自 `D:\CodeData\Matcha\Matcha\Resources\Themes\Light.json`。

| 用途 | Token key | 分类 | 当前值 |
| --- | --- | --- | --- |
| 上层背景 | `colorBgContainer` | `colors` | `#FFFFFFFF` |
| 下层背景 | `colorPrimaryNav` | `colors` | `#0059B3` |
| 图标/返回按钮 | `colorText` | `colors` | `#E0000000` |
| 返回图标 | `colorText` | `colors` | `#E0000000` |
| 返回区域 hover 背景 | `colorBgContainer` | `colors` | `#FFFFFFFF` |
| 普通按钮 hover | `colorFillHover` | `colors` | `#0D000000` |
| 普通按钮 pressed | `colorFillSecondaryHover` | `colors` | `#1A000000` |
| 关闭按钮 hover | `colorErrorHover` | `colors` | `#D93840` |
| 关闭按钮 pressed | `colorErrorActive` | `colors` | `#A6081B` |
| 底部分割线 | `colorDivider` | `colors` | `#E3E3E6` |
| 上层高度 | `controlHeightSM` | `controlHeight` | `24` |
| 下层高度 | `controlHeightXL` | `controlHeight` | `40` |
| 返回区域收起宽度/高度 | `controlHeightSM` | `controlHeight` | `24` |
| 返回区域展开宽度/高度 | `controlHeightSM + controlHeightXL` | `controlHeight` | `64` |
| 返回区域左偏移 | `spaceXS` | `space` | `8` |
| 窗口按钮宽度 | `containerWidthLG` | `containerWidth` | `36` |
| 图标尺寸 | `iconSizeSM` | `iconSize` | `16` |
| resize 命中宽度 | `spaceXS` | `space` | `8` |
| 上层左内边距 | `spaceXXS` | `space` | `4` |

禁止在设计或代码草案中使用未在 `Light.json` 出现的 key。标题栏按钮宽度、resize 命中宽度、文字色、按钮 hover 色都必须从上表已有 key 中选择；控件不新增标题栏专用 Token 中转结构。

`NyanMainTitleBar` 不新增标题栏专用 Token 中转结构，直接使用 Matcha 当前主题文件中已经存在的 Token。控件内部取值规则如下：

| 控件语义 | Matcha 取值方式 | 说明 |
| --- | --- | --- |
| 上层标题区高度 | `controlHeightSM` | 固定作为上层标题区高度。 |
| 下层主题区高度 | `controlHeightXL` | 固定作为下层主题区高度。 |
| 返回区域高度/宽度 | `controlHeightSM + controlHeightXL` | 当前标题栏固定存在上下两层，返回区域默认跨越上下两层。 |
| 返回区域左偏移 | `spaceXS` | 为左上角 resize 命中保留 8px 空间。 |
| 返回按钮默认背景 | `colorPrimaryNav` | 返回按钮本体默认背景，与下层主题色保持一致；不用于填充整个返回区域。 |
| 返回按钮 hover 背景 | `colorBgContainer` 并设置透明度 | hover 时使用现有容器背景色加透明度，不新增专用背景 Token。 |
| 返回图标颜色 | `colorText` | 返回主页图标正常色。 |
| 禁用态图标颜色 | `colorText` | 禁用态不单独切换图标颜色，只影响交互和按钮本体绘制。 |
| 返回图标绘制尺寸 | `iconSizeSM * 2` | 返回主页图标比窗口按钮图标更突出。 |

### 取值原则

不为 Token 值定义成员变量。尺寸、颜色都在使用点直接读取。输出样式表颜色时不额外定义颜色转换辅助函数，直接使用 `QColor::name(QColor::HexArgb)`：

```c++
const int upperHeight = Theme().DimensionPx("controlHeightSM").value_or(24);
const int lowerHeight = Theme().DimensionPx("controlHeightXL").value_or(40);
const int buttonWidth = Theme().DimensionPx("containerWidthLG").value_or(36);
const int iconSize = Theme().DimensionPx("iconSizeSM").value_or(16);
const int resizeWidth = Theme().DimensionPx("spaceXS").value_or(8);
const int routeCollapsedSize = upperHeight;
const int routeExpandedSize = upperHeight + lowerHeight;

const QColor upperBg = Theme().Color("colorBgContainer").value_or(QColor("#FFFFFFFF"));
const QColor lowerBg = Theme().Color("colorPrimaryNav").value_or(QColor("#0059B3"));
const QColor text = Theme().Color("colorText").value_or(QColor("#E0000000"));
const QString upperBgName = upperBg.name(QColor::HexArgb);
```

这样做的目标是：让尺寸、颜色和图标语义统一来自主题系统；主题切换时只需要重新 `ApplyTheme()` 和 `update()`，不维护一批可能过期的缓存字段。

## 资源图标取值设计

### 使用原则

`NyanMainTitleBar` 不直接依赖图片文件路径，也不硬编码 SVG / PNG 资源路径。默认图标统一通过 Matcha 主题系统解析：

```c++
Theme().ResolveIcon(fw::IconId(fw::icons::Home), fw::IconToken::iconSizeSM, iconColor)
```

这样做的目的是让图标资源来源、尺寸语义和颜色语义都收敛到当前主题系统中，避免控件内部直接拼接资源路径或硬编码图片文件。

### 图标资源清单

| 使用位置 | 默认资源枚举 | 解析方式 | 尺寸 Token | 颜色 Token | 说明 |
| --- | --- | --- | --- | --- | --- |
| 返回主页区域 | `fw::icons::Home` | `Theme().ResolveIcon(fw::IconId(fw::icons::Home), fw::IconToken::iconSizeSM, iconColor)` | `iconSizeSM`，绘制区域为 `iconSizeSM * 2` | `colorText` | 当前按“返回主页”语义使用 Matcha 已注册的 Home 图标；业务层可通过 `SetRouteBackIcon()` 覆盖。 |
| 最小化按钮 | `fw::icons::Minimize` | `Theme().ResolveIcon(fw::IconId(fw::icons::Minimize), fw::IconToken::iconSizeSM, iconColor)` | `iconSizeSM` | `colorText` | 在 `ApplyTheme()` 中设置到 `_minimizeButton`。 |
| 最大化按钮 | `fw::icons::Maximize` | `Theme().ResolveIcon(fw::IconId(fw::icons::Maximize), fw::IconToken::iconSizeSM, iconColor)` | `iconSizeSM` | `colorText` | 父窗口未最大化时使用。 |
| 还原按钮 | `fw::icons::Restore` | `Theme().ResolveIcon(fw::IconId(fw::icons::Restore), fw::IconToken::iconSizeSM, iconColor)` | `iconSizeSM` | `colorText` | 父窗口最大化后由 `UpdateMaximizeButton()` 切换使用。 |
| 关闭按钮 | `fw::icons::Close` | `Theme().ResolveIcon(fw::IconId(fw::icons::Close), fw::IconToken::iconSizeSM, iconColor)` | `iconSizeSM` | `colorText` | 按钮 hover / pressed 背景使用关闭按钮专用颜色，图标本身仍使用 `colorText`。 |

### 返回图标覆盖规则

返回主页区域默认使用 `fw::icons::Home`。当业务层调用：

```c++
SetRouteBackIcon(const QIcon& icon)
```

并传入非空 `QIcon` 时，`PaintRouteBackButton()` 直接绘制业务层传入的图标，不再通过 `Theme().ResolveIcon()` 重新着色。传入空 `QIcon` 时恢复默认 `fw::icons::Home` 解析逻辑。

### 实现约束

| 约束 | 说明 |
| --- | --- |
| 不新增资源路径常量 | 控件内部不保存 `:/...` 形式的资源路径。 |
| 不缓存图标 Token 值 | 图标尺寸和颜色在 `ApplyTheme()`、`UpdateMaximizeButton()`、`PaintRouteBackButton()` 使用点直接读取。 |
| 返回区域不创建真实按钮 | 返回区域图标由 `PaintRouteBackButton()` 手绘，不能改回 `_homeButton` 或 `_routeBackButton` 子控件。 |
| 最大化图标必须动态切换 | `WindowStateChange` 后调用 `UpdateMaximizeButton()`，根据父窗口状态在 `Maximize` / `Restore` 之间切换。 |
| 自定义返回图标不强制重染色 | 外部传入的 `QIcon` 视为业务层已经处理好的图标资源，控件只负责绘制。 |

## 绘制和布局设计

### 双层结构

`NyanMainTitleBar` 总高度固定由两个真实 Token 相加：

```text
controlHeightSM + controlHeightXL = 24 + 40 = 64
```

| 区域 | 坐标 | 背景 |
| --- | --- | --- |
| 上层标题区 | `QRect(0, 0, width(), upperHeight)` | `colorBgContainer` |
| 下层主题区 | `QRect(0, upperHeight, width(), lowerHeight)` | `colorPrimaryNav` |
| 底部分割线 | `QRect(0, height() - 1, width(), 1)` | `colorDivider` |

整体布局由 `NyanMainTitleBar` 自身的 `QGridLayout` 管理：返回区域不作为 `QToolButton` 子控件，也不再创建 `_upperBar` / `_defaultLowerBar` 视觉容器；`QGridLayout` 直接安装在 `NyanMainTitleBar` 自身，第 0 列预留返回区域，菜单和窗口按钮直接挂在上层行。上下背景只由 `paintEvent()` 统一绘制。

```text
RouteBack paint area | NyanMenuBar | title paint area / stretch | Min | Max/Restore | Close
```

返回区域可见时：

| 状态 | 返回区域矩形 |
| --- | --- |
| 当前固定双层 | `QRect(spaceXS, 0, controlHeightSM + controlHeightXL, controlHeightSM + controlHeightXL)` |
| 未来如果支持关闭下层 | 可退化为 `QRect(spaceXS, 0, controlHeightSM, controlHeightSM)` |

返回区域始终存在，左侧始终预留 `spaceXS + 返回区域宽度`，避免主页入口出现/消失时影响菜单和标题定位，同时让左上角 `spaceXS` 宽度保留给窗口 resize 命中。

下层布局：

默认下层：

```text
不创建 defaultLowerBar 或其他占位 widget，由 paintEvent() 直接绘制 colorPrimaryNav 背景。
```

自定义客户区：

```text
customCentral，业务层通过 SetCustomCentral(widget) 放入下层整行区域。
```

`SetCustomCentral(nullptr)` 移除自定义客户区后，下层恢复为 `paintEvent()` 绘制的默认主题色区域。自定义客户区只影响标题栏下层，不改变父窗口布局、父窗口背景或父窗口边距。

### 返回区域绘制和交互设计

`NyanMainTitleBar` 的返回主页能力由 `RouteBackRect()` / `PaintRouteBackButton()` / `eventFilter()` 共同实现。返回区域只保存状态并手绘，不依赖 `QToolButton` 默认行为。

| 项 | 设计 |
| --- | --- |
| 控件形态 | 不创建 `_homeButton` / `_routeBackButton` 子控件，只保存状态并手绘。 |
| 可用状态 | `_routeBackEnabled` 默认 `true`；禁用后清空 hover/pressed，不响应点击。禁用且形态为 `Circle` 时仍绘制圆形按钮背景，但不绘制 hover 悬浮效果；禁用且形态为 `Rect` 时可不绘制按钮本体背景。图标仍按 `colorText` 语义绘制。 |
| 图标设置 | 提供 `SetRouteBackIcon(const QIcon& icon)`，业务层可覆盖默认图标；未设置时使用 `fw::icons::Home`。 |
| 背景形态 | `RouteBackButtonShape::Circle` 或 `RouteBackButtonShape::Rect`，默认 Circle。 |
| 默认背景 | `PaintRouteBackButton()` 不填充整个返回区域；返回区域底色由 `paintEvent()` 的上层/下层背景决定，按钮本体默认使用 `colorPrimaryNav`。 |
| hover 背景 | 使用 `colorBgContainer` 并设置透明度，形成半透明覆盖效果。 |
| pressed 状态 | pressed 只用于 release 判定和刷新；不因为 pressed 单独切换背景。 |
| 图标 | 默认使用 `fw::icons::Home` 表达返回主页语义；如果业务需要返回箭头或其他语义，通过 `SetRouteBackIcon()` 覆盖图标，控件仍只发出 `RouteBackRequested()`。 |
| 点击触发 | press 命中返回区域只记录状态；release 时仍命中才发出 `RouteBackRequested()`。 |
| 拖拽排除 | `RouteBackRect()` 命中区域不允许触发窗口移动或 resize。 |
| 标题居中预留 | 图片标题居中计算时，左侧预留宽度应包含 `RouteBackRect().right() + 1` 和菜单宽度，确保返回区域左偏移也计入布局占位。 |

返回区域相关辅助函数建议：

```c++
[[nodiscard]] auto RouteBackRect() const -> QRect;
void PaintRouteBackButton(QPainter* painter);
[[nodiscard]] auto IsRouteBackHit(const QPoint& pos) const -> bool;
```

窗口缩放相关辅助函数建议：

```c++
[[nodiscard]] auto ResizeEdgesAt(const QPoint& hostPos) const -> Qt::Edges;
[[nodiscard]] auto ResizeEdgesAtGlobalPos(const QPoint& globalPos) const -> Qt::Edges;
[[nodiscard]] auto IsHostRelatedWidget(QObject* watched) const -> bool;
void UpdateResizeCursor(Qt::Edges edges);
```

其中 `IsHostRelatedWidget()` 用于配合 `qApp->installEventFilter(this)` 过滤出属于宿主窗口的 QWidget。`ResizeEdgesAtGlobalPos()` 统一从全局鼠标坐标判断父窗口边缘命中，避免 central widget 或其他子控件覆盖父窗口后无法触发 resize。

### 绘制边界

`NyanMainTitleBar` 明确不做以下事情：

| 不做的事情 | 原因 |
| --- | --- |
| 不绘制父窗口背景 | 控件只负责自身，不侵入窗口壳绘制。 |
| 不绘制父窗口阴影 | 阴影属于窗口装饰层，不属于标题栏控件。 |
| 不设置父窗口透明背景 | 透明和合成策略应由窗口壳统一控制。 |
| 不修改父窗口 `contentsMargins()` | 避免控件创建时偷偷改变父窗口布局。 |
| 不负责 `MainTitleBarNode` 装配 | 后续可能没有该 Node，控件本身保持可独立挂载。 |

## 实现代码

以下代码是后续实现时建议采用的目标结构。它强调可落地的关键逻辑，不要求逐字照抄；真正编码时仍应结合当前文件已有接口和编译结果微调。

### NyanMainTitleBar.h

```c++
#pragma once

#include <Matcha/Core/Macros.h>
#include <Matcha/Theming/ThemeAware.h>

#include <QIcon>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QWidget>

class QGridLayout;
class QPainter;
class QToolButton;

namespace matcha::gui {

class NyanMenuBar;

enum class RouteBackButtonShape {
    Rect,
    Circle
};

class MATCHA_EXPORT NyanMainTitleBar : public QWidget, public ThemeAware {
    Q_OBJECT

public:

    const QColor hover = Theme().Color("colorFillHover").value_or(QColor("#0D000000"));
    const QColor pressed = Theme().Color("colorFillSecondaryHover").value_or(QColor("#1A000000"));
    const QColor closeHover = Theme().Color("colorErrorHover").value_or(QColor("#D93840"));
    const QColor closePressed = Theme().Color("colorErrorActive").value_or(QColor("#A6081B"));

    const QString normalStyle = QStringLiteral(
        "QToolButton{border:none;background:transparent;padding:0;}"
        "QToolButton:hover{background:%1;}"
        "QToolButton:pressed{background:%2;}")
        .arg(hover.name(QColor::HexArgb), pressed.name(QColor::HexArgb));
    const QString closeStyle = QStringLiteral(
        "QToolButton{border:none;background:transparent;padding:0;}"
        "QToolButton:hover{background:%1;}"
        "QToolButton:pressed{background:%2;}")
        .arg(closeHover.name(QColor::HexArgb), closePressed.name(QColor::HexArgb));

    _minimizeButton->setStyleSheet(normalStyle);
    _maximizeButton->setStyleSheet(normalStyle);
    _closeButton->setStyleSheet(closeStyle);

    for (auto* button : {_minimizeButton, _maximizeButton, _closeButton}) {
        button->setFixedSize(buttonWidth, upperHeight);
        button->setIconSize(QSize(iconSize, iconSize));
    }

    const QColor iconColor = Theme().Color("colorText").value_or(QColor("#E0000000"));
    _minimizeButton->setIcon(QIcon(Theme().ResolveIcon(fw::IconId(fw::icons::Minimize), fw::IconToken::iconSizeSM, iconColor)));
    _closeButton->setIcon(QIcon(Theme().ResolveIcon(fw::IconId(fw::icons::Close), fw::IconToken::iconSizeSM, iconColor)));
    UpdateMaximizeButton();
    update();
}

void NyanMainTitleBar::UpdateMaximizeButton()
{
    const QColor iconColor = Theme().Color("colorText").value_or(QColor("#E0000000"));
    const auto icon = _hostWindow->isMaximized() ? fw::icons::Restore : fw::icons::Maximize;
    _maximizeButton->setIcon(QIcon(Theme().ResolveIcon(fw::IconId(icon), fw::IconToken::iconSizeSM, iconColor)));
}

auto NyanMainTitleBar::RouteBackRect() const -> QRect
{
    const int upperHeight = Theme().DimensionPx("controlHeightSM").value_or(24);
    const int lowerHeight = Theme().DimensionPx("controlHeightXL").value_or(40);
    const int leftOffset = Theme().DimensionPx("spaceXS").value_or(8);
    const int size = upperHeight + lowerHeight;
    const int height = this->height();
    return QRect(leftOffset, 0, size, height);
}

auto NyanMainTitleBar::IsRouteBackHit(const QPoint& pos) const -> bool
{
    return _routeBackEnabled && RouteBackRect().contains(pos);
}

void NyanMainTitleBar::PaintRouteBackButton(QPainter* painter)
{
    if (painter == nullptr) {
        return;
    }

    const QRect routeRect = RouteBackRect();
    if (routeRect.isEmpty()) {
        return;
    }

    const int buttonSize = routeRect.width();
    QRect buttonRect(
        routeRect.left() + (routeRect.width() - buttonSize) / 2,
        routeRect.top() + (routeRect.height() - buttonSize) / 2,
        buttonSize,
        buttonSize);
    buttonRect.adjust(2, 2, -2, -2);

    if (_routeBackEnabled || _routeBackShape == RouteBackButtonShape::Circle) {
        painter->save();
        painter->setPen(Qt::NoPen);

        QColor background = Theme().Color("colorPrimaryNav").value_or(QColor("#0059B3"));
        if (_routeBackEnabled && _routeBackHovered) {
            background = Theme().Color("colorBgContainer").value_or(QColor("#FFFFFFFF"));
            background.setAlpha(150);
        }
        painter->setBrush(background);

        if (_routeBackShape == RouteBackButtonShape::Circle) {
            painter->drawEllipse(buttonRect);
        } else {
            painter->drawRect(buttonRect);
        }
        painter->restore();
    }

    painter->setRenderHints(QPainter::SmoothPixmapTransform | QPainter::Antialiasing);

    const QColor iconColor = Theme().Color("colorText").value_or(QColor("#E0000000"));
    const int iconSide = Theme().DimensionPx("iconSizeSM").value_or(16) * 2;
    const QRect iconRect(
        buttonRect.center().x() - iconSide / 2,
        buttonRect.center().y() - iconSide / 2,
        iconSide,
        iconSide);

    const QIcon icon = _routeBackIcon.isNull()
        ? QIcon(Theme().ResolveIcon(fw::IconId(fw::icons::Home), fw::IconToken::iconSizeSM, iconColor))
        : _routeBackIcon;
    painter->drawPixmap(iconRect, icon.pixmap(iconRect.size()));
}

void NyanMainTitleBar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int upperHeight = Theme().DimensionPx("controlHeightSM").value_or(24);
    const QColor upperBg = Theme().Color("colorBgContainer").value_or(QColor("#FFFFFFFF"));
    const QColor lowerBg = Theme().Color("colorPrimaryNav").value_or(QColor("#0059B3"));
    const QColor divider = Theme().Color("colorDivider").value_or(QColor("#E3E3E6"));

    // 只绘制标题栏自身，不绘制父窗口背景或阴影。
    painter.fillRect(QRect(0, 0, width(), upperHeight), upperBg);
    painter.fillRect(QRect(0, upperHeight, width(), height() - upperHeight), lowerBg);
    painter.fillRect(QRect(0, height() - 1, width(), 1), divider);
    PaintRouteBackButton(&painter);

    if (!_titleLogo.isNull()) {
        const int leftReserved = RouteBackRect().right() + 1
            + _menuBar->width();
        const int rightReserved = _minimizeButton->width()
            + _maximizeButton->width()
            + _closeButton->width();
        const int reserved = std::max(leftReserved, rightReserved);
        const QRect titleRect(reserved, 0, std::max(0, width() - reserved * 2), upperHeight);
        if (titleRect.width() > 0) {
            const int padding = Theme().DimensionPx("spaceXXS").value_or(4);
            const QSize maxLogoSize(titleRect.width(), std::max(0, upperHeight - padding * 2));
            const QPixmap logo = _titleLogo.scaled(maxLogoSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            const QPoint logoTopLeft(
                titleRect.x() + (titleRect.width() - logo.width()) / 2,
                titleRect.y() + (titleRect.height() - logo.height()) / 2);
            painter.drawPixmap(logoTopLeft, logo);
        }
    }
}

auto NyanMainTitleBar::eventFilter(QObject* watched, QEvent* event) -> bool
{
    if (event == nullptr || _hostWindow == nullptr) {
        return QWidget::eventFilter(watched, event);
    }

    if (watched == _hostWindow && event->type() == QEvent::WindowStateChange) {
        UpdateMaximizeButton();
        return false;
    }

    if (!IsHostRelatedWidget(watched)) {
        return QWidget::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() != Qt::LeftButton) {
            return false;
        }

        const QPoint globalPos = mouse->globalPosition().toPoint();
        if (IsRouteBackHit(mapFromGlobal(globalPos))) {
            _routeBackPressed = true;
            _routeBackHovered = true;
            update(RouteBackRect());
            return true;
        }

        if (_hostWindow->isMaximized() || _hostWindow->isFullScreen()) {
            return false;
        }

        const Qt::Edges edges = ResizeEdgesAtGlobalPos(globalPos);
        if (edges != Qt::Edges{}) {
            if (QWindow* handle = _hostWindow->windowHandle(); handle != nullptr && handle->startSystemResize(edges)) {
                UpdateResizeCursor(edges);
                return true;
            }
            _mouseOperation = MouseOperation::Resize;
            _resizeEdges = edges;
            _pressGlobalPos = globalPos;
            _pressWindowGeometry = _hostWindow->geometry();
            _hostWindow->grabMouse();
            UpdateResizeCursor(edges);
            return true;
        }

        if (IsTitleDragArea(mapFromGlobal(globalPos))) {
            if (QWindow* handle = _hostWindow->windowHandle(); handle != nullptr && handle->startSystemMove()) {
                return true;
            }
            _mouseOperation = MouseOperation::Move;
            _pressGlobalPos = globalPos;
            _pressWindowTopLeft = _hostWindow->frameGeometry().topLeft();
            return true;
        }
        break;
    }
    case QEvent::MouseMove: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        const QPoint globalPos = mouse->globalPosition().toPoint();
        const bool routeHovered = IsRouteBackHit(mapFromGlobal(globalPos));
        if (_routeBackHovered != routeHovered) {
            _routeBackHovered = routeHovered;
            update(RouteBackRect());
        }
        if (_mouseOperation == MouseOperation::Move) {
            _hostWindow->move(_pressWindowTopLeft + globalPos - _pressGlobalPos);
            return true;
        }
        if (_mouseOperation == MouseOperation::Resize) {
            ResizeHostWindow(globalPos);
            return true;
        }
        UpdateResizeCursor(ResizeEdgesAtGlobalPos(globalPos));
        break;
    }
    case QEvent::MouseButtonRelease: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton && _routeBackPressed) {
            const bool contains = IsRouteBackHit(mapFromGlobal(mouse->globalPosition().toPoint()));
            _routeBackPressed = false;
            _routeBackHovered = contains;
            update(RouteBackRect());
            if (contains) {
                Q_EMIT RouteBackRequested();
            }
            return true;
        }
        const bool wasResizing = _mouseOperation == MouseOperation::Resize;
        _mouseOperation = MouseOperation::None;
        _resizeEdges = {};
        if (wasResizing) {
            _hostWindow->releaseMouse();
        }
        UpdateResizeCursor(ResizeEdgesAtGlobalPos(mouse->globalPosition().toPoint()));
        break;
    }
    case QEvent::MouseButtonDblClick: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton && IsTitleDragArea(mapFromGlobal(mouse->globalPosition().toPoint()))) {
            _hostWindow->isMaximized() ? _hostWindow->showNormal() : _hostWindow->showMaximized();
            UpdateMaximizeButton();
            Q_EMIT MaximizeRequested();
            return true;
        }
        break;
    }
    default:
        break;
    }

    return QWidget::eventFilter(watched, event);
}

auto NyanMainTitleBar::IsTitleDragArea(const QPoint& pos) const -> bool
{
    const int upperHeight = Theme().DimensionPx("controlHeightSM").value_or(24);
    if (!QRect(0, 0, width(), upperHeight).contains(pos)) {
        return false;
    }

    // 返回区域、菜单和窗口按钮不作为拖拽区域。
    if (RouteBackRect().contains(pos)) {
        return false;
    }
    for (const QWidget* child : {_menuBar, _minimizeButton, _maximizeButton, _closeButton}) {
        if (child != nullptr && QRect(child->mapTo(this, QPoint(0, 0)), child->size()).contains(pos)) {
            return false;
        }
    }
    return true;
}

auto NyanMainTitleBar::ResizeEdgesAtGlobalPos(const QPoint& globalPos) const -> Qt::Edges
{
    const QPoint titlePos = mapFromGlobal(globalPos);
    if (rect().contains(titlePos) && !IsTitleDragArea(titlePos)) {
        return {};
    }
    return ResizeEdgesAt(_hostWindow->mapFromGlobal(globalPos));
}

auto NyanMainTitleBar::IsHostRelatedWidget(QObject* watched) const -> bool
{
    if (watched == this || watched == _hostWindow || watched == _customCentral) {
        return true;
    }

    const auto* widget = qobject_cast<const QWidget*>(watched);
    return widget != nullptr && widget->window() == _hostWindow;
}

void NyanMainTitleBar::UpdateResizeCursor(Qt::Edges edges)
{
    if (_hostWindow == nullptr || _hostWindow->isMaximized() || _hostWindow->isFullScreen()) {
        edges = {};
    }

    Qt::CursorShape cursor = Qt::ArrowCursor;
    if (edges == Qt::LeftEdge || edges == Qt::RightEdge) {
        cursor = Qt::SizeHorCursor;
    } else if (edges == Qt::TopEdge || edges == Qt::BottomEdge) {
        cursor = Qt::SizeVerCursor;
    } else if (edges == (Qt::LeftEdge | Qt::TopEdge) || edges == (Qt::RightEdge | Qt::BottomEdge)) {
        cursor = Qt::SizeFDiagCursor;
    } else if (edges == (Qt::RightEdge | Qt::TopEdge) || edges == (Qt::LeftEdge | Qt::BottomEdge)) {
        cursor = Qt::SizeBDiagCursor;
    }
    _hostWindow->setCursor(cursor);
}

auto NyanMainTitleBar::ResizeEdgesAt(const QPoint& hostPos) const -> Qt::Edges
{
    if (_hostWindow == nullptr || _hostWindow->isMaximized() || _hostWindow->isFullScreen()) {
        return {};
    }

    const int border = Theme().DimensionPx("spaceXS").value_or(8);
    const QRect rect = _hostWindow->rect();
    Qt::Edges edges;

    if (hostPos.x() >= 0 && hostPos.x() < border) {
        edges |= Qt::LeftEdge;
    } else if (hostPos.x() < rect.width() && hostPos.x() >= rect.width() - border) {
        edges |= Qt::RightEdge;
    }

    if (hostPos.y() >= 0 && hostPos.y() < border) {
        edges |= Qt::TopEdge;
    } else if (hostPos.y() < rect.height() && hostPos.y() >= rect.height() - border) {
        edges |= Qt::BottomEdge;
    }

    return edges;
}

void NyanMainTitleBar::ResizeHostWindow(const QPoint& globalPos)
{
    QRect geometry = _pressWindowGeometry;
    const QPoint delta = globalPos - _pressGlobalPos;
    const QSize minSize = _hostWindow->minimumSize();

    if (_resizeEdges.testFlag(Qt::LeftEdge)) {
        geometry.setLeft(std::min(geometry.left() + delta.x(), geometry.right() - minSize.width() + 1));
    }
    if (_resizeEdges.testFlag(Qt::RightEdge)) {
        geometry.setRight(std::max(geometry.right() + delta.x(), geometry.left() + minSize.width() - 1));
    }
    if (_resizeEdges.testFlag(Qt::TopEdge)) {
        geometry.setTop(std::min(geometry.top() + delta.y(), geometry.bottom() - minSize.height() + 1));
    }
    if (_resizeEdges.testFlag(Qt::BottomEdge)) {
        geometry.setBottom(std::max(geometry.bottom() + delta.y(), geometry.top() + minSize.height() - 1));
    }

    _hostWindow->setGeometry(geometry);
}

void NyanMainTitleBar::OnThemeChanged()
{
    ApplyTheme();
}

} // namespace matcha::gui
```

## 实施步骤

| 步骤 | 文件 | 内容 | 验证 |
| --- | --- | --- | --- |
| 1 | `Matcha\Include\Matcha\Widgets\Shell\NyanMainTitleBar.h` | 修改构造函数为必须传入 `QWidget* parent`，删除默认 `nullptr`。删除 `kHeight`、`kButtonSize` 这类固定标题栏尺寸常量。 | 头文件不再允许无父窗口创建；不出现标题栏尺寸类 Token 缓存成员。 |
| 2 | `Matcha\Source\Widgets\Shell\NyanMainTitleBar.cpp` | 建立 `QGridLayout` 上下两层结构，上层高度从 `controlHeightSM` 读取，下层高度从 `controlHeightXL` 读取。 | 标题栏总高度为 64px，上层 24px，下层 40px。 |
| 3 | `NyanMainTitleBar.cpp` | `paintEvent()` 统一绘制上下两层：上层 `colorBgContainer`，下层 `colorPrimaryNav`，底部 `colorDivider`。 | 视觉上为上白下导航主题色；不绘制父窗口背景。 |
| 4 | `NyanMainTitleBar.cpp` | `ApplyTheme()` 直接读取 `Light.json` 真实 key，设置按钮尺寸、图标尺寸、hover/pressed 样式；样式表颜色直接使用 `QColor::name(QColor::HexArgb)`。 | 不出现任何未登记在 `Light.json` 的字符串 key，也不新增颜色转换辅助函数。 |
| 5 | `NyanMainTitleBar.h/.cpp` | 实现手绘返回主页区域：增加 `RouteBackRect()`、`PaintRouteBackButton()`、可用/图标/形态状态和 `RouteBackRequested()`。 | 不创建真实返回按钮控件；返回区域始终存在，release 仍命中时才发出信号。 |
| 6 | `NyanMainTitleBar.cpp` | 增加 `SetCustomCentral()` 和 `CustomCentral()`，允许业务层替换下层整行客户区。 | 传入 widget 后下层显示自定义客户区；传入 `nullptr` 后恢复默认主题色下层。 |
| 7 | `NyanMainTitleBar.h/.cpp` | 删除字符串标题接口和相关成员。 | 编译期无法再通过字符串设置标题。 |
| 8 | `NyanMainTitleBar.cpp` | `SetTitleLogo()` 只保存图片标题，`paintEvent()` 在上层标题区按比例居中绘制图片。 | 图片标题居中显示，左右空间不足时按比例缩放，不绘制文字。 |
| 9 | `NyanMainTitleBar.cpp` | 安装自身、父窗口、`qApp` 应用级事件过滤器和自定义下层客户区事件过滤器，处理父窗口状态变化、返回区域 hover/pressed/release、标题栏拖动、父窗口边缘 resize。 | 返回区域优先处理鼠标事件；标题栏空白区可移动窗口；父窗口边缘即使被 central widget 或其他子控件覆盖，也可通过全局坐标命中并拖拽缩放。 |
| 10 | `NyanMainTitleBar.cpp` | 窗口按钮点击时直接操作父窗口，并发出原有信号。 | 最小化、最大化/还原、关闭行为正常；最大化图标能切换。 |
| 11 | `WindowNode.cpp` 或 Shell 装配点 | 在最小窗口链路中装配 `NyanMainTitleBar`，保持 Workbench、DocumentArea、DevTools 等业务区域与标题栏控件解耦。 | NyanCad Demo 可以在最小窗口链路中独立验证标题栏。 |

## 验证清单

| 验证项 | 预期 |
| --- | --- |
| `parent == nullptr` | 断言或运行时失败，不允许无宿主窗口创建。 |
| Token key 检查 | 代码中所有字符串 key 均能在 `Light.json` 找到。 |
| 成员变量检查 | 不存在标题栏高度、按钮宽度、按钮高度、图标尺寸、resize 命中宽度这类 Token 值缓存成员。 |
| 双层绘制 | 上层为 `colorBgContainer` 白色，下层为 `colorPrimaryNav` 导航主题色。 |
| 父窗口绘制边界 | 标题栏不绘制父窗体背景、不绘制阴影、不修改父窗体 `contentsMargins()`。 |
| 颜色输出 | 样式表颜色使用 `QColor::name(QColor::HexArgb)`，不定义额外颜色转换辅助函数。 |
| 返回区域默认状态 | 返回区域始终存在，默认可用、圆形，默认图标为 `Home`。 |
| 返回区域绘制 | 不存在 `_homeButton` / `_routeBackButton` 子控件；`paintEvent()` 负责上下层背景，`PaintRouteBackButton()` 只绘制返回按钮本体、hover 状态和返回主页图标。 |
| 返回区域点击 | press 命中只记录 pressed；release 仍命中时发出 `RouteBackRequested()`。 |
| 返回区域形态 | 支持 Circle / Rect，默认 Circle。 |
| 返回区域禁用 | 禁用后不响应点击，hover/pressed 清空；形态为 Circle 时仍绘制圆形按钮背景但没有 hover 悬浮效果，形态为 Rect 时可不绘制按钮本体背景；图标仍按 `colorText` 语义绘制。 |
| 菜单区 | `MenuBar()` 返回非空，现有菜单逻辑可以继续挂载。 |
| 自定义客户区 | `SetCustomCentral(widget)` 后下层显示传入 widget；`SetCustomCentral(nullptr)` 后恢复默认下层主题色。 |
| 字符串标题 | 不存在字符串标题接口和相关成员，不会绘制文字标题。 |
| 图片标题 | `SetTitleLogo()` 设置图片后，上层标题区按比例居中绘制图片。 |
| 窗口按钮 | 最小化、最大化/还原、关闭按钮可点击，且保留 `MinimizeRequested()`、`MaximizeRequested()`、`CloseRequested()`。 |
| 最大化状态 | 父窗口最大化后显示还原图标，恢复后显示最大化图标。 |
| 移动窗口 | 拖拽上层空白区域可移动父窗口。 |
| 拖拽 resize | 父窗口边缘可改变大小；central widget 填满窗口或鼠标事件落在宿主窗口子控件上时，仍可通过应用级事件过滤器触发 resize。 |

## 后续注意事项

| 项 | 说明 |
| --- | --- |
| 菜单系统 | 当前只保留 `NyanMenuBar` 入口，不重构菜单项、弹出菜单或命令系统。 |
| `MainTitleBarNode` | Node 只承担装配职责。后续如果删除 Node，应由 Shell 装配层直接创建 `NyanMainTitleBar(parentWindow)`。 |
| 专用 Token | 当前不新增专用标题栏 Token。若后续确实需要，应先扩展 `Light.json` 和主题解析，再在控件中使用真实 key。 |
| 父窗口无边框 | 标题栏不主动设置 `Qt::FramelessWindowHint`。是否启用无边框由窗口壳统一控制。 |
| 下层内容 | 默认绘制下层主题色；需要恢复文档工具栏或其他客户区时，通过 `SetCustomCentral()` 挂到下层区域，不把标题栏重新拆成多个互相耦合的壳控件。 |
