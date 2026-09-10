#include <iostream>
#include <fstream>
#include <filesystem>
#include "MMU.hpp"

// disable the boot rom
void MMU :: disable_bootROM() {
    boot_rom_mapped = false;
}

// get the eram size
u32 MMU :: find_eram_size(std::string active_save_file, u8 eram_header) {
    u32 eram_size = 0;

    switch(eram_header) {
        case 0x00: eram_size = 0;       break;
        case 0x02: eram_size = 0x2000;  break;
        case 0x03: eram_size = 0x8000;  break;
        case 0x04: eram_size = 0x20000; break;
        case 0x05: eram_size = 0x10000; break;
        default:   eram_size = 0;       break;
    }

    // resize to zero if no eram avaiblable on the cartridge
    if(eram_size == 0) {
        eram_bank.resize(0);
        return 0;
    }

    // trying to load existing an save file from disk
    std::ifstream file(active_save_file, std::ios::binary | std::ios::ate);

    // if there file does not open i.e there are no save files
    if(!file.is_open()) {
        eram_bank.resize(eram_size, 0xff);
    }

    // save file exist for this game
    else {
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<u8>eram_buffer;
        eram_buffer.resize(size);

        if(file.read(reinterpret_cast<char*>(eram_buffer.data()), size)) {
            std::cout << "ERAM file read successfully" << std::endl;
        }
        else {
            std::cerr << "Error: Failed to read the ERAM file" << std::endl;
        }

        if(eram_buffer.size() < eram_size) {
            eram_buffer.resize(eram_size, 0xff);
        }

        eram_bank = std::move(eram_buffer);
    }

    return eram_size;
}

// load rom and eram 
void MMU :: load_ROM(const char* RomFile) {
    // open ROM file and move pointer to the end to get size
    std::ifstream file(RomFile, std::ios::binary | std::ios::ate);

    if(!file.is_open()) {
        std::cerr << "Failed to open ROM file: " << RomFile << std::endl;
        return;
    }

    // verify that the ROM is atleats 32kb i.e fixed ROM(16kb) + switchable ROM(16kb)
    std::streamsize size = file.tellg();
    if(size < 0x8000) {
        std::cerr << "Error: The ROM is invalid" << std::endl;
        return;
    }
    
    // create temporary buffer and resize it to the size of the ROM
    std::vector<u8>buffer;
    buffer.resize(size);

    // move the pointer to the begining of the file and load the contents into buffer
    file.seekg(0, std::ios::beg);
    if(file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        std::cout << "ROM file read successfully" << std::endl;
    }
    else {
        std::cerr << "Error: Failed to read the ROM file" << std::endl;
    }

    // get the ROM size from the header
    u32 header_rom_size = 32768 << buffer[0x0148];

    // check if header_rom_size is greater than buffer and pad 
    if(buffer.size() < header_rom_size) {
        buffer.resize(header_rom_size, 0xff);
    }

    // set rom_size variable in the MBC_bus abstract class
    u32 rom_size = static_cast<u32>(buffer.size());
    mbc1->set_rom_size(rom_size);
    rom_bank = std::move(buffer);

    // create a save file
    std::filesystem::path save_file = RomFile;
    save_file.replace_extension(".sav");
    active_save_path = save_file.string();

    // get the RAM size
    u32 eram_size = find_eram_size(active_save_path, buffer[0x0149]); 
    mbc1->set_ram_size(eram_size);
}

// save eram: called when the game exits
void MMU :: save_eram() {
    // check if there was any eram capacity during gameplay
    if(mbc1->get_eram_size() == 0 || eram_bank.empty()) {
        return;
    }

    // open and overwrite its current data
    std::ofstream save_file(active_save_path, std::ios::binary | std::ios::trunc);

    // write to the file
    if(!save_file.is_open()) {
        std::cerr << "Error: Unable to save current progress" << std::endl;
    }

    else {
        save_file.write(reinterpret_cast<const char*>(eram_bank.data()), eram_bank.size());
        save_file.close();

        std::cout << "Game progress saved successfully" << std::endl;
    }
}

