#ifndef TIMER_HPP
#define TIMER_HPP
#include <cstdint>
#include "EmulatorClock.hpp"
#include "system_bus.hpp"

class Timer : public SystemBus {
    private:
        // internal registers i.e 16-bit internal timer(div 1st 8-bits), TIMA, TMA, TAC
        u16 internal_timer_div = 0;
        u8 TIMA = 0;
        u8 TMA  = 0;
        u8 TAC  = 0;

        // frequency bits in the internal_timer_div register i.e this will set the frequency of TIMA
        // TAC bits 1-0: 00 = Bit 9, 01 = Bit 3, 10 = Bit 5, 11 = Bit 7 i.e TAC = internal_timer_div bit
        static constexpr u8 TAC_frequency_bit[4] = {9, 3, 5, 7};

        // check if TAC enable bit is on(i.e bit 3 is 1) and check if frequency bit in the internal timer is set
        bool verify_TAC_signal() const{
            // TAC is disabled
            if((TAC & 0x04) == 0) {return false;}

            // get lowest 2 bits of TAC and verify the whether the bit location is turned on in internal_timer_div
            u8 bit_shift    = TAC_frequency_bit[(TAC & 0x03)];
            u16 shifted_bit = 1 << bit_shift;
            return (internal_timer_div & shifted_bit) != 0;
        }

    public:
        Timer() = default;

        // cycle the timer based on the number of cycles passed from the CPU
        bool cycle_tick(u32 cycles);

        // SystemBus interface implementation
        bool respond_to_operation(u16 address) const override;
        u8 read_from_IO(u16 address) override;
        void write_to_IO(u16 address, u8 value) override;
        u8 read_from_bytes(u16 address) const override;
        void write_to_bytes(u16 address, u8 value) override;
        u8 read_from_bytes_dma(u16 value) const override;
};


#endif // TIMER_HPP