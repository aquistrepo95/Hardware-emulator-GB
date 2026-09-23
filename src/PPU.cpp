#include <iostream>
#include "PPU.hpp"

PPU :: PPU(SystemBus& bus) : system_bus(bus) {
}

bool PPU :: respond_to_operation(u16 address) const {
    return (address >= 0x8000 && address <= 0x9fff) ||
           (address >= 0xfe00 && address <= 0xfe9f) ||
           (address >= 0xff40 && address <= 0xff4b);
}

u8 PPU :: read_from_IO(u16 address) {
    // vram 
    if(address >= 0x8000 && address <= 0x9fff) {
        return vram[address - 0x8000];
    }

    // OAM
    if(address >= 0xfe00 && address <= 0xfe9f) {
        return oam[address - 0xfe00];
    }

    // IO registers
    switch(address) {
        case 0xff40: return PPU_rg.lcdc;
        case 0xff41: return PPU_rg.stat | 0x80; 
        case 0xff42: return PPU_rg.scy;
        case 0xff43: return PPU_rg.scx;
        case 0xff44: return PPU_rg.ly;   
        case 0xff45: return PPU_rg.lyc;
        case 0xff46: return PPU_rg.oam_dma;   
        case 0xff47: return PPU_rg.bgp;
        case 0xff48: return PPU_rg.obp0;
        case 0xff49: return PPU_rg.obp1;
        case 0xff4a: return PPU_rg.wy;
        case 0xff4b: return PPU_rg.wx;
        default: return 0xff;
    }
}

void PPU :: write_to_IO(u16 address, u8 value) {
    // vram
    if(address >= 0x8000 && address <= 0x9fff) {
        vram[address - 0x8000] = value;
        return;
    }

    // OAM 
    if(address >= 0xfe00 && address <= 0xfe9f) {
        oam[address - 0xfe00] = value;
        return;
    }

    // IO registers
    switch(address) {
        case 0xff40: PPU_rg.lcdc = value; break;
        case 0xff41: PPU_rg.stat = (value & 0xf8) | (PPU_rg.stat & 0x07); break; // 
        case 0xff42: PPU_rg.scy  = value; break;
        case 0xff43: PPU_rg.scx  = value; break;
        case 0xff44: break;
        case 0xff45: PPU_rg.lyc  = value; break;
        case 0xff46: PPU_rg.oam_dma = value; DMA_OAM_copy(value); break; // starts DMA transfer from ROM/RAM to OAM
        case 0xff47: PPU_rg.bgp  = value; break;
        case 0xff48: PPU_rg.obp0 = value; break;
        case 0xff49: PPU_rg.obp1 = value; break;
        case 0xff4a: PPU_rg.wy   = value; break;
        case 0xff4b: PPU_rg.wx   = value; break;
        default: break;
    }
}

// DMA OAM transfer
void PPU :: DMA_OAM_copy(u8 value) {
    PPU_rg.is_oam_dma_active_ = true; // set the flag to indicate that the OAM DMA transfer is active
    PPU_rg.dma_source_address = static_cast<u16>(value) << 8; // set the source address for the DMA transfer (shift bits to represent value * 0x100)
    PPU_rg.oam_dma_offset = 0; // reset the offset for the OAM DMA transfer
    PPU_rg.delay_dma_oam = 4; // set the delay for the OAM DMA transfer (4 T-cycles)
}

