[TOC]



# NyanDocumentToolBar 标签页操作说明

## 1. 工作目标

本文分析 `Matcha\Source\Widgets\Shell\NyanDocumentToolBar.cpp` 及其直接关联的标签页代码，说明 `NyanDocumentToolBar` 如何承载文档标签页，以及业务层应如何创建、读取、更新、删除标签页。

结论先行：

- `NyanDocumentToolBar` 本身不直接创建单个标签页，它只负责创建工具栏布局，并通过 `SetTabBar()` 挂入一个 `NyanTabBar`。
- 真实的标签页 CRUD 在 `NyanTabBar` 中实现。
- `DocumentToolBarNode` 在构造时创建 `TabBarNode`，再把 `TabBarNode` 内部的 `NyanTabBar` 安装到 `NyanDocumentToolBar`。
- 点击标题栏样式标签栏里的 `+` 按钮时，`NyanTabBar` 发出 `AddTabRequested()` 信号；`TabBarNode` 将其转成 `TabPageAddRequested` 通知。业务层收到通知后创建实际页面数据，再调用 `TabBarNode::AddTab()` 更新 UI。
- `TabBarNode` 已经转发新增、切换、关闭、拖出、拖入、重排通知，业务层可以统一通过 UiNode 通知系统接收标签页操作请求。

## 2. 控件描述

### 2.1 NyanDocumentToolBar 的职责

源码位置：

- `Matcha\Include\Matcha\Widgets\Shell\NyanDocumentToolBar.h`
- `Matcha\Source\Widgets\Shell\NyanDocumentToolBar.cpp`

`NyanDocumentToolBar` 是 shell header 区域中的文档工具栏，固定高度 `36px`，继承自 `QWidget` 和 `ThemeAware`。

它的布局从左到右为：

| 区域 | 成员 | 说明 |
| --- | --- | --- |
| 模块选择框 | `_moduleCombo` | 左侧模块下拉框，选中项变化时发出 `ModuleChanged(const QString&)` |
| 分隔线 | `_separator` | `QFrame::VLine`，用于分隔模块选择框和标签页区域 |
| 标签栏插槽 | `_tabBar` | 由外部通过 `SetTabBar(NyanTabBar*)` 注入 |
| 全局按钮容器 | `_globalButtonContainer` | 右侧业务按钮容器，默认隐藏 |

`NyanDocumentToolBar` 只管理“工具栏中的标签栏控件位置”，不管理标签页集合，也不保存 `PageId -> tab` 的关系。

### 2.2 NyanTabBar 的职责

源码位置：

- `Matcha\Include\Matcha\Widgets\Shell\NyanTabBar.h`
- `Matcha\Source\Widgets\Shell\NyanTabBar.cpp`

`NyanTabBar` 是真正的标签栏控件。它是自绘 `QWidget`，不继承 `QTabBar`。它持有：

```cpp
std::vector<NyanTabItem*> _items;
int _activeIndex = -1;
```

每个标签页由一个 `NyanTabItem` 表示，标签页身份统一使用 `matcha::fw::PageId`。

### 2.3 TabBarNode 的职责

源码位置：

- `Matcha\Include\Matcha\Tree\Composition\Document\TabBarNode.h`
- `Matcha\Source\Tree\Composition\Document\TabBarNode.cpp`

`TabBarNode` 是 UiNode 层包装：

- 内部创建一个 `gui::NyanTabBar`。
- 将底层 widget 信号转换成通知系统中的 tab 通知。
- 调用 `AddTab()` 时，除了更新 `NyanTabBar`，还会创建对应的 `TabItemNode` 子节点。

`DocumentToolBarNode` 在构造函数中完成组装：

```cpp
auto tabBarNode = std::make_unique<TabBarNode>(
    "doc-tabbar",
    gui::TabStyle::TitleBar,
    _toolBar);

_toolBar->SetTabBar(tabBarNode->TabBar());
AddNode(std::move(tabBarNode));
```

## 3. 新增标签页的完整流程

### 3.1 工具栏安装标签栏

`NyanDocumentToolBar::SetTabBar(NyanTabBar* tabBar)` 的逻辑：

1. 如果已有 `_tabBar`，先从 `_layout` 中移除旧控件。
2. 保存新指针到 `_tabBar`。
3. 找到 `_separator` 之后的位置。
4. 用 `insertWidget(insertIndex, _tabBar, 1)` 插入标签栏，并给 stretch 设为 `1`，让标签栏占据中间伸缩空间。

