import Foundation

// WCAG 2.1 color math backing the UI's contrast rules (docs/DECISIONS.md D12).
// Pure Foundation on purpose: the app's principle palette is user-editable data,
// so the *app* derives display colors from it per appearance mode — this file is
// the testable derivation (the seeded colors as-is fail AA in one mode or the
// other for all 13 hues; see ColorMathTests for the enforcement).

public enum ColorMath {

    /// Approximate macOS window background per appearance — the capsule/badge
    /// backdrop these colors must read against.
    public static let lightWindowBackground = "#ECECEC"
    public static let darkWindowBackground = "#282828"

    public static func channels(ofHex hex: String) -> (Int, Int, Int)? {
        let digits = hex.hasPrefix("#") ? String(hex.dropFirst()) : hex
        guard digits.count == 6, let value = UInt64(digits, radix: 16) else { return nil }
        return (Int((value >> 16) & 0xFF), Int((value >> 8) & 0xFF), Int(value & 0xFF))
    }

    public static func hex(_ channels: (Int, Int, Int)) -> String {
        String(format: "#%02X%02X%02X", channels.0, channels.1, channels.2)
    }

    /// WCAG 2.1 relative luminance of an sRGB "#RRGGBB" (nil when malformed).
    public static func relativeLuminance(ofHex hex: String) -> Double? {
        guard let (r, g, b) = channels(ofHex: hex) else { return nil }
        func linear(_ v: Int) -> Double {
            let x = Double(v) / 255
            return x <= 0.03928 ? x / 12.92 : pow((x + 0.055) / 1.055, 2.4)
        }
        return 0.2126 * linear(r) + 0.7152 * linear(g) + 0.0722 * linear(b)
    }

    /// WCAG contrast ratio (1…21); nil when either hex is malformed.
    public static func contrastRatio(_ a: String, _ b: String) -> Double? {
        guard let la = relativeLuminance(ofHex: a), let lb = relativeLuminance(ofHex: b) else {
            return nil
        }
        let hi = max(la, lb), lo = min(la, lb)
        return (hi + 0.05) / (lo + 0.05)
    }

    /// Alpha-blend "#RRGGBB" `foreground` over "#RRGGBB" `background`; "#RRGGBB" out.
    public static func blend(_ foreground: String, over background: String,
                             alpha: Double) -> String? {
        guard let f = channels(ofHex: foreground), let b = channels(ofHex: background) else {
            return nil
        }
        let mix = { (f: Int, b: Int) in Int((Double(f) * alpha + Double(b) * (1 - alpha)).rounded()) }
        return hex((mix(f.0, b.0), mix(f.1, b.1), mix(f.2, b.2)))
    }

    /// The text color for tint-identified UI (badges: fill/border keep the tint as
    /// the visual identity; the *text* must read over the 18%-tint capsule this
    /// UI paints). Returns `tint` unchanged when it already meets `target`;
    /// otherwise the nearest mix toward black (light mode) or white (dark mode)
    /// that does. Nil when `tint` is malformed (callers fall back to `.primary`).
    public static func readableTextHex(tint: String, darkMode: Bool,
                                       target: Double = 4.5) -> String? {
        guard let channel = channels(ofHex: tint) else { return nil }
        let windowBackground = darkMode ? darkWindowBackground : lightWindowBackground
        let backdrop = blend(tint, over: windowBackground, alpha: 0.18) ?? windowBackground
        if (contrastRatio(tint, backdrop) ?? 0) >= target { return tint }
        let stop: (Int, Int, Int) = darkMode ? (255, 255, 255) : (0, 0, 0)
        for step in stride(from: 0.05, through: 1.0, by: 0.05) {
            let mix = { (v: Int, s: Int) in Int((Double(v) + (Double(s) - Double(v)) * step).rounded()) }
            let candidate = hex((mix(channel.0, stop.0), mix(channel.1, stop.1), mix(channel.2, stop.2)))
            if (contrastRatio(candidate, backdrop) ?? 0) >= target { return candidate }
        }
        return darkMode ? "#FFFFFF" : "#000000"
    }
}