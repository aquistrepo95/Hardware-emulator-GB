#ifndef CPU_HPP
#define CPU_HPP
#include <cstdint>
#include <array>
#include <functional>
#include "MMU.hpp"
#include "EmulatorClock.hpp"

class CPU {
    private:
        // registers struct to hold all the registers of the CPU
        struct registers
        {
            // program counter and stack pointer
            u16 program_counter{};
            u16 stack_pointer{};

            // 8 bit registers, may be combined to form 16 bit registers for specific instructions(little endian)
            union
            {
                struct 
                {
                    u8 f;  
                    u8 a; 

                };
                u16 af = 0x01b0; // 16 bit combination
            };

            union
            {
                struct 
                {
                    u8 c; 
                    u8 b;
                };
                u16 bc = 0x0013; // 16 bit combination
            };

            union
            {
                struct 
                {
                    u8 e; 
                    u8 d;
                };
                u16 de = 0x00d8; // 16 bit combination
            };

            union
            {
                struct 
                {
                    u8 l; 
                    u8 h;
                };
                u16 hl = 0x014d; // 16 bit combination
            };  

            // flags for f register
            static constexpr u8 Zero_flag      = 0x80; //10000000
            static constexpr u8 Subtract_flag  = 0x40; //01000000
            static constexpr u8 HalfCarry_flag = 0x20; //00100000
            static constexpr u8 Carry_flag     = 0x10; //00010000

            // getter to verify value in f register
            bool get_Zero_flag() const{
                return (f & Zero_flag) != 0;
            }

            bool get_Subtract_flag() const{
                return (f & Subtract_flag) != 0;
            }

            bool get_HalfCarry_flag() const{
                return (f & HalfCarry_flag) != 0;
            }

            bool get_Carry_flag() const{
                return (f & Carry_flag) != 0;
            }

            // setting flag in the f register and ensuring that the last 4 bits are set to 0
            // condition is a boolean value that determines whether to set the bit or not
            void set_flag(u8 curr_flag, bool condition) {
                if(condition) {
                    f |= curr_flag;
                }

                else {
                    f &= ~curr_flag;
                }

                // ensure the lower nibble is set to 0
                f &= 0xf0;
            }   
        };

        // object for registers struct
        registers rg;

        // Non CB opcode cycles array
        static constexpr std::array<u8, 0x100> nonCB_opcode_cycles = {
            4, 12, 8, 8, 4, 4, 8, 4, 20, 8, 8, 8, 4, 4, 8, 4,
            4, 12, 8, 8, 4, 4, 8, 4, 12, 8, 8, 8, 4, 4, 8, 4,
            12, 12, 8, 8, 4, 4, 8, 4, 12, 8, 8, 8, 4, 4, 8, 4,
            12, 12, 8, 8, 12, 12, 12, 4, 12, 8, 8, 8, 4, 4, 8, 4,
            4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
            4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
            4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
            8, 8, 8, 8, 8, 8, 4, 8, 4, 4, 4, 4, 4, 4, 8, 4,
            4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
            4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
            4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
            4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
            20, 12, 16, 16, 24, 16, 8, 16, 20, 12, 16, 0, 24, 24, 8, 16,
            20, 12, 16, 0, 24, 16, 8, 16, 20, 16, 16, 0, 24, 0, 8, 16,
            12, 12, 8, 0, 0, 16, 8, 16, 16, 4, 16, 0, 0, 0, 8, 16, 
            12, 12, 8, 4, 0, 16, 8, 16, 12, 8, 16, 4, 0, 0, 8, 16            
        }; 

        // CB opcode cycles array
        static constexpr std::array<u8, 0x100> CB_opcode_cycles = {
            8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
            8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
            8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
            8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
            8, 8, 8, 8, 8, 8, 12, 8, 8, 8, 8, 8, 8, 8, 12, 8,
            8, 8, 8, 8, 8, 8, 12, 8, 8, 8, 8, 8, 8, 8, 12, 8,
            8, 8, 8, 8, 8, 8, 12, 8, 8, 8, 8, 8, 8, 8, 12, 8,
            8, 8, 8, 8, 8, 8, 12, 8, 8, 8, 8, 8, 8, 8, 12, 8,
            8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
            8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
            8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
            8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
            8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
            8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
            8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
            8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8
        };

         // declare 
        u8 opcode{};

        // mmu object
        std::reference_wrapper<MMU> mmu;
        //MMU &mmu;

        // halt flag
        bool halt_flag = false;
        bool halt_bug  = false;

