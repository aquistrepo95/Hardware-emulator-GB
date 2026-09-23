#ifndef PPU_HPP
#define PPU_HPP
#include <array>
#include "types.hpp"
#include "system_bus.hpp"

class PPU : public SystemBus {
    private:
    std::array<u8, 0xA0> oam{};
    std::array<u8, 0x2000> vram{};

    struct PPU_registers{ // ff40 - ff4b
        u8 lcdc = 0x91; // ff40: LCD control
        u8 stat = 0x85; // ff41: LCD status
        u8 scy  = 0x00; // ff42: scroll y
        u8 scx  = 0x00; // ff43: scroll x
        u8 ly   = 0x00; // ff44: scanline tracker
        u8 lyc  = 0x00; // ff45: scanline compare 
        u8 oam_dma  = 0xff; // ff46: DMA address start
        u8 bgp  = 0xfc; // ff47: palette
        u8 obp0 = 0xff; // ff48: sprite palette 0
        u8 obp1 = 0xff; // ff49: sprite palette 1
        u8 wy   = 0x00; // ff4a: window y position
        u8 wx   = 0x00; // ff4b: window x position

        // tracking oam_dma operations
        bool is_oam_dma_active_  = false;
        u16 dma_source_address   = 0x0000;
        u16 oam_dma_offset       = 0; // count the current number of bytes copied during the OAM DMA transfer(total: 160 bytes)
        u8  delay_dma_oam        = 4;
        int oam_dma_bytes_copied = 0;
    };

    // PPU object
    PPU_registers PPU_rg;

    // System bus object
    std::reference_wrapper<SystemBus> system_bus;

    // track scanline count
    int current_scanline = 0;

    // window line counter
    int window_line_counter = 0;

    // frame buffer for rendering
    std::array<std::array<u32, 160>, 144> frame_front{};
    std::array<std::array<u32, 160>, 144> frame_back{};

    // frame ready flag
    bool frame_ready = false;   

    // OAM sprite struct 4 bytes + 1 byte for the OAM index
    struct OAM_sprite {
        u8 y_position;
        u8 x_position;
        u8 tile_index;
        u8 attributes;
        u8 oam_idx;
    }; 

    // OAM sprite vector for the current scanline
    std::vector<OAM_sprite> oam_sprites_current_scanline;

    // color palette for rendering
   const std::array<u32, 4> color_pallete = {
        0xffffffff, // White
        0xffaaaaaa, // Light Gray
        0xff555555, // Dark Gray
        0xff000000  // Black
    };

    public:
    // constructor
    PPU(SystemBus& bus);

    // respond if address id within PPU range
    bool respond_to_operation(u16 address) const override;

    // read from the MMU using system_bus
    virtual u8 read_from_IO(u16 address) override;

    // write to PPU using system_bus
    virtual void write_to_IO(u16 address, u8 value) override;

    // OAM DMA transfer
    void DMA_OAM_copy(u8 value);

    // PPU cycle
    void cycle_tick(u32 cycles);

    // update mode PPU mode based on the current scanline and cycle count
    void update_mode();

    // draw scanline: render the current scanline based on the PPU registers and memory
    void draw_scanline();

    // fetch the tile data for the current scanline based on the PPU registers and memory
    u8 fetch_tile_data_current_scanline(u8, int, int, bool);

    // draw sprites for the current scanline based on the OAM sprite vector
    void draw_sprites_current_scanline(const u8[]);

    // handle OAM sprites for the current scanline
    void handle_oam_sprites();

    // verify if OAM DMA transfer is active
    bool is_oam_dma_running() const;

    // swap the front and back frame buffers for rendering
    void swap_frame_buffers();

    // getters for frame buffer to display
    const std::array<std::array<u32, 160>, 144>& get_frame_buffer() const;

    // check if the frame is ready for rendering
    bool is_frame_ready() const;

    // reset the frame ready flag after rendering
    void reset_frame_ready();

};

#endif //PPU_HPP