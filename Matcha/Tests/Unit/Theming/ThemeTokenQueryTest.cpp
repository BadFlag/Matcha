#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc2y-extensions"
#endif

#include "doctest.h"

#include <Matcha/Theming/NyanTheme.h>

#include "QtAppGuard.h"

#include <QColor>
#include <QString>

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

    const auto shadow = theme.Shadow("shadowSM");
    REQUIRE(shadow.has_value());
    REQUIRE(shadow->size() == 3);

    const auto& first = shadow->front();
    CHECK(first.offsetX == 0);
    CHECK(first.offsetY == 1);
    CHECK(first.blurRadius == 2);
    CHECK(first.spread == -2);
    CHECK(first.color == QColor(0, 10, 26, 40));

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

TEST_CASE("Dynamic tokens remain queryable but do not define core theme token existence") {
    NyanTheme theme(QStringLiteral(MATCHA_TEST_PALETTE_DIR));
    RegisterAndLoadLight(theme);

    CHECK_FALSE(theme.DynamicColor("Test/MyColor").has_value());

    const std::array defs = {
        IThemeService::DynamicColorDef {
            .key = "Test/MyColor",
            .lightValue = QColor(255, 0, 0),
            .darkValue = QColor(0, 0, 255),
        },
    };
    theme.RegisterDynamicTokens(defs);

    const auto color = theme.DynamicColor("Test/MyColor");
    REQUIRE(color.has_value());
    CHECK(*color == QColor(255, 0, 0));
    CHECK_FALSE(theme.HasToken(TokenKind::Color, "Test/MyColor"));
}

} // TEST_SUITE

#ifdef __clang__
#pragma clang diagnostic pop
#endif
