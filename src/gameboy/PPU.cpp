//
// Created by riddarvid on 2/23/21.
//

#include "PPU.h"
#include "MMU.h"
#include "Definitions.h"
#include <memory>
#include <cassert>
#include <iostream>

PPU::PPU(std::shared_ptr<MMU> memory) {
    this->memory = memory;
    this->reset();
}

void PPU::reset() {
    this->SCY = 0;
    this->SCX = 0;
    this->LY = 0;
    this->LYC = 0;
    this->DMA = 0;
    this->WY = 0;
    this->WX = 0;
    this->BGP = 0;
    this->OBP0 = 0;
    this->OBP1 = 0;
    this->STAT = 0;
    this->LCDC = 0;

    this->accumulatedCycles = 0;
    this->modeFlag = OAM_SEARCH;
    this->readyToDraw = false;
    this->anyStatConditionLastUpdate = false;

    frameBuffer.fill(0);
    bgWindowColorIndexesThisLine.fill(0);
    spritesNextScanLine.clear();
}

uint8_t PPU::read(uint16_t addr) const {
    switch (addr) {
        case LCDC_ADDRESS:
            return this->LCDC;
        case STAT_ADDRESS:
            return this->STAT;
        case SCY_ADDRESS:
            return this->SCY;
        case SCX_ADDRESS:
            return this->SCX;
        case LY_ADDRESS:
            return this->LY;
        case LYC_ADDRESS:
            return this->LYC;
        case DMA_ADDRESS:
            return this->DMA;
        case BGP_ADDRESS:
            return this->BGP;
        case OBP0_ADDRESS:
            return this->OBP0;
        case OBP1_ADDRESS:
            return this->OBP1;
        case WY_ADDRESS:
            return this->WY;
        case WX_ADDRESS:
            return this->WX;
        default:
            return 0;
    }
}

void PPU::write(uint16_t addr, uint8_t data) {
    switch (addr) {
        case LCDC_ADDRESS:
            this->LCDC = data;
            break;
        case STAT_ADDRESS:
            this->STAT = data;
            break;
        case SCY_ADDRESS:
            this->SCY = data;
            break;
        case SCX_ADDRESS:
            this->SCX = data;
            break;
        case LY_ADDRESS:
            break;
        case LYC_ADDRESS:
            this->LYC = data;
            break;
        case DMA_ADDRESS:
            this->dma_transfer(data);
            break;
        case BGP_ADDRESS:
            this->BGP = data;
            break;
        case OBP0_ADDRESS:
            this->OBP0 = data;
            break;
        case OBP1_ADDRESS:
            this->OBP1 = data;
            break;
        case WY_ADDRESS:
            this->WY = data;
            break;
        case WX_ADDRESS:
            this->WX = data;
            break;
        default:
            std::cout << "Tried to write data: " << (int)data << " to address: " << (int)addr << std::endl;
    }
}



void PPU::update(uint16_t cpuCycles) {
    accumulatedCycles += cpuCycles;
    switch(modeFlag) { //depending on which mode the PPU is in
        case HBLANK:
            if (accumulatedCycles >= HBLANK_THRESHOLD) {
                accumulatedCycles -= HBLANK_THRESHOLD;
                LY++;

                if (LY < LCD_HEIGHT) { //If we still have lines to draw
                    modeFlag = OAM_SEARCH;
                } else { //If we have drawn the entire screen
                    modeFlag = VBLANK;
                    vBlankInterrupt();
                }
            }
            break;

        case VBLANK:
            if (accumulatedCycles >= VBLANK_LINE_THRESHOLD) {
                accumulatedCycles -= VBLANK_LINE_THRESHOLD;
                LY++;

                if (LY == LCD_HEIGHT + 10) { //If the VBLANK should end: Reset LY and clear frame buffer
                    LY = 0;
                    modeFlag = OAM_SEARCH;
                    // If leaving VBLANK we are ready to draw the screen
                    readyToDraw = true;
                }
            }
            break;

        case OAM_SEARCH:
            if (LY == 0) {
                frameBuffer.fill(0);
            }
            if (accumulatedCycles >= OAM_SEARCH_THRESHOLD) {
                accumulatedCycles -= OAM_SEARCH_THRESHOLD;
                coincidenceFlag = (LYC == LY); // Set coincidence flag before drawing the scanline.
                loadSpritesNextScanLine();
                modeFlag = SCANLINE_DRAW;
            }
            break;

        case SCANLINE_DRAW:
            if (accumulatedCycles >= SCANLINE_DRAW_THRESHOLD) {
                accumulatedCycles -= SCANLINE_DRAW_THRESHOLD;
                processNextLine();
                modeFlag = HBLANK;
            }
            break;
    }

    bool meetsStatConditionsCurrent = meetsStatConditions();
    if (!anyStatConditionLastUpdate) {
        if (meetsStatConditionsCurrent) {
            statInterrupt();
        }
    }
    anyStatConditionLastUpdate = meetsStatConditionsCurrent;
}