// PPU cycle
void PPU :: cycle_tick(u32 cycles) {
    if(PPU_rg.is_oam_dma_active_) {
        u32 current_cycle_in_progress = cycles;

        // initial delay of 4 cycles before starting the OAM DMA transfer
        if(PPU_rg.delay_dma_oam > 0) {
            if(current_cycle_in_progress >= PPU_rg.delay_dma_oam) {
                current_cycle_in_progress -= PPU_rg.delay_dma_oam;
                PPU_rg.delay_dma_oam = 0;
            } else {
                PPU_rg.delay_dma_oam -= current_cycle_in_progress;
                return; // comeback here
                // current_cycle_in_progress = 0;
            }
        }

        // OAM DMA transfer
        while(current_cycle_in_progress >= 4 && PPU_rg.is_oam_dma_active_) {
            // copying 160 bytes from source address to OAM
            if(PPU_rg.oam_dma_bytes_copied < 160) {
                u16 source_address = PPU_rg.dma_source_address + PPU_rg.oam_dma_bytes_copied;
                u8 data = system_bus.get().read_from_bytes(source_address);
                oam[PPU_rg.oam_dma_bytes_copied] = data;
                PPU_rg.oam_dma_bytes_copied++;
                current_cycle_in_progress -= 4;
            }
            else {
                PPU_rg.is_oam_dma_active_ = false; 
                PPU_rg.oam_dma_bytes_copied = 0; 
            }
        }
    }

    // Update scanline counter
    current_scanline += cycles;

    // check if scanline counter has reached 456 cycles (one scanline)
    if(current_scanline >= 456) {
        // reset scanline counter for the next scanline by subtracting 456 cycles
        current_scanline -= 456;

        // increment LY register(count for the number of scanlines rendered in the current frame)
        PPU_rg.ly++;

        // reset LY register if it exceeds 153 
        if(PPU_rg.ly > 153) {
            PPU_rg.ly = 0; // set LY register to 0 for the next frame

            // swap the front and back frame buffers for rendering
            swap_frame_buffers();

            // reset window line counter for the next frame
            window_line_counter = 0;
        }
    }

    // update PPU mode based on the current scanline and cycle count
    update_mode();
}

// update PPU mode based on the current scanline and cycle count
void PPU :: update_mode() {
    u8 current_mode = PPU_rg.stat & 0x03; // get the current mode from the STAT register
    u8 new_mode = 0x00; // initialize new mode variable
    bool stat_interrupt_triggered = false; // flag to track if a STAT interrupt has been triggered

    // set the new mode based on the current scanline and cycle count
    if(PPU_rg.ly >= 144) {
        new_mode = 1; // VBlank mode
    } 
    else if(current_scanline < 80) {
        new_mode = 2; // OAM search mode
    } 
    else if(current_scanline < 252) {
        new_mode = 3; // Pixel transfer mode
    } 
    else {
        new_mode = 0; // HBlank mode
    }

    // check if the mode has changed, update ppu mode bits(bits 0 and 1) in the STAT register, 
    if(new_mode != current_mode) {
        PPU_rg.stat = (PPU_rg.stat & 0xfc) | new_mode; // update bit 0 and 1 in the STAT register with the new mode

        // trigger the corresponding interrupt if the mode has changed
        if(new_mode == 0 && (PPU_rg.stat & 0x08)) {
            stat_interrupt_triggered = true; // HBlank interrupt
        } 
        else if(new_mode == 1 && (PPU_rg.stat & 0x10)) {
            stat_interrupt_triggered = true; // VBlank interrupt
        } 
        else if(new_mode == 2 && (PPU_rg.stat & 0x20)) {
            stat_interrupt_triggered = true; // OAM interrupt
        }

        // Set the interrupt flag in the IF register (0xff0f) if the new mode is VBlank (mode 1: frame rendering is complete)
        if(new_mode == 1) {
            u8 interrupt_flag = system_bus.get().read_from_bytes(0xff0f);
            interrupt_flag |= 0x01; // set the v-blank interrupt bit
            system_bus.get().write_to_bytes(0xff0f, interrupt_flag);
        }

        // handle OAM scanline rendering when entering OAM mode (mode 2: OAM search)
        if(new_mode == 2) {
            // handle OAM scanline rendering for the current scanline
            handle_oam_sprites();
        }

        // handle pixel generation and window line counter increment when entering HBlank mode (mode 0: current scanline rendering is complete)
        if(new_mode == 0) {
            // handle pixel generation for the current scanline and update the back buffer
            draw_scanline();
            
            // window line counter increment
            bool window_enabled = (PPU_rg.lcdc & 0x20) != 0; // check if window is enabled
            bool window_y_match = (PPU_rg.ly >= PPU_rg.wy); // check if the current scanline is at or below the window y position
            bool window_x_match = (PPU_rg.wx <= 166); // check if the window x position is within the visible range (0-166)

            if(window_enabled && window_y_match && window_x_match) {
                window_line_counter++; // increment the window line counter
            }
        }    
    }

    // get the current state of the coincidence flag in the STAT register i.e LYC == LY
    bool coincidence_flag = (PPU_rg.stat & 0x04) != 0;

    // check if the LY register matches the LYC register
    if(PPU_rg.ly == PPU_rg.lyc) {
        PPU_rg.stat |= 0x04; // set coincidence flag in the STAT register

        if(!coincidence_flag && (PPU_rg.stat & 0x40)) {
            stat_interrupt_triggered = true; // trigger a STAT interrupt if the coincidence flag was previously cleared and is now set(rising edge)
        }
    }
    else {
        PPU_rg.stat &= 0xfb; // unset the coincidence flag in the STAT register
    }

    // trigger a STAT interrupt if necessary
    if(stat_interrupt_triggered) {
        // update the STAT interrupt bit in the IF register (0xff0f)
        u8 interrupt_flag = system_bus.get().read_from_bytes(0xff0f);
        interrupt_flag |= 0x02; // set the STAT interrupt bit
        system_bus.get().write_to_bytes(0xff0f, interrupt_flag);
    }
}