注意：`SetTabBar()` 只从布局移除旧 tab bar，不负责删除旧对象。对象生命周期由 Qt parent 或创建者管理。

### 3.2 点击加号只发出请求

`NyanTabBar` 在 `TabStyle::TitleBar` 样式下绘制加号按钮：

- `paintEvent()` 调用 `DrawAddButton()`。
- `AddButtonRect()` 计算加号位置，位于最后一个标签页右侧；没有标签时从 `x = 0` 开始。
- `mousePressEvent()` 命中加号区域时发出：

```cpp
Q_EMIT AddTabRequested();
```

这一步没有创建 `PageId`，也没有调用 `AddTab()`。原因是 widget 层不知道业务页面如何创建、标题是什么、是否允许创建。

### 3.3 业务层创建页面并更新标签栏

业务层接到 `TabPageAddRequested` 通知后，应完成：

1. 分配或创建业务页面，得到新的 `PageId`。
2. 准备标签标题。
3. 调用 `TabBarNode::AddTab(pageId, title)`。
4. 调用 `SetActiveTab(pageId)`，如果希望新标签立即激活。

推荐从 UiNode 层调用 `TabBarNode::AddTab()`，因为它会同步创建 `TabItemNode`。直接调用 widget 层 `NyanTabBar::AddTab()` 只会创建视觉标签，不会维护 UiNode 子节点。当前 `TabBarNode` 只提供尾部新增；如果未来需要按位置插入，应先补齐 node 层插入能力，再封装底层 `NyanTabBar::InsertTab()`，避免视觉顺序和 UiNode 子节点顺序不一致。

## 4. 标签页 CRUD/CURD 操作

这里的 CRUD 指标签页 UI 及其 UiNode 包装层操作；若内部口径写作 CURD，本文按同一组创建、读取、更新、删除操作理解。实际文档模型、页面内容、文件保存等业务数据不在这些控件中实现。

### 4.1 Create：创建标签页

#### Widget 层

```cpp
auto* item = tabBar->AddTab(pageId, "Untitled");
```

函数：

```cpp
auto NyanTabBar::AddTab(fw::PageId pageId, const QString& title) -> NyanTabItem*;
auto NyanTabBar::InsertTab(int index, fw::PageId pageId, const QString& title) -> NyanTabItem*;
```

行为：

- 创建 `NyanTabItem(_style, pageId, title, this)`。
- 加入 `_items`。
- 如果当前没有激活标签，将新标签设为激活。
- 连接 `NyanTabItem::Pressed` 到 `NyanTabBar::OnItemPressed`。
- 连接 `NyanTabItem::CloseRequested` 到 `NyanTabBar::TabCloseRequested`。
- 调用 `RecalcLayout()` 重新排列标签。
- 调用 `UpdateAutoHide()` 更新浮动样式下的显示状态。

`InsertTab()` 相比 `AddTab()` 多了插入位置处理：

- `index` 会被 clamp 到 `[0, TabCount()]`。
- 如果插入位置在当前激活标签之前或等于当前激活标签，会把 `_activeIndex` 后移。

#### UiNode 层

```cpp
int index = tabBarNode->AddTab(pageId, "Untitled");
```

函数：

```cpp
auto TabBarNode::AddTab(PageId pageId, std::string_view title) -> int;
```

行为：

- 将 `std::string_view` 标题转换成 `QString`。
- 调用 `_tabBar->AddTab(pageId, titleStr)`。
- 创建 `TabItemNode("tab-" + pageId, pageId, title)`。
- 将 `NyanTabItem*` 绑定给 `TabItemNode`。
- 将 `TabItemNode` 加入当前节点子节点。
- 返回标签页索引。

### 4.2 Read：读取标签页状态

#### 从 NyanDocumentToolBar 读取标签栏

```cpp
gui::NyanTabBar* tabBar = documentToolBar->GetTabBar();
```

函数：

```cpp
auto NyanDocumentToolBar::GetTabBar() const -> NyanTabBar*;
```

#### 从 NyanTabBar 读取标签页

```cpp
int count = tabBar->TabCount();
int index = tabBar->IndexOfPage(pageId);
auto activePageId = tabBar->ActivePageId();
auto* item = tabBar->FindItem(pageId);
```

相关函数：