        // Emulator clock object
        std::reference_wrapper<EmulatorClock> clock;
        //EmulatorClock& clock;

        // Interupt Master Enable flag
        bool IME_flag = false;
        int IME_enable_pending = 0;


    public:

        // constructor 
        CPU(std::reference_wrapper<MMU> m, std::reference_wrapper<EmulatorClock> c);

        // cpu cycle 
        void CPU_cycle();

        // alias for array pointers
        using gameboyinstructions = void (CPU::*)();

        // table for non CB opcodes   
        using NonCB_MainTable  = std::array<gameboyinstructions, 0xff + 1 >; // main table for non CB opcodes
        
        // table for CB opcodes
        using CB_MainTable     = std::array<gameboyinstructions, 0xff + 1 >; // main table for CB opcodes

        // read and write functions for NON-CB instructions in registers
        constexpr u8 read_register(u8 reg);
        constexpr void write_register(u8 reg, u8 value);

        // execute interrupts
        void execute_interrupts();
 
    private:
        // stack operations
        void PUSH_stack();
        void POP_stack();

        // 0x00 NOP
        void _0x00_NOP();

        // HALT
        void HALT();

        // 0x10 STOP
        void _0x1000_STOP();

        // LD
        void LD_r16_imm16();
        void LD_r16_a();
        void LD_r8_imm8();
        void LD_imm16_sp();
        void LD_a_r16();
        void LD_r8_r8();
        void LD_imm8_a(); 
        void LD_c_a();
        void LD_a_c();
        void LD_imm16_a();
        void LD_a_imm8();
        void LD_HL_SP_imm8();
        void LD_SP_HL();
        void LD_a_imm16();

        // increment functions
        void INC_r16();
        void INC_r8();

        // decrement functions
        void DEC_r16();
        void DEC_r8();

        // rotate functions
        void RLCA();
        void RLA();
        void RRCA();
        void RRA();

        // flip all bits
        void CPL();
        void CCF();

        // adjust a register
        void DAA();

        // set carry flag
        void SCF();

        // add functions
        void ADD_a_r8();
        void ADC_a_r8();
        void ADD_a_imm8();
        void ADC_a_imm8();
        void ADD_HL_r16();
        void ADD_SP_imm8();

        // subtract functions
        void SUB_a_r8();
        void SBC_a_r8();
        void SUB_a_imm8();
        void SBC_a_imm8();

        // AND, OR, XOR, CP functions
        void AND_a_r8();
        void AND_a_imm8();
        void OR_a_r8();
        void OR_a_imm8();
        void XOR_a_r8();
        void XOR_a_imm8();
        void CP_a_r8();
        void CP_imm8();

        // RET functions
        void RET_cond();
        void RET();
        void RETI();

        // JUMP
        void JR_cond_imm8();
        void JR_imm8();
        void JP_cond_imm16();
        void JP_imm16();
        void JP_hl();

        // CALL functions
        void CALL_cond_i16(); //CALL cond, addr
        void CALL_i16();      //CALL addr

        // RST functions
        void RST();

        // DI 
        void DI();

        // EI
        void EI();

        // invalid opcode handler
        void invalid();


        // CB functions
        void RLC();
        void RRC();
        void RL();
        void RR();
        void SLA();
        void SRA();
        void SWAP();
        void SRL();
        void BIT_b3_r8();
        void RES_b3_r8();
        void SET_b3_r8();

