#include "Matcha/Tree/Controls/PushButtonNode.h"
#include "Matcha/Tree/UiNodeNotification.h"

#include "Matcha/Widgets/Controls/NyanPushButton.h"
#include "Matcha/Theming/IThemeService.h"

#include <QColor>
#include <QIcon>
#include <QString>

#include <utility>

namespace matcha::fw {

namespace {

[[nodiscard]] auto PushButtonIconColor(
    const gui::IThemeService& theme, gui::ButtonVariant variant) -> QColor
{
    switch (variant) {
    case gui::ButtonVariant::Primary:
    case gui::ButtonVariant::Danger:
        return theme.Color("colorTextRev").value_or(QColor("#E0FFFFFF"));
    case gui::ButtonVariant::Link:
        return theme.Color("colorPrimary").value_or(QColor("#0066FF"));
    case gui::ButtonVariant::Ghost:
    case gui::ButtonVariant::Text:
    case gui::ButtonVariant::Dashed:
    case gui::ButtonVariant::Secondary:
    default:
        return theme.Color("colorText").value_or(QColor("#E0000000"));
    }
}

} // namespace

MATCHA_IMPLEMENT_CLASS(PushButtonNode, WidgetNode)

PushButtonNode::PushButtonNode(std::string id)
    : WidgetNode(std::move(id), NodeType::PushButton)
{
}

PushButtonNode::~PushButtonNode() = default;

void PushButtonNode::SetText(std::string_view text)
{
    EnsureWidget();
    if (auto* w = qobject_cast<gui::NyanPushButton*>(_widget)) {
        w->setText(QString::fromUtf8(text.data(), static_cast<int>(text.size())));
    }
}

auto PushButtonNode::Text() const -> std::string
{
    if (auto* w = qobject_cast<gui::NyanPushButton*>(_widget)) {
        return w->text().toStdString();
    }
    return {};
}

void PushButtonNode::SetVariant(gui::ButtonVariant variant)
{
    EnsureWidget();
    if (auto* w = qobject_cast<gui::NyanPushButton*>(_widget)) {
        w->SetVariant(variant);
        OnIconChanged();
    }
}

auto PushButtonNode::Variant() const -> gui::ButtonVariant
{
    if (auto* w = qobject_cast<gui::NyanPushButton*>(_widget)) {
        return w->Variant();
    }
    return gui::ButtonVariant::Secondary;
}

auto PushButtonNode::CreateWidget(QWidget* parent) -> QWidget*
{
    auto* w = new gui::NyanPushButton(parent);
    QObject::connect(w, &QPushButton::clicked, w, [this]() {
        Activated notif;
        SendNotification(this, notif);
    });
    QObject::connect(w, &QPushButton::pressed, w, [this]() {
        Pressed notif;
        SendNotification(this, notif);
    });
    QObject::connect(w, &QPushButton::released, w, [this]() {
        Released notif;
        SendNotification(this, notif);
    });
    return w;
}

void PushButtonNode::OnIconChanged()
{
    auto* w = qobject_cast<gui::NyanPushButton*>(_widget);
    if (w == nullptr || !gui::HasThemeService()) {
        return;
    }
    if (_iconId.empty()) {
        w->setIcon(QIcon());
        return;
    }
    const int sizePx = static_cast<int>(_iconSize);
    const auto& theme = gui::GetThemeService();
    const QColor fg = PushButtonIconColor(theme, w->Variant());
    const QPixmap pm = theme.ResolveIcon(_iconId, _iconSize, fg);
    if (!pm.isNull()) {
        w->setIcon(QIcon(pm));
        w->setIconSize(QSize(sizePx, sizePx));
    }
}

} // namespace matcha::fw