// draw scanline: render the current scanline based on the PPU registers and memory
void PPU :: draw_scanline() {
    if((PPU_rg.lcdc & 0x80) == 0) {
        return;
    }

    // store bg and window color ids for the current scanline
    u8 bg_color_ids[160]{};

    // gather background and window enabled flags from the LCDC register
    bool bg_win_enabled = (PPU_rg.lcdc & 0x01) != 0; // check if background and window are enabled
    bool window_enabled = (PPU_rg.lcdc & 0x20) != 0; // check if window is enabled

    // loop through current scanline pixels (0-159) and render the background, window, and sprites based on the PPU registers and memory
    for(int x = 0; x < 160; x++) {
        u8 curr_color_id = 0;
        u8 curr_pallete = PPU_rg.bgp;

        if(bg_win_enabled) {
            // check whether to render the window or the background
            bool render_window = window_enabled && (PPU_rg.ly >= PPU_rg.wy) && (x + 7 >= PPU_rg.wx);

            // window
            if(render_window) {
                int window_x = x + 7 - PPU_rg.wx; // calculate the window x position
                int window_y = window_line_counter; // use the window line counter for the y position

                u16 tile_map = (PPU_rg.lcdc & 0x40) ? 0x9c00 : 0x9800; // window tile map base address
                u16 tile_map_addr = tile_map + ((window_y / 8) * 32) + (window_x / 8); // calculate the tile map address for the window
                u8  tile_id = read_from_IO(tile_map_addr); // read the tile index from VRAM

                curr_color_id = fetch_tile_data_current_scanline(tile_id, window_x % 8, window_y % 8, (PPU_rg.lcdc & 0x10) != 0); // fetch the tile data for the current scanline and window position
            }
            else{ // background
                int bg_x = (x + PPU_rg.scx) % 256; // calculate the background x position
                int bg_y = (PPU_rg.ly + PPU_rg.scy) % 256; // calculate the background y position

                u16 tile_map = (PPU_rg.lcdc & 0x08) ? 0x9c00 : 0x9800; // background tile map base address
                u16 tile_map_addr = tile_map + ((bg_y / 8) * 32) + (bg_x / 8); // calculate the tile map address for the background
                u8  tile_id = read_from_IO(tile_map_addr); // read the tile index from VRAM

                curr_color_id = fetch_tile_data_current_scanline(tile_id, bg_x % 8, bg_y % 8, (PPU_rg.lcdc & 0x10) != 0); // fetch the tile data for the current scanline and background position
            }
        }

        bg_color_ids[x] = curr_color_id; // store the background color id for the current pixel
        u8 palette_value = (PPU_rg.bgp >> (curr_color_id * 2)) & 0x03; // get the palette value for the current color id
        frame_back[PPU_rg.ly][x] = color_pallete[palette_value]; // comeback here
    }

    // handle sprite rendering for the current scanline
    if(PPU_rg.lcdc & 0x02) {
        draw_sprites_current_scanline(bg_color_ids); // draw the sprites for the current scanline using the background color ids
    }
}

