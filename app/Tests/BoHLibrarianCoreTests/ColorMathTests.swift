import XCTest
@testable import BoHLibrarianCore

/// WCAG math backing badge contrast (docs/DECISIONS.md D12). The seeded principle
/// palette is the fixture: as tint-on-tint text every hue fails AA in one mode or
/// the other, so `readableTextHex` must lift text to 4.5:1 per mode while keeping
/// unproblematic tints untouched.
final class ColorMathTests: XCTestCase {

    private static let seededTints = [
        "#7B241C", "#D35400", "#884EA0", "#E74C3C", "#148F77", "#F4D03F",
        "#5D6D7E", "#717D7E", "#27AE60", "#EC87C0", "#6E2C00", "#5DADE2", "#85C1E9",
    ]

    func testChannelParsingAndMalformedInput() throws {
        let prefixed = try XCTUnwrap(ColorMath.channels(ofHex: "#1A2B3C"))
        XCTAssertTrue(prefixed == (0x1A, 0x2B, 0x3C))
        let bare = try XCTUnwrap(ColorMath.channels(ofHex: "1A2B3C"))
        XCTAssertTrue(bare == (0x1A, 0x2B, 0x3C), "# optional")
        XCTAssertNil(ColorMath.channels(ofHex: "nope"))
        XCTAssertNil(ColorMath.channels(ofHex: "#12345"))
        XCTAssertNil(ColorMath.channels(ofHex: "#1A2B3G"))
        XCTAssertNil(ColorMath.relativeLuminance(ofHex: "nope"))
        XCTAssertNil(ColorMath.contrastRatio("#000000", "nope"))
        XCTAssertNil(ColorMath.blend("#000000", over: "nope", alpha: 0.5))
        XCTAssertNil(ColorMath.readableTextHex(tint: "nope", darkMode: false))
    }

    func testLuminanceAndContrastReferenceValues() {
        XCTAssertEqual(ColorMath.relativeLuminance(ofHex: "#000000")!, 0, accuracy: 0.0001)
        XCTAssertEqual(ColorMath.relativeLuminance(ofHex: "#FFFFFF")!, 1, accuracy: 0.0001)
        XCTAssertEqual(ColorMath.contrastRatio("#000000", "#FFFFFF")!, 21, accuracy: 0.01)
        // #767676 is the canonical 4.5:1 grey against white.
        XCTAssertEqual(ColorMath.contrastRatio("#767676", "#FFFFFF")!, 4.54, accuracy: 0.05)
    }

    func testBlending() {
        XCTAssertEqual(ColorMath.blend("#FFFFFF", over: "#000000", alpha: 0), "#000000")
        XCTAssertEqual(ColorMath.blend("#FFFFFF", over: "#000000", alpha: 1), "#FFFFFF")
        // 0x7B (123) at 18% over black: 22.14 -> 22 = 0x16.
        XCTAssertEqual(ColorMath.blend("#7B0000", over: "#000000", alpha: 0.18), "#160000")
        // 123*0.18 + 255*0.82 = 231.24 -> 231 = 0xE7.
        XCTAssertEqual(ColorMath.blend("#7B0000", over: "#FF0000", alpha: 0.18), "#E70000")
    }

    /// The enforcement: every seeded principle reads at 4.5:1 over its own capsule
    /// backdrop in BOTH appearance modes (the raw tints fail 13/13 in some mode).
    func testReadableTextMeetsAAForEverySeededPrincipleInBothModes() throws {
        for tint in Self.seededTints {
            for darkMode in [false, true] {
                let text = try XCTUnwrap(ColorMath.readableTextHex(tint: tint, darkMode: darkMode),
                                         "\(tint) light=\(!darkMode)")
                let windowBackground = darkMode ? ColorMath.darkWindowBackground
                                                : ColorMath.lightWindowBackground
                let backdrop = try XCTUnwrap(
                    ColorMath.blend(tint, over: windowBackground, alpha: 0.18))
                let ratio = try XCTUnwrap(ColorMath.contrastRatio(text, backdrop))
                XCTAssertGreaterThanOrEqual(ratio, 4.5,
                    "\(tint) (darkMode=\(darkMode)) text \(text) reads \(ratio)")
            }
        }
    }

    func testReadableTextKeepsAlreadyReadableTintsUntouched() {
        // Edge (#7B241C) and Scale (#6E2C00) pass light mode as-is; not dark.
        XCTAssertEqual(ColorMath.readableTextHex(tint: "#7B241C", darkMode: false), "#7B241C")
        XCTAssertEqual(ColorMath.readableTextHex(tint: "#6E2C00", darkMode: false), "#6E2C00")
        // Lantern (#F4D03F) reads well on dark and is untouched there.
        XCTAssertEqual(ColorMath.readableTextHex(tint: "#F4D03F", darkMode: true), "#F4D03F")
        // …and is lifted in light mode (it read 1.21:1 as raw tint text).
        XCTAssertNotEqual(ColorMath.readableTextHex(tint: "#F4D03F", darkMode: false), "#F4D03F")
        // Extreme stop: black tint under light mode stays black (already max).
        XCTAssertEqual(ColorMath.readableTextHex(tint: "#000000", darkMode: false), "#000000")
    }
}