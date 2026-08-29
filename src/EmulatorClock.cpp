#include "EmulatorClock.hpp" 


EmulatorClock :: EmulatorClock(Timer& t, PPU& p) : timer(t), ppu(p) {}


bool EmulatorClock :: cycle_tick(u32 cycles) {
    total_cycles += cycles;

    // timer cycles
    bool overflow_interupt = timer.cycle_tick(cycles);

    // PPU cycles
    ppu.cycle_tick(cycles);

    // add PPU and APU here

    return overflow_interupt;
}