/**
 * @file NyanPushButton.cpp
 * @brief Implementation of NyanPushButton token-driven button.
 */

#include <Matcha/Widgets/Controls/NyanPushButton.h>

#include <Matcha/Theming/IThemeService.h>

#include <QBasicTimer>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMetaObject>
#include <QMouseEvent>
#include <QObject>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QRectF>
#include <QSizePolicy>
#include <QTimerEvent>

#include <algorithm>

namespace matcha::gui {

namespace {

enum class ButtonVisualState : uint8_t {
    Normal,
    Hover,
    Pressed,
    Checked,
    Focused,
    Disabled,
    Loading,
};

struct ButtonStyleToken {
    int heightSm = 24;
    int heightMd = 32;
    int heightLg = 40;
    int paddingHorizontalSm = 8;
    int paddingHorizontalMd = 12;
    int paddingHorizontalLg = 16;
    int iconSizeSm = 14;
    int iconSizeMd = 16;
    int iconSizeLg = 18;
    int iconGap = 6;
    int radius = 3;
    int borderWidth = 1;
    QColor defaultBg = QColor("#FFFFFFFF");
    QColor defaultBgHover = QColor("#0D000000");
    QColor defaultBgPressed = QColor("#1A000000");
    QColor defaultText = QColor("#E0000000");
    QColor defaultTextDisabled = QColor("#40000000");
    QColor defaultBorder = QColor("#D8D8DA");
    QColor defaultBorderHover = QColor("#0066FF");
    QColor defaultBorderPressed = QColor("#004FD9");
    QColor defaultBottomLineColor = QColor("#E3E3E6");
    QColor primaryBg = QColor("#0066FF");
    QColor primaryBgHover = QColor("#2986FF");
    QColor primaryBgPressed = QColor("#004FD9");
    QColor primaryText = QColor("#E0FFFFFF");
    QColor primaryTextDisabled = QColor("#40000000");
    QColor primaryBorder = QColor("#0066FF");
    QColor dangerBg = QColor("#CC1423");
    QColor dangerBgHover = QColor("#D93840");
    QColor dangerBgPressed = QColor("#A6081B");
    QColor dangerText = QColor("#E0FFFFFF");
    QColor dangerTextDisabled = QColor("#40000000");
    QColor dangerBorder = QColor("#CC1423");
    QColor textBgHover = QColor("#0D000000");
    QColor linkText = QColor("#0066FF");
    QColor linkTextHover = QColor("#2986FF");
    QColor linkTextPressed = QColor("#004FD9");
    QColor checkedBg = QColor("#F0F7FF");
    QColor checkedText = QColor("#0066FF");
    QColor checkedBorder = QColor("#0066FF");
    QColor focusRing = QColor("#0066FF");
    FontSpec font;
};

[[nodiscard]] auto IsFilledVariant(ButtonVariant variant) -> bool
{
    return variant == ButtonVariant::Primary || variant == ButtonVariant::Danger;
}

[[nodiscard]] auto IsFrameVisibleVariant(ButtonVariant variant) -> bool
{
    return variant != ButtonVariant::Text && variant != ButtonVariant::Link;
}

} // namespace

class NyanPushButtonPrivate {
    Q_DECLARE_PUBLIC(NyanPushButton)

public:
    explicit NyanPushButtonPrivate(NyanPushButton* q)
        : q_ptr(q)
    {
    }

    ~NyanPushButtonPrivate()
    {
        QObject::disconnect(theme_connection_);
    }

    void Initialize()
    {
        Q_Q(NyanPushButton);

        q->setMouseTracking(true);
        q->setAttribute(Qt::WA_Hover, true);
        q->setFocusPolicy(Qt::StrongFocus);
        q->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        q->setFlat(true);

        BindThemeService();
        RefreshFromTheme();
        ApplyFixedHeight();
    }

    void BindThemeService()
    {
        Q_Q(NyanPushButton);

        QObject::disconnect(theme_connection_);
        auto& theme = GetThemeService();
        theme_connection_ = QObject::connect(
            &theme, &IThemeService::ThemeChanged, q, [this](const QString&) {
                Q_Q(NyanPushButton);
                q->OnThemeChanged();
            });
    }

