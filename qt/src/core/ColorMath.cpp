#include "ColorMath.h"

#include <algorithm>
#include <cmath>

namespace boh {

std::optional<ColorMath::Channels> ColorMath::channels(const QString& hex)
{
    const QString digits = hex.startsWith(QLatin1Char('#')) ? hex.mid(1) : hex;
    if (digits.size() != 6)
        return std::nullopt;
    bool ok = false;
    const qulonglong value = digits.toULongLong(&ok, 16);
    if (!ok)
        return std::nullopt;
    return Channels{int((value >> 16) & 0xFF), int((value >> 8) & 0xFF), int(value & 0xFF)};
}

QString ColorMath::hex(Channels channels)
{
    return QString::asprintf("#%02X%02X%02X", channels.r, channels.g, channels.b);
}

std::optional<double> ColorMath::relativeLuminance(const QString& hex)
{
    const auto c = channels(hex);
    if (!c)
        return std::nullopt;
    const auto linear = [](int v) {
        const double x = double(v) / 255;
        return x <= 0.03928 ? x / 12.92 : std::pow((x + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(c->r) + 0.7152 * linear(c->g) + 0.0722 * linear(c->b);
}

std::optional<double> ColorMath::contrastRatio(const QString& a, const QString& b)
{
    const auto la = relativeLuminance(a);
    const auto lb = relativeLuminance(b);
    if (!la || !lb)
        return std::nullopt;
    const double hi = std::max(*la, *lb);
    const double lo = std::min(*la, *lb);
    return (hi + 0.05) / (lo + 0.05);
}

std::optional<QString> ColorMath::blend(const QString& foreground, const QString& background,
                                        double alpha)
{
    const auto f = channels(foreground);
    const auto b = channels(background);
    if (!f || !b)
        return std::nullopt;
    const auto mix = [alpha](int f, int b) {
        return int(std::round(double(f) * alpha + double(b) * (1 - alpha)));
    };
    return hex(Channels{mix(f->r, b->r), mix(f->g, b->g), mix(f->b, b->b)});
}

std::optional<QString> ColorMath::readableTextHex(const QString& tint, bool darkMode, double target)
{
    const auto channel = channels(tint);
    if (!channel)
        return std::nullopt;
    const QString windowBackground = darkMode ? darkWindowBackground() : lightWindowBackground();
    const QString backdrop = blend(tint, windowBackground, 0.18).value_or(windowBackground);
    if ((contrastRatio(tint, backdrop).value_or(0.0)) >= target)
        return tint;
    const Channels stop = darkMode ? Channels{255, 255, 255} : Channels{0, 0, 0};
    for (double step = 0.05; step <= 1.0 + 1e-9; step += 0.05) {
        const auto mix = [&](int v, int s) {
            return int(std::round(double(v) + (double(s) - double(v)) * step));
        };
        const QString candidate = hex(Channels{mix(channel->r, stop.r), mix(channel->g, stop.g),
                                               mix(channel->b, stop.b)});
        if ((contrastRatio(candidate, backdrop).value_or(0.0)) >= target)
            return candidate;
    }
    return darkMode ? QStringLiteral("#FFFFFF") : QStringLiteral("#000000");
}

} // namespace boh
