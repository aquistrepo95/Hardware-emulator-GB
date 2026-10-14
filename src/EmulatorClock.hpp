#ifndef EMULATORCLOCK_HPP
#define EMULATORCLOCK_HPP
#include "types.hpp"
#include "Timer.hpp"
#include "PPU.hpp"
#include "APU.hpp"


class EmulatorClock {
    private:
        u64 total_cycles = 0;
        Timer& timer;
        PPU& ppu;
        APU& apu; 

    public:
        EmulatorClock(Timer& timer, PPU& ppu, APU& apu); // add ppu and apu shortly
        bool cycle_tick(u32 cycles);
};



#endif // EMULATORCLOCK_HPP