        // load the main table for non CB opcodes at compile time
        static constexpr NonCB_MainTable MainTableNOCB = []() {
            NonCB_MainTable table{};

            table[0x00] = &CPU::_0x00_NOP;    table[0x10] = &CPU::_0x1000_STOP; table[0x20] = &CPU::JR_cond_imm8; table[0x30] = &CPU::JR_cond_imm8; table[0x40] = &CPU::LD_r8_r8;
            table[0x01] = &CPU::LD_r16_imm16; table[0x11] = &CPU::LD_r16_imm16; table[0x21] = &CPU::LD_r16_imm16; table[0x31] = &CPU::LD_r16_imm16; table[0x41] = &CPU::LD_r8_r8;
            table[0x02] = &CPU::LD_r16_a;     table[0x12] = &CPU::LD_r16_a;     table[0x22] = &CPU::LD_r16_a;     table[0x32] = &CPU::LD_r16_a;     table[0x42] = &CPU::LD_r8_r8;
            table[0x03] = &CPU::INC_r16;      table[0x13] = &CPU::INC_r16;      table[0x23] = &CPU::INC_r16;      table[0x33] = &CPU::INC_r16;      table[0x43] = &CPU::LD_r8_r8;
            table[0x04] = &CPU::INC_r8;       table[0x14] = &CPU::INC_r8;       table[0x24] = &CPU::INC_r8;       table[0x34] = &CPU::INC_r8;       table[0x44] = &CPU::LD_r8_r8;
            table[0x05] = &CPU::DEC_r8;       table[0x15] = &CPU::DEC_r8;       table[0x25] = &CPU::DEC_r8;       table[0x35] = &CPU::DEC_r8;       table[0x45] = &CPU::LD_r8_r8;
            table[0x06] = &CPU::LD_r8_imm8;   table[0x16] = &CPU::LD_r8_imm8;   table[0x26] = &CPU::LD_r8_imm8;   table[0x36] = &CPU::LD_r8_imm8;   table[0x46] = &CPU::LD_r8_r8;
            table[0x07] = &CPU::RLCA;         table[0x17] = &CPU::RLA;          table[0x27] = &CPU::DAA;          table[0x37] = &CPU::SCF;          table[0x47] = &CPU::LD_r8_r8;
            table[0x08] = &CPU::LD_imm16_sp;  table[0x18] = &CPU::JR_imm8;      table[0x28] = &CPU::JR_cond_imm8; table[0x38] = &CPU::JR_cond_imm8; table[0x48] = &CPU::LD_r8_r8;
            table[0x09] = &CPU::ADD_HL_r16;   table[0x19] = &CPU::ADD_HL_r16;   table[0x29] = &CPU::ADD_HL_r16;   table[0x39] = &CPU::ADD_HL_r16;   table[0x49] = &CPU::LD_r8_r8;
            table[0x0a] = &CPU::LD_a_r16;     table[0x1a] = &CPU::LD_a_r16;     table[0x2a] = &CPU::LD_a_r16;     table[0x3a] = &CPU::LD_a_r16;     table[0x4a] = &CPU::LD_r8_r8;
            table[0x0b] = &CPU::DEC_r16;      table[0x1b] = &CPU::DEC_r16;      table[0x2b] = &CPU::DEC_r16;      table[0x3b] = &CPU::DEC_r16;      table[0x4b] = &CPU::LD_r8_r8;
            table[0x0c] = &CPU::INC_r8;       table[0x1c] = &CPU::INC_r8;       table[0x2c] = &CPU::INC_r8;       table[0x3c] = &CPU::INC_r8;       table[0x4c] = &CPU::LD_r8_r8;
            table[0x0d] = &CPU::DEC_r8;       table[0x1d] = &CPU::DEC_r8;       table[0x2d] = &CPU::DEC_r8;       table[0x3d] = &CPU::DEC_r8;       table[0x4d] = &CPU::LD_r8_r8;
            table[0x0e] = &CPU::LD_r8_imm8;   table[0x1e] = &CPU::LD_r8_imm8;   table[0x2e] = &CPU::LD_r8_imm8;   table[0x3e] = &CPU::LD_r8_imm8;   table[0x4e] = &CPU::LD_r8_r8;
            table[0x0f] = &CPU::RRCA;         table[0x1f] = &CPU::RRA;          table[0x2f] = &CPU::CPL;          table[0x3f] = &CPU::CCF;          table[0x4f] = &CPU::LD_r8_r8;
            
            table[0x50] = &CPU::LD_r8_r8;     table[0x60] = &CPU::LD_r8_r8;     table[0x70] = &CPU::LD_r8_r8;     table[0x80] = &CPU::ADD_a_r8;     table[0x90] = &CPU::SUB_a_r8;
            table[0x51] = &CPU::LD_r8_r8;     table[0x61] = &CPU::LD_r8_r8;     table[0x71] = &CPU::LD_r8_r8;     table[0x81] = &CPU::ADD_a_r8;     table[0x91] = &CPU::SUB_a_r8;
            table[0x52] = &CPU::LD_r8_r8;     table[0x62] = &CPU::LD_r8_r8;     table[0x72] = &CPU::LD_r8_r8;     table[0x82] = &CPU::ADD_a_r8;     table[0x92] = &CPU::SUB_a_r8;
            table[0x53] = &CPU::LD_r8_r8;     table[0x63] = &CPU::LD_r8_r8;     table[0x73] = &CPU::LD_r8_r8;     table[0x83] = &CPU::ADD_a_r8;     table[0x93] = &CPU::SUB_a_r8;
            table[0x54] = &CPU::LD_r8_r8;     table[0x64] = &CPU::LD_r8_r8;     table[0x74] = &CPU::LD_r8_r8;     table[0x84] = &CPU::ADD_a_r8;     table[0x94] = &CPU::SUB_a_r8;
            table[0x55] = &CPU::LD_r8_r8;     table[0x65] = &CPU::LD_r8_r8;     table[0x75] = &CPU::LD_r8_r8;     table[0x85] = &CPU::ADD_a_r8;     table[0x95] = &CPU::SUB_a_r8;
            table[0x56] = &CPU::LD_r8_r8;     table[0x66] = &CPU::LD_r8_r8;     table[0x76] = &CPU::HALT;         table[0x86] = &CPU::ADD_a_r8;     table[0x96] = &CPU::SUB_a_r8;
            table[0x57] = &CPU::LD_r8_r8;     table[0x67] = &CPU::LD_r8_r8;     table[0x77] = &CPU::LD_r8_r8;     table[0x87] = &CPU::ADD_a_r8;     table[0x97] = &CPU::SUB_a_r8;
            table[0x58] = &CPU::LD_r8_r8;     table[0x68] = &CPU::LD_r8_r8;     table[0x78] = &CPU::LD_r8_r8;     table[0x88] = &CPU::ADC_a_r8;     table[0x98] = &CPU::SBC_a_r8;
            table[0x59] = &CPU::LD_r8_r8;     table[0x69] = &CPU::LD_r8_r8;     table[0x79] = &CPU::LD_r8_r8;     table[0x89] = &CPU::ADC_a_r8;     table[0x99] = &CPU::SBC_a_r8;
            table[0x5a] = &CPU::LD_r8_r8;     table[0x6a] = &CPU::LD_r8_r8;     table[0x7a] = &CPU::LD_r8_r8;     table[0x8a] = &CPU::ADC_a_r8;     table[0x9a] = &CPU::SBC_a_r8;
            table[0x5b] = &CPU::LD_r8_r8;     table[0x6b] = &CPU::LD_r8_r8;     table[0x7b] = &CPU::LD_r8_r8;     table[0x8b] = &CPU::ADC_a_r8;     table[0x9b] = &CPU::SBC_a_r8;
            table[0x5c] = &CPU::LD_r8_r8;     table[0x6c] = &CPU::LD_r8_r8;     table[0x7c] = &CPU::LD_r8_r8;     table[0x8c] = &CPU::ADC_a_r8;     table[0x9c] = &CPU::SBC_a_r8;
            table[0x5d] = &CPU::LD_r8_r8;     table[0x6d] = &CPU::LD_r8_r8;     table[0x7d] = &CPU::LD_r8_r8;     table[0x8d] = &CPU::ADC_a_r8;     table[0x9d] = &CPU::SBC_a_r8;
            table[0x5e] = &CPU::LD_r8_r8;     table[0x6e] = &CPU::LD_r8_r8;     table[0x7e] = &CPU::LD_r8_r8;     table[0x8e] = &CPU::ADC_a_r8;     table[0x9e] = &CPU::SBC_a_r8;
            table[0x5f] = &CPU::LD_r8_r8;     table[0x6f] = &CPU::LD_r8_r8;     table[0x7f] = &CPU::LD_r8_r8;     table[0x8f] = &CPU::ADC_a_r8;     table[0x9f] = &CPU::SBC_a_r8;
            
            table[0xa0] = &CPU::AND_a_r8;     table[0xb0] = &CPU::OR_a_r8;      table[0xc0] = &CPU::RET_cond;     table[0xd0] = &CPU::RET_cond;     table[0xe0] = &CPU::LD_imm8_a;
            table[0xa1] = &CPU::AND_a_r8;     table[0xb1] = &CPU::OR_a_r8;      table[0xc1] = &CPU::POP_stack;    table[0xd1] = &CPU::POP_stack;    table[0xe1] = &CPU::POP_stack;
            table[0xa2] = &CPU::AND_a_r8;     table[0xb2] = &CPU::OR_a_r8;      table[0xc2] = &CPU::JP_cond_imm16;table[0xd2] = &CPU::JP_cond_imm16;table[0xe2] = &CPU::LD_c_a;
            table[0xa3] = &CPU::AND_a_r8;     table[0xb3] = &CPU::OR_a_r8;      table[0xc3] = &CPU::JP_imm16;     table[0xd3] = &CPU::invalid;      table[0xe3] = &CPU::invalid;
            table[0xa4] = &CPU::AND_a_r8;     table[0xb4] = &CPU::OR_a_r8;      table[0xc4] = &CPU::CALL_cond_i16;table[0xd4] = &CPU::CALL_cond_i16;table[0xe4] = &CPU::invalid;
            table[0xa5] = &CPU::AND_a_r8;     table[0xb5] = &CPU::OR_a_r8;      table[0xc5] = &CPU::PUSH_stack;   table[0xd5] = &CPU::PUSH_stack;   table[0xe5] = &CPU::PUSH_stack;
            table[0xa6] = &CPU::AND_a_r8;     table[0xb6] = &CPU::OR_a_r8;      table[0xc6] = &CPU::ADD_a_imm8;   table[0xd6] = &CPU::SUB_a_imm8;   table[0xe6] = &CPU::AND_a_imm8;
            table[0xa7] = &CPU::ADC_a_r8;     table[0xb7] = &CPU::OR_a_r8;      table[0xc7] = &CPU::RST;          table[0xd7] = &CPU::RST;          table[0xe7] = &CPU::RST;
            table[0xa8] = &CPU::XOR_a_r8;     table[0xb8] = &CPU::CP_a_r8;      table[0xc8] = &CPU::RET_cond;     table[0xd8] = &CPU::RET_cond;     table[0xe8] = &CPU::ADD_SP_imm8;
            table[0xa9] = &CPU::XOR_a_r8;     table[0xb9] = &CPU::CP_a_r8;      table[0xc9] = &CPU::RET;          table[0xd9] = &CPU::RETI;         table[0xe9] = &CPU::JP_hl;
            table[0xaa] = &CPU::XOR_a_r8;     table[0xba] = &CPU::CP_a_r8;      table[0xca] = &CPU::JP_cond_imm16;table[0xda] = &CPU::JP_cond_imm16;table[0xea] = &CPU::LD_imm16_a;
            table[0xab] = &CPU::XOR_a_r8;     table[0xbb] = &CPU::CP_a_r8;      table[0xcb] = &CPU::invalid;      table[0xdb] = &CPU::invalid;      table[0xeb] = &CPU::invalid;
            table[0xac] = &CPU::XOR_a_r8;     table[0xbc] = &CPU::CP_a_r8;      table[0xcc] = &CPU::CALL_cond_i16;table[0xdc] = &CPU::CALL_cond_i16;table[0xec] = &CPU::invalid;
            table[0xad] = &CPU::XOR_a_r8;     table[0xbd] = &CPU::CP_a_r8;      table[0xcd] = &CPU::CALL_i16;     table[0xdd] = &CPU::invalid;      table[0xed] = &CPU::invalid;
            table[0xae] = &CPU::XOR_a_r8;     table[0xbe] = &CPU::CP_a_r8;      table[0xce] = &CPU::ADC_a_imm8;   table[0xde] = &CPU::SBC_a_imm8;   table[0xee] = &CPU::XOR_a_imm8;
            table[0xaf] = &CPU::XOR_a_r8;     table[0xbf] = &CPU::CP_a_r8;      table[0xcf] = &CPU::RST;          table[0xdf] = &CPU::RST;          table[0xef] = &CPU::RST;

            table[0xf0] = &CPU::LD_a_imm8;
            table[0xf1] = &CPU::POP_stack;
            table[0xf2] = &CPU::LD_a_c;
            table[0xf3] = &CPU::DI;
            table[0xf4] = &CPU::invalid;
            table[0xf5] = &CPU::PUSH_stack;
            table[0xf6] = &CPU::OR_a_imm8;
            table[0xf7] = &CPU::RST;
            table[0xf8] = &CPU::LD_HL_SP_imm8;
            table[0xf9] = &CPU::LD_SP_HL;
            table[0xfa] = &CPU::LD_a_imm16;
            table[0xfb] = &CPU::EI;
            table[0xfc] = &CPU::invalid;
            table[0xfd] = &CPU::invalid;
            table[0xfe] = &CPU::CP_imm8;
            table[0xff] = &CPU::RST;

            return table;
        }();
    

