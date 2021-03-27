//
// Created by Algot on 2021-03-11.
//

#pragma once

#include "MBC.h"
#include <cstdint>
#include <string>
#include <vector>

class Cartridge {
public:
    Cartridge();
    void reset();

    uint8_t read(uint16_t addr) const;
    void write(uint16_t addr, uint8_t data);

    void write_TEST(uint16_t addr, uint8_t data);
    bool load_rom(const std::string& filepath);

    // For cartridges with timer
    void update(uint8_t cycles);

private:
    enum CartridgeType {
        // When adding cartridge support add it to Cartridge::init_mbc()
        ROM_ONLY = 0x0,
        MBC1 = 0x1,
        MBC1_R = 0x2,
        MBC1_R_B = 0x3,
        MBC3_T_B = 0xf,
        MBC3_T_R_B = 0x10,
        MBC3 = 0x11,
        MBC_R = 0x12,
        MBC_R_B = 0x13,
    };

    enum ROM_Size {
        ROM_32KB = 0x0,
        ROM_64KB = 0x1,
        ROM_128KB = 0x2,
        ROM_256KB = 0x3,
        ROM_512KB = 0x4,
        ROM_1MB = 0x5,
        ROM_2MB = 0x6,
        ROM_4MB = 0x7,
        ROM_8MB = 0x8,
    };

    enum RAM_Size {
        RAM_NO_RAM = 0x0,
        RAM_8KB = 0x2,
        RAM_32KB = 0x3,
        RAM_128KB = 0x4,
        RAM_64KB = 0x5,
    };

    uint8_t cartridge_type;
    uint8_t rom_size;
    uint8_t ram_size;
    std::vector<uint8_t> rom;
    std::vector<uint8_t> ram;
    std::unique_ptr<MBC> mbc;
    std::string filepath;

    bool init_rom();
    bool init_ram();
    bool init_mbc();
};
