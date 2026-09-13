#ifndef MMU_HPP
#define MMU_HPP
#include <array>
#include <vector>
#include <cstdint>
#include <memory>
#include <string>
#include <functional>
#include "system_bus.hpp"
#include "MBC_bus.hpp"
#include "PPU.hpp"
#include "APU.hpp"
#include "Timer.hpp"

class MMU : public SystemBus {
    private:
        // boot rom mapped i.e nintento logo
        bool boot_rom_mapped = true;

        // memory allocation for cartridge
        std::vector<u8> rom_bank; // rom
        std::vector<u8> eram_bank; //external ram 

        // save path .sav
        std::string active_save_path;

        // memory allocation for gameboy internal components
        std::array<u8, 0x1fff + 1> wram{}; // 8kb wram
        std::array<u8, 0x7f> hram{}; // 127 hram
        u8 ie{}; // 8bit interrupt enable register

        // IO device vector
        std::vector<std::reference_wrapper<SystemBus>> IO_devices;

        // IO device reference to the PPU,APU,Timer,Joypad
        std::reference_wrapper<PPU> ppu_mmu;
        std::reference_wrapper<APU> apu_mmu;
        std::reference_wrapper<Timer> timer_mmu;
        //std::reference_wrapper<Joypad> joypad_mmu;

        // pointer to the MBC
        std::unique_ptr<MBC_bus> mbc1;

    public:
        //constructor
        MMU(std::reference_wrapper<PPU> ppu, std::reference_wrapper<APU> apu, std::reference_wrapper<Timer> timer);

        // disable the boot ROM
        void disable_bootROM();

        // find the RAM size
        u32 find_eram_size(std::string active_save_path, u8 ram_size_header);

        // load the game ROM
        void load_ROM(const char*);

        // stop and save the ram state
        void save_eram();

        // getter and setter(read and write)
        virtual u8 read_from_bytes(u16 address) const override;
        virtual void write_to_bytes(u16 address, u8 value) override;

        // verify interrupts
        bool pending_interrupts() const;

        // function to add IO to IO_devices
        void add_IO_devices(SystemBus& IO_device) {
            IO_devices.push_back(std::ref(IO_device));
        }

};

#endif //MMU_HPP