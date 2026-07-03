#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc2y-extensions"
#endif

#include "doctest.h"

#include <Matcha/Theming/NyanTheme.h>

#include "QtAppGuard.h"

#include <QColor>
#include <QFile>
#include <QString>
#include <QTemporaryDir>

#include <array>
#include <utility>

using namespace matcha::gui;
using namespace matcha::fw;

namespace {
  void RegisterAndLoadLight(NyanTheme& theme) {
    matcha::test::QtAppGuard::Ensure();

    const auto lightPath = QStringLiteral(MATCHA_TEST_PALETTE_DIR "/Light.json");
    REQUIRE(theme.RegisterTheme(lightPath));
    theme.SetTheme(kThemeLight);
    REQUIRE(theme.CurrentTheme() == kThemeLight);
  }

  void RegisterAndLoadFixture(NyanTheme& theme, const QString& fileName, const QString& themeName) {
    matcha::test::QtAppGuard::Ensure();

    const auto path = QStringLiteral(MATCHA_TEST_FIXTURE_DIR "/") + fileName;
    REQUIRE(theme.RegisterTheme(path));
    theme.SetTheme(themeName);
    REQUIRE(theme.CurrentTheme() == themeName);
  }
}

TEST_SUITE("ThemeTokenQuery") {

TEST_CASE("RegisterTheme uses json name and rejects invalid inputs") {
    matcha::test::QtAppGuard::Ensure();
    NyanTheme theme(QStringLiteral(MATCHA_TEST_PALETTE_DIR));

    CHECK(theme.RegisterTheme(QStringLiteral(MATCHA_TEST_PALETTE_DIR "/Light.json")));
    CHECK_FALSE(theme.RegisterTheme(QStringLiteral("/nonexistent/theme.json")));
    CHECK_FALSE(theme.RegisterTheme(QStringLiteral(MATCHA_TEST_FIXTURE_DIR "/MissingNameTheme.json")));
}

TEST_CASE("RegisterTheme only uses json root name as theme identity") {
    matcha::test::QtAppGuard::Ensure();
    NyanTheme theme(QStringLiteral(MATCHA_TEST_PALETTE_DIR));
    RegisterAndLoadLight(theme);

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const auto path = tempDir.filePath(QStringLiteral("PathNameIsIgnored.json"));

    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const auto json = QByteArray(R"({
        "name": "JsonDeclaredName",
        "colors": {
            "colorPrimary": "#112233"
        }
    })");
    REQUIRE(file.write(json) == json.size());
    file.close();

    REQUIRE(theme.RegisterTheme(path));

    theme.SetTheme(QStringLiteral("PathNameIsIgnored"));
    CHECK(theme.CurrentTheme() == kThemeLight);

    theme.SetTheme(QStringLiteral("JsonDeclaredName"));
    CHECK(theme.CurrentTheme() == QStringLiteral("JsonDeclaredName"));
    CHECK(theme.Color("colorPrimary") == QColor::fromString(QStringLiteral("#112233")));
}

TEST_CASE("C API migration boundary ignores passed name and isDark") {
    matcha::test::QtAppGuard::Ensure();
    NyanTheme theme(QStringLiteral(MATCHA_TEST_PALETTE_DIR));
    RegisterAndLoadLight(theme);

    const auto registerThroughCapiBoundary =
        [](NyanTheme& service, const char* name, const QString& jsonPath, int isDark) {
            (void)name;
            (void)isDark;
            return service.RegisterTheme(jsonPath);
        };

    REQUIRE(registerThroughCapiBoundary(
        theme,
        "CallerProvidedName",
        QStringLiteral(MATCHA_TEST_FIXTURE_DIR "/HighContrast.json"),
        1
    ));

    theme.SetTheme(QStringLiteral("CallerProvidedName"));
    CHECK(theme.CurrentTheme() == kThemeLight);

    theme.SetTheme(kThemeHighContrast);
    CHECK(theme.CurrentTheme() == kThemeHighContrast);
}

