/**
 * @file NyanTheme.cpp
 * @brief NyanTheme concrete implementation of IThemeService.
 */

// Windows headers MUST come before Qt to avoid macro pollution.
// We need SystemParameterscolorInfoW / GetDoubleClickTime for §8.7 platform timing queries.
#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QOperatingSystemVersion>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Matcha/Theming/NyanTheme.h"
#include "Matcha/Theming/Palette/ContrastChecker.h"
#include "Matcha/Theming/Palette/DefaultPalette.h"
#include "SvgIconProvider.h"

namespace matcha::gui {

  // ============================================================================
  // ColorToken name table (must match ColorToken enum order exactly)
  // ============================================================================

  namespace {

    constexpr std::array<const char*, kColorTokenCount> kColorTokenNames = {
    // 主题色：品牌色 / 品牌辅助色 (13 个)
    "colorPrimaryBase",
    "colorPrimaryBg",
    "colorPrimaryBgActive",
    "colorPrimaryDisabled",
    "colorPrimaryBorder",
    "colorPrimaryBgHover",
    "colorPrimary",
    "colorPrimaryActive",
    "colorPrimaryGradient",
    "colorPrimaryNavBase",
    "colorPrimaryNav",
    "colorPrimaryNavRev",
    "colorPrimaryNavSecondaryRev",
    "colorPrimaryNavTertiaryRev",
    "colorPrimaryNavQuaternaryRev",

    // 功能色：成功/警告/错误 (24 个)
    "colorSuccessBase",
    "colorSuccessBg",
    "colorSuccessBgActive",
    "colorSuccessDisabled",
    "colorSuccessBorder",
    "colorSuccessHover",
    "colorSuccess",
    "colorSuccessActive",
    "colorSuccessGradient",
    "colorWarningBase",
    "colorWarningBg",
    "colorWarningBgActive",
    "colorWarningDisabled",
    "colorWarningBorder",
    "colorWarningHover",
    "colorWarning",
    "colorWarningActive",
    "colorWarningGradient",
    "colorErrorBase",
    "colorErrorBg",
    "colorErrorBgActive",
    "colorErrorDisabled",
    "colorErrorBorder",
    "colorErrorHover",
    "colorError",
    "colorErrorActive",
    "colorErrorGradient",

    // 文本色：文本 / 反色文本 (8 个)
    "colorTextBase",
    "colorText",
    "colorTextSecondary",
    "colorTextTertiary",
    "colorTextQuaternary",
    "colorTextRev",
    "colorTextSecondaryRev",
    "colorTextTertiaryRev",
    "colorTextQuaternaryRev",

    // 中性色阶：填充 / 交互 / 边框 (22 个)
    "colorBgBase",
    "colorFill",
    "colorFillSecondary",
    "colorFillTertiary",
    "colorFillQuaternary",
    "colorFillHover",
    "colorFillSecondaryHover",
    "colorFillTertiaryHover",
    "colorFillQuaternaryHover",
    "colorBgContainer",
    "colorBgContainerSecondary",
    "colorBgContainerTertiary",
    "colorBgContainerQuaternary",
    "colorBgContainerQuinary",
    "colorBgRender",
    "colorBgRenderSecondary",
    "colorBgRenderTertiary",
    "colorBgRenderQuaternary",
    "colorBorder",
    "colorBorderSecondary",
    "colorDivider",
    "colorDividerSecondary",
    "colorDividerTertiary",

    // 特殊用途Token (9 个)
    "OnAccent",
    "OnAccentSecondary",
    "Focus",
    "Selection",
    "Link",
    "Scrim",
    "Overlay",
    "Shadow",
    "Separator",
};

    [[nodiscard]] auto TokenKey(std::string_view key) -> std::string {
      return {key.data(), key.size()};
    }

    struct PlatformFontFamilies {
      QString sansFamily;
      QString monoFamily;
    };

    [[nodiscard]] auto DetectPlatformFontFamilies() -> PlatformFontFamilies {
      PlatformFontFamilies families;

#if defined(Q_OS_WIN)
      families.sansFamily = QStringLiteral("Segoe UI");
      families.monoFamily = QStringLiteral("Cascadia Code");
#elif defined(Q_OS_MACOS)
      families.sansFamily = QStringLiteral("SF Pro Text");
      families.monoFamily = QStringLiteral("SF Mono");
#else
      families.sansFamily = QStringLiteral("Noto Sans");
      families.monoFamily = QStringLiteral("Noto Sans Mono");
#endif

      const auto availableFamilies = QFontDatabase::families();
      if (!availableFamilies.contains(families.sansFamily)) {
        families.sansFamily = QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
      }
      if (!availableFamilies.contains(families.monoFamily)) {
        families.monoFamily = QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
      }

      return families;
    }

    [[nodiscard]] auto DefaultFontSpec(QString family, int sizeInPt) -> FontSpec {
      return FontSpec{
          .family = std::move(family),
          .sizeInPt = sizeInPt,
          .weight = 400,
          .italic = false,
          .lineHeightMultiplier = 1.4,
          .letterSpacing = 0.0,
      };
    }

    [[nodiscard]] auto ScaledFontSpec(FontSpec spec, float scale) -> FontSpec {
      const int scaled = static_cast<int>(std::lroundf(static_cast<float>(spec.sizeInPt) * scale));
      spec.sizeInPt = std::max(scaled, 6);
      return spec;
    }

    [[nodiscard]] auto LegacyColorTokenKey(ColorToken token) -> std::optional<std::string_view> {
      const auto idx = std::to_underlying(token);
      if (idx >= kColorTokenNames.size()) {
        return std::nullopt;
      }
      return std::string_view{kColorTokenNames[idx]};
    }

    [[nodiscard]] auto LegacySpaceTokenKey(fw::SpaceToken token) -> std::optional<std::string_view> {
      switch (std::to_underlying(token)) {
        case 2:
          return "spaceXXXS";
        case 4:
          return "spaceXXS";
        case 8:
          return "spaceXS";
        case 12:
          return "spaceSM";
        case 16:
          return "spaceMS";
        case 20:
          return "spaceMD";
        case 24:
          return "spaceLG";
        case 28:
          return "spaceXL";
        case 32:
          return "spaceXXL";
        default:
          return std::nullopt;
      }
    }

    [[nodiscard]] auto LegacyRadiusTokenKey(fw::RadiusToken token) -> std::optional<std::string_view> {
      switch (std::to_underlying(token)) {
        case 1:
          return "radiusSmall";
        case 3:
          return "radiusDefault";
        case 6:
          return "radiusLarge";
        case 255:
          return "radiusRound";
        default:
          return std::nullopt;
      }
    }

    [[nodiscard]] auto LegacyFontRoleKey(FontRole role) -> std::optional<std::string_view> {
      switch (role) {
        case FontRole::fontWeightRegular:
        case FontRole::fontItalic:
        case FontRole::fontLetterSpacing:
        case FontRole::fontSizeBase:
          return "fontSM";
        case FontRole::fontWeightMedium:
          return "fontMS";
        case FontRole::fontWeightBold:
        case FontRole::fontLineHeight:
          return "fontLG";
        case FontRole::fontSizeXS:
          return "fontXS";
        case FontRole::fontSizeSM:
          return "fontSM";
        case FontRole::fontSizeMD:
          return "fontMD";
        case FontRole::fontSizeLG:
        case FontRole::fontSizeXL:
        case FontRole::fontSizeXXL:
          return "fontLG";
        default:
          return std::nullopt;
      }
    }

    [[nodiscard]] auto LegacyShadowTokenKey(ShadowToken token) -> std::optional<std::string_view> {
      switch (token) {
        case ShadowToken::boxShadow:
          return "shadowSM";
        case ShadowToken::boxShadowSecondary:
          return "shadowMS";
        case ShadowToken::boxShadowTertiary:
          return "shadowMD";
        default:
          return std::nullopt;
      }
    }

    [[nodiscard]] auto LegacyShadowFromLayer(const ShadowLayerSpec& layer) -> ShadowSpec {
      return ShadowSpec{
          .offsetX = layer.offsetX,
          .offsetY = layer.offsetY,
          .blurRadius = layer.blurRadius,
          .opacity = layer.color.isValid() ? layer.color.alphaF() : 0.0,
      };
    }

    [[nodiscard]] auto ScaledPx(int basePx, float scale) -> int {
      return static_cast<int>(std::lroundf(static_cast<float>(basePx) * scale));
    }

    [[nodiscard]] auto ResolveColorValue(
        const NyanTheme& theme, const std::optional<std::string>& key, ColorToken fallback
    ) -> QColor {
      if (key) {
        if (const auto color = theme.Color(*key)) {
          return *color;
        }
      }
      return theme.Color(fallback);
    }

    [[nodiscard]] auto ResolveSpacingValue(
        const NyanTheme& theme, const std::optional<std::string>& key, SpaceToken fallback
    ) -> int {
      if (key) {
        if (const auto dimension = theme.DimensionPx(*key)) {
          return ScaledPx(*dimension, theme.CurrentDensityScale());
        }
      }
      return theme.SpacingPx(fallback);
    }

    [[nodiscard]] auto ResolveRadiusValue(
        const NyanTheme& theme, const std::optional<std::string>& key, RadiusToken fallback
    ) -> int {
      if (key) {
        if (const auto dimension = theme.DimensionPx(*key)) {
          return ScaledPx(*dimension, theme.CurrentDensityScale());
        }
      }
      return ScaledPx(theme.Radius(fallback), theme.CurrentDensityScale());
    }

    [[nodiscard]] auto ResolveMinHeightValue(
        const NyanTheme& theme, const std::optional<std::string>& key, SizeToken fallback
    ) -> int {
      if (key) {
        if (const auto dimension = theme.DimensionPx(*key)) {
          return ScaledPx(*dimension, theme.CurrentDensityScale());
        }
      }
      return ScaledPx(fw::ToPixels(fallback), theme.CurrentDensityScale());
    }

    [[nodiscard]] auto ResolveFontValue(
        const NyanTheme& theme, const std::optional<std::string>& key, FontRole fallback
    ) -> FontSpec {
      if (key) {
        if (const auto spec = theme.Font(*key)) {
          return *spec;
        }
      }
      return theme.Font(fallback);
    }

    [[nodiscard]] auto ScaleShadowLayer(const ShadowLayerSpec& layer, float scale) -> ShadowLayerSpec {
      return ShadowLayerSpec{
          .offsetX = ScaledPx(layer.offsetX, scale),
          .offsetY = ScaledPx(layer.offsetY, scale),
          .blurRadius = ScaledPx(layer.blurRadius, scale),
          .spread = ScaledPx(layer.spread, scale),
          .color = layer.color,
      };
    }

    [[nodiscard]] auto ResolveShadowLayers(
        const NyanTheme& theme, const std::optional<std::string>& key, ShadowToken fallback
    ) -> std::vector<ShadowLayerSpec> {
      std::vector<ShadowLayerSpec> result;
      const float scale = theme.CurrentDensityScale();
      if (key) {
        if (const auto layers = theme.Shadow(*key)) {
          result.reserve(layers->size());
          for (const auto& layer : *layers) {
            result.push_back(ScaleShadowLayer(layer, scale));
          }
          return result;
        }
      }

      const auto& legacy = theme.Shadow(fallback);
      if (legacy.blurRadius > 0 || legacy.offsetX != 0 || legacy.offsetY != 0 || legacy.opacity > 0.0) {
        QColor color(0, 0, 0);
        color.setAlphaF(std::clamp(static_cast<double>(legacy.opacity), 0.0, 1.0));
        result.push_back(ShadowLayerSpec{
            .offsetX = ScaledPx(legacy.offsetX, scale),
            .offsetY = ScaledPx(legacy.offsetY, scale),
            .blurRadius = ScaledPx(legacy.blurRadius, scale),
            .spread = 0,
            .color = color,
        });
      }
      return result;
    }

    void BindStateStyleKeys(VariantStyle& variant) {
      for (auto& state : variant.colors) {
        if (const auto key = LegacyColorTokenKey(state.background)) {
          state.backgroundKey = std::string{*key};
        }
        if (const auto key = LegacyColorTokenKey(state.foreground)) {
          state.foregroundKey = std::string{*key};
        }
        if (const auto key = LegacyColorTokenKey(state.border)) {
          state.borderKey = std::string{*key};
        }
        if (const auto key = LegacySpaceTokenKey(state.borderWidth)) {
          state.borderWidthKey = std::string{*key};
        }
      }
    }