    void RefreshFromTheme()
    {
        const auto& theme = GetThemeService();
        auto token = ButtonStyleToken{};

        token.heightSm = theme.DimensionPx("controlHeightSM").value_or(token.heightSm);
        token.heightMd = theme.DimensionPx("controlHeightMD").value_or(token.heightMd);
        token.heightLg = theme.DimensionPx("controlHeightXL").value_or(token.heightLg);
        token.paddingHorizontalSm = theme.DimensionPx("spaceXS").value_or(token.paddingHorizontalSm);
        token.paddingHorizontalMd = theme.DimensionPx("spaceSM").value_or(token.paddingHorizontalMd);
        token.paddingHorizontalLg = theme.DimensionPx("spaceMS").value_or(token.paddingHorizontalLg);
        token.iconSizeSm = theme.DimensionPx("iconSizeSM").value_or(token.iconSizeSm);
        token.iconSizeMd = theme.DimensionPx("iconSizeSM").value_or(token.iconSizeMd);
        token.iconSizeLg = theme.DimensionPx("iconSizeMS").value_or(token.iconSizeLg);
        token.iconGap = theme.DimensionPx("spaceXXS").value_or(token.iconGap);
        token.radius = theme.DimensionPx("radiusLarge").value_or(token.radius);
        token.borderWidth = theme.DimensionPx("lineWidthMS").value_or(token.borderWidth);

        token.defaultBg = theme.Color("colorBgContainer").value_or(token.defaultBg);
        token.defaultBgHover = theme.Color("colorBgContainer").value_or(token.defaultBg);
        token.defaultBgPressed = theme.Color("colorBgContainer").value_or(token.defaultBg);
        token.defaultText = theme.Color("colorText").value_or(token.defaultText);
        token.defaultTextDisabled = theme.Color("colorTextQuaternary").value_or(token.defaultTextDisabled);
        token.defaultBorder = theme.Color("colorBorder").value_or(token.defaultBorder);
        token.defaultBorderHover = theme.Color("colorPrimary").value_or(token.defaultBorderHover);
        token.defaultBorderPressed = theme.Color("colorPrimaryActive").value_or(token.defaultBorderPressed);
        token.defaultBottomLineColor = theme.Color("colorDivider").value_or(token.defaultBottomLineColor);

        token.primaryBg = theme.Color("colorPrimary").value_or(token.primaryBg);
        token.primaryBgHover = theme.Color("colorPrimaryBgHover").value_or(token.primaryBgHover);
        token.primaryBgPressed = theme.Color("colorPrimaryActive").value_or(token.primaryBgPressed);
        token.primaryText = theme.Color("colorTextRev").value_or(token.primaryText);
        token.primaryTextDisabled = theme.Color("colorTextQuaternary").value_or(token.primaryTextDisabled);
        token.primaryBorder = theme.Color("colorPrimary").value_or(token.primaryBorder);

        token.dangerBg = theme.Color("colorError").value_or(token.dangerBg);
        token.dangerBgHover = theme.Color("colorErrorHover").value_or(token.dangerBgHover);
        token.dangerBgPressed = theme.Color("colorErrorActive").value_or(token.dangerBgPressed);
        token.dangerText = theme.Color("colorTextRev").value_or(token.dangerText);
        token.dangerTextDisabled = theme.Color("colorTextQuaternary").value_or(token.dangerTextDisabled);
        token.dangerBorder = theme.Color("colorError").value_or(token.dangerBorder);

        token.textBgHover = theme.Color("colorFillHover").value_or(token.textBgHover);
        token.linkText = theme.Color("colorPrimary").value_or(token.linkText);
        token.linkTextHover = theme.Color("colorPrimaryBgHover").value_or(token.linkTextHover);
        token.linkTextPressed = theme.Color("colorPrimaryActive").value_or(token.linkTextPressed);
        token.checkedBg = theme.Color("colorPrimaryBg").value_or(token.checkedBg);
        token.checkedText = theme.Color("colorPrimary").value_or(token.checkedText);
        token.checkedBorder = theme.Color("colorPrimary").value_or(token.checkedBorder);
        token.focusRing = theme.Color("Focus").value_or(token.primaryBg);

        token.font = theme.Font("fontMS").value_or(FontSpec{});

        button_token_ = token;
        UpdateFontFromToken();
        ApplyFixedHeight();
    }

