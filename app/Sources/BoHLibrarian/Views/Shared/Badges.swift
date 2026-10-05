import SwiftUI

// Small shared UI pieces used across screens.

// MARK: - Principle badge

/// Colored capsule showing a principle and its level, tinted from the
/// seeded `Principles.color` hex value.
struct PrincipleBadge: View {
    let name: String?
    let level: Int
    let colorHex: String?

    var body: some View {
        let tint = Color(hex: colorHex)
        HStack(spacing: 3) {
            Text(name ?? "?")
                .font(.caption2.weight(.semibold))
            Text("\(level)")
                .font(.caption2.weight(.bold))
        }
        .padding(.horizontal, 6)
        .padding(.vertical, 2)
        .background(Capsule().fill(tint.opacity(0.18)))
        .overlay(Capsule().strokeBorder(tint.opacity(0.35)))
        .foregroundStyle(tint)
        .help(name.map { "\($0) \(level)" } ?? "unknown principle")
    }
}

// MARK: - Hex color

extension Color {
    /// Parses the seeded "#RRGGBB" strings; falls back to a neutral grey when
    /// malformed or absent.
    init(hex: String?, fallback: Color = Color(nsColor: .systemGray)) {
        guard let hex, let parsed = Self.parseHex(hex) else {
            self = fallback
            return
        }
        self = parsed
    }

    private static func parseHex(_ hex: String) -> Color? {
        let digits = hex.hasPrefix("#") ? String(hex.dropFirst()) : hex
        guard digits.count == 6, let value = UInt64(digits, radix: 16) else { return nil }
        let red = Double((value >> 16) & 0xFF) / 255.0
        let green = Double((value >> 8) & 0xFF) / 255.0
        let blue = Double(value & 0xFF) / 255.0
        return Color(red: red, green: green, blue: blue)
    }
}