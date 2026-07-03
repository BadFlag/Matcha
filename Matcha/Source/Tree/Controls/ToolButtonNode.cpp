#include "Matcha/Tree/Controls/ToolButtonNode.h"
#include "Matcha/Tree/UiNodeNotification.h"

#include "Matcha/Widgets/Controls/NyanToolButton.h"
#include "Matcha/Theming/IThemeService.h"

#include <QIcon>
#include <QString>

#include <utility>

namespace matcha::fw {

MATCHA_IMPLEMENT_CLASS(ToolButtonNode, WidgetNode)

ToolButtonNode::ToolButtonNode(std::string id)
    : WidgetNode(std::move(id), NodeType::ToolButton)
{
}

ToolButtonNode::~ToolButtonNode() = default;

void ToolButtonNode::SetText(std::string_view text)
{
    EnsureWidget();
    if (auto* w = qobject_cast<gui::NyanToolButton*>(_widget)) {
        w->setText(QString::fromUtf8(text.data(), static_cast<int>(text.size())));
    }
}

auto ToolButtonNode::Text() const -> std::string
{
    if (auto* w = qobject_cast<gui::NyanToolButton*>(_widget)) {
        return w->text().toStdString();
    }
    return {};
}

void ToolButtonNode::SetCheckable(bool checkable)
{
    EnsureWidget();
    if (auto* w = qobject_cast<gui::NyanToolButton*>(_widget)) {
        w->setCheckable(checkable);
    }
}

auto ToolButtonNode::IsCheckable() const -> bool
{
    if (auto* w = qobject_cast<gui::NyanToolButton*>(_widget)) {
        return w->isCheckable();
    }
    return false;
}

void ToolButtonNode::SetChecked(bool checked)
{
    EnsureWidget();
    if (auto* w = qobject_cast<gui::NyanToolButton*>(_widget)) {
        w->setChecked(checked);
        OnIconChanged();
    }
}

auto ToolButtonNode::IsChecked() const -> bool
{
    if (auto* w = qobject_cast<gui::NyanToolButton*>(_widget)) {
        return w->isChecked();
    }
    return false;
}

auto ToolButtonNode::CreateWidget(QWidget* parent) -> QWidget*
{
    auto* w = new gui::NyanToolButton(parent);
    QObject::connect(w, &QToolButton::clicked, w, [this]() {
        Activated notif;
        SendNotification(this, notif);
    });
    QObject::connect(w, &QToolButton::toggled, w, [this](bool) {
        OnIconChanged();
    });
    QObject::connect(w, &gui::NyanToolButton::RightClicked, w, [this]() {
        RightClicked notif;
        SendNotification(this, notif);
    });
    return w;
}

void ToolButtonNode::OnIconChanged()
{
    auto* w = qobject_cast<gui::NyanToolButton*>(_widget);
    if (w == nullptr || !gui::HasThemeService()) {
        return;
    }
    if (_iconId.empty()) {
        w->setIcon(QIcon());
        return;
    }
    const int sizePx = static_cast<int>(_iconSize);
    const std::size_t variantIdx = (w->isCheckable() && w->isChecked()) ? 1U : 0U;
    const QColor fg = gui::GetThemeService()
        .Resolve(gui::WidgetKind::ToolButton, variantIdx, gui::InteractionState::Normal)
        .foreground;
    const QPixmap pm = gui::GetThemeService().ResolveIcon(_iconId, _iconSize, fg);
    if (!pm.isNull()) {
        w->setIcon(QIcon(pm));
        w->setIconSize(QSize(sizePx, sizePx));
    }
}

} // namespace matcha::fw