| 函数 | 参数 | 返回值 | 说明 |
| --- | --- | --- | --- |
| `TabCount()` | 无 | `int` | 当前标签数量 |
| `ItemAt(int index)` | `index`：标签索引 | `NyanTabItem*` | 越界返回 `nullptr` |
| `FindItem(fw::PageId pageId)` | `pageId`：页面 ID | `NyanTabItem*` | 找不到返回 `nullptr` |
| `IndexOfPage(fw::PageId pageId)` | `pageId`：页面 ID | `int` | 找不到返回 `-1` |
| `ActivePageId()` | 无 | `fw::PageId` | 没有激活标签时返回 `PageId::From(0)` |
| `ActiveIndex()` | 无 | `int` | 没有激活标签时返回 `-1` |
| `Style()` | 无 | `TabStyle` | 返回 `TitleBar` 或 `Floating` |

#### 从 TabBarNode 读取标签页

```cpp
auto* tabNode = tabBarNode->FindTab(pageId);
int count = tabBarNode->TabCount();
```

相关函数：

| 函数 | 参数 | 返回值 | 说明 |
| --- | --- | --- | --- |
| `TabBar()` | 无 | `gui::NyanTabBar*` | 获取底层 widget |
| `TabCount()` | 无 | `int` | 透传 `_tabBar->TabCount()` |
| `FindTab(PageId pageId)` | `pageId`：页面 ID | `TabItemNode*` | 在子节点中查找标签节点 |

### 4.3 Update：更新标签页

#### 激活标签

```cpp
tabBarNode->SetActiveTab(pageId);
```

底层函数：

```cpp
void NyanTabBar::SetActiveTab(fw::PageId pageId);
void TabBarNode::SetActiveTab(PageId pageId);
```

行为：

- 通过 `IndexOfPage(pageId)` 找到目标索引。
- 目标不存在或已经是当前激活项时直接返回。
- 将旧激活项 `SetActive(false)`。
- 更新 `_activeIndex`。
- 将新激活项 `SetActive(true)`。

#### 修改标题

```cpp
tabBarNode->SetTabTitle(pageId, "Part-A.step");
```

底层函数：

```cpp
void NyanTabBar::SetTabTitle(fw::PageId pageId, const QString& title);
void TabBarNode::SetTabTitle(PageId pageId, std::string_view title);
```

行为：

- widget 层查找 `NyanTabItem`，找到后调用 `SetTitle(title)` 并触发重绘。
- node 层额外更新 `TabItemNode` 的标题元数据。
- 标签宽度是固定值，标题过长时由 `NyanTabItem` 绘制时做 `elidedText(..., Qt::ElideMiddle, ...)`。

#### 移动或重排标签

```cpp
tabBarNode->MoveTab(fromIndex, toIndex);
```

底层函数：

```cpp
void NyanTabBar::MoveTab(int fromIndex, int toIndex);
void TabBarNode::MoveTab(int fromIndex, int toIndex);
```

行为：

- `fromIndex` 或 `toIndex` 越界时直接返回。
- 从 `_items` 中取出对应项并插入新位置。
- 调整 `_activeIndex`，确保激活状态跟随原来的激活标签。
- 调用 `RecalcLayout()`。

用户水平拖动标签时，`NyanTabItem` 会调用 `NyanTabBar::RequestReorder()`；当跨过相邻标签中线后，`NyanTabBar` 调用 `MoveTab()` 并发出：

```cpp
TabReordered(pageId, oldIndex, newIndex)
```

`TabBarNode` 会转成 `Notification::TabReordered`。

### 4.4 Delete：删除标签页

```cpp
tabBarNode->RemoveTab(pageId);
```

底层函数：

```cpp
void NyanTabBar::RemoveTab(fw::PageId pageId);
void TabBarNode::RemoveTab(PageId pageId);
```

行为：

- 通过 `IndexOfPage(pageId)` 查找标签。
- 找不到则直接返回。
- 从 `_items` 中移除 `NyanTabItem*`。
- 对 widget 调用 `deleteLater()`。
- 如果删除的是激活标签：
  - 没有剩余标签时 `_activeIndex = -1`。
  - 还有剩余标签时激活同位置标签；如果删除的是最后一个，则激活新的最后一个。
- 如果删除位置在当前激活标签之前，`_activeIndex` 前移。
- 调用 `RecalcLayout()` 和 `UpdateAutoHide()`。
- `TabBarNode::RemoveTab()` 还会找到对应 `TabItemNode` 并从子节点树移除。

用户点击标签关闭按钮时，`NyanTabItem` 先发出：