    void UpdateFontFromToken()
    {
        QFont font = q_ptr->font();
        if (!button_token_.font.family.isEmpty()) {
            font.setFamily(button_token_.font.family);
        }
        font.setPointSize(std::max(1, button_token_.font.sizeInPt));
        font.setWeight(static_cast<QFont::Weight>(button_token_.font.weight));
        font.setItalic(button_token_.font.italic);
        font.setLetterSpacing(QFont::AbsoluteSpacing, button_token_.font.letterSpacing);
        q_ptr->setFont(font);
    }

    void ApplyFixedHeight()
    {
        q_ptr->setFixedHeight(HeightForSize());
    }

    void StartLoadingTimer()
    {
        if (!loading_timer_.isActive()) {
            loading_timer_.start(33, q_ptr);
        }
    }

    void StopLoadingTimer()
    {
        if (loading_timer_.isActive()) {
            loading_timer_.stop();
        }
        loading_angle_ = 0;
    }

    [[nodiscard]] auto ResolveVisualState() const -> ButtonVisualState
    {
        if (!q_ptr->isEnabled()) {
            return ButtonVisualState::Disabled;
        }
        if (loading_ && loading_blocks_click_) {
            return ButtonVisualState::Loading;
        }
        if (pressed_ || key_pressed_ || q_ptr->isDown()) {
            return ButtonVisualState::Pressed;
        }
        if (q_ptr->isCheckable() && q_ptr->isChecked()) {
            return ButtonVisualState::Checked;
        }
        if (q_ptr->underMouse()) {
            return ButtonVisualState::Hover;
        }
        if (q_ptr->hasFocus()) {
            return ButtonVisualState::Focused;
        }
        return ButtonVisualState::Normal;
    }

