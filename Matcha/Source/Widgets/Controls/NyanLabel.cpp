/**
 * @file NyanLabel.cpp
 * @brief Implementation of NyanLabel role-based themed label.
 */

#include <Matcha/Widgets/Controls/NyanLabel.h>

#include "../_Private/SimpleWidgetEventFilter.h"

#include <Matcha/Interaction/Focus/MnemonicManager.h>
#include <Matcha/Interaction/Focus/MnemonicState.h>

#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>

#include <utility>

namespace matcha::gui {

namespace {

[[nodiscard]] auto LabelFontKey(LabelRole role) -> const char*
{
    switch (role) {
    case LabelRole::Title:   return "fontLG";
    case LabelRole::Name:    return "fontSM";
    case LabelRole::Body:    return "fontMD";
    case LabelRole::Caption: return "fontXS";
    default:                 return "fontMD";
    }
}

[[nodiscard]] auto LabelVariantIndex(LabelRole role) -> std::size_t
{
    return static_cast<std::size_t>(std::to_underlying(role));
}

} // anonymous namespace

NyanLabel::NyanLabel(QWidget* parent)
    : QLabel(parent)
    , ThemeAware(WidgetKind::Label)
{
    setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    _swFilter = new SimpleWidgetEventFilter(this, nullptr);
}

NyanLabel::NyanLabel(const QString& text, QWidget* parent)
    : QLabel(text, parent)
    , ThemeAware(WidgetKind::Label)
{
    setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    _swFilter = new SimpleWidgetEventFilter(this, nullptr);
}

NyanLabel::~NyanLabel()
{
    UnregisterMnemonic();
}

void NyanLabel::SetRole(LabelRole role)
{
    _role = role;
    update();
}

auto NyanLabel::Role() const -> LabelRole
{
    return _role;
}

void NyanLabel::SetElideMode(Qt::TextElideMode mode)
{
    _elideMode = mode;
    update();
}

auto NyanLabel::ElideMode() const -> Qt::TextElideMode
{
    return _elideMode;
}

void NyanLabel::SetBuddy(QWidget* buddy)
{
    _buddy = buddy;
    UpdateMnemonicRegistration();
}

auto NyanLabel::Buddy() const -> QWidget*
{
    return _buddy;
}

void NyanLabel::setText(const QString& text)
{
    QLabel::setText(text);
    UpdateMnemonicRegistration();
    update();
}

void NyanLabel::paintEvent(QPaintEvent* /*event*/)
{
    if (text().isEmpty()) {
        return;
    }

    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

    const auto istate = !isEnabled() ? InteractionState::Disabled
                                     : InteractionState::Normal;
    const auto style = Theme().Resolve(WidgetKind::Label, LabelVariantIndex(_role), istate);

    QFont f = style.font;
    if (const auto fontSpec = Theme().Font(LabelFontKey(_role))) {
        f = QFont(fontSpec->family, fontSpec->sizeInPt, fontSpec->weight, fontSpec->italic);
        if (fontSpec->letterSpacing != 0.0) {
            f.setLetterSpacing(QFont::AbsoluteSpacing, fontSpec->letterSpacing);
        }
    }
    p.setFont(f);
    p.setOpacity(style.opacity);
    p.setPen(style.foreground);

    // Check if mnemonic underline should be shown
    auto* ms = GetMnemonicState();
    bool showUnderline = (ms != nullptr) && ms->ShouldShowUnderline();

    // Elide text if necessary
    const QFontMetrics fm(f);
    const QString elidedText = fm.elidedText(text(), _elideMode, rect().width());

    // Use DrawMnemonicText if the text has a mnemonic marker
    if (text().contains(QLatin1Char('&'))) {
        MnemonicState::DrawMnemonicText(p, rect(), alignment(), text(), showUnderline);
    } else {
        p.drawText(rect(), alignment(), elidedText);
    }
}

void NyanLabel::OnThemeChanged()
{
    update();
}

void NyanLabel::UpdateMnemonicRegistration()
{
    UnregisterMnemonic();

    if (_buddy == nullptr) { return; }

    auto parsed = MnemonicState::Parse(text());
    if (parsed.mnemonicChar.isNull()) { return; }

    auto* mgr = fw::GetMnemonicManager();
    if (mgr == nullptr) { return; }

    char16_t ch = parsed.mnemonicChar.unicode();
    QWidget* buddy = _buddy;
    _mnemonicId = mgr->Register({
        fw::MnemonicScope::Global,
        ch,
        [buddy]() {
            if (buddy != nullptr) {
                buddy->setFocus(Qt::ShortcutFocusReason);
            }
        },
        {} // no aliveToken — label outlives its registration
    });
}

void NyanLabel::UnregisterMnemonic()
{
    if (_mnemonicId == 0) { return; }

    if (auto* mgr = fw::GetMnemonicManager()) {
        mgr->Unregister(_mnemonicId);
    }
    _mnemonicId = 0;
}

} // namespace matcha::gui