```cpp
CloseRequested(pageId)
```

`NyanTabBar` 将其转发为：

```cpp
TabCloseRequested(pageId)
```

`TabBarNode` 再转成：

```cpp
Notification::TabPageCloseRequested(pageId)
```

是否真的关闭页面、是否提示保存、是否调用 `RemoveTab()`，由业务层决定。

## 5. 函数与参数清单

### 5.1 NyanDocumentToolBar

| 函数 | 参数 | 返回值 | 说明 |
| --- | --- | --- | --- |
| `NyanDocumentToolBar(QWidget* parent = nullptr)` | `parent`：Qt 父控件 | 无 | 构造控件，初始化布局和样式 |
| `SetTabBar(NyanTabBar* tabBar)` | `tabBar`：要安装的标签栏 widget，可为空 | `void` | 将标签栏插入到分隔线之后 |
| `GetTabBar() const` | 无 | `NyanTabBar*` | 返回当前标签栏指针 |
| `SetModuleItems(const QStringList& items)` | `items`：模块名列表 | `void` | 重置模块下拉框选项 |
| `CurrentModule() const` | 无 | `QString` | 返回当前模块名 |
| `SetCurrentModule(const QString& name)` | `name`：模块名 | `void` | 如果存在同名选项，则切换当前模块 |
| `GlobalButtonContainer()` | 无 | `QWidget*` | 返回右侧全局按钮容器 |
| `sizeHint() const` | 无 | `QSize` | 返回 `{800, 36}` |
| `minimumSizeHint() const` | 无 | `QSize` | 返回 `{200, 36}` |

信号：

| 信号 | 参数 | 触发时机 |
| --- | --- | --- |
| `ModuleChanged(const QString& moduleName)` | `moduleName`：新模块名 | `_moduleCombo` 当前文本变化 |

### 5.2 NyanTabBar

| 函数 | 参数 | 返回值 | 说明 |
| --- | --- | --- | --- |
| `NyanTabBar(TabStyle style, QWidget* parent = nullptr)` | `style`：显示样式；`parent`：Qt 父控件 | 无 | 创建自绘标签栏 |
| `AddTab(fw::PageId pageId, const QString& title)` | `pageId`：页面 ID；`title`：标题 | `NyanTabItem*` | 尾部新增标签 |
| `InsertTab(int index, fw::PageId pageId, const QString& title)` | `index`：插入位置；`pageId`：页面 ID；`title`：标题 | `NyanTabItem*` | 指定位置插入标签，索引会被 clamp |
| `RemoveTab(fw::PageId pageId)` | `pageId`：页面 ID | `void` | 删除标签 |
| `SetActiveTab(fw::PageId pageId)` | `pageId`：页面 ID | `void` | 设置激活标签 |
| `SetTabTitle(fw::PageId pageId, const QString& title)` | `pageId`：页面 ID；`title`：新标题 | `void` | 修改标签标题 |
| `MoveTab(int fromIndex, int toIndex)` | `fromIndex`：原位置；`toIndex`：目标位置 | `void` | 移动标签 |
| `ItemAt(int index) const` | `index`：标签索引 | `NyanTabItem*` | 获取指定位置标签 |
| `FindItem(fw::PageId pageId) const` | `pageId`：页面 ID | `NyanTabItem*` | 按页面 ID 查找标签 |
| `IndexOfPage(fw::PageId pageId) const` | `pageId`：页面 ID | `int` | 返回标签索引，找不到返回 `-1` |
| `ActivePageId() const` | 无 | `fw::PageId` | 返回当前激活页面 ID |
| `ActiveIndex() const` | 无 | `int` | 返回当前激活索引 |
| `TabCount() const` | 无 | `int` | 返回标签数 |
| `Style() const` | 无 | `TabStyle` | 返回标签栏样式 |
| `RequestReorder(NyanTabItem* item, int globalX)` | `item`：被拖动标签；`globalX`：鼠标全局 X 坐标 | `void` | 标签内部拖动重排入口 |
| `HandleDragResult(fw::PageId pageId, Qt::DropAction result)` | `pageId`：页面 ID；`result`：拖拽结果 | `void` | 拖出标签栏后的结果处理 |

信号：