    void Paint(QPainter* painter) const
    {
        if (painter == nullptr || q_ptr->rect().isEmpty()) {
            return;
        }

        painter->save();
        painter->setRenderHints(
            QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
        painter->setFont(q_ptr->font());

        const ButtonVisualState visualState = ResolveVisualState();
        const QRect foreground = ForegroundRect();
        PaintFrame(painter, visualState, foreground);
        PaintContent(painter, visualState, foreground);
        PaintFocusRing(painter, foreground);

        painter->restore();
    }

    [[nodiscard]] auto CalculateSizeHint() const -> QSize
    {
        const int height = HeightForSize();
        const int padding = HorizontalPadding();
        const int iconSide = IconSize();
        const bool hasText = !q_ptr->text().isEmpty();
        const bool hasIcon = !q_ptr->icon().isNull();
        const bool hasVisualIcon = hasIcon || loading_;

        const QFontMetrics metrics(q_ptr->font());
        int width = padding * 2;

        if (hasText) {
            width += metrics.horizontalAdvance(q_ptr->text());
        }
        if (hasVisualIcon) {
            width += iconSide;
            if (hasText) {
                width += button_token_.iconGap;
            }
        }
        if (button_shape_ == ButtonShape::Square && !hasText) {
            width = std::max(width, height);
        }

        return {std::max(width, height), height};
    }

    [[nodiscard]] auto HeightForSize() const -> int
    {
        switch (button_size_) {
        case ButtonSize::Small:
            return std::max(16, button_token_.heightSm);
        case ButtonSize::Large:
            return std::max(16, button_token_.heightLg);
        case ButtonSize::Medium:
        default:
            return std::max(16, button_token_.heightMd);
        }
    }

    [[nodiscard]] auto HorizontalPadding() const -> int
    {
        switch (button_size_) {
        case ButtonSize::Small:
            return std::max(0, button_token_.paddingHorizontalSm);
        case ButtonSize::Large:
            return std::max(0, button_token_.paddingHorizontalLg);
        case ButtonSize::Medium:
        default:
            return std::max(0, button_token_.paddingHorizontalMd);
        }
    }

    [[nodiscard]] auto IconSize() const -> int
    {
        switch (button_size_) {
        case ButtonSize::Small:
            return std::max(8, button_token_.iconSizeSm);
        case ButtonSize::Large:
            return std::max(8, button_token_.iconSizeLg);
        case ButtonSize::Medium:
        default:
            return std::max(8, button_token_.iconSizeMd);
        }
    }

    [[nodiscard]] auto ShouldBlockClick() const -> bool
    {
        return loading_ && loading_blocks_click_;
    }

    [[nodiscard]] auto ForegroundRect() const -> QRect
    {
        const int inset = std::max(0, button_token_.borderWidth);
        return q_ptr->rect().adjusted(inset, inset, -inset, -inset);
    }

    [[nodiscard]] auto BackgroundColor(ButtonVisualState visualState) const -> QColor
    {
        const bool pressed = pressed_ || key_pressed_ || q_ptr->isDown();
        const bool checked = q_ptr->isCheckable() && q_ptr->isChecked();
        const bool hovered = q_ptr->underMouse();

        if (visualState == ButtonVisualState::Disabled) {
            if (variant_ == ButtonVariant::Text || variant_ == ButtonVariant::Link) {
                return QColor(Qt::transparent);
            }
            if (IsFilledVariant(variant_)) {
                QColor disabled = button_token_.defaultBgPressed;
                disabled.setAlpha(90);
                return disabled;
            }
            return button_token_.defaultBgPressed;
        }
        if (checked && !IsFilledVariant(variant_)) {
            return button_token_.checkedBg;
        }

        switch (variant_) {
        case ButtonVariant::Primary:
            return pressed ? button_token_.primaryBgPressed
                           : (hovered ? button_token_.primaryBgHover : button_token_.primaryBg);
        case ButtonVariant::Danger:
            return pressed ? button_token_.dangerBgPressed
                           : (hovered ? button_token_.dangerBgHover : button_token_.dangerBg);
        case ButtonVariant::Ghost:
        case ButtonVariant::Text:
            return hovered || pressed ? button_token_.textBgHover : QColor(Qt::transparent);
        case ButtonVariant::Link:
            return QColor(Qt::transparent);
        case ButtonVariant::Dashed:
        case ButtonVariant::Secondary:
        default:
            return pressed ? button_token_.defaultBgPressed
                           : (hovered ? button_token_.defaultBgHover : button_token_.defaultBg);
        }
    }

    [[nodiscard]] auto BorderColor(ButtonVisualState visualState) const -> QColor
    {
        const bool pressed = pressed_ || key_pressed_ || q_ptr->isDown();
        const bool checked = q_ptr->isCheckable() && q_ptr->isChecked();
        const bool hovered = q_ptr->underMouse();

        if (visualState == ButtonVisualState::Disabled) {
            return button_token_.defaultBorder;
        }
        if (checked && !IsFilledVariant(variant_)) {
            return button_token_.checkedBorder;
        }

        switch (variant_) {
        case ButtonVariant::Primary:
            return button_token_.primaryBorder;
        case ButtonVariant::Danger:
            return pressed ? button_token_.dangerBgPressed
                           : (hovered ? button_token_.dangerBgHover : button_token_.dangerBorder);
        case ButtonVariant::Text:
        case ButtonVariant::Link:
            return QColor(Qt::transparent);
        case ButtonVariant::Ghost:
        case ButtonVariant::Dashed:
        case ButtonVariant::Secondary:
        default:
            return pressed ? button_token_.defaultBorderPressed
                           : (hovered ? button_token_.defaultBorderHover : button_token_.defaultBorder);
        }
    }

    [[nodiscard]] auto TextColor(ButtonVisualState visualState) const -> QColor
    {
        const bool pressed = pressed_ || key_pressed_ || q_ptr->isDown();
        const bool checked = q_ptr->isCheckable() && q_ptr->isChecked();
        const bool hovered = q_ptr->underMouse();

        if (visualState == ButtonVisualState::Disabled) {
            if (variant_ == ButtonVariant::Primary) {
                return button_token_.primaryTextDisabled;
            }
            if (variant_ == ButtonVariant::Danger) {
                return button_token_.dangerTextDisabled;
            }
            return button_token_.defaultTextDisabled;
        }
        if (checked && !IsFilledVariant(variant_)) {
            return button_token_.checkedText;
        }

        switch (variant_) {
        case ButtonVariant::Primary:
            return button_token_.primaryText;
        case ButtonVariant::Danger:
            return button_token_.dangerText;
        case ButtonVariant::Link:
            return pressed ? button_token_.linkTextPressed
                           : (hovered ? button_token_.linkTextHover : button_token_.linkText);
        case ButtonVariant::Ghost:
        case ButtonVariant::Text:
            return pressed ? button_token_.linkTextPressed
                           : (hovered ? button_token_.linkTextHover : button_token_.defaultText);
        case ButtonVariant::Dashed:
        case ButtonVariant::Secondary:
        default:
            return pressed ? button_token_.defaultBorderPressed
                           : (hovered ? button_token_.defaultBorderHover : button_token_.defaultText);
        }
    }

    [[nodiscard]] auto CornerRadius(const QRect& rect) const -> qreal
    {
        if (button_shape_ == ButtonShape::Round) {
            return rect.height() / 2.0;
        }
        return std::max(0, button_token_.radius);
    }

    void PaintFrame(QPainter* painter, ButtonVisualState visualState, const QRect& rect) const
    {
        if (painter == nullptr || rect.isEmpty()) {
            return;
        }

        const qreal radius = CornerRadius(rect);
        const QColor background = BackgroundColor(visualState);
        if (background.isValid() && background.alpha() > 0) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(background);
            painter->drawRoundedRect(rect, radius, radius);
        }

        if (IsFrameVisibleVariant(variant_)) {
            const QColor border = BorderColor(visualState);
            if (border.isValid() && border.alpha() > 0 && button_token_.borderWidth > 0) {
                QPen pen(border, button_token_.borderWidth);
                if (variant_ == ButtonVariant::Dashed) {
                    pen.setStyle(Qt::DashLine);
                }
                painter->setPen(pen);
                painter->setBrush(Qt::NoBrush);
                painter->drawRoundedRect(rect, radius, radius);
            }
        }

        const bool pressed = pressed_ || key_pressed_ || q_ptr->isDown();
        if (pressed
            || variant_ != ButtonVariant::Secondary
            || !button_token_.defaultBottomLineColor.isValid()
            || button_token_.defaultBottomLineColor.alpha() == 0) {
            return;
        }

        painter->setPen(QPen(button_token_.defaultBottomLineColor, 1));
        const int lineRadius = static_cast<int>(radius);
        painter->drawLine(rect.left() + lineRadius, rect.bottom(), rect.right() - lineRadius, rect.bottom());
    }

