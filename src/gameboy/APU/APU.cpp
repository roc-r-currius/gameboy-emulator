//
// Created by riddarvid on 3/24/21.
//

#include "APU.h"

APU::APU(std::shared_ptr<MMU> memory) {
    this->accumulated_cycles = 0;
    this->memory = memory;
    this->reset();
}

uint8_t APU::read(uint16_t address) const {
    if (IO_WAVEFORM_RAM_START <= address && address <= IO_WAVEFORM_RAM_END) {
        return this->wavePatternRAM[address - IO_WAVEFORM_RAM_START];
    }
    switch (address) {
        case NR10_ADDRESS:
            return this->NR10;
        case NR11_ADDRESS:
            return this->NR11;
        case NR12_ADDRESS:
            return this->NR12;
        case NR13_ADDRESS:
            return this->NR13;
        case NR14_ADDRESS:
            return this->NR14;
        case NR50_ADDRESS:
            return this->NR50;
        case NR51_ADDRESS:
            return this->NR51;
        case NR52_ADDRESS:
            return this->NR52;
        default:
            std::cout << "Tried to read from unimplemented audio address " << address << std::endl;
            return 0;
    }
}

void APU::write(uint16_t address, uint8_t data) {
    if (IO_WAVEFORM_RAM_START <= address && address <= IO_WAVEFORM_RAM_END) {
        this->wavePatternRAM[address - IO_WAVEFORM_RAM_START] = data;
    }
    switch (address) {
        case NR10_ADDRESS:
            this->NR10 = data;
            return;
        case NR11_ADDRESS:
            this->NR11 = data;
            return;
        case NR12_ADDRESS:
            this->NR12 = data;
            return;
        case NR13_ADDRESS:
            this->NR13 = data;
            return;
        case NR14_ADDRESS:
            this->NR14 = data;
            // If trigger bit is set
            if(this->NR14 & 0x80) {
                trigger_event(0);
            }
            return;
        case NR50_ADDRESS:
            this->NR50 = data;
            return;
        case NR51_ADDRESS:
            this->NR51 = data;
            return;
        case NR52_ADDRESS:
            this->NR52 = data;
            return;
        default:
            std::cout << "Tried to write to unimplemented audio address " << address << " data " << (int)data << std::endl;
    }
}

void APU::reset() {
    NR10 = 0;
    NR11 = 0;
    NR12 = 0;
    NR13 = 0;
    NR14 = 0;

    NR21 = 0;
    NR22 = 0;
    NR23 = 0;
    NR24 = 0;

    NR30 = 0;
    NR31 = 0;
    NR32 = 0;
    NR33 = 0;
    NR34 = 0;

    wavePatternRAM.fill(0);

    NR41 = 0;
    NR42 = 0;
    NR43 = 0;
    NR44 = 0;

    NR50 = 0;
    NR51 = 0;
    NR52 = 0;
}

void APU::trigger_event(uint8_t source) {
    //TODO: Complete functionality
    /**
    - Done - Channel is enabled (see length counter).
    - Done - If length counter is zero, it is set to 64 (256 for wave channel).
    - Done-ish (since frequency timer does not change in our program, it does not need to be reset)
        - Frequency timer is reloaded with period.
    - Pass - Volume envelope timer is reloaded with period.
    - Pass - Channel volume is reloaded from NRx2.
    - Pass - Noise channel's LFSR bits are all set to 1.
    - Pass - Wave channel's position is set to 0 but sample buffer is NOT refilled.
    - Pass - Square 1's sweep does several things (see frequency sweep).
     */

    switch(source) {
        case 0:
            //If length counter is zero, it is set to 64
            if(!(this->NR11 & 0x3F)) {
                this->NR11 |= 64;
            }
    }
}

void APU::length_step() {
    //TODO: Update length
}

void APU::vol_envelope_step() {
    //TODO: Update volume length
}

void APU::sweep_step() {
    //TODO: Update with sweep
}

void APU::update(uint16_t cpuCycles) {
    this->accumulated_cycles += cpuCycles;
    if(this->accumulated_cycles < CLOCK_CYCLE_THRESHOLD) {
        return;
    }
    this->accumulated_cycles -= CLOCK_CYCLE_THRESHOLD;
    this->state++;
    state %= 8;

    if(state % 2 == 0) {
        length_step();
    }
    if(state == 7) {
        vol_envelope_step();
    }
    if(state % 4 == 2) {
        sweep_step();
    }
}