| 信号 | 参数 | 说明 |
| --- | --- | --- |
| `TabPressed(PageId pageId)` | 被点击标签的页面 ID | 用户点击标签后触发 |
| `TabCloseRequested(PageId pageId)` | 请求关闭的页面 ID | 用户点击标签关闭按钮后触发 |
| `TabReordered(PageId pageId, int oldIndex, int newIndex)` | 页面 ID、原索引、新索引 | 标签水平拖动重排后触发 |
| `TabDropReceived(PageId pageId, int insertIndex)` | 页面 ID、插入位置 | 外部标签拖入当前标签栏后触发 |
| `TabDraggedToVoid(PageId pageId, QPoint globalPos)` | 页面 ID、全局坐标 | 标签拖到无接收目标处后触发 |
| `AddTabRequested()` | 无 | 用户点击 `+` 新建按钮后触发 |

### 5.3 TabBarNode

| 函数 | 参数 | 返回值 | 说明 |
| --- | --- | --- | --- |
| `TabBarNode(std::string id, gui::TabStyle style, QWidget* parentWidget = nullptr)` | `id`：节点 ID；`style`：标签栏样式；`parentWidget`：Qt 父控件 | 无 | 创建 UiNode 和底层 `NyanTabBar` |
| `TabBar()` | 无 | `gui::NyanTabBar*` | 获取底层标签栏 |
| `Widget()` | 无 | `QWidget*` | 获取底层 widget |
| `AddTab(PageId pageId, std::string_view title)` | `pageId`：页面 ID；`title`：标题 | `int` | 新增标签并创建 `TabItemNode` |
| `RemoveTab(PageId pageId)` | `pageId`：页面 ID | `void` | 删除标签并移除 `TabItemNode` |
| `SetActiveTab(PageId pageId)` | `pageId`：页面 ID | `void` | 设置激活标签 |
| `SetTabTitle(PageId pageId, std::string_view title)` | `pageId`：页面 ID；`title`：新标题 | `void` | 修改标签标题和 node 元数据 |
| `MoveTab(int fromIndex, int toIndex)` | `fromIndex`：原位置；`toIndex`：目标位置 | `void` | 移动标签 |
| `TabCount() const` | 无 | `int` | 返回标签数 |
| `FindTab(PageId pageId)` | `pageId`：页面 ID | `TabItemNode*` | 查找标签节点 |

通知：

| 通知 | 参数 | 说明 |
| --- | --- | --- |
| `TabPageAddRequested` | 无 | 用户点击 `+` 后由 `TabBarNode` 发出，表示业务层应创建新页面 |
| `TabPageSwitched` | `PageId pageId` | 用户点击标签后由 `TabBarNode` 发出 |
| `TabPageCloseRequested` | `PageId pageId` | 用户点击标签关闭按钮后由 `TabBarNode` 发出 |
| `TabPageDraggedOut` | `PageId pageId`, `int globalX`, `int globalY` | 标签拖到无接收目标处后由 `TabBarNode` 发出 |
| `TabDroppedIn` | `PageId pageId`, `int insertIndex` | 外部标签拖入当前标签栏后由 `TabBarNode` 发出 |
| `TabReordered` | `PageId pageId`, `int oldIndex`, `int newIndex` | 标签水平重排后由 `TabBarNode` 发出 |

## 6. 示例代码

### 6.1 通过 DocumentToolBarNode 新增标签

推荐用于业务层或组合层，因为 `TabBarNode` 会维护 `TabItemNode`。

```cpp
#include "Matcha/Tree/Composition/Shell/DocumentToolBarNode.h"
#include "Matcha/Tree/Composition/Document/TabBarNode.h"
#include "Matcha/Core/StrongId.h"

using namespace matcha;

void AddDocumentTab(fw::DocumentToolBarNode* docToolBarNode)
{
    if (docToolBarNode == nullptr) {
        return;
    }

    auto tabBarNode = docToolBarNode->GetTabBar();
    if (!tabBarNode) {
        return;
    }

    const fw::PageId pageId = fw::PageId::From(1001);
    const int index = tabBarNode->AddTab(pageId, "Untitled");
    tabBarNode->SetActiveTab(pageId);

    // index 是新增后的标签位置，可用于同步业务模型中的页面顺序。
    (void)index;
}
```

### 6.2 响应 `+` 按钮请求

`TabBarNode` 会把底层 `NyanTabBar::AddTabRequested()` 转成 `TabPageAddRequested` 通知。业务层订阅该通知后，再创建真实页面并调用 `TabBarNode::AddTab()`：