TEST_CASE("SetTheme failures do not change current theme or reset loaded tokens") {
    matcha::test::QtAppGuard::Ensure();
    NyanTheme theme(QStringLiteral(MATCHA_TEST_PALETTE_DIR));
    RegisterAndLoadLight(theme);
    const auto lightPrimary = theme.Color("colorPrimary");
    REQUIRE(lightPrimary.has_value());

    theme.SetTheme(QStringLiteral("UnregisteredTheme"));
    CHECK(theme.CurrentTheme() == kThemeLight);
    CHECK(theme.Color("colorPrimary") == lightPrimary);

    QTemporaryDir tempDir;
    REQUIRE(tempDir.isValid());
    const auto path = tempDir.filePath(QStringLiteral("Breakable.json"));
    {
        QFile file(path);
        REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        const auto validJson = QByteArray(R"({"name":"Breakable","colors":{"colorPrimary":"#445566"}})");
        REQUIRE(file.write(validJson) == validJson.size());
    }
    REQUIRE(theme.RegisterTheme(path));
    {
        QFile file(path);
        REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        const auto invalidJson = QByteArray(R"({"name":"Breakable")");
        REQUIRE(file.write(invalidJson) == invalidJson.size());
    }

    theme.SetTheme(QStringLiteral("Breakable"));
    CHECK(theme.CurrentTheme() == kThemeLight);
    CHECK(theme.Color("colorPrimary") == lightPrimary);
}

TEST_CASE("Color key query returns Light.json colors and excludes gradients") {
    NyanTheme theme(QStringLiteral(MATCHA_TEST_PALETTE_DIR));
    RegisterAndLoadLight(theme);

    const auto primary = theme.Color("colorPrimary");
    REQUIRE(primary.has_value());
    CHECK(*primary == QColor::fromString(QStringLiteral("#0066FF")));

    CHECK_FALSE(theme.Color("colorPrimaryGradient").has_value());
    CHECK_FALSE(theme.Color("missingColor").has_value());
}

TEST_CASE("Gradient key query parses pipe-delimited stops") {
    NyanTheme theme(QStringLiteral(MATCHA_TEST_PALETTE_DIR));
    RegisterAndLoadLight(theme);

    const auto gradient = theme.Gradient("colorPrimaryGradient");
    REQUIRE(gradient.has_value());
    REQUIRE(gradient->stops.size() == 2);

    CHECK(gradient->stops[0].position == doctest::Approx(0.0));
    CHECK(gradient->stops[0].color == QColor::fromString(QStringLiteral("#0F0077FF")));
    CHECK(gradient->stops[1].position == doctest::Approx(1.0));
    CHECK(gradient->stops[1].color == QColor::fromString(QStringLiteral("#2B0066FF")));

    CHECK_FALSE(theme.Gradient("colorPrimary").has_value());
    CHECK_FALSE(theme.Gradient("missingGradient").has_value());
}

TEST_CASE("Light.json exposes semantic gradient tokens as GradientSpec") {
    NyanTheme theme(QStringLiteral(MATCHA_TEST_PALETTE_DIR));
    RegisterAndLoadLight(theme);

    const std::array keys = {
        "colorPrimaryGradient",
        "colorSuccessGradient",
        "colorWarningGradient",
        "colorErrorGradient",
    };

    for (const auto* key : keys) {
        const auto gradient = theme.Gradient(key);
        REQUIRE(gradient.has_value());
        REQUIRE(gradient->stops.size() == 2);
        CHECK(gradient->stops.front().position == doctest::Approx(0.0));
        CHECK(gradient->stops.back().position == doctest::Approx(1.0));
        CHECK(gradient->stops.front().color.isValid());
        CHECK(gradient->stops.back().color.isValid());
        CHECK_FALSE(theme.Color(key).has_value());
        CHECK(theme.HasToken(TokenKind::Gradient, key));
        CHECK_FALSE(theme.HasToken(TokenKind::Color, key));
    }
}

TEST_CASE("Gradient parser supports multiple stops and rejects invalid definitions") {
    NyanTheme theme(QStringLiteral(MATCHA_TEST_PALETTE_DIR));
    RegisterAndLoadFixture(
        theme,
        QStringLiteral("GradientTheme.json"),
        QStringLiteral("GradientFixture")
    );

    const auto gradient = theme.Gradient("colorPrimaryGradient");
    REQUIRE(gradient.has_value());
    REQUIRE(gradient->stops.size() == 3);
    CHECK(gradient->stops[0].position == doctest::Approx(0.0));
    CHECK(gradient->stops[0].color == QColor::fromString(QStringLiteral("#000000")));
    CHECK(gradient->stops[1].position == doctest::Approx(0.5));
    CHECK(gradient->stops[1].color == QColor::fromString(QStringLiteral("#808080")));
    CHECK(gradient->stops[2].position == doctest::Approx(1.0));
    CHECK(gradient->stops[2].color == QColor::fromString(QStringLiteral("#FFFFFF")));

    CHECK_FALSE(theme.Gradient("colorWarningGradient").has_value());
    CHECK_FALSE(theme.HasToken(TokenKind::Gradient, "colorWarningGradient"));
    CHECK_FALSE(theme.Color("colorWarningGradient").has_value());

    CHECK_FALSE(theme.Gradient("colorErrorGradient").has_value());
    CHECK_FALSE(theme.HasToken(TokenKind::Gradient, "colorErrorGradient"));
    CHECK_FALSE(theme.Color("colorErrorGradient").has_value());
}

TEST_CASE("Dimension key query covers spacing radius line icon control and container groups") {
    NyanTheme theme(QStringLiteral(MATCHA_TEST_PALETTE_DIR));
    RegisterAndLoadLight(theme);

    CHECK(theme.DimensionPx("spaceXXS") == 4);
    CHECK(theme.DimensionPx("radiusDefault") == 3);
    CHECK(theme.DimensionPx("lineWidthMD") == 2);
    CHECK(theme.DimensionPx("iconSizeMD") == 24);
    CHECK(theme.DimensionPx("controlHeightMD") == 32);
    CHECK(theme.DimensionPx("containerWidthMD") == 32);
    CHECK_FALSE(theme.DimensionPx("missingDimension").has_value());
}

TEST_CASE("Font key query returns complete FontSpec") {
    NyanTheme theme(QStringLiteral(MATCHA_TEST_PALETTE_DIR));
    RegisterAndLoadLight(theme);

    const std::array expected = {
        std::pair{"fontXS", 10},
        std::pair{"fontSM", 12},
        std::pair{"fontMS", 14},
        std::pair{"fontMD", 16},
        std::pair{"fontLG", 16},
    };
    for (const auto& [key, size] : expected) {
        const auto font = theme.Font(key);
        REQUIRE(font.has_value());
        CHECK(!font->family.isEmpty());
        CHECK(font->sizeInPt == size);
        CHECK(font->weight == 400);
        CHECK_FALSE(font->italic);
        CHECK(font->lineHeightMultiplier == doctest::Approx(1.4));
        CHECK(font->letterSpacing == doctest::Approx(0.0));
    }

    const auto font = theme.Font("fontSM");
    REQUIRE(font.has_value());
    CHECK(!font->family.isEmpty());
    CHECK(font->sizeInPt == 12);
    CHECK(font->weight == 400);
    CHECK(font->lineHeightMultiplier == doctest::Approx(1.4));

    theme.SetFontScale(1.5F);
    const auto scaled = theme.Font("fontSM");
    REQUIRE(scaled.has_value());
    CHECK(scaled->sizeInPt == 18);

    CHECK_FALSE(theme.Font("missingFont").has_value());
}

TEST_CASE("Font tokens merge complete JSON fields over defaults") {
    NyanTheme theme(QStringLiteral(MATCHA_TEST_PALETTE_DIR));
    RegisterAndLoadFixture(
        theme,
        QStringLiteral("FontCompleteTheme.json"),
        QStringLiteral("FontComplete")
    );

    const auto xs = theme.Font("fontXS");
    REQUIRE(xs.has_value());
    CHECK(xs->family == QStringLiteral("Fixture Sans"));
    CHECK(xs->sizeInPt == 9);
    CHECK(xs->weight == 500);
    CHECK(xs->italic);
    CHECK(xs->lineHeightMultiplier == doctest::Approx(1.25));
    CHECK(xs->letterSpacing == doctest::Approx(0.5));

    const auto sm = theme.Font("fontSM");
    REQUIRE(sm.has_value());
    CHECK(!sm->family.isEmpty());
    CHECK(sm->sizeInPt == 11);
    CHECK(sm->weight == 450);
    CHECK(sm->lineHeightMultiplier == doctest::Approx(1.5));

    const auto lg = theme.Font("fontLG");
    REQUIRE(lg.has_value());
    CHECK(lg->sizeInPt == 17);
    CHECK(lg->weight == 700);
}

TEST_CASE("Font tokens keep default fields when JSON omits or invalidates them") {
    NyanTheme theme(QStringLiteral(MATCHA_TEST_PALETTE_DIR));
    RegisterAndLoadFixture(
        theme,
        QStringLiteral("FontFallbackTheme.json"),
        QStringLiteral("FontFallback")
    );

    const auto sm = theme.Font("fontSM");
    REQUIRE(sm.has_value());
    CHECK(!sm->family.isEmpty());
    CHECK(sm->sizeInPt == 13);
    CHECK(sm->weight == 400);
    CHECK_FALSE(sm->italic);
    CHECK(sm->lineHeightMultiplier == doctest::Approx(1.4));
    CHECK(sm->letterSpacing == doctest::Approx(0.0));

    const auto ms = theme.Font("fontMS");
    REQUIRE(ms.has_value());
    CHECK(!ms->family.isEmpty());
    CHECK(ms->sizeInPt == 14);
    CHECK(ms->weight == 400);
    CHECK(ms->lineHeightMultiplier == doctest::Approx(1.4));
}

TEST_CASE("Shadow key query returns full multi-layer structure") {
    NyanTheme theme(QStringLiteral(MATCHA_TEST_PALETTE_DIR));
    RegisterAndLoadLight(theme);

    const auto shadowSM = theme.Shadow("shadowSM");
    REQUIRE(shadowSM.has_value());
    REQUIRE(shadowSM->size() == 3);

    const auto& first = shadowSM->front();
    CHECK(first.offsetX == 0);
    CHECK(first.offsetY == 1);
    CHECK(first.blurRadius == 2);
    CHECK(first.spread == -2);
    CHECK(first.color == QColor(0, 10, 26, 40));

    const auto shadowMS = theme.Shadow("shadowMS");
    REQUIRE(shadowMS.has_value());
    REQUIRE(shadowMS->size() == 3);
    CHECK(shadowMS->front().offsetY == 3);
    CHECK(shadowMS->front().blurRadius == 6);
    CHECK(shadowMS->front().spread == -4);

    const auto shadowMD = theme.Shadow("shadowMD");
    REQUIRE(shadowMD.has_value());
    REQUIRE(shadowMD->size() == 3);
    CHECK(shadowMD->front().offsetY == 6);
    CHECK(shadowMD->front().blurRadius == 16);
    CHECK(shadowMD->front().spread == -8);

    CHECK_FALSE(theme.Shadow("missingShadow").has_value());
}

TEST_CASE("HasToken checks token existence without fallback") {
    NyanTheme theme(QStringLiteral(MATCHA_TEST_PALETTE_DIR));
    RegisterAndLoadLight(theme);

    CHECK(theme.HasToken(TokenKind::Color, "colorPrimary"));
    CHECK(theme.HasToken(TokenKind::Gradient, "colorPrimaryGradient"));
    CHECK(theme.HasToken(TokenKind::Dimension, "spaceXXS"));
    CHECK(theme.HasToken(TokenKind::Font, "fontSM"));
    CHECK(theme.HasToken(TokenKind::Shadow, "shadowSM"));

    CHECK_FALSE(theme.HasToken(TokenKind::Color, "colorPrimaryGradient"));
    CHECK_FALSE(theme.HasToken(TokenKind::Gradient, "colorPrimary"));
    CHECK_FALSE(theme.HasToken(TokenKind::Dimension, "missingDimension"));
    CHECK_FALSE(theme.HasToken(TokenKind::Shadow, "missingShadow"));
    CHECK_FALSE(theme.HasToken(TokenKind::Shadow, "boxShadow"));
    CHECK_FALSE(theme.HasToken(TokenKind::Shadow, "shadow"));
}

TEST_CASE("Legacy enum APIs map through new key stores during migration") {
    NyanTheme theme(QStringLiteral(MATCHA_TEST_PALETTE_DIR));
    RegisterAndLoadFixture(
        theme,
        QStringLiteral("FontCompleteTheme.json"),
        QStringLiteral("FontComplete")
    );

    CHECK(theme.Color(ColorToken::colorPrimary) == QColor::fromString(QStringLiteral("#0066FF")));
    CHECK(theme.SpacingPx(SpaceToken::marginXXS) == 4);
    CHECK(theme.Radius(RadiusToken::borderRadiusMD) == 3);

    const auto fontXS = theme.Font(FontRole::fontSizeXS);
    const auto fontSM = theme.Font(FontRole::fontSizeSM);
    const auto fontMS = theme.Font(FontRole::fontWeightMedium);
    const auto fontMD = theme.Font(FontRole::fontSizeMD);
    const auto fontLG = theme.Font(FontRole::fontLineHeight);
    CHECK(fontXS.sizeInPt == 9);
    CHECK(fontSM.sizeInPt == 11);
    CHECK(fontMS.sizeInPt == 13);
    CHECK(fontMD.sizeInPt == 15);
    CHECK(fontLG.sizeInPt == 17);

    const auto& shadow = theme.Shadow(ShadowToken::boxShadow);
    CHECK(shadow.offsetY == 1);
    CHECK(shadow.blurRadius == 2);
    CHECK(shadow.opacity > 0.0);
}

TEST_CASE("Legacy ShadowToken compatibility maps to first layer of shadow key") {
    NyanTheme theme(QStringLiteral(MATCHA_TEST_PALETTE_DIR));
    RegisterAndLoadFixture(
        theme,
        QStringLiteral("ShadowTheme.json"),
        QStringLiteral("ShadowFixture")
    );

    const auto& flat = theme.Shadow(ShadowToken::shadow);
    CHECK(flat.offsetX == 0);
    CHECK(flat.offsetY == 0);
    CHECK(flat.blurRadius == 0);
    CHECK(flat.opacity == doctest::Approx(0.0));

    const auto& low = theme.Shadow(ShadowToken::boxShadow);
    CHECK(low.offsetX == 2);
    CHECK(low.offsetY == 4);
    CHECK(low.blurRadius == 10);
    CHECK(low.opacity == doctest::Approx(128.0 / 255.0));

    const auto& medium = theme.Shadow(ShadowToken::boxShadowSecondary);
    CHECK(medium.offsetX == 0);
    CHECK(medium.offsetY == 8);
    CHECK(medium.blurRadius == 20);
    CHECK(medium.opacity == doctest::Approx(96.0 / 255.0));

    const auto& high = theme.Shadow(ShadowToken::boxShadowTertiary);
    CHECK(high.offsetX == 0);
    CHECK(high.offsetY == 12);
    CHECK(high.blurRadius == 30);
    CHECK(high.opacity == doctest::Approx(64.0 / 255.0));
}

TEST_CASE("SetTheme refreshes shadow store between themes") {
    NyanTheme theme(QStringLiteral(MATCHA_TEST_PALETTE_DIR));
    RegisterAndLoadLight(theme);

    auto shadow = theme.Shadow("shadowSM");
    REQUIRE(shadow.has_value());
    REQUIRE(shadow->size() == 3);
    CHECK(shadow->front().offsetY == 1);

    REQUIRE(theme.RegisterTheme(QStringLiteral(MATCHA_TEST_FIXTURE_DIR "/ShadowTheme.json")));
    theme.SetTheme(QStringLiteral("ShadowFixture"));
    REQUIRE(theme.CurrentTheme() == QStringLiteral("ShadowFixture"));

    shadow = theme.Shadow("shadowSM");
    REQUIRE(shadow.has_value());
    REQUIRE(shadow->size() == 1);
    CHECK(shadow->front().offsetX == 2);
    CHECK(shadow->front().offsetY == 4);
    CHECK(shadow->front().blurRadius == 10);
    CHECK(shadow->front().spread == 1);
    CHECK(shadow->front().color == QColor(1, 2, 3, 128));

    theme.SetTheme(kThemeLight);
    REQUIRE(theme.CurrentTheme() == kThemeLight);
    shadow = theme.Shadow("shadowSM");
    REQUIRE(shadow.has_value());
    REQUIRE(shadow->size() == 3);
    CHECK(shadow->front().offsetY == 1);
}

TEST_CASE("Invalid shadow layer fields fail load without polluting current theme") {
    NyanTheme theme(QStringLiteral(MATCHA_TEST_PALETTE_DIR));
    RegisterAndLoadLight(theme);

    const auto lightPrimary = theme.Color("colorPrimary");
    REQUIRE(lightPrimary.has_value());
    const auto lightShadow = theme.Shadow("shadowSM");
    REQUIRE(lightShadow.has_value());
    REQUIRE(lightShadow->size() == 3);
    const auto lightFirstLayer = lightShadow->front();

    REQUIRE(theme.RegisterTheme(QStringLiteral(MATCHA_TEST_FIXTURE_DIR "/InvalidShadowTheme.json")));
    theme.SetTheme(QStringLiteral("InvalidShadow"));

    CHECK(theme.CurrentTheme() == kThemeLight);
    CHECK(theme.Color("colorPrimary") == lightPrimary);

    const auto shadowAfterFailure = theme.Shadow("shadowSM");
    REQUIRE(shadowAfterFailure.has_value());
    REQUIRE(shadowAfterFailure->size() == 3);
    CHECK(shadowAfterFailure->front().offsetX == lightFirstLayer.offsetX);
    CHECK(shadowAfterFailure->front().offsetY == lightFirstLayer.offsetY);
    CHECK(shadowAfterFailure->front().blurRadius == lightFirstLayer.blurRadius);
    CHECK(shadowAfterFailure->front().spread == lightFirstLayer.spread);
    CHECK(shadowAfterFailure->front().color == lightFirstLayer.color);
}

TEST_CASE("Legacy gradient ColorToken does not consume gradient tokens as colors") {
    NyanTheme theme(QStringLiteral(MATCHA_TEST_PALETTE_DIR));
    RegisterAndLoadLight(theme);

    CHECK_FALSE(theme.Color("colorPrimaryGradient").has_value());
    REQUIRE(theme.Gradient("colorPrimaryGradient").has_value());

    const auto legacyGradientColor = theme.Color(ColorToken::colorPrimaryGradient);
    CHECK(legacyGradientColor.isValid());
    CHECK(legacyGradientColor.alpha() == 0);
}

TEST_CASE("Dynamic tokens remain queryable but do not define core theme token existence") {
    NyanTheme theme(QStringLiteral(MATCHA_TEST_PALETTE_DIR));
    RegisterAndLoadLight(theme);
    REQUIRE(theme.RegisterTheme(QStringLiteral(MATCHA_TEST_PALETTE_DIR "/Dark.json")));
    REQUIRE(theme.RegisterTheme(QStringLiteral(MATCHA_TEST_FIXTURE_DIR "/HighContrast.json")));

    CHECK_FALSE(theme.DynamicColor("Test/MyColor").has_value());

    const std::array defs = {
        IThemeService::DynamicColorDef {
            .key = "Test/MyColor",
            .value = QColor(255, 0, 0),
        },
    };
    theme.RegisterDynamicTokens(defs);

    auto color = theme.DynamicColor("Test/MyColor");
    REQUIRE(color.has_value());
    CHECK(*color == QColor(255, 0, 0));
    CHECK_FALSE(theme.HasToken(TokenKind::Color, "Test/MyColor"));

    const std::array themeDefs = {
        IThemeService::DynamicThemeColorDef {
            .key = "Test/MyColor",
            .themeName = "Dark",
            .value = QColor(0, 0, 255),
        },
    };
    theme.RegisterDynamicThemeColors(themeDefs);

    theme.SetTheme(kThemeDark);
    color = theme.DynamicColor("Test/MyColor");
    REQUIRE(color.has_value());
    CHECK(*color == QColor(0, 0, 255));

    theme.SetTheme(kThemeHighContrast);
    color = theme.DynamicColor("Test/MyColor");
    REQUIRE(color.has_value());
    CHECK(*color == QColor(255, 0, 0));

    std::array<std::string_view, 1> keys = {"Test/MyColor"};
    theme.UnregisterDynamicTokens(keys);
    CHECK_FALSE(theme.DynamicColor("Test/MyColor").has_value());
}

TEST_CASE("kThemeLight and kThemeDark are built-in theme name constants only") {
    NyanTheme theme(QStringLiteral(MATCHA_TEST_PALETTE_DIR));
    theme.SetTheme(kThemeLight);
    REQUIRE(theme.CurrentTheme() == kThemeLight);
    const auto lightPrimary = theme.Color("colorPrimary");
    REQUIRE(lightPrimary.has_value());

    theme.SetTheme(kThemeDark);
    REQUIRE(theme.CurrentTheme() == kThemeDark);
    const auto darkPrimary = theme.Color("colorPrimary");
    REQUIRE(darkPrimary.has_value());

    CHECK(kThemeLight == QStringLiteral("Light"));
    CHECK(kThemeDark == QStringLiteral("Dark"));
    CHECK(*lightPrimary == *darkPrimary);
}

} // TEST_SUITE

#ifdef __clang__
#pragma clang diagnostic pop
#endif