// reads
u8 MMU :: read_from_bytes(u16 address) const {
    // boot rom i.e nintendo logo
    if(boot_rom_mapped == true && address >= 0x0000 && address <= 0x0100) {
        return 0xff;
    }

    // handle cartridge fixed bank
    else if(address >= 0x0000 && address <= 0x3fff) {
        return rom_bank[mbc1->get_rom_lower_offset() + address];
    }

    // handle cartridge switchable banks
    else if(address >= 0x4000 && address <= 0x7fff) {
        u32 location = mbc1->get_rom_upper_offset() + (address - 0x4000); 
        return rom_bank[location];
    }

    // route video ram reads to the PPU
    else if(address >= 0x8000 && address <= 0x9fff) {
        for(auto device : IO_devices) {
            if(device.get().respond_to_operation(address)) {
                return device.get().read_from_IO(address);
            }
        }

        return 0xff;
    }

    // handle external ram reads
    else if(address >= 0xa000 && address <= 0xbfff) {
        if(eram_bank.empty() || mbc1->get_eram_enabled() == false) {
            return 0xff;
        }

        return eram_bank[mbc1->get_eram_offset() + (address - 0xa000)];
    }

    // handle working ram reads(echo ram ranges are included here to mirror c000–ddff i.e wram for nonCB operations)
    else if(address >= 0xc000 && address <= 0xfdff) {
        return wram[(address - 0xc000) % 0x2000]; // modulo incase it read echo ram ranges**
    }

    // route OAM reads to the PPU
    else if(address >= 0xfe00 && address <= 0xfe9f) {
        for(auto & device : IO_devices) {
            if(device.get().respond_to_operation(address)) {
                return device.get().read_from_IO(address);
            }
        }

        return 0xff;
    }
    
    // Unused Memory(prohibited)
    else if(address >= 0xfea0 && address <= 0xfeff) {
        return 0x00;
    }

    // route I/O reads to IO 
    else if(address >= 0xff00 && address <= 0xff7f) {
        for(auto & device : IO_devices) {
            if(device.get().respond_to_operation(address)) {
                return device.get().read_from_IO(address);
            }
        }

        return 0xff;
    }

    // handle high ram reads 
    else if(address >= 0xff80 && address <= 0xfffe) {
        return hram[address - 0xff80];
    }

    // handle interupt enabler
    else if(address == 0xffff) {    
        return ie;
    }
}

// Writes
void MMU :: write_to_bytes(u16 address, u8 value) {
    // ROM banks
    if(address >= 0x0000 && address <= 0x7fff) {
        if(mbc1) {
            mbc1->calculate_and_find_banks(address, value);
        }
    }

    // route video ram writes to the PPU
    else if(address >= 0x8000 && address <= 0x9fff) {
        for(auto & device : IO_devices) {
            if(device.get().respond_to_operation(address)) {
                device.get().write_to_IO(address, value);
            }
        }          
    }

    // handle external ram writes
    else if(address >= 0xa000 && address <= 0xbfff) {
        if(mbc1) {
            if(!mbc1->get_eram_enabled() | eram_bank.size() == 0) {
                return;
            }

            eram_bank[mbc1->get_eram_offset() + (address - 0xa000)] = value;
        }
    }

    // handle working ram(echo ram ranges are included here)
    else if(address >= 0xc000 && address <= 0xfdff) {
        wram[(address - 0xc000) % 0x2000] = value;
    }

    // route OAM writes to the PPU
    else if(address >= 0xfe00 && address <= 0xfe9f) {
        for(auto & device : IO_devices) {
            if(device.get().respond_to_operation(address)) {
                device.get().write_to_IO(address, value);
            }
        }        
    }

    // Unuseable Memory(prohibited)
    else if(address >= 0xfea0 && address <= 0xfeff) {
        return;
    }

    // boot rom i.e nintendo logo
    else if(address == 0xff50) {
        return;    
    }

    // route I/O writes to IO 
    else if(address >= 0xff00 && address <= 0xff7f) {
        for(auto & device : IO_devices) {
            if(device.get().respond_to_operation(address)) {
                device.get().write_to_IO(address, value);
            }
        }        
    }

    // handle high ram writes
    else if(address >= 0xff80 && address <= 0xfffe) {
        hram[address - 0xff80] = value;
    }

    // handle interupt enabler writes
    else if(address == 0xffff){
        ie = value;
    }
}

// verify pending interrupts
bool MMU :: pending_interrupts() const {
    u8 interrupt_flag_    = read_from_bytes(0xff0f); 
    u8 interrupt_enabled_ = read_from_bytes(0xffff);

    return (interrupt_flag_ & interrupt_enabled_ & 0x1F) != 0;
}

// read MMU from IO 
u8 MMU :: read_bytes(u16 address) const {
    
}

// write to MMU from IO
void MMU :: write_bytes(u16 address, u8 value) {
    
}