```cpp
#include "Matcha/Tree/Composition/Document/TabBarNode.h"
#include "Matcha/Event/EventNode.h"
#include "Matcha/Tree/UiNodeNotification.h"

#include <vector>

using namespace matcha;

void WireAddTabRequest(fw::TabBarNode* tabBarNode,
                       std::vector<matcha::ScopedSubscription>& subscriptions)
{
    if (tabBarNode == nullptr) {
        return;
    }

    subscriptions.emplace_back(*tabBarNode, tabBarNode, "TabPageAddRequested",
        [tabBarNode](matcha::EventNode& /*sender*/, matcha::Notification& n) {
            if (n.As<fw::TabPageAddRequested>() == nullptr) {
                return;
            }

            // 实际项目中应由 DocumentManager / WorkbenchManager 创建真实页面。
            static uint64_t nextPageId = 2000;
            const fw::PageId pageId = fw::PageId::From(++nextPageId);

            tabBarNode->AddTab(pageId, "Untitled");
            tabBarNode->SetActiveTab(pageId);
        });
}
```

补齐后的信号链路如下：

```text
用户点击 NyanTabBar 的 + 按钮
        |
        v
NyanTabBar::mousePressEvent()
        |
        v
Q_EMIT AddTabRequested()
        |
        v
TabBarNode 构造函数中的 QObject::connect(...) 捕获该信号
        |
        v
创建 fw::TabPageAddRequested 通知
        |
        v
SendNotification(this, notif)
        |
        v
业务层收到通知后创建真实页面，生成 PageId
        |
        v
调用 TabBarNode::AddTab(pageId, title)
        |
        v
调用 TabBarNode::SetActiveTab(pageId)
```

### 6.3 查询、改名、切换、关闭标签

```cpp
void UpdateDocumentTabs(fw::TabBarNode* tabBarNode)
{
    if (tabBarNode == nullptr) {
        return;
    }

    const fw::PageId pageId = fw::PageId::From(1001);

    if (tabBarNode->FindTab(pageId) == nullptr) {
        return;
    }

    tabBarNode->SetTabTitle(pageId, "Part-A.step");
    tabBarNode->SetActiveTab(pageId);

    const int count = tabBarNode->TabCount();
    if (count > 1) {
        tabBarNode->MoveTab(0, count - 1);
    }

    // 删除 UI 标签前，业务层通常应先确认页面是否可关闭。
    tabBarNode->RemoveTab(pageId);
}
```

### 6.4 处理关闭请求

`TabBarNode` 会把关闭点击转成 `TabPageCloseRequested` 通知。伪代码如下：

```cpp
void OnTabPageCloseRequested(fw::TabBarNode* tabBarNode,
                             fw::PageId pageId)
{
    if (tabBarNode == nullptr) {
        return;
    }

    // 这里应先检查文档是否有未保存修改、是否允许关闭。
    const bool canClose = true;
    if (!canClose) {
        return;
    }

    // 先关闭业务页面，再移除 UI 标签。
    tabBarNode->RemoveTab(pageId);
}
```

## 7. 接入建议

1. 新增标签页优先走 `TabBarNode::AddTab()`，不要只调用 `NyanTabBar::AddTab()`，否则 UiNode 树不会同步创建 `TabItemNode`。正确流程是业务层先创建真实页面并生成 `PageId`，再调用 `TabBarNode::AddTab(pageId, title)`，最后按需调用 `SetActiveTab(pageId)`。当前 `TabBarNode` 只封装尾部新增；需要指定位置插入时，应先补齐 node 层插入 API，避免直接调用 `NyanTabBar::InsertTab()` 导致视觉标签和 UiNode 子节点不同步。
2. `NyanDocumentToolBar::SetTabBar()` 是安装标签栏，不是新增标签页；不要在业务代码中把它当成“添加一个 tab”的接口。
3. `AddTabRequested()` 是 widget 层信号，`TabBarNode` 已将其转发为 `TabPageAddRequested` 通知。业务层应订阅 `TabPageAddRequested`，收到通知后创建页面和 `PageId`，再调用 `TabBarNode::AddTab()`。该通知不携带 `PageId`，因为点击 `+` 时真实页面尚未创建。
4. 关闭、拖出、拖入、重排都是“请求或通知”语义。业务层应先更新真实页面模型，再调用 `TabBarNode` 的 CRUD 方法同步 UI。
5. `PageId::From(0)` 在 `ActivePageId()` 中表示无激活标签的兜底返回值，不应作为正常页面 ID 使用。