// fetch the tile data for the current scanline based on the PPU registers and memory
u8 PPU :: fetch_tile_data_current_scanline(u8 tile_id, int x, int y, bool bg_win_tile_area) {
    u16 tile_data_base = bg_win_tile_area ? 0x8000 : 0x9000; // determine the tile data base address based on the LCDC register
    u16 tile_data_addr = 0x0000;

    if(bg_win_tile_area) {
        tile_data_addr = tile_data_base + (tile_id * 16); // calulate the tile address using tile_data_base = 1
    } else {
        int8_t signed_tile_id = static_cast<int8_t>(tile_id);
        tile_data_addr = tile_data_base + (signed_tile_id * 16); // calculate the tile address using tile_data_base = 0
    }

    // calulate the row address and fetch the two bytes of tile data for the current scanline
    u16 row_addr = tile_data_addr + (y * 2);
    u8 tile_data_low = read_from_IO(row_addr); // fetch the low byte of tile data
    u8 tile_data_high = read_from_IO(row_addr + 1); // fetch the high byte of tile data

    // extract the color id for the current pixel from the tile data
    int bit_index = 7 - x; // calculate the bit index for the current pixel
    u8 color_id = ((tile_data_high >> bit_index) & 0x01) << 1 | ((tile_data_low >> bit_index) & 0x01); 
}

// draw sprites for the current scanline based on the PPU registers and memory
void PPU :: draw_sprites_current_scanline(const u8 bg_color_ids[]) {
    // check the oam sprite size from the LCDC register (bit 2)
    u8 sprite_height = (PPU_rg.lcdc & 0x04) ? 16 : 8;

    // iterate through the visible sprites for the current scanline
    for(std::size_t curr_sprite = 0; curr_sprite < oam_sprites_current_scanline.size(); curr_sprite++) {
        const auto& sprite = oam_sprites_current_scanline[curr_sprite];

        
    }
}

// handle OAM sprites for the current scanline
void PPU :: handle_oam_sprites() {
    // clear the vector of OAM sprites for the current scanline
    oam_sprites_current_scanline.clear();

    // get the sprite height from the LCDC register (bit 2)
    u8 sprite_height = (PPU_rg.lcdc & 0x04) ? 16 : 8;

    // iterate through the OAM memory to find sprites that are visible on the current scanline
    for(int x = 0; x < 40; x++) {
        u8 sprite_y = oam[x * 4]; // get the y position of the sprite
        u8 sprite_x = oam[x * 4 + 1]; // get the x position of the sprite

        // check if the sprite is visible on the current scanline
        if(PPU_rg.ly >= (sprite_y - 16) && PPU_rg.ly < (sprite_y - 16 + sprite_height)) {
            OAM_sprite sprite;
            sprite.y_position = sprite_y;
            sprite.x_position = sprite_x;
            sprite.tile_index = oam[x * 4 + 2];
            sprite.attributes = oam[x * 4 + 3];
            sprite.oam_idx = x;

            // add the visible sprite to the vector for the current scanline
            oam_sprites_current_scanline.push_back(sprite);

            // limit the number of sprites per scanline to 10 (hardware limitation)
            if(oam_sprites_current_scanline.size() >= 10) {
                break;
            }
        }
    }

    // sort the visible sprites by their x position (left to right) and OAM index
    std::sort(oam_sprites_current_scanline.begin(), oam_sprites_current_scanline.end(), [](const OAM_sprite& a, const OAM_sprite& b) {
        if(a.x_position == b.x_position) {
            return a.oam_idx > b.oam_idx;
        }
        return a.x_position > b.x_position;
    });
}

// check if OAM DMA transfer is active
bool PPU :: is_oam_dma_running() const {
    return PPU_rg.is_oam_dma_active_;
}

// swap the front and back frame buffers for rendering
void PPU :: swap_frame_buffers() {
    std::swap(frame_back, frame_front);
    frame_ready = true; // set the frame ready flag after swapping i.e for SDL3 rendering
}

// getter for frame buffer to display
const std::array<std::array<u32, 160>, 144>& PPU :: get_frame_buffer() const {
    return frame_front;
}

// check if the frame is ready for rendering
bool PPU :: is_frame_ready() const {
    return frame_ready;
}

// reset the frame ready flag after rendering
void PPU :: reset_frame_ready() {
    frame_ready = false;
}