const std::array<uint8_t, LCD_WIDTH * LCD_HEIGHT>* PPU::getFrameBuffer() const {
    return &frameBuffer;
}

bool PPU::isReadyToDraw() const {
    return this->readyToDraw;
}

void PPU::confirmDraw() {
    this->readyToDraw = false;
}

void PPU::processNextLine() {
    if (lcdDisplayEnable) {
        if (bgWindowDisplayEnable) {
            drawBackgroundScanLine();
            if (windowDisplayEnable) {
                drawWindowScanLine();
            }
        }
        if (objectDisplayEnable) {
            drawObjectScanLine();
        }
    }
}

void PPU::drawBackgroundScanLine() {
    uint16_t bgMapStartAddress;
    if (bgTileMapSelect) {
        bgMapStartAddress = BG_WINDOW_MAP1;
    } else {
        bgMapStartAddress = BG_WINDOW_MAP0;
    }

    //For each pixel in the current row, find the correct tile ID, then the
    //correct pixel in that tile
    for (uint8_t x = 0; x < LCD_WIDTH; ++x) {
        uint8_t absolutePixelX = (SCX + x) % BACKGROUND_WIDTH;
        uint8_t absolutePixelY = (SCY + LY) % BACKGROUND_HEIGHT;
        uint8_t tileID = getTileID(bgMapStartAddress, absolutePixelX, absolutePixelY);
        uint8_t colorIndex = getTilePixelColorIndex(bgWindowTileSetSelect, tileID, absolutePixelX % 8, absolutePixelY % 8);
        uint8_t pixel = getColor(BGP, colorIndex);
        frameBuffer[LY * LCD_WIDTH + x] = pixel;
        bgWindowColorIndexesThisLine[x] = pixel;
    }
}

void PPU::drawWindowScanLine() {
    if (WY > LY) { //Window hasn't begun yet
        return;
    }
    uint16_t windowMapStartAddress;
    if (windowTileMapSelect) {
        windowMapStartAddress = BG_WINDOW_MAP1;
    } else {
        windowMapStartAddress = BG_WINDOW_MAP0;
    }
    int startX = WX - 7;
    if (startX < 0) {
        startX = 0;
    }
    for (int x = startX; x < LCD_WIDTH; ++x) {
        uint8_t absolutePixelX = (x - startX); //TODO check hardware bug when 0 < WX <= 6 and WX = 166 What is the intended behaviour?
        uint8_t absolutePixelY = (LY - WY);
        uint8_t tileID = getTileID(windowMapStartAddress, absolutePixelX, absolutePixelY);
        uint8_t colorIndex = getTilePixelColorIndex(bgWindowTileSetSelect, tileID, absolutePixelX, absolutePixelY);
        uint8_t pixel = getColor(BGP, colorIndex);
        frameBuffer[LY * LCD_WIDTH + x] = pixel;
        bgWindowColorIndexesThisLine[x] = pixel;
    }
}

void PPU::drawObjectScanLine() {
    for (int x = 0; x < LCD_WIDTH; ++x) {
        std::shared_ptr<Sprite> highestPriority = getHighestPrioritySprite(x);
        if (highestPriority != nullptr) {
            highestPriority->print();
            uint8_t colorIndex = getSpritePixelColorIndex(highestPriority, x, LY);
            if (colorIndex != 0) {
                uint8_t pixel;
                if (highestPriority->getPaletteNumber()) {
                    pixel = getColor(OBP1, colorIndex);
                } else {
                    pixel = getColor(OBP0, colorIndex);
                }
                frameBuffer[LY * LCD_WIDTH + x] = pixel;
            }
        }
    }
}

void PPU::loadSpritesNextScanLine() {
    spritesNextScanLine.clear();
    for (int i = 0; i < 40 && spritesNextScanLine.size() < 10; ++i) {
        std::shared_ptr<Sprite> sprite = loadSprite(i);
        if (sprite->coversLine(LY, objectSize)) {
            spritesNextScanLine.push_back(sprite);
        }
    }
}

