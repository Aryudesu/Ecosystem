#include "Ecosystem/SpriteAsset.h"

#include <mygame/graphics/ImageManager.h>
#include <mygame/io/FileUtil.h>

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace ecosystem::graphics {
namespace {

constexpr int SpriteSheetId = 100;
constexpr int CellSize = 24;
constexpr int Columns = 4;
constexpr int Rows = 3;
constexpr int SheetWidth = CellSize * Columns;
constexpr int SheetHeight = CellSize * Rows;
constexpr const char* SpriteSheetPath = "assets/img.bmp";

using Pattern = std::array<std::string_view, CellSize>;

// Sprite sheet layout (4 x 3):
// row 0: Grass, Seed, -, -
// row 1: Herbivore Walk1, Walk2, Run1, Run2
// row 2: Carnivore Walk1, Walk2, Run1, Run2

constexpr Pattern GrassPattern = {
    "........................",
    "........................",
    "........................",
    "........................",
    "........................",
    "........................",
    "........................",
    "........................",
    "...........lll..........",
    "...........lll..........",
    "...........lll...g......",
    "...........lll.ggg......",
    ".......ggg.lll.ggg......",
    ".......ggg.lll.ggg......",
    "......dggglll.gggg.lll..",
    "....dddggglll.ggg..lll..",
    "....ddddgglll.ggg.lll...",
    ".....dddgglll.ggg.lll...",
    ".....ddddglllggggllll...",
    ".....ddddglllgggglll....",
    "....ddddddddddddddddd...",
    "....ddddddddddddddddd...",
    "........................",
    "........................",
};

constexpr Pattern SeedPattern = {
    "........................",
    "........................",
    "........................",
    "........................",
    "........................",
    "........................",
    "........................",
    "............g...........",
    "...........gg...........",
    "..........gg............",
    "...........g............",
    "...........SS...........",
    "..........SSSS..........",
    ".........SSSSSS.........",
    ".........SSSSSS.........",
    ".........sSSSSs.........",
    "..........ssss..........",
    "...........ss...........",
    "........................",
    "........................",
    "........................",
    "........................",
    "........................",
    "........................",
};

constexpr Pattern HerbivorePattern = {
    "........................",
    "........................",
    "................ww......",
    "................pw.ww...",
    "................ww.pw...",
    "................ww.ww...",
    "................ww.ww...",
    "...............wwwwww...",
    "...............wwwwwww..",
    "...............wwwwkww..",
    "........bbbbbbbwwwwwww..",
    ".......Bbbbbbbbwwwwwwp..",
    "...wwwwBBBBBBBBwwwwwww..",
    "...wwwwCCCBBBBBwwwwww...",
    "...wwwwCCCBBBBBwwwww....",
    "...wwwbCCCBBBBBwwwww....",
    ".....CCCCCBBBBBwwwww....",
    ".....CCCCCBBBBBBBB......",
    ".......wwwwwbBwwwww.....",
    ".......wwwww..wwwww.....",
    ".......wwwww..wwwww.....",
    "........................",
    "........................",
    "........................",
};

constexpr Pattern CarnivorePattern = {
    "........................",
    "........................",
    "........................",
    "................r...r...",
    "................r...r...",
    "................rr.rr...",
    "...............rrr.rrr..",
    "...............rrrrrrr..",
    "..........RRRRRRRRRkRr..",
    "........rrrrrrrRRRRRRR..",
    ".......RrrrrrrrRRRwwwww.",
    "......oRRRRRRRRRRRwwwwk.",
    ".....ooRRRRRRRRRRRwwwww.",
    "....oooRRRRRRRRRRwwwwww.",
    "...ooooRRRRRRRRRRwww....",
    "...ooooRRRRRRRRRRwww....",
    "...wwwoRRrrrRRRrrwww....",
    "...wwwoRRrrrRRRrrwww....",
    "...www...rrr...rrr......",
    ".........rrr...rrr......",
    ".........krr...rrk......",
    "........................",
    "........................",
    "........................",
};

struct Rgb {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
};

constexpr Rgb ColorFor(char pixel) {
    switch (pixel) {
    case 'd': return {32, 115, 50};
    case 'g': return {68, 185, 72};
    case 'l': return {125, 225, 95};
    case 'b': return {45, 85, 145};
    case 'B': return {105, 165, 235};
    case 'C': return {165, 205, 250};
    case 'w': return {245, 245, 235};
    case 'p': return {235, 120, 175};
    case 'k': return {20, 20, 20};
    case 'r': return {125, 45, 35};
    case 'R': return {200, 75, 55};
    case 'o': return {235, 130, 70};
    case 's': return {120, 72, 38};
    case 'S': return {190, 128, 68};
    default:  return {255, 0, 255};
    }
}

Rgb PatternPixel(const Pattern& pattern, int x, int y) {
    if (x < 0 || x >= CellSize || y < 0 || y >= CellSize) {
        return ColorFor('.');
    }
    return ColorFor(pattern[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)]);
}

enum class AnimalPose {
    Walk1,
    Walk2,
    Run1,
    Run2,
};

Rgb AnimalPosePixel(const Pattern& pattern, int x, int y, AnimalPose pose) {
    int bodyOffsetX = 0;
    int bodyOffsetY = 0;
    int legStride = 0;

    switch (pose) {
    case AnimalPose::Walk1:
        legStride = -1;
        break;
    case AnimalPose::Walk2:
        bodyOffsetY = 1;
        legStride = 1;
        break;
    case AnimalPose::Run1:
        bodyOffsetX = -1;
        bodyOffsetY = -1;
        legStride = -2;
        break;
    case AnimalPose::Run2:
        bodyOffsetX = 1;
        bodyOffsetY = 1;
        legStride = 2;
        break;
    }

    int sourceX = x - bodyOffsetX;
    int sourceY = y - bodyOffsetY;

    // Separate the two lower halves in opposite directions so the legs visibly stride.
    // This deliberately exaggerates the motion at 24x24 resolution.
    if (y >= 18) {
        if (x < CellSize / 2) {
            sourceX += legStride;
        } else {
            sourceX -= legStride;
        }
    }

    return PatternPixel(pattern, sourceX, sourceY);
}

Rgb SheetPixel(int x, int y) {
    const int column = x / CellSize;
    const int row = y / CellSize;
    const int localX = x % CellSize;
    const int localY = y % CellSize;
    const int cell = row * Columns + column;

    switch (cell) {
    case 0: return PatternPixel(GrassPattern, localX, localY);
    case 1: return PatternPixel(SeedPattern, localX, localY);

    case 4: return AnimalPosePixel(HerbivorePattern, localX, localY, AnimalPose::Walk1);
    case 5: return AnimalPosePixel(HerbivorePattern, localX, localY, AnimalPose::Walk2);
    case 6: return AnimalPosePixel(HerbivorePattern, localX, localY, AnimalPose::Run1);
    case 7: return AnimalPosePixel(HerbivorePattern, localX, localY, AnimalPose::Run2);

    case 8: return AnimalPosePixel(CarnivorePattern, localX, localY, AnimalPose::Walk1);
    case 9: return AnimalPosePixel(CarnivorePattern, localX, localY, AnimalPose::Walk2);
    case 10: return AnimalPosePixel(CarnivorePattern, localX, localY, AnimalPose::Run1);
    case 11: return AnimalPosePixel(CarnivorePattern, localX, localY, AnimalPose::Run2);

    default: return ColorFor('.');
    }
}

void Put16(std::vector<unsigned char>& out, std::size_t offset, std::uint16_t value) {
    out[offset] = static_cast<unsigned char>(value & 0xffu);
    out[offset + 1] = static_cast<unsigned char>((value >> 8u) & 0xffu);
}

void Put32(std::vector<unsigned char>& out, std::size_t offset, std::uint32_t value) {
    out[offset] = static_cast<unsigned char>(value & 0xffu);
    out[offset + 1] = static_cast<unsigned char>((value >> 8u) & 0xffu);
    out[offset + 2] = static_cast<unsigned char>((value >> 16u) & 0xffu);
    out[offset + 3] = static_cast<unsigned char>((value >> 24u) & 0xffu);
}

bool WriteSpriteSheetBmp() {
    constexpr std::size_t HeaderSize = 54;
    constexpr int RowStride = ((SheetWidth * 3 + 3) / 4) * 4;
    constexpr std::size_t PixelBytes = static_cast<std::size_t>(RowStride) * SheetHeight;
    constexpr std::size_t FileSize = HeaderSize + PixelBytes;

    std::vector<unsigned char> bytes(FileSize, 0);
    bytes[0] = 'B';
    bytes[1] = 'M';
    Put32(bytes, 2, static_cast<std::uint32_t>(FileSize));
    Put32(bytes, 10, static_cast<std::uint32_t>(HeaderSize));
    Put32(bytes, 14, 40);
    Put32(bytes, 18, static_cast<std::uint32_t>(SheetWidth));
    Put32(bytes, 22, static_cast<std::uint32_t>(SheetHeight));
    Put16(bytes, 26, 1);
    Put16(bytes, 28, 24);
    Put32(bytes, 34, static_cast<std::uint32_t>(PixelBytes));

    for (int y = 0; y < SheetHeight; ++y) {
        const int bmpY = SheetHeight - 1 - y;
        const std::size_t rowStart = HeaderSize + static_cast<std::size_t>(bmpY * RowStride);
        for (int x = 0; x < SheetWidth; ++x) {
            const Rgb color = SheetPixel(x, y);
            const std::size_t index = rowStart + static_cast<std::size_t>(x * 3);
            bytes[index] = color.b;
            bytes[index + 1] = color.g;
            bytes[index + 2] = color.r;
        }
    }

    return mygame::file::WriteAllBytes(SpriteSheetPath, bytes);
}

bool ready = false;

} // namespace