    void PaintContent(QPainter* painter, ButtonVisualState visualState, const QRect& rect) const
    {
        if (painter == nullptr || rect.isEmpty()) {
            return;
        }

        const QString text = q_ptr->text();
        const QIcon icon = q_ptr->icon();
        const bool hasText = !text.isEmpty();
        const bool hasIcon = !icon.isNull();
        const bool hasVisualIcon = loading_ || hasIcon;
        const QSize iconSide(IconSize(), IconSize());
        const QFontMetrics metrics(painter->font());
        const int textWidth = hasText ? metrics.horizontalAdvance(text) : 0;
        const int iconWidth = hasVisualIcon ? iconSide.width() : 0;
        const int gap = (hasVisualIcon && hasText) ? button_token_.iconGap : 0;
        int cursorX = rect.left() + (rect.width() - iconWidth - gap - textWidth) / 2;

        QRect iconRect;
        QRect textRect;
        const auto placeIcon = [&]() {
            const int iconTop = rect.top() + (rect.height() - iconSide.height()) / 2;
            iconRect = QRect(cursorX, iconTop, iconSide.width(), iconSide.height());
            cursorX += iconSide.width() + gap;
        };
        const auto placeText = [&]() {
            textRect = QRect(cursorX, rect.top(), textWidth, rect.height());
            cursorX += textWidth + gap;
        };

        if (icon_position_ == ButtonIconPosition::Left) {
            if (hasVisualIcon) {
                placeIcon();
            }
            if (hasText) {
                placeText();
            }
        } else {
            if (hasText) {
                placeText();
            }
            if (hasVisualIcon) {
                placeIcon();
            }
        }

        const QColor contentColor = TextColor(visualState);
        painter->setPen(contentColor);
        if (loading_) {
            PaintLoading(painter, contentColor, iconRect);
        } else if (hasIcon) {
            const QIcon::Mode mode = q_ptr->isEnabled() ? QIcon::Normal : QIcon::Disabled;
            const QIcon::State state = q_ptr->isCheckable() && q_ptr->isChecked() ? QIcon::On : QIcon::Off;
            icon.paint(painter, iconRect, Qt::AlignCenter, mode, state);
        }

        if (hasText) {
            painter->drawText(textRect, Qt::AlignCenter, text);
        }
    }

    void PaintLoading(QPainter* painter, const QColor& color, const QRect& rect) const
    {
        if (painter == nullptr || rect.isEmpty()) {
            return;
        }

        painter->save();
        painter->setPen(QPen(color, std::max(2, button_token_.borderWidth + 1), Qt::SolidLine, Qt::RoundCap));
        painter->setBrush(Qt::NoBrush);
        painter->translate(rect.left() + rect.width() / 2.0, rect.top() + rect.height() / 2.0);
        painter->rotate(loading_angle_);

        const QRectF arcRect(
            -rect.width() / 2.0 + 2.0,
            -rect.height() / 2.0 + 2.0,
            rect.width() - 4.0,
            rect.height() - 4.0);
        painter->drawArc(arcRect, 0, 270 * 16);
        painter->restore();
    }

