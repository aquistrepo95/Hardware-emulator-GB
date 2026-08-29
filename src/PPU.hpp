#ifndef PPU_HPP
#define PPU_HPP
#include <array>
#include "types.hpp"
#include "system_bus.hpp"

class PPU : public SystemBus {
    private:
    std::array<u8, 0xA0> oam{};
    std::array<u8, 0x2000> vram{};
    //std::array<u8, 0x80> io{};

    struct PPU_registers{ // ff40 - ff4b
        u8 lcdc = 0x91; // ff40: LCD control
        u8 stat = 0x85; // ff41: LCD status
        u8 scy  = 0x00; // ff42: scroll y
        u8 scx  = 0x00; // ff43: scroll x
        u8 ly   = 0x00; // ff44: scanline
        u8 lyc  = 0x00; // ff45: scanline compare 
        u8 oam_dma  = 0xff; // ff46: DMA address start
        u8 bgp  = 0xfc; // ff47: palette
        u8 obp0 = 0xff; // ff48: sprite palette 0
        u8 obp1 = 0xff; // ff49: sprite palette 1
        u8 wy   = 0x00; // ff4a: window y position
        u8 wx   = 0x00; // ff4b: window x position

        // tracking oam_dma operations
        bool is_oam_dma_active_ = false;
        u16 dma_source_address  = 0x0000;
        u16 oam_dma_offset      = 0;
    };

    // PPU object
    PPU_registers PPU_rg;

    public:
    // constructor
    PPU();

    // respond if address id within PPU range
    bool respond_to_operation(u16 address) const override;

    // read from PPU using system_bus
    virtual u8 read_from_IO(u16 address) override;

    // write to PPU using system_bus
    virtual void write_to_IO(u16 address, u8 value);

    // OAM DMA transfer
    void DMA_OAM_copy(u8 value);

    // PPU cycle
    void cycle_tick(u32 cycles);
};

#endif //PPU_HPP