std::shared_ptr<Sprite> PPU::loadSprite(int index) {
    uint16_t startAddress = OAM_START + index * 4;
    uint8_t yByte = memory->read(startAddress);
    uint8_t xByte = memory->read(startAddress + 1);
    uint8_t tileIndex = memory->read(startAddress + 2);
    uint8_t flags = memory->read(startAddress + 3);
    return std::make_shared<Sprite>(yByte, xByte, tileIndex, flags);
}

/**
 *
 * @param bgMapStart
 * @param pixelAbsoluteX
 * @param pixelAbsoluteY
 * @return The ID of the tile containing the target pixel
 */
uint8_t PPU::getTileID(uint16_t bgMapStart, uint8_t pixelAbsoluteX, uint8_t pixelAbsoluteY) {
    uint16_t tileAbsoluteX = pixelAbsoluteX / 8; //Divide by 8 since the width and height of a tile is 8
    uint16_t tileAbsoluteY = pixelAbsoluteY / 8;
    uint16_t offset = tileAbsoluteY * 32 + tileAbsoluteX; //Convert from 2D matrix to array index
    return memory->read(bgMapStart + offset);
}

uint8_t PPU::getTilePixelColorIndex(uint8_t tileSet, uint8_t id, uint8_t x, uint8_t y) { //TODO test
    uint16_t startAddress;
    uint16_t address;

    if (tileSet) { //Find the address of the tile with id tileID, depending on addressing mode
        startAddress = BG_WINDOW_TILE_DATA1;
        address = id * 16 + startAddress;
    } else {
        startAddress = BG_WINDOW_TILE_DATA0;
        auto signedID = (int8_t)id;
        address = signedID * 16 + startAddress;
    }

    x = 7 - x;

    uint8_t lowByte = memory->read(address + y * 2);
    uint8_t highByte = memory->read(address + y * 2 + 1);

    uint8_t lowBit = (lowByte >> x) & 1;
    uint8_t highBit = (highByte >> x) & 1;

    uint8_t pixelColor = (highBit << 1) | lowBit;
    return pixelColor;
}

std::shared_ptr<Sprite> PPU::getHighestPrioritySprite(int lcdX) {
    std::shared_ptr<Sprite> highestPriority = nullptr;
    for (const auto &current : spritesNextScanLine) {
        if (current->containsX(lcdX)) {
            if (current->hasHigherPriorityThan(highestPriority)) {
                highestPriority = current;
            }
        }
    }
    return highestPriority;
}

uint8_t PPU::getSpritePixelColorIndex(const std::shared_ptr<Sprite>& sprite, uint8_t lcdX, uint8_t lcdY) {
    if ((sprite->backgroundOverSprite()) && (bgWindowColorIndexesThisLine[lcdX] != 0)) { //TODO should this be color index 0 or color 0?
        return 0;
    }
    uint8_t tileID = sprite->getTileID(lcdY);
    uint8_t tileX = sprite->getTileX(lcdX);
    uint8_t tileY = sprite->getTileY(lcdY);
    return getTilePixelColorIndex(1, tileID, tileX, tileY); //Sprites always use tile set 1
}

uint8_t PPU::getColor(uint8_t palette, uint8_t colorIndex) {
    uint8_t bitmask = 0b11;
    return ((palette >> 2 * colorIndex) & bitmask);
}

bool PPU::meetsStatConditions() const {
    auto lycInterrupt = (lycEqualsLyInterruptEnable && coincidenceFlag);
    auto oamInterrupt = (oamInterruptEnable && (modeFlag == OAM_SEARCH));
    auto vBlankInterrupt = (vBlankInterruptEnable && (modeFlag == VBLANK));
    auto hBlankInterrupt = (hBlankInterruptEnable && (modeFlag == HBLANK));

    return (lycInterrupt || oamInterrupt || vBlankInterrupt || hBlankInterrupt);
}

void PPU::vBlankInterrupt() {
    memory->raise_interrupt_flag(V_BLANK_IF_BIT);
}

void PPU::statInterrupt() {
    memory->raise_interrupt_flag(STAT_IF_BIT);
}

void PPU::dma_transfer(uint8_t data) {
    if (0x00 <= data && data <= 0xdf) {
        uint16_t start_addr = (data << 8);
        for (uint8_t i = 0; i <= 0x9f; i++) {
            this->memory->write(OAM_START + i, this->read(start_addr+i));
        }
    } else {
        std::cout << "Tried to use DMA transfer with invalid input: " << data << std::endl;
    }
}