    void PaintFocusRing(QPainter* painter, const QRect& rect) const
    {
        if (painter == nullptr || rect.isEmpty() || !q_ptr->hasFocus()) {
            return;
        }

        QColor ringColor = button_token_.focusRing;
        ringColor.setAlpha(200);

        QPen pen(ringColor, 2);
        pen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);

        const QRect ringRect = q_ptr->rect().adjusted(1, 1, -1, -1);
        painter->drawRoundedRect(ringRect, CornerRadius(rect), CornerRadius(rect));
    }

    NyanPushButton* q_ptr = nullptr;
    ButtonStyleToken button_token_;
    ButtonVariant variant_ = ButtonVariant::Secondary;
    ButtonSize button_size_ = ButtonSize::Medium;
    ButtonShape button_shape_ = ButtonShape::Default;
    ButtonIconPosition icon_position_ = ButtonIconPosition::Left;
    bool loading_ = false;
    bool loading_blocks_click_ = true;
    bool pressed_ = false;
    bool key_pressed_ = false;
    int loading_angle_ = 0;
    QBasicTimer loading_timer_;
    QMetaObject::Connection theme_connection_;
};

// ============================================================================
// Construction
// ============================================================================

NyanPushButton::NyanPushButton(QWidget* parent)
    : QPushButton(parent)
    , d_ptr(new NyanPushButtonPrivate(this))
{
    Q_D(NyanPushButton);
    d->Initialize();
}

NyanPushButton::NyanPushButton(const QString& text, QWidget* parent)
    : QPushButton(text, parent)
    , d_ptr(new NyanPushButtonPrivate(this))
{
    Q_D(NyanPushButton);
    d->Initialize();
}

NyanPushButton::NyanPushButton(const QIcon& icon, const QString& text,
                               QWidget* parent)
    : QPushButton(icon, text, parent)
    , d_ptr(new NyanPushButtonPrivate(this))
{
    Q_D(NyanPushButton);
    d->Initialize();
}

NyanPushButton::~NyanPushButton() = default;

// ============================================================================
// Public API
// ============================================================================

void NyanPushButton::SetVariant(ButtonVariant variant)
{
    Q_D(NyanPushButton);
    if (d->variant_ == variant) {
        return;
    }
    d->variant_ = variant;
    update();
    emit VariantChanged(variant);
}

auto NyanPushButton::Variant() const -> ButtonVariant
{
    Q_D(const NyanPushButton);
    return d->variant_;
}

void NyanPushButton::SetSize(ButtonSize size)
{
    Q_D(NyanPushButton);
    if (d->button_size_ == size) {
        return;
    }
    d->button_size_ = size;
    d->ApplyFixedHeight();
    updateGeometry();
    update();
    emit SizeChanged(size);
}

auto NyanPushButton::Size() const -> ButtonSize
{
    Q_D(const NyanPushButton);
    return d->button_size_;
}

void NyanPushButton::SetShape(ButtonShape shape)
{
    Q_D(NyanPushButton);
    if (d->button_shape_ == shape) {
        return;
    }
    d->button_shape_ = shape;
    updateGeometry();
    update();
    emit ShapeChanged(shape);
}

auto NyanPushButton::Shape() const -> ButtonShape
{
    Q_D(const NyanPushButton);
    return d->button_shape_;
}

void NyanPushButton::SetLoading(bool loading)
{
    Q_D(NyanPushButton);
    if (d->loading_ == loading) {
        return;
    }
    d->loading_ = loading;
    if (loading) {
        d->StartLoadingTimer();
    } else {
        d->StopLoadingTimer();
    }
    updateGeometry();
    update();
    emit LoadingChanged(loading);
}

auto NyanPushButton::IsLoading() const -> bool
{
    Q_D(const NyanPushButton);
    return d->loading_;
}

void NyanPushButton::SetLoadingBlocksClick(bool blocksClick)
{
    Q_D(NyanPushButton);
    d->loading_blocks_click_ = blocksClick;
}

auto NyanPushButton::LoadingBlocksClick() const -> bool
{
    Q_D(const NyanPushButton);
    return d->loading_blocks_click_;
}