        // load the main table for CB opcodes at compile time
        static constexpr CB_MainTable MainTableCB = []() {
            CB_MainTable table{};

            table[0x00] = &CPU::RLC;          table[0x10] = &CPU::RL;           table[0x20] = &CPU::SLA;          table[0x30] = &CPU::SWAP;         table[0x40] = &CPU::BIT_b3_r8;
            table[0x01] = &CPU::RLC;          table[0x11] = &CPU::RL;           table[0x21] = &CPU::SLA;          table[0x31] = &CPU::SWAP;         table[0x41] = &CPU::BIT_b3_r8;
            table[0x02] = &CPU::RLC;          table[0x12] = &CPU::RL;           table[0x22] = &CPU::SLA;          table[0x32] = &CPU::SWAP;         table[0x42] = &CPU::BIT_b3_r8;
            table[0x03] = &CPU::RLC;          table[0x13] = &CPU::RL;           table[0x23] = &CPU::SLA;          table[0x33] = &CPU::SWAP;         table[0x43] = &CPU::BIT_b3_r8;
            table[0x04] = &CPU::RLC;          table[0x14] = &CPU::RL;           table[0x24] = &CPU::SLA;          table[0x34] = &CPU::SWAP;         table[0x44] = &CPU::BIT_b3_r8;
            table[0x05] = &CPU::RLC;          table[0x15] = &CPU::RL;           table[0x25] = &CPU::SLA;          table[0x35] = &CPU::SWAP;         table[0x45] = &CPU::BIT_b3_r8;
            table[0x06] = &CPU::RLC;          table[0x16] = &CPU::RL;           table[0x26] = &CPU::SLA;          table[0x36] = &CPU::SWAP;         table[0x46] = &CPU::BIT_b3_r8;
            table[0x07] = &CPU::RLC;          table[0x17] = &CPU::RL;           table[0x27] = &CPU::SLA;          table[0x37] = &CPU::SWAP;         table[0x47] = &CPU::BIT_b3_r8;
            table[0x08] = &CPU::RRC;          table[0x18] = &CPU::RR;           table[0x28] = &CPU::SRA;          table[0x38] = &CPU::SRL;          table[0x48] = &CPU::BIT_b3_r8;
            table[0x09] = &CPU::RRC;          table[0x19] = &CPU::RR;           table[0x29] = &CPU::SRA;          table[0x39] = &CPU::SRL;          table[0x49] = &CPU::BIT_b3_r8;
            table[0x0a] = &CPU::RRC;          table[0x1a] = &CPU::RR;           table[0x2a] = &CPU::SRA;          table[0x3a] = &CPU::SRL;          table[0x4a] = &CPU::BIT_b3_r8;
            table[0x0b] = &CPU::RRC;          table[0x1b] = &CPU::RR;           table[0x2b] = &CPU::SRA;          table[0x3b] = &CPU::SRL;          table[0x4b] = &CPU::BIT_b3_r8;
            table[0x0c] = &CPU::RRC;          table[0x1c] = &CPU::RR;           table[0x2c] = &CPU::SRA;          table[0x3c] = &CPU::SRL;          table[0x4c] = &CPU::BIT_b3_r8;
            table[0x0d] = &CPU::RRC;          table[0x1d] = &CPU::RR;           table[0x2d] = &CPU::SRA;          table[0x3d] = &CPU::SRL;          table[0x4d] = &CPU::BIT_b3_r8;
            table[0x0e] = &CPU::RRC;          table[0x1e] = &CPU::RR;           table[0x2e] = &CPU::SRA;          table[0x3e] = &CPU::SRL;          table[0x4e] = &CPU::BIT_b3_r8;
            table[0x0f] = &CPU::RRC;          table[0x1f] = &CPU::RR;           table[0x2f] = &CPU::SRA;          table[0x3f] = &CPU::SRL;          table[0x4f] = &CPU::BIT_b3_r8;
            
            table[0x50] = &CPU::BIT_b3_r8;    table[0x60] = &CPU::BIT_b3_r8;    table[0x70] = &CPU::BIT_b3_r8;    table[0x80] = &CPU::RES_b3_r8;    table[0x90] = &CPU::RES_b3_r8;
            table[0x51] = &CPU::BIT_b3_r8;    table[0x61] = &CPU::BIT_b3_r8;    table[0x71] = &CPU::BIT_b3_r8;    table[0x81] = &CPU::RES_b3_r8;    table[0x91] = &CPU::RES_b3_r8;
            table[0x52] = &CPU::BIT_b3_r8;    table[0x62] = &CPU::BIT_b3_r8;    table[0x72] = &CPU::BIT_b3_r8;    table[0x82] = &CPU::RES_b3_r8;    table[0x92] = &CPU::RES_b3_r8;
            table[0x53] = &CPU::BIT_b3_r8;    table[0x63] = &CPU::BIT_b3_r8;    table[0x73] = &CPU::BIT_b3_r8;    table[0x83] = &CPU::RES_b3_r8;    table[0x93] = &CPU::RES_b3_r8;
            table[0x54] = &CPU::BIT_b3_r8;    table[0x64] = &CPU::BIT_b3_r8;    table[0x74] = &CPU::BIT_b3_r8;    table[0x84] = &CPU::RES_b3_r8;    table[0x94] = &CPU::RES_b3_r8;
            table[0x55] = &CPU::BIT_b3_r8;    table[0x65] = &CPU::BIT_b3_r8;    table[0x75] = &CPU::BIT_b3_r8;    table[0x85] = &CPU::RES_b3_r8;    table[0x95] = &CPU::RES_b3_r8;
            table[0x56] = &CPU::BIT_b3_r8;    table[0x66] = &CPU::BIT_b3_r8;    table[0x76] = &CPU::BIT_b3_r8;    table[0x86] = &CPU::RES_b3_r8;    table[0x96] = &CPU::RES_b3_r8;
            table[0x57] = &CPU::BIT_b3_r8;    table[0x67] = &CPU::BIT_b3_r8;    table[0x77] = &CPU::BIT_b3_r8;    table[0x87] = &CPU::RES_b3_r8;    table[0x97] = &CPU::RES_b3_r8;
            table[0x58] = &CPU::BIT_b3_r8;    table[0x68] = &CPU::BIT_b3_r8;    table[0x78] = &CPU::BIT_b3_r8;    table[0x88] = &CPU::RES_b3_r8;    table[0x98] = &CPU::RES_b3_r8;
            table[0x59] = &CPU::BIT_b3_r8;    table[0x69] = &CPU::BIT_b3_r8;    table[0x79] = &CPU::BIT_b3_r8;    table[0x89] = &CPU::RES_b3_r8;    table[0x99] = &CPU::RES_b3_r8;
            table[0x5a] = &CPU::BIT_b3_r8;    table[0x6a] = &CPU::BIT_b3_r8;    table[0x7a] = &CPU::BIT_b3_r8;    table[0x8a] = &CPU::RES_b3_r8;    table[0x9a] = &CPU::RES_b3_r8;
            table[0x5b] = &CPU::BIT_b3_r8;    table[0x6b] = &CPU::BIT_b3_r8;    table[0x7b] = &CPU::BIT_b3_r8;    table[0x8b] = &CPU::RES_b3_r8;    table[0x9b] = &CPU::RES_b3_r8;
            table[0x5c] = &CPU::BIT_b3_r8;    table[0x6c] = &CPU::BIT_b3_r8;    table[0x7c] = &CPU::BIT_b3_r8;    table[0x8c] = &CPU::RES_b3_r8;    table[0x9c] = &CPU::RES_b3_r8;
            table[0x5d] = &CPU::BIT_b3_r8;    table[0x6d] = &CPU::BIT_b3_r8;    table[0x7d] = &CPU::BIT_b3_r8;    table[0x8d] = &CPU::RES_b3_r8;    table[0x9d] = &CPU::RES_b3_r8;
            table[0x5e] = &CPU::BIT_b3_r8;    table[0x6e] = &CPU::BIT_b3_r8;    table[0x7e] = &CPU::BIT_b3_r8;    table[0x8e] = &CPU::RES_b3_r8;    table[0x9e] = &CPU::RES_b3_r8;
            table[0x5f] = &CPU::BIT_b3_r8;    table[0x6f] = &CPU::BIT_b3_r8;    table[0x7f] = &CPU::BIT_b3_r8;    table[0x8f] = &CPU::RES_b3_r8;    table[0x9f] = &CPU::RES_b3_r8;
            
            table[0xa0] = &CPU::RES_b3_r8;    table[0xb0] = &CPU::RES_b3_r8;    table[0xc0] = &CPU::SET_b3_r8;    table[0xd0] = &CPU::SET_b3_r8;    table[0xe0] = &CPU::SET_b3_r8;
            table[0xa1] = &CPU::RES_b3_r8;    table[0xb1] = &CPU::RES_b3_r8;    table[0xc1] = &CPU::SET_b3_r8;    table[0xd1] = &CPU::SET_b3_r8;    table[0xe1] = &CPU::SET_b3_r8;
            table[0xa2] = &CPU::RES_b3_r8;    table[0xb2] = &CPU::RES_b3_r8;    table[0xc2] = &CPU::SET_b3_r8;    table[0xd2] = &CPU::SET_b3_r8;    table[0xe2] = &CPU::SET_b3_r8;
            table[0xa3] = &CPU::RES_b3_r8;    table[0xb3] = &CPU::RES_b3_r8;    table[0xc3] = &CPU::SET_b3_r8;    table[0xd3] = &CPU::SET_b3_r8;    table[0xe3] = &CPU::SET_b3_r8;
            table[0xa4] = &CPU::RES_b3_r8;    table[0xb4] = &CPU::RES_b3_r8;    table[0xc4] = &CPU::SET_b3_r8;    table[0xd4] = &CPU::SET_b3_r8;    table[0xe4] = &CPU::SET_b3_r8;
            table[0xa5] = &CPU::RES_b3_r8;    table[0xb5] = &CPU::RES_b3_r8;    table[0xc5] = &CPU::SET_b3_r8;    table[0xd5] = &CPU::SET_b3_r8;    table[0xe5] = &CPU::SET_b3_r8;
            table[0xa6] = &CPU::RES_b3_r8;    table[0xb6] = &CPU::RES_b3_r8;    table[0xc6] = &CPU::SET_b3_r8;    table[0xd6] = &CPU::SET_b3_r8;    table[0xe6] = &CPU::SET_b3_r8;
            table[0xa7] = &CPU::RES_b3_r8;    table[0xb7] = &CPU::RES_b3_r8;    table[0xc7] = &CPU::SET_b3_r8;    table[0xd7] = &CPU::SET_b3_r8;    table[0xe7] = &CPU::SET_b3_r8;
            table[0xa8] = &CPU::RES_b3_r8;    table[0xb8] = &CPU::RES_b3_r8;    table[0xc8] = &CPU::SET_b3_r8;    table[0xd8] = &CPU::SET_b3_r8;    table[0xe8] = &CPU::SET_b3_r8;
            table[0xa9] = &CPU::RES_b3_r8;    table[0xb9] = &CPU::RES_b3_r8;    table[0xc9] = &CPU::SET_b3_r8;    table[0xd9] = &CPU::SET_b3_r8;    table[0xe9] = &CPU::SET_b3_r8;
            table[0xaa] = &CPU::RES_b3_r8;    table[0xba] = &CPU::RES_b3_r8;    table[0xca] = &CPU::SET_b3_r8;    table[0xda] = &CPU::SET_b3_r8;    table[0xea] = &CPU::SET_b3_r8;
            table[0xab] = &CPU::RES_b3_r8;    table[0xbb] = &CPU::RES_b3_r8;    table[0xcb] = &CPU::SET_b3_r8;    table[0xdb] = &CPU::SET_b3_r8;    table[0xeb] = &CPU::SET_b3_r8;
            table[0xac] = &CPU::RES_b3_r8;    table[0xbc] = &CPU::RES_b3_r8;    table[0xcc] = &CPU::SET_b3_r8;    table[0xdc] = &CPU::SET_b3_r8;    table[0xec] = &CPU::SET_b3_r8;
            table[0xad] = &CPU::RES_b3_r8;    table[0xbd] = &CPU::RES_b3_r8;    table[0xcd] = &CPU::SET_b3_r8;    table[0xdd] = &CPU::SET_b3_r8;    table[0xed] = &CPU::SET_b3_r8;
            table[0xae] = &CPU::RES_b3_r8;    table[0xbe] = &CPU::RES_b3_r8;    table[0xce] = &CPU::SET_b3_r8;    table[0xde] = &CPU::SET_b3_r8;    table[0xee] = &CPU::SET_b3_r8;
            table[0xaf] = &CPU::RES_b3_r8;    table[0xbf] = &CPU::RES_b3_r8;    table[0xcf] = &CPU::SET_b3_r8;    table[0xdf] = &CPU::SET_b3_r8;    table[0xef] = &CPU::SET_b3_r8;

            table[0xf0] = &CPU::SET_b3_r8;
            table[0xf1] = &CPU::SET_b3_r8;
            table[0xf2] = &CPU::SET_b3_r8;
            table[0xf3] = &CPU::SET_b3_r8;
            table[0xf4] = &CPU::SET_b3_r8;
            table[0xf5] = &CPU::SET_b3_r8;
            table[0xf6] = &CPU::SET_b3_r8;
            table[0xf7] = &CPU::SET_b3_r8;
            table[0xf8] = &CPU::SET_b3_r8;
            table[0xf9] = &CPU::SET_b3_r8;
            table[0xfa] = &CPU::SET_b3_r8;
            table[0xfb] = &CPU::SET_b3_r8;
            table[0xfc] = &CPU::SET_b3_r8;
            table[0xfd] = &CPU::SET_b3_r8;
            table[0xfe] = &CPU::SET_b3_r8;
            table[0xff] = &CPU::SET_b3_r8;
            
            return table;
        }();
};

#endif // CPU_HPP