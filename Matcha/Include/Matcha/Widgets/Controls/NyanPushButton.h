#pragma once

/**
 * @file NyanPushButton.h
 * @brief Token-driven multi-variant push button with size presets.
 *
 * Inherits QPushButton for Qt button semantics. Visual values are loaded
 * directly from theme token keys and painted with QPainter.
 *
 * @par Old project reference
 * - `old/NyanGuis/PublicInterfaces/NyanPushButton.h` (8 PBTNState enum)
 * - `old/NyanGuis/Gui/src/NyanPushButtonPrivate.cpp` (QSS color map:
 *   State1=Primary, State2=Secondary, State3=Ghost, State4=Danger,
 *   border-radius:3px, 12 color slots per state: bg/fg/border x 4 interaction)
 *
 * @see IThemeService.h for string-key token queries.
 */

#include <Matcha/Core/Macros.h>
#include <Matcha/Tree/FSM/WidgetEnums.h>

#include <QScopedPointer>
#include <QPushButton>

class QEvent;
class QKeyEvent;
class QMouseEvent;
class QTimerEvent;

namespace matcha::gui {

class NyanPushButtonPrivate;

/**
 * @brief Height preset for push buttons.
 *
 * The underlying value is kept for compatibility; the rendered height is read
 * from theme token keys.
 */
enum class ButtonSize : uint8_t {
    Small  = 24, ///< Compact toolbar button
    Medium = 32, ///< Default button height
    Large  = 40, ///< Prominent dialog button
};

/**
 * @brief Button corner behavior.
 */
enum class ButtonShape : uint8_t {
    Default, ///< Token-defined corner radius
    Round,   ///< Capsule radius based on height
    Square,  ///< Icon-like square button when no text is present
};

/**
 * @brief Icon placement relative to the button text.
 */
enum class ButtonIconPosition : uint8_t {
    Left,
    Right,
};

/**
 * @brief Theme-aware multi-variant push button.
 *
 * Supports icon+text, icon-only, and text-only modes.
 * Checkable mode inherits from QPushButton::setCheckable().
 * All painting uses direct QPainter calls with design tokens.
 */
class MATCHA_EXPORT NyanPushButton : public QPushButton {
    Q_OBJECT

public:
    /**
     * @brief Construct a push button.
     * @param theme Theme service reference (must outlive this widget).
     * @param parent Optional parent widget.
     */
    explicit NyanPushButton(QWidget* parent = nullptr);

    /**
     * @brief Construct a push button with text.
     * @param theme Theme service reference.
     * @param text  Button label text.
     * @param parent Optional parent widget.
     */
    explicit NyanPushButton(const QString& text, QWidget* parent = nullptr);

    /**
     * @brief Construct a push button with icon and text.
     * @param theme Theme service reference.
     * @param icon  Button icon.
     * @param text  Button label text.
     * @param parent Optional parent widget.
     */
    NyanPushButton(const QIcon& icon, const QString& text,
                   QWidget* parent = nullptr);

    /// @brief Destructor.
    ~NyanPushButton() override;

    NyanPushButton(const NyanPushButton&)            = delete;
    NyanPushButton& operator=(const NyanPushButton&) = delete;
    NyanPushButton(NyanPushButton&&)                 = delete;
    NyanPushButton& operator=(NyanPushButton&&)      = delete;

    /// @brief Set the visual variant.
    void SetVariant(ButtonVariant variant);

    /// @brief Get the current visual variant.
    [[nodiscard]] auto Variant() const -> ButtonVariant;

    /// @brief Set the size preset (changes fixed height).
    void SetSize(ButtonSize size);

    /// @brief Get the current size preset.
    [[nodiscard]] auto Size() const -> ButtonSize;

    /// @brief Set the button shape.
    void SetShape(ButtonShape shape);

    /// @brief Get the current button shape.
    [[nodiscard]] auto Shape() const -> ButtonShape;

    /// @brief Show or hide the loading spinner.
    void SetLoading(bool loading);

    /// @brief Return whether loading mode is active.
    [[nodiscard]] auto IsLoading() const -> bool;

    /// @brief Configure whether loading mode blocks click/key activation.
    void SetLoadingBlocksClick(bool blocksClick);

    /// @brief Return whether loading mode blocks click/key activation.
    [[nodiscard]] auto LoadingBlocksClick() const -> bool;

    /// @brief Set icon placement relative to text.
    void SetIconPosition(ButtonIconPosition iconPosition);

    /// @brief Get icon placement relative to text.
    [[nodiscard]] auto IconPosition() const -> ButtonIconPosition;

    /// @brief Convenience helper for switching to or from the danger variant.
    void SetDanger(bool danger);

    /// @brief Return whether the button currently uses the danger variant.
    [[nodiscard]] auto IsDanger() const -> bool;

    /// @brief Token-based preferred size.
    [[nodiscard]] auto sizeHint() const -> QSize override;

    /// @brief Token-based minimum size.
    [[nodiscard]] auto minimumSizeHint() const -> QSize override;

protected:
    /// @brief Custom paint: rounded rect + text/icon using variant colors.
    void paintEvent(QPaintEvent* event) override;

    bool event(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void changeEvent(QEvent* event) override;
    void timerEvent(QTimerEvent* event) override;

Q_SIGNALS:
    void VariantChanged(ButtonVariant variant);
    void SizeChanged(ButtonSize size);
    void ShapeChanged(ButtonShape shape);
    void LoadingChanged(bool loading);
    void IconPositionChanged(ButtonIconPosition iconPosition);

private:
    Q_DECLARE_PRIVATE(NyanPushButton)

    void OnThemeChanged();

    QScopedPointer<NyanPushButtonPrivate> d_ptr;
};

} // namespace matcha::gui