    void BindWidgetStyleKeys(WidgetStyleSheet& sheet) {
      if (const auto key = LegacyRadiusTokenKey(sheet.radius)) {
        sheet.radiusKey = std::string{*key};
      }
      if (const auto key = LegacySpaceTokenKey(sheet.paddingH)) {
        sheet.paddingHKey = std::string{*key};
      }
      if (const auto key = LegacySpaceTokenKey(sheet.paddingV)) {
        sheet.paddingVKey = std::string{*key};
      }
      if (const auto key = LegacySpaceTokenKey(sheet.gap)) {
        sheet.gapKey = std::string{*key};
      }
      if (const auto key = LegacyFontRoleKey(sheet.font)) {
        sheet.fontKey = std::string{*key};
      }
      if (const auto key = LegacyShadowTokenKey(sheet.elevation)) {
        sheet.shadowKey = std::string{*key};
      }
    }

    [[nodiscard]] auto ParsePipeGradientToken(QString value, std::string_view key) -> std::optional<GradientSpec> {
      const auto stops = value.split(u'|', Qt::SkipEmptyParts);
      if (stops.size() < 2) {
        qWarning() << "Invalid gradient token:" << QString::fromUtf8(key.data(), static_cast<qsizetype>(key.size()))
                   << value;
        return std::nullopt;
      }

      GradientSpec spec;
      spec.stops.reserve(static_cast<std::size_t>(stops.size()));

      for (qsizetype i = 0; i < stops.size(); ++i) {
        const QColor color = QColor::fromString(stops.at(i).trimmed());
        if (!color.isValid()) {
          qWarning() << "Invalid gradient color:"
                     << QString::fromUtf8(key.data(), static_cast<qsizetype>(key.size())) << stops.at(i);
          return std::nullopt;
        }

        const auto denominator = static_cast<double>(stops.size() - 1);
        spec.stops.push_back(GradientStop{
            .position = static_cast<double>(i) / denominator,
            .color = color,
        });
      }

      return spec;
    }

  }  // anonymous namespace

  // ============================================================================
  // Constructor
  // ============================================================================

  NyanTheme::NyanTheme(QString palettePath, QObject* parent)
      : IThemeService(parent), _palettePath(std::move(palettePath)) {
    QueryPlatformTimings();
  }

  // ============================================================================
  // Theme Lifecycle
  // ============================================================================

  void NyanTheme::SetTheme(const QString& name) {
    const auto regKey = name.toStdString();
    auto regIt = _themeRegistry.find(regKey);
    if (regIt == _themeRegistry.end()) {
      // 没有找到直接退出，必须先注册主题，然后再设置主题才能生效。[TODO 缺少告警打印]
      const QString candidate = _palettePath + u'/' + name + QStringLiteral(".json");
      if (!RegisterTheme(candidate)) {
        qWarning() << "Theme is not registered:" << name;
        return;
      }
      regIt = _themeRegistry.find(regKey);
      if (regIt == _themeRegistry.end()) {
        qWarning() << "Registered theme name does not match requested theme:" << name;
        return;
      }
    }
    if (!TryLoadPalette(name)) {
      return;
    }

    BuildStyleSheets();
    ApplyComponentOverrides();
    InvalidateIconCache();

#ifndef NDEBUG
    // D2: Debug-build WCAG AA contrast validation for all widget kinds
    for (std::size_t k = 0; k < kWidgetKindCount; ++k) {
      const auto kind = static_cast<WidgetKind>(k);
      const auto& sheet = ResolveStyleSheet(kind);
      if (sheet.variants.empty()) {
        continue;
      }
      const auto& normal = sheet.variants[0].colors[std::to_underlying(fw::InteractionState::Normal)];
      const QColor fg = Color(normal.foreground);
      const QColor bg = Color(normal.background);
      const double ratio = ContrastChecker::Ratio(fg, bg);
      if (ratio < 4.5) {
        qWarning("A11y: WidgetKind(%zu) contrast ratio %.2f < 4.5:1 (AA)", k, ratio);
      }
    }
#endif
    BuildGlobalStyleSheet();

    _currentTheme = name;
    emit ThemeChanged(_currentTheme);
  }

  auto NyanTheme::CurrentTheme() const -> const QString& {
    return _currentTheme;
  }

