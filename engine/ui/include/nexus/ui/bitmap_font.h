#pragma once

#include "nexus/core/types.h"
#include "nexus/core/math.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace nexus::ui {

// ─────────────────────────────────────────────────────────────────────────────
// Glyph — a single character's metrics in the font atlas
// ─────────────────────────────────────────────────────────────────────────────

struct Glyph {
    u32   codepoint{0};
    float x0{0}, y0{0};    // Top-left UV in atlas (normalized 0-1)
    float x1{0}, y1{0};    // Bottom-right UV in atlas
    float width{0};         // Glyph pixel width
    float height{0};        // Glyph pixel height
    float x_offset{0};      // Horizontal offset when rendering
    float y_offset{0};      // Vertical offset when rendering
    float advance{0};       // Horizontal advance to next character
};

// ─────────────────────────────────────────────────────────────────────────────
// BitmapFont — a font atlas with glyph lookup for text rendering
// ─────────────────────────────────────────────────────────────────────────────

class BitmapFont {
public:
    BitmapFont() = default;

    /// Generate a font atlas from a TrueType font file using stb_truetype-style rasterization.
    /// For v1.0, we provide a built-in monospace bitmap font (ASCII 32-126).
    bool load_builtin(float font_size = 16.0f);

    /// Load from a BMFont-format .fnt file (text format).
    bool load_bmfont(const std::string& fnt_path);

    /// Get glyph info for a codepoint.
    const Glyph* get_glyph(u32 codepoint) const;

    /// Measure text dimensions (width, height).
    Vec2 measure_text(const std::string& text) const;

    /// Get kerning between two codepoints (0 if not defined).
    float get_kerning(u32 left, u32 right) const;

    /// Atlas texture data (single-channel grayscale).
    const u8* atlas_data() const { return atlas_.data(); }
    u32 atlas_width() const { return atlas_width_; }
    u32 atlas_height() const { return atlas_height_; }

    /// Font metrics.
    float line_height() const { return line_height_; }
    float base_line() const { return base_line_; }
    float font_size() const { return font_size_; }

    /// Is the font loaded?
    bool is_valid() const { return !atlas_.empty() && !glyphs_.empty(); }

    /// Generate vertex data for rendering text.
    /// Returns a flat array of quads: [x, y, u, v] * 4 vertices per glyph.
    struct TextVertex {
        float x, y;
        float u, v;
    };
    std::vector<TextVertex> generate_vertices(const std::string& text, Vec2 position) const;

private:
    std::unordered_map<u32, Glyph> glyphs_;
    std::vector<u8> atlas_;
    u32 atlas_width_{0};
    u32 atlas_height_{0};
    float font_size_{16.0f};
    float line_height_{20.0f};
    float base_line_{16.0f};
};

} // namespace nexus::ui
