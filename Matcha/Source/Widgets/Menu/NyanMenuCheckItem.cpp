#include <Matcha/Widgets/Menu/NyanMenuCheckItem.h>

#include <QPainter>

namespace matcha::gui {

NyanMenuCheckItem::NyanMenuCheckItem(QWidget* parent)
    : NyanMenuItem(parent)
{
    // Connect to our own Triggered signal to toggle
    connect(this, &NyanMenuItem::Triggered, this, &NyanMenuCheckItem::Toggle);
}

NyanMenuCheckItem::~NyanMenuCheckItem() = default;

// -- Check State --

void NyanMenuCheckItem::SetChecked(bool checked)
{
    if (_checked != checked) {
        _checked = checked;
        update();
        Q_EMIT Toggled(_checked);
    }
}

auto NyanMenuCheckItem::IsChecked() const -> bool
{
    return _checked;
}

void NyanMenuCheckItem::Toggle()
{
    SetChecked(!_checked);
}

// -- Paint --

void NyanMenuCheckItem::DrawContent(QPainter& painter, const QRect& rect, bool hovered, bool pressed) const
{
    // Call base class to draw background and text
    NyanMenuItem::DrawContent(painter, rect, hovered, pressed);

    // Draw checkmark if checked
    if (_checked) {
        DrawCheckmark(painter, rect);
    }
}

void NyanMenuCheckItem::DrawCheckmark(QPainter& painter, const QRect& rect) const
{
    const auto& theme = Theme();

    // Checkmark in icon area (same position as icon)
    const int iconSize = theme.DimensionPx("iconSizeSM").value_or(16);
    const int iconLeft = theme.DimensionPx("spaceXXS").value_or(4);
    const int lineWidth = theme.DimensionPx("lineWidthMD").value_or(2);

    QRect checkRect(rect.x() + iconLeft, rect.center().y() - (iconSize / 2), iconSize, iconSize);

    // Draw checkmark
    painter.setPen(QPen(theme.Color(ColorToken::colorPrimary), lineWidth));

    // Checkmark path: derive geometry from icon size so it scales with theme tokens.
    const int x = checkRect.center().x();
    const int y = checkRect.center().y();
    const int leftOffset = iconSize / 4;
    const int middleXOffset = lineWidth / 2;
    const int lowerYOffset = (iconSize * 3) / 16;
    const int rightOffset = (iconSize * 5) / 16;
    const int upperYOffset = iconSize / 4;
    painter.drawLine(x - leftOffset, y, x - middleXOffset, y + lowerYOffset);
    painter.drawLine(x - middleXOffset, y + lowerYOffset, x + rightOffset, y - upperYOffset);
}

} // namespace matcha::gui