void NyanPushButton::SetIconPosition(ButtonIconPosition iconPosition)
{
    Q_D(NyanPushButton);
    if (d->icon_position_ == iconPosition) {
        return;
    }
    d->icon_position_ = iconPosition;
    update();
    emit IconPositionChanged(iconPosition);
}

auto NyanPushButton::IconPosition() const -> ButtonIconPosition
{
    Q_D(const NyanPushButton);
    return d->icon_position_;
}

void NyanPushButton::SetDanger(bool danger)
{
    SetVariant(danger ? ButtonVariant::Danger : ButtonVariant::Secondary);
}

auto NyanPushButton::IsDanger() const -> bool
{
    Q_D(const NyanPushButton);
    return d->variant_ == ButtonVariant::Danger;
}

auto NyanPushButton::sizeHint() const -> QSize
{
    Q_D(const NyanPushButton);
    return d->CalculateSizeHint();
}

auto NyanPushButton::minimumSizeHint() const -> QSize
{
    Q_D(const NyanPushButton);
    const int height = d->HeightForSize();
    return {height, height};
}

// ============================================================================
// Painting and Events
// ============================================================================

void NyanPushButton::paintEvent(QPaintEvent* /*event*/)
{
    Q_D(NyanPushButton);
    QPainter painter(this);
    d->Paint(&painter);
}

bool NyanPushButton::event(QEvent* event)
{
    if (event != nullptr) {
        switch (event->type()) {
        case QEvent::Enter:
        case QEvent::Leave:
        case QEvent::HoverEnter:
        case QEvent::HoverLeave:
        case QEvent::HoverMove:
            update();
            break;
        default:
            break;
        }
    }
    return QPushButton::event(event);
}

void NyanPushButton::mousePressEvent(QMouseEvent* event)
{
    Q_D(NyanPushButton);
    if (d->ShouldBlockClick()) {
        if (event != nullptr) {
            event->accept();
        }
        return;
    }

    if (event != nullptr && event->button() == Qt::LeftButton) {
        d->pressed_ = true;
        update();
    }
    QPushButton::mousePressEvent(event);
}

void NyanPushButton::mouseReleaseEvent(QMouseEvent* event)
{
    Q_D(NyanPushButton);
    const bool wasPressed = d->pressed_;
    d->pressed_ = false;
    if (wasPressed) {
        update();
    }

    if (d->ShouldBlockClick()) {
        if (event != nullptr) {
            event->accept();
        }
        return;
    }

    QPushButton::mouseReleaseEvent(event);
}

void NyanPushButton::keyPressEvent(QKeyEvent* event)
{
    Q_D(NyanPushButton);
    if (d->ShouldBlockClick()) {
        if (event != nullptr) {
            event->accept();
        }
        return;
    }

    if (event != nullptr && (event->key() == Qt::Key_Space
        || event->key() == Qt::Key_Return
        || event->key() == Qt::Key_Enter)) {
        d->key_pressed_ = true;
        update();
    }
    QPushButton::keyPressEvent(event);
}

void NyanPushButton::keyReleaseEvent(QKeyEvent* event)
{
    Q_D(NyanPushButton);
    const bool wasKeyPressed = d->key_pressed_;
    d->key_pressed_ = false;
    if (wasKeyPressed) {
        update();
    }

    if (d->ShouldBlockClick()) {
        if (event != nullptr) {
            event->accept();
        }
        return;
    }

    QPushButton::keyReleaseEvent(event);
}

void NyanPushButton::changeEvent(QEvent* event)
{
    QPushButton::changeEvent(event);
    if (event == nullptr) {
        return;
    }

    switch (event->type()) {
    case QEvent::EnabledChange:
        update();
        break;
    case QEvent::FontChange:
        updateGeometry();
        update();
        break;
    default:
        break;
    }
}

void NyanPushButton::timerEvent(QTimerEvent* event)
{
    Q_D(NyanPushButton);
    if (event != nullptr && event->timerId() == d->loading_timer_.timerId()) {
        d->loading_angle_ = (d->loading_angle_ + 30) % 360;
        update();
        return;
    }

    QPushButton::timerEvent(event);
}

void NyanPushButton::OnThemeChanged()
{
    Q_D(NyanPushButton);
    d->RefreshFromTheme();
    updateGeometry();
    update();
}

} // namespace matcha::gui