  auto NyanTheme::RegisterTheme(const QString& jsonPath) -> bool {
    if (!QFile::exists(jsonPath)) {
      return false;
    }

    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
      return false;
    }

    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
      return false;
    }

    const auto name = ReadThemeName(doc.object());
    if (name.isEmpty()) {
      return false;
    }

    _themeRegistry[name.toStdString()] = ThemeEntry{name, jsonPath};
    return true;
  }

  // ============================================================================
  // Palette Loading
  // ============================================================================

  void NyanTheme::InitializeDefaultTokens() {
    for (std::size_t i = 0; i < kColorTokenCount; ++i) {
      _colors[i] = QColor::fromRgba(static_cast<QRgb>(defaults::kLightDefaultColors[i]));
    }

    _colorTokensByKey.clear();
    _gradientTokensByKey.clear();
    _fontTokensByKey.clear();
    _dimensionTokensByKey.clear();
    _shadowTokensByKey.clear();
    _springS = fw::SpringSpec{};

    const auto platformFonts = DetectPlatformFontFamilies();
    _fontTokensByKey.emplace("fontXS", DefaultFontSpec(platformFonts.sansFamily, 10));
    _fontTokensByKey.emplace("fontSM", DefaultFontSpec(platformFonts.sansFamily, 12));
    _fontTokensByKey.emplace("fontMS", DefaultFontSpec(platformFonts.sansFamily, 14));
    _fontTokensByKey.emplace("fontMD", DefaultFontSpec(platformFonts.sansFamily, 16));
    _fontTokensByKey.emplace("fontLG", DefaultFontSpec(platformFonts.sansFamily, 16));

    _dimensionTokensByKey.emplace("spaceXXXS", 2);
    _dimensionTokensByKey.emplace("spaceXXS", 4);
    _dimensionTokensByKey.emplace("spaceXS", 8);
    _dimensionTokensByKey.emplace("spaceSM", 12);
    _dimensionTokensByKey.emplace("spaceMS", 16);
    _dimensionTokensByKey.emplace("spaceMD", 20);
    _dimensionTokensByKey.emplace("spaceLG", 24);
    _dimensionTokensByKey.emplace("spaceXL", 28);
    _dimensionTokensByKey.emplace("spaceXXL", 32);
    _dimensionTokensByKey.emplace("spaceXXXL", 64);

    _dimensionTokensByKey.emplace("radiusSmall", 1);
    _dimensionTokensByKey.emplace("radiusDefault", 3);
    _dimensionTokensByKey.emplace("radiusLarge", 6);
    _dimensionTokensByKey.emplace("radiusRound", 255);

    _dimensionTokensByKey.emplace("lineWidthMS", 1);
    _dimensionTokensByKey.emplace("lineWidthMD", 2);
    _dimensionTokensByKey.emplace("lineWidthLG", 3);
    _dimensionTokensByKey.emplace("lineWidthXL", 4);

    _dimensionTokensByKey.emplace("iconSizeXXS", 8);
    _dimensionTokensByKey.emplace("iconSizeXS", 12);
    _dimensionTokensByKey.emplace("iconSizeSM", 16);
    _dimensionTokensByKey.emplace("iconSizeMS", 20);
    _dimensionTokensByKey.emplace("iconSizeMD", 24);
    _dimensionTokensByKey.emplace("iconSizeLG", 32);
    _dimensionTokensByKey.emplace("iconSizeXL", 40);
    _dimensionTokensByKey.emplace("iconSizeXXL", 56);
    _dimensionTokensByKey.emplace("iconSizeXXXL", 64);

    _dimensionTokensByKey.emplace("controlHeightXS", 20);
    _dimensionTokensByKey.emplace("controlHeightSM", 24);
    _dimensionTokensByKey.emplace("controlHeightMS", 28);
    _dimensionTokensByKey.emplace("controlHeightMD", 32);
    _dimensionTokensByKey.emplace("controlHeightLG", 36);
    _dimensionTokensByKey.emplace("controlHeightXL", 40);
    _dimensionTokensByKey.emplace("controlHeightXXL", 44);
    _dimensionTokensByKey.emplace("controlHeightXXXL", 48);

    _dimensionTokensByKey.emplace("containerWidthXS", 20);
    _dimensionTokensByKey.emplace("containerWidthSM", 24);
    _dimensionTokensByKey.emplace("containerWidthMS", 28);
    _dimensionTokensByKey.emplace("containerWidthMD", 32);
    _dimensionTokensByKey.emplace("containerWidthLG", 36);
    _dimensionTokensByKey.emplace("containerWidthXL", 40);

    BuildFonts();
    BuildShadows();
  }

  void NyanTheme::LoadPalette(const QString& themeName) {
    (void)TryLoadPalette(themeName);
  }

  auto NyanTheme::TryLoadPalette(const QString& themeName) -> bool {
    // Resolve the JSON file path for this theme
    QString filePath;
    const auto regKey = themeName.toStdString();
    const auto regIt = _themeRegistry.find(regKey);
    if (regIt != _themeRegistry.end()) {
      filePath = regIt->second.jsonPath;
    }
    else {
      // 内置主题：在<name>调色板目录中寻找.json
      // 代码这里走到了else，没找到主题
      const QString candidate = _palettePath + u'/' + themeName + QStringLiteral(".json");
      if (QFile::exists(candidate)) {
        filePath = candidate;
      }
      else {
        qWarning() << "Theme file not found:" << candidate;
        return false;
      }
    }

    /*
      这里的修改方案：
        1 亮色主题作为内置主题，后期将配置文件内容直接固化或者Token硬编码
        2 打开文件如果不存在或者失败不应该切换当前已设置的主题
    */

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
      qWarning() << "Failed to open theme file:" << filePath << file.errorString();
      return false;
    }

    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
      qWarning() << "Theme JSON root is not an object:" << filePath;
      return false;
    }

    const auto root = doc.object();
    const auto name = ReadThemeName(root);
    if (name.isEmpty() || name != themeName) {
      qWarning() << "Theme name mismatch:" << filePath << "expected" << themeName << "actual" << name;
      return false;
    }

    const auto previousColors = _colors;
    const auto previousColorTokensByKey = _colorTokensByKey;
    const auto previousGradientTokensByKey = _gradientTokensByKey;
    const auto previousFontTokensByKey = _fontTokensByKey;
    const auto previousDimensionTokensByKey = _dimensionTokensByKey;
    const auto previousShadowTokensByKey = _shadowTokensByKey;
    const auto previousFonts = _fonts;
    const auto previousShadows = _shadows;
    const auto previousSpring = _springS;

    InitializeDefaultTokens();
    ApplyColorTokens(root.value(QStringLiteral("colors")).toObject());
    ApplyColorOverrides(root.value(QStringLiteral("colorOverrides")).toObject());
    ApplySpringTokens(root.value(QStringLiteral("spring")).toObject());
    ApplyFontTokens(root.value(QStringLiteral("fonts")).toObject());
    BuildFonts();
    ApplyMetricTokens(root);
    if (!ApplyShadowTokens(root.value(QStringLiteral("shadows")).toObject())) {
      _colors = previousColors;
      _colorTokensByKey = previousColorTokensByKey;
      _gradientTokensByKey = previousGradientTokensByKey;
      _fontTokensByKey = previousFontTokensByKey;
      _dimensionTokensByKey = previousDimensionTokensByKey;
      _shadowTokensByKey = previousShadowTokensByKey;
      _fonts = previousFonts;
      _shadows = previousShadows;
      _springS = previousSpring;
      return false;
    }
    return true;
  }

  auto NyanTheme::ReadThemeName(const QJsonObject& root) const -> QString {
    const auto name = root.value(QStringLiteral("name")).toString().trimmed();
    return name;
  }

  void NyanTheme::ApplyColorTokens(const QJsonObject& colors) {
    if (colors.isEmpty()) {
      return;
    }

    for (auto it = colors.constBegin(); it != colors.constEnd(); ++it) {
      const auto key = it.key().toStdString();
      const auto value = it.value().toString();
      if (value.isEmpty()) {
        continue;
      }

      if (it.key().endsWith(QStringLiteral("Gradient")) || value.contains(u'|')) {
        if (const auto gradient = ParsePipeGradientToken(value, key)) {
          _gradientTokensByKey[key] = *gradient;
        }
        continue;
      }

      const QColor c = QColor::fromString(value);
      if (c.isValid()) {
        _colorTokensByKey[key] = c;
      }
    }
  }

  void NyanTheme::ApplyColorOverrides(const QJsonObject& overrides) {
    if (overrides.isEmpty()) {
      return;
    }

    for (auto it = overrides.constBegin(); it != overrides.constEnd(); ++it) {
      const auto key = it.key().toStdString();
      const auto value = it.value().toString();
      if (value.isEmpty()) {
        continue;
      }

      const QColor c = QColor::fromString(value);
      if (c.isValid()) {
        _colorTokensByKey[key] = c;
      }
    }
  }

  void NyanTheme::ApplySpringTokens(const QJsonObject& spring) {
    if (spring.isEmpty()) {
      return;
    }

    if (spring.contains(QStringLiteral("mass"))) {
      _springS.mass = static_cast<float>(spring.value(QStringLiteral("mass")).toDouble(1.0));
    }
    if (spring.contains(QStringLiteral("stiffness"))) {
      _springS.stiffness = static_cast<float>(spring.value(QStringLiteral("stiffness")).toDouble(200.0));
    }
    if (spring.contains(QStringLiteral("damping"))) {
      _springS.damping = static_cast<float>(spring.value(QStringLiteral("damping")).toDouble(20.0));
    }
  }

  void NyanTheme::ApplyFontTokens(const QJsonObject& fonts) {
    if (fonts.isEmpty()) {
      return;
    }

    const auto fallbackFamily = DetectPlatformFontFamilies().sansFamily;

    for (auto it = fonts.constBegin(); it != fonts.constEnd(); ++it) {
      const auto fontObj = it.value().toObject();
      if (fontObj.isEmpty()) {
        continue;
      }

      const auto tokenKey = it.key().toStdString();
      auto spec = FontSpec{};
      if (const auto existing = _fontTokensByKey.find(tokenKey); existing != _fontTokensByKey.end()) {
        spec = existing->second;
      }
      else {
        spec = DefaultFontSpec(fallbackFamily, FontSpec{}.sizeInPt);
      }

      if (fontObj.contains(QStringLiteral("family"))) {
        const auto family = fontObj.value(QStringLiteral("family")).toString().trimmed();
        if (!family.isEmpty()) {
          spec.family = family;
        }
      }
      if (fontObj.contains(QStringLiteral("size"))) {
        const auto size = fontObj.value(QStringLiteral("size")).toInt(spec.sizeInPt);
        if (size > 0) {
          spec.sizeInPt = size;
        }
      }
      if (fontObj.contains(QStringLiteral("weight"))) {
        const auto weight = fontObj.value(QStringLiteral("weight")).toInt(spec.weight);
        if (weight > 0) {
          spec.weight = weight;
        }
      }
      if (fontObj.contains(QStringLiteral("italic"))) {
        spec.italic = fontObj.value(QStringLiteral("italic")).toBool(spec.italic);
      }
      if (fontObj.contains(QStringLiteral("lineHeightMultiplier"))) {
        const auto lineHeight = fontObj.value(QStringLiteral("lineHeightMultiplier")).toDouble(spec.lineHeightMultiplier);
        if (lineHeight > 0.0) {
          spec.lineHeightMultiplier = lineHeight;
        }
      }
      else if (fontObj.contains(QStringLiteral("lineHeight"))) {
        const auto lineHeight = fontObj.value(QStringLiteral("lineHeight")).toDouble(spec.lineHeightMultiplier);
        if (lineHeight > 0.0) {
          spec.lineHeightMultiplier = lineHeight;
        }
      }
      if (fontObj.contains(QStringLiteral("letterSpacing"))) {
        spec.letterSpacing = fontObj.value(QStringLiteral("letterSpacing")).toDouble(spec.letterSpacing);
      }
      if (spec.family.isEmpty()) {
        spec.family = fallbackFamily;
      }

      _fontTokensByKey[tokenKey] = spec;
    }
  }

  void NyanTheme::ApplyMetricTokens(const QJsonObject& root) {
    const std::array groups = {
      QStringLiteral("space"),
      QStringLiteral("radius"),
      QStringLiteral("lineWidth"),
      QStringLiteral("iconSize"),
      QStringLiteral("controlHeight"),
      QStringLiteral("containerWidth"),
    };

    for (const auto& groupName : groups) {
      const auto group = root.value(groupName).toObject();
      if (group.isEmpty()) {
        continue;
      }

      for (auto it = group.constBegin(); it != group.constEnd(); ++it) {
        if (!it.value().isDouble()) {
          continue;
        }
        _dimensionTokensByKey[it.key().toStdString()] = it.value().toInt();
      }
    }
  }

  auto NyanTheme::ApplyShadowTokens(const QJsonObject& shadows) -> bool {
    if (shadows.isEmpty()) {
      return true;
    }

    std::unordered_map<std::string, std::vector<ShadowLayerSpec>> parsed;
    parsed.reserve(static_cast<std::size_t>(shadows.size()));

    for (auto it = shadows.constBegin(); it != shadows.constEnd(); ++it) {
      const auto tokenName = it.key();
      const auto layersJson = it.value().toArray();
      if (layersJson.isEmpty()) {
        qWarning() << "Invalid shadow token: expected non-empty layer array" << tokenName;
        return false;
      }

      std::vector<ShadowLayerSpec> layers;
      layers.reserve(static_cast<std::size_t>(layersJson.size()));

      for (const auto& layerValue : layersJson) {
        const auto layerObj = layerValue.toObject();
        if (layerObj.isEmpty()) {
          qWarning() << "Invalid shadow layer: expected object" << tokenName;
          return false;
        }

        const auto orientation = layerObj.value(QStringLiteral("orientation")).toArray();
        if (orientation.size() < 2 || !orientation.at(0).isDouble() || !orientation.at(1).isDouble()) {
          qWarning() << "Invalid shadow layer orientation" << tokenName;
          return false;
        }
        if (!layerObj.value(QStringLiteral("blur")).isDouble() || !layerObj.value(QStringLiteral("spread")).isDouble()) {
          qWarning() << "Invalid shadow layer blur/spread" << tokenName;
          return false;
        }
        const auto color = layerObj.value(QStringLiteral("color")).toArray();
        if (color.size() < 4 || !color.at(0).isDouble() || !color.at(1).isDouble() || !color.at(2).isDouble()
            || !color.at(3).isDouble()) {
          qWarning() << "Invalid shadow layer color" << tokenName;
          return false;
        }

        ShadowLayerSpec layer;
        layer.offsetX = orientation.at(0).toInt();
        layer.offsetY = orientation.at(1).toInt();
        layer.blurRadius = layerObj.value(QStringLiteral("blur")).toInt();
        layer.spread = layerObj.value(QStringLiteral("spread")).toInt();
        layer.color = QColor(
            color.at(0).toInt(),
            color.at(1).toInt(),
            color.at(2).toInt(),
            color.at(3).toInt()
        );

        layers.push_back(layer);
      }

      parsed[tokenName.toStdString()] = std::move(layers);
    }

    _shadowTokensByKey = std::move(parsed);
    return true;
  }

  // ============================================================================
  // Font Detection
  // ============================================================================

  void NyanTheme::BuildFonts() {
    const auto fallback = ScaledFontSpec(DefaultFontSpec(DetectPlatformFontFamilies().sansFamily, FontSpec{}.sizeInPt), _fontScale);
    for (std::size_t i = 0; i < kFontRoleCount; ++i) {
      auto spec = fallback;
      const auto role = static_cast<FontRole>(static_cast<uint8_t>(i));
      if (const auto key = LegacyFontRoleKey(role)) {
        if (const auto resolved = Font(*key)) {
          spec = *resolved;
        }
      }
      _fonts[i] = spec;
    }
  }

  // ============================================================================
  // Shadow Derivation
  // ============================================================================

  void NyanTheme::BuildShadows() {
    // Flat: no shadow
    _shadows[std::to_underlying(ShadowToken::shadow)] = {
      .offsetX = 0,
      .offsetY = 0,
      .blurRadius = 0,
      .opacity = 0.0,
    };

    // Low: 2px blur, 6% opacity
    _shadows[std::to_underlying(ShadowToken::boxShadow)] = {
      .offsetX = 0,
      .offsetY = 1,
      .blurRadius = 2,
      .opacity = 0.06,
    };

    // Medium: 4px blur, 10% opacity
    _shadows[std::to_underlying(ShadowToken::boxShadowSecondary)] = {
      .offsetX = 0,
      .offsetY = 2,
      .blurRadius = 4,
      .opacity = 0.10,
    };

    // High: 8px blur, 15% opacity
    _shadows[std::to_underlying(ShadowToken::boxShadowTertiary)] = {
      .offsetX = 0,
      .offsetY = 4,
      .blurRadius = 8,
      .opacity = 0.15,
    };

  }

  // ============================================================================
  // WidgetStyleSheet Assembly
  // ============================================================================

  void NyanTheme::BuildDefaultVariants(WidgetKind kind) {
    const auto idx = static_cast<std::size_t>(std::to_underlying(kind));

    // Default single-variant: Normal state uses neutral palette
    VariantStyle defaultVariant{};
    auto& c = defaultVariant.colors;

    c[std::to_underlying(InteractionState::Normal)]
        = {ColorToken::colorBgContainer, ColorToken::colorText, ColorToken::colorDivider};
    c[std::to_underlying(InteractionState::Hovered)]
        = {ColorToken::colorFillHover, ColorToken::colorText, ColorToken::colorBorder};
    c[std::to_underlying(InteractionState::Pressed)]
        = {ColorToken::colorFillTertiaryHover, ColorToken::colorText, ColorToken::colorBorderSecondary};
    c[std::to_underlying(InteractionState::Disabled)]
        = {ColorToken::colorBgContainerTertiary, ColorToken::colorTextQuaternary, ColorToken::colorDivider};
    c[std::to_underlying(InteractionState::Focused)]
        = {ColorToken::colorBgContainer, ColorToken::colorText, ColorToken::Focus};
    c[std::to_underlying(InteractionState::Selected)]
        = {ColorToken::colorPrimaryBgHover, ColorToken::colorPrimary, ColorToken::colorPrimary};
    c[std::to_underlying(InteractionState::Error)]
        = {ColorToken::colorErrorBg, ColorToken::colorError, ColorToken::colorError};
    c[std::to_underlying(InteractionState::DragOver)]
        = {ColorToken::colorPrimaryBg, ColorToken::colorPrimary, ColorToken::colorPrimaryBgHover};

    // Apply disabled state opacity and cursor defaults
    c[std::to_underlying(InteractionState::Disabled)].opacity = 0.45F;
    c[std::to_underlying(InteractionState::Disabled)].cursor = fw::CursorToken::Forbidden;
    for (auto s :
         {InteractionState::Normal, InteractionState::Hovered, InteractionState::Pressed, InteractionState::Focused,
          InteractionState::Selected}) {
      c[std::to_underlying(s)].cursor = fw::CursorToken::Pointer;
    }

    _variantStorage[idx] = {defaultVariant};

    // PushButton: 4 variants (colorPrimary, Secondary, Ghost, Danger)
    if (kind == WidgetKind::PushButton) {
      VariantStyle primary{};
      primary.colors[std::to_underlying(InteractionState::Normal)]
          = {ColorToken::colorPrimary, ColorToken::OnAccent, ColorToken::colorPrimary};
      primary.colors[std::to_underlying(InteractionState::Hovered)]
          = {ColorToken::colorPrimaryBgHover, ColorToken::OnAccent, ColorToken::colorPrimaryBgHover};
      primary.colors[std::to_underlying(InteractionState::Pressed)]
          = {ColorToken::colorPrimaryActive, ColorToken::OnAccent, ColorToken::colorPrimaryActive};
      primary.colors[std::to_underlying(InteractionState::Disabled)]
          = {ColorToken::colorPrimaryBorder, ColorToken::colorTextQuaternary, ColorToken::colorPrimaryBorder};
      primary.colors[std::to_underlying(InteractionState::Focused)]
          = {ColorToken::colorPrimary, ColorToken::OnAccent, ColorToken::Focus};
      primary.colors[std::to_underlying(InteractionState::Selected)]
          = {ColorToken::colorPrimary, ColorToken::OnAccent, ColorToken::colorPrimary};
      primary.colors[std::to_underlying(InteractionState::Error)]
          = {ColorToken::colorError, ColorToken::OnAccent, ColorToken::colorError};

      VariantStyle secondary{};
      secondary.colors[std::to_underlying(InteractionState::Normal)]
          = {ColorToken::colorBgContainer, ColorToken::colorPrimary, ColorToken::colorBorder};
      secondary.colors[std::to_underlying(InteractionState::Hovered)]
          = {ColorToken::colorPrimaryBgHover, ColorToken::colorPrimaryBgHover, ColorToken::colorPrimaryBgHover};
      secondary.colors[std::to_underlying(InteractionState::Pressed)]
          = {ColorToken::colorPrimaryBgHover, ColorToken::colorPrimaryActive, ColorToken::colorPrimaryActive};
      secondary.colors[std::to_underlying(InteractionState::Disabled)]
          = {ColorToken::colorBgContainerTertiary, ColorToken::colorTextQuaternary, ColorToken::colorDivider};
      secondary.colors[std::to_underlying(InteractionState::Focused)]
          = {ColorToken::colorBgContainer, ColorToken::colorPrimary, ColorToken::Focus};
      secondary.colors[std::to_underlying(InteractionState::Selected)]
          = {ColorToken::colorPrimaryBgHover, ColorToken::colorPrimary, ColorToken::colorPrimary};
      secondary.colors[std::to_underlying(InteractionState::Error)]
          = {ColorToken::colorErrorBg, ColorToken::colorError, ColorToken::colorError};

      VariantStyle ghost{};
      ghost.colors[std::to_underlying(InteractionState::Normal)]
          = {ColorToken::colorBgContainer, ColorToken::colorText, ColorToken::colorBgContainer};
      ghost.colors[std::to_underlying(InteractionState::Hovered)]
          = {ColorToken::colorFillHover, ColorToken::colorText, ColorToken::colorFillHover};
      ghost.colors[std::to_underlying(InteractionState::Pressed)]
          = {ColorToken::colorFillTertiaryHover, ColorToken::colorText, ColorToken::colorFillTertiaryHover};
      ghost.colors[std::to_underlying(InteractionState::Disabled)]
          = {ColorToken::colorBgContainer, ColorToken::colorTextQuaternary, ColorToken::colorBgContainer};
      ghost.colors[std::to_underlying(InteractionState::Focused)]
          = {ColorToken::colorBgContainer, ColorToken::colorText, ColorToken::Focus};
      ghost.colors[std::to_underlying(InteractionState::Selected)]
          = {ColorToken::colorPrimaryBgHover, ColorToken::colorPrimary, ColorToken::colorPrimaryBgHover};
      ghost.colors[std::to_underlying(InteractionState::Error)]
          = {ColorToken::colorErrorBg, ColorToken::colorError, ColorToken::colorErrorBg};

      VariantStyle danger{};
      danger.colors[std::to_underlying(InteractionState::Normal)]
          = {ColorToken::colorError, ColorToken::OnAccent, ColorToken::colorError};
      danger.colors[std::to_underlying(InteractionState::Hovered)]
          = {ColorToken::colorErrorHover, ColorToken::OnAccent, ColorToken::colorErrorHover};
      danger.colors[std::to_underlying(InteractionState::Pressed)]
          = {ColorToken::colorErrorActive, ColorToken::OnAccent, ColorToken::colorErrorActive};
      danger.colors[std::to_underlying(InteractionState::Disabled)]
          = {ColorToken::colorErrorBorder, ColorToken::colorTextQuaternary, ColorToken::colorErrorBorder};
      danger.colors[std::to_underlying(InteractionState::Focused)]
          = {ColorToken::colorError, ColorToken::OnAccent, ColorToken::Focus};
      danger.colors[std::to_underlying(InteractionState::Selected)]
          = {ColorToken::colorError, ColorToken::OnAccent, ColorToken::colorError};
      danger.colors[std::to_underlying(InteractionState::Error)]
          = {ColorToken::colorError, ColorToken::OnAccent, ColorToken::colorError};

      BindStateStyleKeys(primary);
      BindStateStyleKeys(secondary);
      BindStateStyleKeys(ghost);
      BindStateStyleKeys(danger);
      _variantStorage[idx] = {primary, secondary, ghost, danger};
    }

    // CheckBox: 3 variants (Unchecked=0, Checked=1, Partial=2)
    if (kind == WidgetKind::CheckBox) {
      VariantStyle unchecked{};
      unchecked.colors[std::to_underlying(InteractionState::Normal)]
          = {ColorToken::colorBgContainerTertiary, ColorToken::colorTextSecondary, ColorToken::colorBorderSecondary};
      unchecked.colors[std::to_underlying(InteractionState::Hovered)]
          = {ColorToken::colorBgContainerTertiary, ColorToken::colorTextSecondary, ColorToken::colorPrimaryBgHover};
      unchecked.colors[std::to_underlying(InteractionState::Pressed)]
          = {ColorToken::colorBgContainerTertiary, ColorToken::colorTextSecondary, ColorToken::colorPrimaryActive};
      unchecked.colors[std::to_underlying(InteractionState::Disabled)]
          = {ColorToken::colorBgContainerTertiary, ColorToken::colorTextQuaternary, ColorToken::colorBorder, 0.45F};
      unchecked.colors[std::to_underlying(InteractionState::Focused)]
          = {ColorToken::colorBgContainerTertiary, ColorToken::colorTextSecondary, ColorToken::Focus};

      VariantStyle checked{};
      checked.colors[std::to_underlying(InteractionState::Normal)]
          = {ColorToken::colorPrimary, ColorToken::OnAccent, ColorToken::colorPrimary};
      checked.colors[std::to_underlying(InteractionState::Hovered)]
          = {ColorToken::colorPrimaryBgHover, ColorToken::OnAccent, ColorToken::colorPrimaryBgHover};
      checked.colors[std::to_underlying(InteractionState::Pressed)]
          = {ColorToken::colorPrimaryActive, ColorToken::OnAccent, ColorToken::colorPrimaryActive};
      checked.colors[std::to_underlying(InteractionState::Disabled)]
          = {ColorToken::colorPrimaryBorder, ColorToken::colorTextQuaternary, ColorToken::colorPrimaryBorder, 0.45F};
      checked.colors[std::to_underlying(InteractionState::Focused)]
          = {ColorToken::colorPrimary, ColorToken::OnAccent, ColorToken::Focus};

      VariantStyle partial{};
      partial.colors[std::to_underlying(InteractionState::Normal)]
          = {ColorToken::colorBgContainerTertiary, ColorToken::colorPrimaryActive, ColorToken::colorBorderSecondary};
      partial.colors[std::to_underlying(InteractionState::Hovered)]
          = {ColorToken::colorBgContainerTertiary, ColorToken::colorPrimaryBgHover, ColorToken::colorPrimaryBgHover};
      partial.colors[std::to_underlying(InteractionState::Pressed)]
          = {ColorToken::colorBgContainerTertiary, ColorToken::colorPrimaryActive, ColorToken::colorPrimaryActive};
      partial.colors[std::to_underlying(InteractionState::Disabled)]
          = {ColorToken::colorBgContainerTertiary, ColorToken::colorTextQuaternary, ColorToken::colorBorder, 0.45F};
      partial.colors[std::to_underlying(InteractionState::Focused)]
          = {ColorToken::colorBgContainerTertiary, ColorToken::colorPrimaryActive, ColorToken::Focus};

      BindStateStyleKeys(unchecked);
      BindStateStyleKeys(checked);
      BindStateStyleKeys(partial);
      _variantStorage[idx] = {unchecked, checked, partial};
    }

    // RadioButton: 2 variants (Unchecked=0, Checked=1)
    if (kind == WidgetKind::RadioButton) {
      VariantStyle unchecked{};
      unchecked.colors[std::to_underlying(InteractionState::Normal)]
          = {ColorToken::colorBgContainerTertiary, ColorToken::colorTextSecondary, ColorToken::colorBorderSecondary};
      unchecked.colors[std::to_underlying(InteractionState::Hovered)]
          = {ColorToken::colorBgContainerTertiary, ColorToken::colorTextSecondary, ColorToken::colorPrimaryBgHover};
      unchecked.colors[std::to_underlying(InteractionState::Disabled)]
          = {ColorToken::colorBgContainerTertiary, ColorToken::colorTextQuaternary, ColorToken::colorBorder, 0.45F};
      unchecked.colors[std::to_underlying(InteractionState::Focused)]
          = {ColorToken::colorBgContainerTertiary, ColorToken::colorTextSecondary, ColorToken::Focus};

      VariantStyle checked{};
      checked.colors[std::to_underlying(InteractionState::Normal)]
          = {ColorToken::colorPrimary, ColorToken::OnAccent, ColorToken::colorPrimary};
      checked.colors[std::to_underlying(InteractionState::Hovered)]
          = {ColorToken::colorPrimaryBgHover, ColorToken::OnAccent, ColorToken::colorPrimaryBgHover};
      checked.colors[std::to_underlying(InteractionState::Disabled)]
          = {ColorToken::colorPrimaryBorder, ColorToken::colorTextQuaternary, ColorToken::colorPrimaryBorder, 0.45F};
      checked.colors[std::to_underlying(InteractionState::Focused)]
          = {ColorToken::colorPrimary, ColorToken::OnAccent, ColorToken::Focus};

      BindStateStyleKeys(unchecked);
      BindStateStyleKeys(checked);
      _variantStorage[idx] = {unchecked, checked};
    }

    // Toggle: 2 variants (Off=0, On=1)
    if (kind == WidgetKind::Toggle) {
      VariantStyle off{};
      off.colors[std::to_underlying(InteractionState::Normal)]
          = {ColorToken::colorFill, ColorToken::colorBgContainerTertiary, ColorToken::colorBorderSecondary};
      off.colors[std::to_underlying(InteractionState::Hovered)]
          = {ColorToken::colorFillHover, ColorToken::colorBgContainerTertiary, ColorToken::colorBorderSecondary};
      off.colors[std::to_underlying(InteractionState::Pressed)] = {
        ColorToken::colorFillTertiaryHover, ColorToken::colorBgContainerTertiary, ColorToken::colorBorderSecondary
      };
      off.colors[std::to_underlying(InteractionState::Disabled)]
          = {ColorToken::colorFill, ColorToken::colorFillTertiaryHover, ColorToken::colorBorder, 0.45F};

      VariantStyle on{};
      on.colors[std::to_underlying(InteractionState::Normal)]
          = {ColorToken::colorPrimary, ColorToken::OnAccent, ColorToken::colorPrimary};
      on.colors[std::to_underlying(InteractionState::Hovered)]
          = {ColorToken::colorPrimaryBgHover, ColorToken::OnAccent, ColorToken::colorPrimaryBgHover};
      on.colors[std::to_underlying(InteractionState::Pressed)]
          = {ColorToken::colorPrimaryActive, ColorToken::OnAccent, ColorToken::colorPrimaryActive};
      on.colors[std::to_underlying(InteractionState::Disabled)]
          = {ColorToken::colorPrimaryBorder, ColorToken::colorTextQuaternary, ColorToken::colorPrimaryBorder, 0.45F};

      BindStateStyleKeys(off);
      BindStateStyleKeys(on);
      _variantStorage[idx] = {off, on};
    }

    // ToolButton: 2 variants (Default=0, Active=1)
    if (kind == WidgetKind::ToolButton) {
      VariantStyle def{};
      def.colors[std::to_underlying(InteractionState::Normal)]
          = {ColorToken::colorBgContainer, ColorToken::colorText, ColorToken::colorBgContainer};
      def.colors[std::to_underlying(InteractionState::Hovered)]
          = {ColorToken::colorFillHover, ColorToken::colorText, ColorToken::colorFillHover};
      def.colors[std::to_underlying(InteractionState::Pressed)]
          = {ColorToken::colorFillTertiaryHover, ColorToken::colorText, ColorToken::colorFillTertiaryHover};
      def.colors[std::to_underlying(InteractionState::Disabled)]
          = {ColorToken::colorBgContainer, ColorToken::colorTextQuaternary, ColorToken::colorBgContainer, 0.45F};
      def.colors[std::to_underlying(InteractionState::Focused)]
          = {ColorToken::colorBgContainer, ColorToken::colorText, ColorToken::Focus};

      VariantStyle active{};
      active.colors[std::to_underlying(InteractionState::Normal)]
          = {ColorToken::colorPrimaryBg, ColorToken::colorPrimary, ColorToken::colorPrimaryBg};
      active.colors[std::to_underlying(InteractionState::Hovered)]
          = {ColorToken::colorPrimaryBgHover, ColorToken::colorPrimaryBgHover, ColorToken::colorPrimaryBgHover};
      active.colors[std::to_underlying(InteractionState::Pressed)]
          = {ColorToken::colorPrimaryBgHover, ColorToken::colorPrimaryActive, ColorToken::colorPrimaryBgHover};
      active.colors[std::to_underlying(InteractionState::Disabled)]
          = {ColorToken::colorBgContainer, ColorToken::colorTextQuaternary, ColorToken::colorBgContainer, 0.45F};
      active.colors[std::to_underlying(InteractionState::Focused)]
          = {ColorToken::colorPrimaryBg, ColorToken::colorPrimary, ColorToken::Focus};

      BindStateStyleKeys(def);
      BindStateStyleKeys(active);
      _variantStorage[idx] = {def, active};
    }

    // LineEdit: 1 variant with focused=colorPrimary border
    if (kind == WidgetKind::LineEdit || kind == WidgetKind::SpinBox) {
      VariantStyle input{};
      input.colors[std::to_underlying(InteractionState::Normal)]
          = {ColorToken::colorBgContainer, ColorToken::colorText, ColorToken::colorBorder};
      input.colors[std::to_underlying(InteractionState::Hovered)]
          = {ColorToken::colorBgContainer, ColorToken::colorText, ColorToken::colorBorderSecondary};
      input.colors[std::to_underlying(InteractionState::Focused)]
          = {ColorToken::colorBgContainer, ColorToken::colorText, ColorToken::colorPrimary};
      input.colors[std::to_underlying(InteractionState::Disabled)]
          = {ColorToken::colorBgContainerTertiary, ColorToken::colorTextQuaternary, ColorToken::colorDivider, 0.45F};
      input.colors[std::to_underlying(InteractionState::Error)]
          = {ColorToken::colorErrorBg, ColorToken::colorText, ColorToken::colorError};

      _variantStorage[idx] = {input};
    }

    // ComboBox: 1 variant with focused=colorPrimary accent
    if (kind == WidgetKind::ComboBox) {
      VariantStyle combo{};
      combo.colors[std::to_underlying(InteractionState::Normal)]
          = {ColorToken::colorFillHover, ColorToken::colorText, ColorToken::colorBorder};
      combo.colors[std::to_underlying(InteractionState::Hovered)]
          = {ColorToken::colorFillHover, ColorToken::colorText, ColorToken::colorBorderSecondary};
      combo.colors[std::to_underlying(InteractionState::Focused)]
          = {ColorToken::colorBgContainerTertiary, ColorToken::colorPrimary, ColorToken::colorPrimaryActive};
      combo.colors[std::to_underlying(InteractionState::Disabled)]
          = {ColorToken::colorFill, ColorToken::colorTextQuaternary, ColorToken::colorBorder, 0.45F};
      combo.colors[std::to_underlying(InteractionState::Error)]
          = {ColorToken::colorErrorBg, ColorToken::colorText, ColorToken::colorError};

      _variantStorage[idx] = {combo};
    }

    // TabWidget: 2 variants (Inactive=0, Active=1)
    if (kind == WidgetKind::TabWidget) {
      VariantStyle inactive{};
      inactive.colors[std::to_underlying(InteractionState::Normal)]
          = {ColorToken::colorBgContainerSecondary, ColorToken::colorTextSecondary, ColorToken::colorDivider};
      inactive.colors[std::to_underlying(InteractionState::Hovered)]
          = {ColorToken::colorFillHover, ColorToken::colorText, ColorToken::colorBorder};
      inactive.colors[std::to_underlying(InteractionState::Pressed)]
          = {ColorToken::colorFillTertiaryHover, ColorToken::colorText, ColorToken::colorBorder};
      inactive.colors[std::to_underlying(InteractionState::Disabled)]
          = {ColorToken::colorBgContainerSecondary, ColorToken::colorTextQuaternary, ColorToken::colorDivider, 0.45F};

      VariantStyle active{};
      active.colors[std::to_underlying(InteractionState::Normal)]
          = {ColorToken::colorBgContainer, ColorToken::colorPrimary, ColorToken::colorPrimary};
      active.colors[std::to_underlying(InteractionState::Hovered)]
          = {ColorToken::colorBgContainer, ColorToken::colorPrimaryBgHover, ColorToken::colorPrimaryBgHover};
      active.colors[std::to_underlying(InteractionState::Pressed)]
          = {ColorToken::colorBgContainer, ColorToken::colorPrimaryActive, ColorToken::colorPrimaryActive};
      active.colors[std::to_underlying(InteractionState::Disabled)]
          = {ColorToken::colorBgContainer, ColorToken::colorTextQuaternary, ColorToken::colorDivider, 0.45F};

      _variantStorage[idx] = {inactive, active};
    }

    // DataTable: 3 variants (Default=0, Selected=1, Striped=2)
    if (kind == WidgetKind::DataTable) {
      VariantStyle def{};
      def.colors[std::to_underlying(InteractionState::Normal)]
          = {ColorToken::colorBgContainer, ColorToken::colorText, ColorToken::colorDivider};
      def.colors[std::to_underlying(InteractionState::Hovered)]
          = {ColorToken::colorFillHover, ColorToken::colorText, ColorToken::colorDivider};
      def.colors[std::to_underlying(InteractionState::Selected)]
          = {ColorToken::colorPrimaryBg, ColorToken::colorText, ColorToken::colorPrimaryBorder};
      def.colors[std::to_underlying(InteractionState::Disabled)]
          = {ColorToken::colorBgContainerTertiary, ColorToken::colorTextQuaternary, ColorToken::colorDivider, 0.45F};

      VariantStyle selected{};
      selected.colors[std::to_underlying(InteractionState::Normal)]
          = {ColorToken::colorPrimaryBg, ColorToken::colorPrimary, ColorToken::colorPrimaryBorder};
      selected.colors[std::to_underlying(InteractionState::Hovered)]
          = {ColorToken::colorPrimaryBgHover, ColorToken::colorPrimaryBgHover, ColorToken::colorPrimaryBorder};
      selected.colors[std::to_underlying(InteractionState::Selected)]
          = {ColorToken::colorPrimaryBgHover, ColorToken::colorPrimary, ColorToken::colorPrimary};

      VariantStyle striped{};
      striped.colors[std::to_underlying(InteractionState::Normal)]
          = {ColorToken::colorBgContainerSecondary, ColorToken::colorText, ColorToken::colorDivider};
      striped.colors[std::to_underlying(InteractionState::Hovered)]
          = {ColorToken::colorFillHover, ColorToken::colorText, ColorToken::colorDivider};
      striped.colors[std::to_underlying(InteractionState::Selected)]
          = {ColorToken::colorPrimaryBg, ColorToken::colorText, ColorToken::colorPrimaryBorder};

      _variantStorage[idx] = {def, selected, striped};
    }

    // ProgressBar: 3 variants (colorPrimary=0, colorSuccess=1, colorError=2)
    if (kind == WidgetKind::ProgressBar) {
      VariantStyle primary{};
      primary.colors[std::to_underlying(InteractionState::Normal)]
          = {ColorToken::colorFill, ColorToken::colorPrimary, ColorToken::colorDivider};

      VariantStyle colorsuccess{};
      colorsuccess.colors[std::to_underlying(InteractionState::Normal)]
          = {ColorToken::colorFill, ColorToken::colorSuccess, ColorToken::colorDivider};

      VariantStyle colorerror{};
      colorerror.colors[std::to_underlying(InteractionState::Normal)]
          = {ColorToken::colorFill, ColorToken::colorError, ColorToken::colorDivider};

      _variantStorage[idx] = {primary, colorsuccess, colorerror};
    }

    // Slider: 1 variant (track=bg, thumb=fg, border=track border)
    if (kind == WidgetKind::Slider) {
      VariantStyle slider{};
      slider.colors[std::to_underlying(InteractionState::Normal)]
          = {ColorToken::colorFill, ColorToken::colorPrimary, ColorToken::colorBorder};
      slider.colors[std::to_underlying(InteractionState::Hovered)]
          = {ColorToken::colorFill, ColorToken::colorPrimaryBgHover, ColorToken::colorBorder};
      slider.colors[std::to_underlying(InteractionState::Pressed)]
          = {ColorToken::colorFill, ColorToken::colorPrimaryActive, ColorToken::colorBorder};
      slider.colors[std::to_underlying(InteractionState::Disabled)]
          = {ColorToken::colorFill, ColorToken::colorBorder, ColorToken::colorDivider, 0.45F};
      slider.colors[std::to_underlying(InteractionState::Focused)]
          = {ColorToken::colorFill, ColorToken::colorPrimary, ColorToken::Focus};

      _variantStorage[idx] = {slider};
    }
  }

  void NyanTheme::BuildStyleSheets() {
    for (std::size_t i = 0; i < kWidgetKindCount; ++i) {
      const auto kind = static_cast<WidgetKind>(static_cast<uint8_t>(i));

      // Build variant color maps
      BuildDefaultVariants(kind);

      // Default geometry tokens
      _styleSheets[i] = WidgetStyleSheet{
        .radius = RadiusToken::borderRadiusMD,
        .paddingH = SpaceToken::marginXXS,
        .paddingV = SpaceToken::marginXXS,
        .font = FontRole::fontSizeSM,
        .elevation = ShadowToken::shadow,
        .transition = {},
        .variants = _variantStorage[i],
      };
    }

    // Per-kind geometry overrides (from architecture spec)
    auto& dialog = _styleSheets[std::to_underlying(WidgetKind::Dialog)];
    dialog.radius = RadiusToken::borderRadiusLG;
    dialog.elevation = ShadowToken::boxShadowTertiary;

    auto& tooltip = _styleSheets[std::to_underlying(WidgetKind::Tooltip)];
    tooltip.radius = RadiusToken::borderRadiusSM;
    tooltip.elevation = ShadowToken::boxShadowTertiary;
    tooltip.font = FontRole::fontSizeSM;

    auto& actionBar = _styleSheets[std::to_underlying(WidgetKind::ActionBar)];
    actionBar.font = FontRole::fontSizeSM;
    actionBar.paddingH = SpaceToken::marginXXXS;
    actionBar.paddingV = SpaceToken::marginXXXS;

    auto& contextMenu = _styleSheets[std::to_underlying(WidgetKind::ContextMenu)];
    contextMenu.elevation = ShadowToken::boxShadowTertiary;
    contextMenu.radius = RadiusToken::borderRadiusMD;

    auto& popConfirm = _styleSheets[std::to_underlying(WidgetKind::PopConfirm)];
    popConfirm.elevation = ShadowToken::boxShadowTertiary;
    popConfirm.radius = RadiusToken::borderRadiusLG;

    auto& comboBox = _styleSheets[std::to_underlying(WidgetKind::ComboBox)];
    comboBox.paddingH = SpaceToken::marginXXS;
    comboBox.paddingV = SpaceToken::marginXXS;

    auto& groupBox = _styleSheets[std::to_underlying(WidgetKind::GroupBox)];
    groupBox.radius = RadiusToken::borderRadiusLG;
    groupBox.elevation = ShadowToken::boxShadowSecondary;

    auto& panel = _styleSheets[std::to_underlying(WidgetKind::Panel)];
    panel.elevation = ShadowToken::boxShadow;

    auto& toolButton = _styleSheets[std::to_underlying(WidgetKind::ToolButton)];
    toolButton.font = FontRole::fontSizeSM;

    auto& statusBar = _styleSheets[std::to_underlying(WidgetKind::StatusBar)];
    statusBar.paddingH = SpaceToken::marginXXXS;
    statusBar.paddingV = SpaceToken::marginXXXS;
    statusBar.font = FontRole::fontSizeSM;

    auto& pushButton = _styleSheets[std::to_underlying(WidgetKind::PushButton)];
    BindWidgetStyleKeys(pushButton);
    pushButton.minHeightKey = "controlHeightMD";

    BindWidgetStyleKeys(toolButton);
    BindWidgetStyleKeys(_styleSheets[std::to_underlying(WidgetKind::CheckBox)]);
    BindWidgetStyleKeys(_styleSheets[std::to_underlying(WidgetKind::RadioButton)]);
    BindWidgetStyleKeys(_styleSheets[std::to_underlying(WidgetKind::Toggle)]);

    BindWidgetStyleKeys(tooltip);
    BindWidgetStyleKeys(popConfirm);
  }

  // ============================================================================
  // Component Override Application
  // ============================================================================

  void NyanTheme::ApplyComponentOverrides() {
    for (const auto& ov : _overrideRegistry) {
      auto& sheet
          = _styleSheets[std::to_underlying(ov.kind)];  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
      if (ov.radius) {
        sheet.radius = *ov.radius;
      }
      if (ov.paddingH) {
        sheet.paddingH = *ov.paddingH;
      }
      if (ov.paddingV) {
        sheet.paddingV = *ov.paddingV;
      }
      if (ov.font) {
        sheet.font = *ov.font;
      }
      if (ov.elevation) {
        sheet.elevation = *ov.elevation;
      }
      // 动画
      if (ov.transition) {
        sheet.transition = *ov.transition;
      }
      if (ov.radiusKey) {
        sheet.radiusKey = *ov.radiusKey;
      }
      if (ov.paddingHKey) {
        sheet.paddingHKey = *ov.paddingHKey;
      }
      if (ov.paddingVKey) {
        sheet.paddingVKey = *ov.paddingVKey;
      }
      if (ov.gapKey) {
        sheet.gapKey = *ov.gapKey;
      }
      if (ov.minHeightKey) {
        sheet.minHeightKey = *ov.minHeightKey;
      }
      if (ov.fontKey) {
        sheet.fontKey = *ov.fontKey;
      }
      if (ov.shadowKey) {
        sheet.shadowKey = *ov.shadowKey;
      }
    }
  }

  // ============================================================================
  // ITokenRegistry Implementation
  // ============================================================================

  void NyanTheme::SetDensity(fw::DensityLevel level) {
    if (_density == level) {
      return;
    }
    _density = level;
    BuildGlobalStyleSheet();
    emit ThemeChanged(_currentTheme);
  }

  auto NyanTheme::CurrentDensity() const -> fw::DensityLevel {
    return _density;
  }

  auto NyanTheme::CurrentDensityScale() const -> float {
    return fw::DensityScale(_density);
  }

  void NyanTheme::SetDirection(fw::TextDirection dir) {
    if (_direction == dir) {
      return;
    }
    _direction = dir;
    InvalidateIconCache();
    emit ThemeChanged(_currentTheme);
  }

  auto NyanTheme::CurrentDirection() const -> fw::TextDirection {
    return _direction;
  }

  // ============================================================================
  // Global Token Queries
  // ============================================================================

  auto NyanTheme::Color(std::string_view key) const -> std::optional<QColor> {
    const auto it = _colorTokensByKey.find(TokenKey(key));
    if (it == _colorTokensByKey.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  auto NyanTheme::Gradient(std::string_view key) const -> std::optional<GradientSpec> {
    const auto it = _gradientTokensByKey.find(TokenKey(key));
    if (it == _gradientTokensByKey.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  auto NyanTheme::DimensionPx(std::string_view key) const -> std::optional<int> {
    const auto it = _dimensionTokensByKey.find(TokenKey(key));
    if (it == _dimensionTokensByKey.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  auto NyanTheme::Font(std::string_view key) const -> std::optional<FontSpec> {
    const auto it = _fontTokensByKey.find(TokenKey(key));
    if (it == _fontTokensByKey.end()) {
      return std::nullopt;
    }

    auto spec = it->second;
    if (spec.family.isEmpty()) {
      spec.family = DetectPlatformFontFamilies().sansFamily;
    }
    return ScaledFontSpec(spec, _fontScale);
  }

  auto NyanTheme::Shadow(std::string_view key) const -> std::optional<std::span<const ShadowLayerSpec>> {
    const auto it = _shadowTokensByKey.find(TokenKey(key));
    if (it == _shadowTokensByKey.end()) {
      return std::nullopt;
    }
    return std::span<const ShadowLayerSpec>{it->second.data(), it->second.size()};
  }

  auto NyanTheme::HasToken(TokenKind kind, std::string_view key) const -> bool {
    const auto storageKey = TokenKey(key);
    switch (kind) {
      case TokenKind::Color:
        return _colorTokensByKey.contains(storageKey);
      case TokenKind::Gradient:
        return _gradientTokensByKey.contains(storageKey);
      case TokenKind::Font:
        return _fontTokensByKey.contains(storageKey);
      case TokenKind::Dimension:
        return _dimensionTokensByKey.contains(storageKey);
      case TokenKind::Shadow:
        return _shadowTokensByKey.contains(storageKey);
    }
    return false;
  }

  auto NyanTheme::Color(ColorToken token) const -> QColor {
    if (const auto key = LegacyColorTokenKey(token)) {
      if (const auto color = Color(*key)) {
        return *color;
      }
    }

    const auto idx = std::to_underlying(token);
    if (idx >= kColorTokenCount) {
      qWarning() << "Invalid color token index:" << idx;
      return _colors[std::to_underlying(ColorToken::colorPrimaryBase)];
    }
    return _colors[idx];
  }

  auto NyanTheme::Color(ColorToken token, InteractionState /*state*/) const -> QColor {
    // For now, state-adjusted colors delegate to the base color.
    // Full state-adjusted color derivation (brightness shift) is Phase 3+ work.
    return Color(token);
  }

  auto NyanTheme::Font(FontRole role) const -> const FontSpec& {
    if (const auto key = LegacyFontRoleKey(role)) {
      if (const auto spec = Font(*key)) {
        thread_local FontSpec legacyFontSpec;
        legacyFontSpec = *spec;
        return legacyFontSpec;
      }
    }

    const auto idx = std::to_underlying(role);
    if (idx >= kFontRoleCount) {
      return _fonts[0];
    }
    return _fonts[idx];
  }

  void NyanTheme::SetFontScale(float factor) {
    factor = std::clamp(factor, fw::kFontScaleMin, fw::kFontScaleMax);
    if (std::abs(factor - _fontScale) < 1e-5F) {
      return;  // no change
    }
    _fontScale = factor;
    BuildFonts();
    emit ThemeChanged(_currentTheme);
  }

  auto NyanTheme::FontScale() const -> float {
    return _fontScale;
  }

  auto NyanTheme::SpacingPx(fw::SpaceToken token) const -> int {
    const int basePx = [this, token] {
      if (const auto key = LegacySpaceTokenKey(token)) {
        if (const auto dimension = DimensionPx(*key)) {
          return *dimension;
        }
      }
      return fw::ToPixels(token);
    }();
    const float scale = CurrentDensityScale();
    return static_cast<int>(std::lroundf(static_cast<float>(basePx) * scale));
  }

  auto NyanTheme::Radius(fw::RadiusToken token) const -> int {
    if (const auto key = LegacyRadiusTokenKey(token)) {
      if (const auto dimension = DimensionPx(*key)) {
        return *dimension;
      }
    }
    return fw::ToPixels(token);
  }

  auto NyanTheme::Shadow(ShadowToken token) const -> const ShadowSpec& {
    if (const auto key = LegacyShadowTokenKey(token)) {
      if (const auto layers = Shadow(*key); layers && !layers->empty()) {
        // Legacy enum API collapses the mapped multi-layer token to its first layer.
        thread_local ShadowSpec legacyShadowSpec;
        legacyShadowSpec = LegacyShadowFromLayer(layers->front());
        return legacyShadowSpec;
      }
    }

    const auto idx = std::to_underlying(token);
    if (idx >= kShadowTokenCount) {
      return _shadows[0];
    }
    return _shadows[idx];
  }

  auto NyanTheme::AnimationMs(fw::AnimationsToken speed) const -> int {
    if (_animOverride >= 0) {
      return _animOverride;
    }
    const auto idx = std::to_underlying(speed);
    if (idx >= fw::kAnimationsTokenCount) {
      return 0;
    }
    return fw::kDefaultAnimationMs[idx];
  }

  // ============================================================================
  // Interaction Timing Tokens (§8.7)
  // ============================================================================

  auto NyanTheme::TimingMs(fw::TimingTokenId id) const -> int {
    const auto idx = std::to_underlying(id);
    if (idx >= fw::kTimingTokenCount) {
      return 0;
    }
    // Override takes precedence if >= 0
    if (_timingOverrides[idx] >= 0) {
      return _timingOverrides[idx];
    }
    return _timingMs[idx];
  }

  void NyanTheme::SetTimingOverride(fw::TimingTokenId id, int ms) {
    const auto idx = std::to_underlying(id);
    if (idx >= fw::kTimingTokenCount) {
      return;
    }
    _timingOverrides[idx] = ms;  // -1 restores default
  }

  void NyanTheme::QueryPlatformTimings() {
    // Start with compiled defaults
    _timingMs = fw::kDefaultTimingMs;

#ifdef _WIN32
    // Windows: query OS for platform-specific timing values
    // §8.7.2 Platform Overrides

    // DoubleClickWindow: GetDoubleClickTime()
    {
      const UINT dcTime = ::GetDoubleClickTime();
      if (dcTime > 0) {
        _timingMs[std::to_underlying(fw::TimingTokenId::DoubleClickWindow)] = static_cast<int>(dcTime);
      }
    }

    // HoverDelay: SPI_GETMOUSEHOVERTIME
    {
      UINT hoverTime = 0;
      if (::SystemParametersInfoW(SPI_GETMOUSEHOVERTIME, 0, &hoverTime, 0) && hoverTime > 0) {
        _timingMs[std::to_underlying(fw::TimingTokenId::HoverDelay)] = static_cast<int>(hoverTime);
      }
    }

    // RepeatKeyInitial: SPI_GETKEYBOARDDELAY (0-3 → 250ms-1000ms, step 250ms)
    {
      DWORD kbDelay = 0;
      if (::SystemParametersInfoW(SPI_GETKEYBOARDDELAY, 0, &kbDelay, 0)) {
        _timingMs[std::to_underlying(fw::TimingTokenId::RepeatKeyInitial)] = static_cast<int>((kbDelay + 1) * 250);
      }
    }

    // RepeatKeyInterval: SPI_GETKEYBOARDSPEED (0-31 → ~400ms-33ms)
    {
      DWORD kbSpeed = 0;
      if (::SystemParametersInfoW(SPI_GETKEYBOARDSPEED, 0, &kbSpeed, 0)) {
        // 0 = ~2.5 chars/s (400ms), 31 = ~30 chars/s (33ms)
        // Linear interpolation: interval = 400 - (kbSpeed * (400-33)/31)
        const int intervalMs = 400 - static_cast<int>(kbSpeed) * 367 / 31;
        _timingMs[std::to_underlying(fw::TimingTokenId::RepeatKeyInterval)] = std::max(intervalMs, 15);
      }
    }
#endif  // _WIN32
  }

  auto NyanTheme::ResolveStyleSheet(WidgetKind kind) const -> const WidgetStyleSheet& {
    const auto idx = std::to_underlying(kind);
    if (idx >= kWidgetKindCount) {
      return _styleSheets[0];
    }
    return _styleSheets[idx];
  }

  // ============================================================================
  // Declarative Style Resolution (RFC-07)
  // ============================================================================

  auto NyanTheme::Resolve(WidgetKind kind, std::size_t variantIndex, InteractionState state) const -> ResolvedStyle {
    const auto& sheet = ResolveStyleSheet(kind);
    // Resolve colors from variant/state matrix
    StateStyle ss;  // default if out of range
    if (!sheet.variants.empty()) {
      const auto vi = std::min(variantIndex, sheet.variants.size() - 1);
      const auto si = static_cast<std::size_t>(std::to_underlying(state));
      if (si < kInteractionStateCount) {
        ss = sheet.variants[vi].colors[si];
      }
    }

    // Build resolved font with letterSpacing applied
    const auto fontSpec = ResolveFontValue(*this, sheet.fontKey, sheet.font);
    QFont resolvedFont(fontSpec.family, fontSpec.sizeInPt, fontSpec.weight, fontSpec.italic);
    if (fontSpec.letterSpacing != 0.0) {
      resolvedFont.setLetterSpacing(QFont::AbsoluteSpacing, fontSpec.letterSpacing);
    }

    // Compute line height in pixels from font metrics and multiplier
    const QFontMetrics fm(resolvedFont);
    const int lineHeightPx
        = static_cast<int>(std::lround(static_cast<double>(fm.height()) * fontSpec.lineHeightMultiplier));

    // Resolve easing
    const int easingType = Easing(sheet.transition.easing);
    auto shadowLayers = ResolveShadowLayers(*this, sheet.shadowKey, sheet.elevation);
    ShadowSpec shadow{};
    if (!shadowLayers.empty()) {
      shadow = LegacyShadowFromLayer(shadowLayers.front());
    }

    return ResolvedStyle{
      .background = ResolveColorValue(*this, ss.backgroundKey, ss.background),
      .foreground = ResolveColorValue(*this, ss.foregroundKey, ss.foreground),
      .border = ResolveColorValue(*this, ss.borderKey, ss.border),
      .radiusPx = ResolveRadiusValue(*this, sheet.radiusKey, sheet.radius),
      .paddingHPx = ResolveSpacingValue(*this, sheet.paddingHKey, sheet.paddingH),
      .paddingVPx = ResolveSpacingValue(*this, sheet.paddingVKey, sheet.paddingV),
      .gapPx = ResolveSpacingValue(*this, sheet.gapKey, sheet.gap),
      .minHeightPx = ResolveMinHeightValue(*this, sheet.minHeightKey, sheet.minHeight),
      .borderWidthPx = ResolveSpacingValue(*this, ss.borderWidthKey, ss.borderWidth),
      .font = resolvedFont,
      .lineHeightPx = lineHeightPx,
      .shadow = shadow,
      .shadowLayers = std::move(shadowLayers),
      .opacity = ss.opacity,
      .durationMs = AnimationMs(sheet.transition.duration),
      .easingType = easingType,
    };
  }

  // ============================================================================
  // Resolve with Instance Override (cascade Layer 3)
  // ============================================================================

  auto NyanTheme::Resolve(
      WidgetKind kind, std::size_t variantIndex, InteractionState state, const InstanceStyleOverride& instanceOverride
  ) const -> ResolvedStyle {
    auto style = Resolve(kind, variantIndex, state);
    instanceOverride.ApplyTo(style);
    return style;
  }

  // ============================================================================
  // Component Overrides
  // ============================================================================

  void NyanTheme::RegisterComponentOverrides(std::span<const ComponentOverride> overrides) {
    _overrideRegistry.insert(_overrideRegistry.end(), overrides.begin(), overrides.end());
  }

  // ============================================================================
  // Dynamic Tokens
  // ============================================================================

  void NyanTheme::RegisterDynamicTokens(std::span<const DynamicColorDef> defs) {
    for (const auto& def : defs) {
      _dynamicColors[std::string(def.key)] = def.value;
    }
  }

  void NyanTheme::RegisterDynamicThemeColors(std::span<const DynamicThemeColorDef> defs) {
    for (const auto& def : defs) {
      if (def.themeName.empty()) {
        continue;
      }
      _dynamicThemeColors[std::string(def.key)][std::string(def.themeName)] = def.value;
    }
  }

  void NyanTheme::RegisterDynamicFonts(std::span<const DynamicFontDef> defs) {
    for (const auto& def : defs) {
      _dynamicFonts[std::string(def.key)] = def.value;
    }
  }

  void NyanTheme::RegisterDynamicSpacings(std::span<const DynamicSpacingDef> defs) {
    for (const auto& def : defs) {
      _dynamicSpacings[std::string(def.key)] = def.basePx;
    }
  }

  auto NyanTheme::DynamicColor(std::string_view key) const -> std::optional<QColor> {
    const auto tokenKey = std::string(key);
    const auto themedIt = _dynamicThemeColors.find(tokenKey);
    if (themedIt != _dynamicThemeColors.end()) {
      const auto themeIt = themedIt->second.find(_currentTheme.toStdString());
      if (themeIt != themedIt->second.end()) {
        return themeIt->second;
      }
    }

    const auto it = _dynamicColors.find(tokenKey);
    if (it != _dynamicColors.end()) {
      return it->second;
    }
    return std::nullopt;
  }

  auto NyanTheme::DynamicFont(std::string_view key) const -> std::optional<FontSpec> {
    const auto it = _dynamicFonts.find(std::string(key));
    if (it == _dynamicFonts.end()) {
      return std::nullopt;
    }
    // Apply global font scale so plugin fonts scale consistently with core fonts
    FontSpec result = it->second;
    const int scaled = static_cast<int>(std::lroundf(static_cast<float>(result.sizeInPt) * _fontScale));
    result.sizeInPt = std::max(scaled, 6);
    return result;
  }

  auto NyanTheme::DynamicSpacingPx(std::string_view key) const -> std::optional<int> {
    const auto it = _dynamicSpacings.find(std::string(key));
    if (it == _dynamicSpacings.end()) {
      return std::nullopt;
    }
    const float scale = fw::DensityScale(_density);
    return static_cast<int>(std::lroundf(static_cast<float>(it->second) * scale));
  }

  void NyanTheme::UnregisterDynamicTokens(std::span<const std::string_view> keys) {
    for (const auto& key : keys) {
      auto k = std::string(key);
      _dynamicColors.erase(k);
      _dynamicThemeColors.erase(k);
      _dynamicFonts.erase(k);
      _dynamicSpacings.erase(k);
    }
  }

  // ============================================================================
  // Icon Resolution (asset:// URI)
  // ============================================================================

  auto NyanTheme::RegisterIconDirectory(std::string_view uriPrefix, const QString& dirPath) -> int {
    QDir dir(dirPath);
    if (!dir.exists()) {
      return 0;
    }
    int count = 0;
    const auto entries = dir.entryInfoList(QStringList{QStringLiteral("*.svg")}, QDir::Files);
    for (const auto& entry : entries) {
      std::string uri(uriPrefix);
      uri += entry.baseName().toStdString();
      _iconRegistry[std::move(uri)] = entry.absoluteFilePath();
      ++count;
    }
    return count;
  }

  auto NyanTheme::ResolveIcon(const fw::IconId& iconId, fw::IconToken size, QColor color) const -> QPixmap {
    if (iconId.empty()) {
      return {};
    }

    // Default colorization: colorText
    if (!color.isValid()) {
      color = Color(ColorToken::colorText);
    }

    const int basePx = static_cast<int>(std::to_underlying(size));
    const float scale = fw::DensityScale(_density);
    const int sizePx = static_cast<int>(std::lroundf(static_cast<float>(basePx) * scale));

    const bool rtlFlip = (_direction == fw::TextDirection::RTL && fw::IsRtlFlippable(iconId));
    const IconCacheKey key{iconId, sizePx, color.rgba(), rtlFlip};

    {
      std::lock_guard lock(_iconCacheMutex);
      auto it = _iconCache.find(key);
      if (it != _iconCache.end()) {
        return it->second;
      }
    }

    // Lookup filesystem path from registry
    auto regIt = _iconRegistry.find(iconId);
    if (regIt == _iconRegistry.end()) {
      return {};
    }

    QPixmap pixmap = detail::LoadAndColorizeSvg(regIt->second, sizePx, color);

    // RTL: horizontally mirror directional icons
    if (rtlFlip && !pixmap.isNull()) {
      pixmap = pixmap.transformed(QTransform().scale(-1, 1));
    }

    {
      std::lock_guard lock(_iconCacheMutex);
      _iconCache[key] = pixmap;
    }

    return pixmap;
  }

  void NyanTheme::InvalidateIconCache() {
    std::lock_guard lock(_iconCacheMutex);
    _iconCache.clear();
  }

  // ============================================================================
  // Test Support
  // ============================================================================

  void NyanTheme::SetAnimationOverride(int forceMs) {
    _animOverride = forceMs;
  }

  auto NyanTheme::Spring() const -> const fw::SpringSpec& {
    return _springS;
  }

  auto NyanTheme::Easing(EasingToken easing) const -> int {
    // Map EasingToken to QEasingCurve::Type integer values
    switch (easing) {
      case EasingToken::Linear: return 0;      // QEasingCurve::Linear
      case EasingToken::OutCubic: return 4;    // QEasingCurve::OutCubic
      case EasingToken::InOutCubic: return 5;  // QEasingCurve::InOutCubic
      case EasingToken::Spring: return 44;     // QEasingCurve::OutElastic (spring approx)
      default: return 4;                       // OutCubic fallback (includes Count_)
    }
  }

  // ============================================================================
  // Global QSS Stylesheet (Design Token driven)
  // ============================================================================

  void NyanTheme::BuildGlobalStyleSheet() {
    if (QApplication::instance() == nullptr) {
      return;
    }

    // Helper: QColor -> "#RRGGBB"
    auto c = [this](ColorToken t) -> QString { return Color(t).name(QColor::HexRgb); };

    const int radius = Radius(RadiusToken::borderRadiusMD);
    const int radiusLg = Radius(RadiusToken::borderRadiusLG);
    const auto& bodyFont = Font(FontRole::fontSizeSM);
    const auto& captionFont = Font(FontRole::fontSizeMD);

    // Legacy/native Qt adapter only. Matcha self-painted widgets consume
    // ResolvedStyle from paintEvent and must not rely on global QSS for the
    // same visual properties.
    QString qss;
    qss.reserve(8192);

    // -- QWidget base --
    qss += QStringLiteral("QWidget { font-family: '%1'; font-size: %2pt; color: %3; }\n")
               .arg(bodyFont.family, QString::number(bodyFont.sizeInPt), c(ColorToken::colorText));

    // -- QLineEdit --
    qss += QStringLiteral(
               "QLineEdit {"
               "  background-color: %1; color: %2; border: 1px solid %3;"
               "  border-radius: %4px; padding: 3px 6px;"
               "  selection-background-color: %5; selection-color: %6;"
               "}\n"
               "QLineEdit:focus {"
               "  border-color: %7;"
               "}\n"
               "QLineEdit:disabled {"
               "  background-color: %8; color: %9;"
               "}\n"
    )
               .arg(
                   c(ColorToken::colorBgContainerTertiary), c(ColorToken::colorText), c(ColorToken::colorBorder),
                   QString::number(radius), c(ColorToken::Selection), c(ColorToken::OnAccent),
                   c(ColorToken::colorPrimary), c(ColorToken::colorFillSecondary), c(ColorToken::colorTextQuaternary)
               );

    // -- QTextEdit / QPlainTextEdit --
    qss += QStringLiteral(
               "QTextEdit, QPlainTextEdit {"
               "  background-color: %1; color: %2; border: 1px solid %3;"
               "  border-radius: %4px; selection-background-color: %5;"
               "}\n"
               "QTextEdit:focus, QPlainTextEdit:focus {"
               "  border-color: %6;"
               "}\n"
    )
               .arg(
                   c(ColorToken::colorBgContainerTertiary), c(ColorToken::colorText), c(ColorToken::colorBorder),
                   QString::number(radius), c(ColorToken::Selection), c(ColorToken::colorPrimary)
               );

    // -- QSpinBox / QDoubleSpinBox --
    qss += QStringLiteral(
               "QSpinBox, QDoubleSpinBox {"
               "  background-color: %1; color: %2; border: 1px solid %3;"
               "  border-radius: %4px; padding: 2px 4px;"
               "}\n"
               "QSpinBox:focus, QDoubleSpinBox:focus {"
               "  border-color: %5;"
               "}\n"
               "QSpinBox::up-button, QDoubleSpinBox::up-button,"
               "QSpinBox::down-button, QDoubleSpinBox::down-button {"
               "  background-color: %6; border: none; width: 16px;"
               "}\n"
               "QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover,"
               "QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover {"
               "  background-color: %7;"
               "}\n"
               "QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {"
               "  image: none; border-left: 4px solid transparent; border-right: 4px solid transparent;"
               "  border-bottom: 5px solid %8; width: 0; height: 0;"
               "}\n"
               "QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {"
               "  image: none; border-left: 4px solid transparent; border-right: 4px solid transparent;"
               "  border-top: 5px solid %8; width: 0; height: 0;"
               "}\n"
               "QSpinBox:disabled, QDoubleSpinBox:disabled {"
               "  background-color: %9; color: %10;"
               "}\n"
    )
               .arg(
                   c(ColorToken::colorBgContainerTertiary), c(ColorToken::colorText), c(ColorToken::colorBorder),
                   QString::number(radius), c(ColorToken::colorPrimary), c(ColorToken::colorFill),
                   c(ColorToken::colorFillHover), c(ColorToken::colorTextSecondary), c(ColorToken::colorFillSecondary),
                   c(ColorToken::colorTextQuaternary)
               );

    // -- QComboBox --
    qss += QStringLiteral(
               "QComboBox {"
               "  background-color: %1; color: %2; border: 1px solid %3;"
               "  border-radius: %4px; padding: 3px 8px 3px 6px;"
               "}\n"
               "QComboBox:hover { border-color: %5; }\n"
               "QComboBox:focus { border-color: %6; }\n"
               "QComboBox::drop-down {"
               "  border: none; width: 20px; background: transparent;"
               "}\n"
               "QComboBox::down-arrow {"
               "  image: none; border-left: 4px solid transparent; border-right: 4px solid transparent;"
               "  border-top: 5px solid %7; width: 0; height: 0;"
               "}\n"
               "QComboBox QAbstractItemView {"
               "  background-color: %8; color: %2; border: 1px solid %3;"
               "  selection-background-color: %9; selection-color: %10;"
               "  outline: none;"
               "}\n"
               "QComboBox:disabled { background-color: %11; color: %12; }\n"
    )
               .arg(
                   c(ColorToken::colorBgContainerTertiary), c(ColorToken::colorText), c(ColorToken::colorBorder),
                   QString::number(radius), c(ColorToken::colorBorderSecondary), c(ColorToken::colorPrimary),
                   c(ColorToken::colorTextSecondary), c(ColorToken::colorBgContainerTertiary),
                   c(ColorToken::colorPrimaryBg), c(ColorToken::colorText), c(ColorToken::colorFillSecondary),
                   c(ColorToken::colorTextQuaternary)
               );

    // -- QCheckBox --
    qss += QStringLiteral(
               "QCheckBox { spacing: 6px; color: %1; }\n"
               "QCheckBox::indicator {"
               "  width: 16px; height: 16px; border-radius: 3px;"
               "  border: 1px solid %2; background-color: %3;"
               "}\n"
               "QCheckBox::indicator:hover { border-color: %4; }\n"
               "QCheckBox::indicator:checked {"
               "  background-color: %5; border-color: %5;"
               "}\n"
               "QCheckBox::indicator:checked:hover {"
               "  background-color: %6; border-color: %6;"
               "}\n"
               "QCheckBox:disabled { color: %7; }\n"
               "QCheckBox::indicator:disabled {"
               "  background-color: %8; border-color: %9;"
               "}\n"
    )
               .arg(
                   c(ColorToken::colorText), c(ColorToken::colorBorder), c(ColorToken::colorBgContainerTertiary),
                   c(ColorToken::colorPrimary), c(ColorToken::colorPrimary), c(ColorToken::colorPrimaryBgHover),
                   c(ColorToken::colorTextQuaternary), c(ColorToken::colorFillSecondary), c(ColorToken::colorDivider)
               );

    // -- QRadioButton --
    qss += QStringLiteral(
               "QRadioButton { spacing: 6px; color: %1; }\n"
               "QRadioButton::indicator {"
               "  width: 16px; height: 16px; border-radius: 8px;"
               "  border: 1px solid %2; background-color: %3;"
               "}\n"
               "QRadioButton::indicator:hover { border-color: %4; }\n"
               "QRadioButton::indicator:checked {"
               "  background-color: %5; border-color: %5;"
               "}\n"
               "QRadioButton:disabled { color: %6; }\n"
               "QRadioButton::indicator:disabled {"
               "  background-color: %7; border-color: %8;"
               "}\n"
    )
               .arg(
                   c(ColorToken::colorText), c(ColorToken::colorBorder), c(ColorToken::colorBgContainerTertiary),
                   c(ColorToken::colorPrimary), c(ColorToken::colorPrimary), c(ColorToken::colorTextQuaternary),
                   c(ColorToken::colorFillSecondary), c(ColorToken::colorDivider)
               );

    // -- QSlider (horizontal) --
    qss += QStringLiteral(
               "QSlider::groove:horizontal {"
               "  height: 4px; background: %1; border-radius: 2px;"
               "}\n"
               "QSlider::handle:horizontal {"
               "  width: 14px; height: 14px; margin: -5px 0;"
               "  background: %2; border: 2px solid %3; border-radius: 7px;"
               "}\n"
               "QSlider::handle:horizontal:hover {"
               "  background: %4; border-color: %5;"
               "}\n"
               "QSlider::sub-page:horizontal { background: %3; border-radius: 2px; }\n"
               "QSlider::add-page:horizontal { background: %1; border-radius: 2px; }\n"
               "QSlider:disabled::groove:horizontal { background: %6; }\n"
               "QSlider:disabled::handle:horizontal {"
               "  background: %7; border-color: %6;"
               "}\n"
    )
               .arg(
                   c(ColorToken::colorBorder), c(ColorToken::colorBgContainerTertiary), c(ColorToken::colorPrimary),
                   c(ColorToken::colorPrimaryBgHover), c(ColorToken::colorPrimaryBgHover),
                   c(ColorToken::colorFillSecondary), c(ColorToken::colorFillSecondary)
               );

    // -- QSlider (vertical) --
    qss += QStringLiteral(
               "QSlider::groove:vertical {"
               "  width: 4px; background: %1; border-radius: 2px;"
               "}\n"
               "QSlider::handle:vertical {"
               "  width: 14px; height: 14px; margin: 0 -5px;"
               "  background: %2; border: 2px solid %3; border-radius: 7px;"
               "}\n"
               "QSlider::handle:vertical:hover {"
               "  background: %4; border-color: %5;"
               "}\n"
    )
               .arg(
                   c(ColorToken::colorBorder), c(ColorToken::colorBgContainerTertiary), c(ColorToken::colorPrimary),
                   c(ColorToken::colorPrimaryBgHover), c(ColorToken::colorPrimaryBgHover)
               );

    // QScrollBar: no global QSS — NyanScrollBar uses custom paintEvent.
    // Widgets that need themed scrollbars should install NyanScrollBar explicitly.

    // -- QScrollArea --
    qss += QStringLiteral("QScrollArea { background: transparent; border: none; }\n");

    // -- QTabWidget / QTabBar --
    qss += QStringLiteral(
               "QTabWidget::pane {"
               "  border: 1px solid %1; background: %2;"
               "  border-radius: %3px;"
               "}\n"
               "QTabBar::tab {"
               "  background: %4; color: %5; border: 1px solid %1;"
               "  padding: 5px 12px; margin-right: 1px;"
               "  border-top-left-radius: %3px; border-top-right-radius: %3px;"
               "}\n"
               "QTabBar::tab:selected {"
               "  background: %2; color: %6; border-bottom-color: %2;"
               "}\n"
               "QTabBar::tab:hover:!selected {"
               "  background: %7;"
               "}\n"
    )
               .arg(
                   c(ColorToken::colorBorder), c(ColorToken::colorBgContainerTertiary), QString::number(radius),
                   c(ColorToken::colorBgRender), c(ColorToken::colorTextSecondary), c(ColorToken::colorText),
                   c(ColorToken::colorFillHover)
               );

    // -- QGroupBox --
    qss += QStringLiteral(
               "QGroupBox {"
               "  border: 1px solid %1; border-radius: %2px;"
               "  margin-top: 8px; padding-top: 12px;"
               "  background: transparent;"
               "}\n"
               "QGroupBox::title {"
               "  subcontrol-origin: margin; subcontrol-position: top left;"
               "  padding: 0 4px; color: %3; font-weight: bold;"
               "}\n"
    )
               .arg(c(ColorToken::colorBorder), QString::number(radius), c(ColorToken::colorText));

    // -- QProgressBar --
    qss += QStringLiteral(
               "QProgressBar {"
               "  background: %1; border: 1px solid %2; border-radius: %3px;"
               "  text-align: center; color: %4; min-height: 18px;"
               "}\n"
               "QProgressBar::chunk {"
               "  background: %5; border-radius: %3px;"
               "}\n"
    )
               .arg(
                   c(ColorToken::colorFillSecondary), c(ColorToken::colorBorder), QString::number(radius),
                   c(ColorToken::colorText), c(ColorToken::colorPrimary)
               );

    // -- QTreeView / QListView / QTableView --
    qss += QStringLiteral(
               "QTreeView, QListView, QTableView {"
               "  background-color: %1; color: %2;"
               "  border: 1px solid %3; border-radius: %4px;"
               "  selection-background-color: %5; selection-color: %6;"
               "  outline: none;"
               "}\n"
               "QTreeView::item:hover, QListView::item:hover, QTableView::item:hover {"
               "  background-color: %7;"
               "}\n"
               "QTreeView::item:selected, QListView::item:selected, QTableView::item:selected {"
               "  background-color: %5; color: %6;"
               "}\n"
               "QTreeView::branch { background: transparent; }\n"
               "QHeaderView::section {"
               "  background-color: %8; color: %2; border: none;"
               "  border-bottom: 1px solid %3; padding: 4px 8px;"
               "  font-weight: 600;"
               "}\n"
               "QHeaderView::section:hover { background-color: %9; }\n"
    )
               .arg(
                   c(ColorToken::colorBgContainerSecondary), c(ColorToken::colorText), c(ColorToken::colorBorder),
                   QString::number(radius), c(ColorToken::colorPrimaryBg), c(ColorToken::colorPrimary),
                   c(ColorToken::colorFillHover), c(ColorToken::colorBgRender), c(ColorToken::colorFillHover)
               );

    // -- QToolTip --
    qss += QStringLiteral(
               "QToolTip {"
               "  background-color: %1; color: %2; border: 1px solid %3;"
               "  border-radius: %4px; padding: 4px 8px;"
               "  font-size: %5pt;"
               "}\n"
    )
               .arg(
                   c(ColorToken::colorBgContainerQuinary), c(ColorToken::OnAccent), c(ColorToken::colorBorder),
                   QString::number(radius), QString::number(captionFont.sizeInPt)
               );

    // -- QMenu --
    qss += QStringLiteral(
               "QMenu {"
               "  background-color: %1; color: %2; border: 1px solid %3;"
               "  border-radius: %4px; padding: 4px 0;"
               "}\n"
               "QMenu::item {"
               "  padding: 5px 28px 5px 12px;"
               "}\n"
               "QMenu::item:selected {"
               "  background-color: %5; color: %6;"
               "}\n"
               "QMenu::item:disabled { color: %7; }\n"
               "QMenu::separator {"
               "  height: 1px; background: %3; margin: 4px 8px;"
               "}\n"
    )
               .arg(
                   c(ColorToken::colorBgContainerTertiary), c(ColorToken::colorText), c(ColorToken::colorBorder),
                   QString::number(radiusLg), c(ColorToken::colorPrimaryBg), c(ColorToken::colorText),
                   c(ColorToken::colorTextQuaternary)
               );

    // -- QMenuBar --
    qss += QStringLiteral(
               "QMenuBar {"
               "  background: transparent; color: %1; border: none;"
               "}\n"
               "QMenuBar::item { padding: 4px 8px; background: transparent; }\n"
               "QMenuBar::item:selected { background-color: %2; }\n"
    )
               .arg(c(ColorToken::colorText), c(ColorToken::colorFillHover));

    // -- QSplitter --
    qss += QStringLiteral(
               "QSplitter::handle { background: %1; }\n"
               "QSplitter::handle:horizontal { width: 2px; }\n"
               "QSplitter::handle:vertical { height: 2px; }\n"
               "QSplitter::handle:hover { background: %2; }\n"
    )
               .arg(c(ColorToken::colorDivider), c(ColorToken::colorPrimary));

    // -- QStatusBar --
    qss += QStringLiteral(
               "QStatusBar {"
               "  background: %1; color: %2; border-top: 1px solid %3;"
               "}\n"
               "QStatusBar::item { border: none; }\n"
    )
               .arg(c(ColorToken::colorBgRender), c(ColorToken::colorTextSecondary), c(ColorToken::colorDivider));

    // -- QLabel (just inherits base font/color; no border/bg) --
    qss += QStringLiteral("QLabel { background: transparent; }\n");

    // -- QDialog --
    qss += QStringLiteral("QDialog { background: %1; }\n").arg(c(ColorToken::colorBgContainerSecondary));

    // -- QFrame --
    qss += QStringLiteral(
               "QFrame[frameShape=\"4\"], QFrame[frameShape=\"5\"] {"
               "  color: %1;"
               "}\n"
    )
               .arg(c(ColorToken::Separator));

    // Apply the complete stylesheet to the application
    qApp->setStyleSheet(qss);
  }

}  // namespace matcha::gui
