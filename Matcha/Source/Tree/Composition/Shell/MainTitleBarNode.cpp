/**
 * @file MainTitleBarNode.cpp
 * @brief MainTitleBarNode implementation -- wraps NyanMainTitleBar (Row 1 only).
 */

#include "Matcha/Tree/Composition/Shell/MainTitleBarNode.h"

#include "Matcha/Tree/ContainerNode.h"
#include "Matcha/Tree/Composition/Menu/MenuBarNode.h"
#include "Matcha/Widgets/Shell/NyanMainTitleBar.h"

namespace matcha::fw {

MATCHA_IMPLEMENT_CLASS(MainTitleBarNode, TitleBarNode)

MainTitleBarNode::MainTitleBarNode(std::string id, UiNode* parentHint)
    : TitleBarNode(std::move(id))
    , _titleBar(new gui::NyanMainTitleBar(parentHint ? parentHint->Widget() : nullptr))
{
    // 绑定标题栏内部现有菜单栏，菜单内容仍由 Composition 层装配。
    auto menuBarNode = std::make_unique<MenuBarNode>("main-menubar");
    menuBarNode->BindMenuBar(_titleBar->MenuBar());
    AddNode(std::move(menuBarNode));

    // NyanMainTitleBar 已移除 quick-command 子区域，后续如需恢复应通过新的自定义下层客户区接入。
}

MainTitleBarNode::~MainTitleBarNode() = default;

void MainTitleBarNode::SetTitle(std::string_view title)
{
    // NyanMainTitleBar 不再绘制字符串标题；Node 仅保留状态用于兼容 TitleBarNode 接口。
    _title.assign(title.data(), title.size());
}

auto MainTitleBarNode::Title() const -> std::string
{
    return _title;
}

auto MainTitleBarNode::Widget() -> QWidget*
{
    return _titleBar;
}

auto MainTitleBarNode::MainTitleBar() -> gui::NyanMainTitleBar*
{
    return _titleBar;
}

auto MainTitleBarNode::GetMenuBar() -> observer_ptr<MenuBarNode>
{
    for (auto* node : ChildrenOfType(NodeType::MenuBar)) {
        if (auto* mb = dynamic_cast<MenuBarNode*>(node)) {
            return make_observer(mb);
        }
    }
    return observer_ptr<MenuBarNode>{};
}

auto MainTitleBarNode::GetQuickCommandSlot() -> observer_ptr<ContainerNode>
{
    // quick-command-slot 已随旧标题栏接口移除。
    return observer_ptr<ContainerNode>{};
}

} // namespace matcha::fw