bool InitializeSprites() {
    auto& images = mygame::ImageManager::GetInstance();
    images.SetTransparentColor(255, 0, 255);

    if (!WriteSpriteSheetBmp()) {
        ready = false;
        return false;
    }

    ready = images.LoadDivided(
        SpriteSheetId,
        SpriteSheetPath,
        Columns,
        Rows,
        CellSize,
        CellSize);

    return ready;
}

void ShutdownSprites() {
    mygame::ImageManager::GetInstance().Destroy(SpriteSheetId);
    ready = false;
}

bool DrawSprite(SpriteIndex sprite, int centerX, int centerY, bool flipHorizontal) {
    if (!ready) return false;

    auto& images = mygame::ImageManager::GetInstance();
    const std::size_t index = static_cast<std::size_t>(sprite);
    if (!flipHorizontal) {
        return images.Draw(
            SpriteSheetId,
            centerX - CellSize / 2,
            centerY - CellSize / 2,
            true,
            index);
    }

    const int handle = images.Handle(SpriteSheetId, index);
    if (handle == -1) return false;
    return DrawTurnGraph(
        centerX - CellSize / 2,
        centerY - CellSize / 2,
        handle,
        TRUE) == 0;
}

bool IsSpriteReady() {
    return ready;
}

} // namespace ecosystem::graphics
