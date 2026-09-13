#include <iostream>
#include "CPU.hpp"

// constructor
CPU :: CPU(std::reference_wrapper<MMU> m, std::reference_wrapper<EmulatorClock> c) : mmu(m), clock(c) {
    // initialize the program counter and the stack pointer 
    rg.program_counter = 0x0100;
    rg.stack_pointer   = 0xfffe;
}

// CPU cycle function
void CPU :: CPU_cycle() {
    // check if the halt flag is set, if so skip the cycle
    if(halt_flag) {
        //std::cout << "CPU is halted" << std::endl;
        clock.get().cycle_tick(4); // still consumes 4 cycles even if halted

        // check if an interrupt is requested and enabled, if so exit the halt state
        if(mmu.get().pending_interrupts()) {
            halt_flag = false;

            if(IME_flag) {
                execute_interrupts();
            }
        }
    }
    else {
        // check if there are pending interupts && IME is true: execute the interrupt
        if(mmu.get().pending_interrupts() && IME_flag) {
            execute_interrupts();
            return;
        }

        // get the current opcode from the mmu
        opcode = mmu.get().read_from_bytes(rg.program_counter);
        std::cout << "Opcode: " << std::hex << opcode << std::endl;
        
        // Halt bug for games that use it
        if(halt_bug) {
            halt_bug = false;
        }
        else {
            rg.program_counter + 1;
        } 

        // call the right table i.e if nonCB OR CB and start decoding the instruction
        if(opcode == 0xcb) {
            opcode = mmu.get().read_from_bytes(rg.program_counter);
            rg.program_counter += 1;
            std::invoke(MainTableCB[opcode], this);
        }
        else {
            std::invoke(MainTableNOCB[opcode], this);
        }

        // update IME_enable_pending or the IME_flag if IME_enable_pending == 0
        IME_enable_pending > 0 ? IME_enable_pending -= 1 : IME_flag = true;
    }
}

// execute interrupts
void CPU :: execute_interrupts() {
    u8 ie_reg = mmu.get().read_from_bytes(0xffff);
    u8 if_reg = mmu.get().read_from_bytes(0xff0f);
    u8 pending_interrupt = ie_reg & if_reg & 0x1f; // last five bits

    u16 vector   = 0;
    u8 bit_mask  = 0;

    if(pending_interrupt & 0x01) { // V-blank
        vector   = 0x0040;
        bit_mask = 0x01;
    }
    else if(pending_interrupt & 0x02) { // LCD STAT
        vector   = 0x0048;
        bit_mask = 0x02;  
    }
    else if(pending_interrupt & 0x04) { // Timer
        vector   = 0x0050;
        bit_mask = 0x04; 
    }
    else if(pending_interrupt & 0x08) { // serial
        vector   = 0x0058;
        bit_mask = 0x08; 
    }
    else if(pending_interrupt & 0x10) { // Joypad
        vector   = 0x0060;
        bit_mask = 0x10; 
    }

    if(bit_mask != 0) {
        IME_flag = false; // ensure no interrupts 

        mmu.get().write_to_bytes(0xff0f, (if_reg & ~bit_mask)); // reset the IF register

        // push the current address of the program counter to the stack to allow execution of the interrupt
        rg.stack_pointer = -1;
        mmu.get().write_to_bytes(rg.stack_pointer, (rg.program_counter >> 8) & 0xff);
        rg.stack_pointer = -1;
        mmu.get().write_to_bytes(rg.stack_pointer, rg.program_counter & 0xff);
        clock.get().cycle_tick(8);

        // set the pc/jump to the interrupt address
        rg.program_counter = vector;
        clock.get().cycle_tick(4);
    }
}

// read function for registers
constexpr u8 CPU :: read_register(u8 reg) {
    switch(reg) {
        case 0: return rg.b;
        case 1: return rg.c;
        case 2: return rg.d;
        case 3: return rg.e;
        case 4: return rg.h;
        case 5: return rg.l;
        case 6: return mmu.get().read_from_bytes(rg.hl);
        case 7: return rg.a;
        default: return 0xff; // invalid register
    }
}

// write function for registers
constexpr void CPU :: write_register(u8 reg, u8 value) {
    switch(reg) {
        case 0: rg.b = value; break;
        case 1: rg.c = value; break;
        case 2: rg.d = value; break;
        case 3: rg.e = value; break;
        case 4: rg.h = value; break;
        case 5: rg.l = value; break;
        case 6: mmu.get().write_to_bytes(rg.hl, value); break;
        case 7: rg.a = value; break;
        default: break; // invalid register
    }
}

// stack operations
// write to stack 
void CPU :: PUSH_stack() {
    u8 source = (opcode & 0x30) >> 4; // bits 5-4
    u16 value = 0;

    switch(source) {
        case 0: value = rg.bc; break;
        case 1: value = rg.de; break;
        case 2: value = rg.hl; break;
        case 3: value = rg.af; break;
        default: break; // invalid register pair
    }  

    // write the high byte to the stack
    rg.stack_pointer -= 1;
    mmu.get().write_to_bytes(rg.stack_pointer, (value >> 8) & 0xff);

    // write the low byte to the stack
    rg.stack_pointer -= 1;
    mmu.get().write_to_bytes(rg.stack_pointer, value & 0xff);
    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

// POP from stack
void CPU :: POP_stack() {
   u8 low_byte = mmu.get().read_from_bytes(rg.stack_pointer);
   rg.stack_pointer += 1;
   u8 high_byte = mmu.get().read_from_bytes(rg.stack_pointer);
   rg.stack_pointer += 1;

   u8 destination = (opcode & 0x30) >> 4; // bits 5-4
    switch(destination) {
          case 0: rg.bc = (static_cast<u16>(high_byte) << 8) | low_byte; break;
          case 1: rg.de = (static_cast<u16>(high_byte) << 8) | low_byte; break;
          case 2: rg.hl = (static_cast<u16>(high_byte) << 8) | low_byte; break;
          case 3: rg.af = (static_cast<u16>(high_byte) << 8) | low_byte; break;
          default: break; // invalid register pair
    }
    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]); 
}

// opcode functions
// NOP
void CPU :: _0x00_NOP() {
    rg.program_counter += 1;
    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

// HALT
void CPU :: HALT() {
    if(IME_flag) {
        halt_flag = true;
    }
    else {
        if(mmu.get().pending_interrupts()) {
            halt_bug = true;
        }
        else{
            halt_flag = true;
        }
    }
    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

// STOP
void CPU :: _0x1000_STOP() {
    u8 dummy_opcode = mmu.get().read_from_bytes(rg.program_counter++);
    halt_flag = true;

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

// LD functions
void CPU :: LD_r16_imm16() {
    u8 low_byte = mmu.get().read_from_bytes(rg.program_counter++);
    u8 high_byte = mmu.get().read_from_bytes(rg.program_counter++);

    u8 destination = (opcode & 0x30) >> 4; // bits 5-4
    switch(destination) {
        case 0: rg.bc = (static_cast<u16>(high_byte) << 8) | low_byte; break;
        case 1: rg.de = (static_cast<u16>(high_byte) << 8) | low_byte; break;
        case 2: rg.hl = (static_cast<u16>(high_byte) << 8) | low_byte; break;
        case 3: rg.stack_pointer = (static_cast<u16>(high_byte) << 8) | low_byte; break;
        default: break; // invalid register pair
    }
    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: LD_r16_a() {
    u8 destination = (opcode & 0x30) >> 4; // bits 5-4
    switch(destination) {
        case 0: mmu.get().write_to_bytes(rg.bc, rg.a); break;
        case 1: mmu.get().write_to_bytes(rg.de, rg.a); break;
        case 2: mmu.get().write_to_bytes(rg.hl, rg.a); break;
        case 3: mmu.get().write_to_bytes(rg.hl, rg.a); break;
        default: break; // invalid register pair
    }

    if(destination == 2) {
        rg.hl += 1; // increment HL after writing to memory
    }
    else if(destination == 3) {
        rg.hl -= 1; // decrement HL after writing to memory
    }
    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: LD_r8_imm8() {
    u8 value = mmu.get().read_from_bytes(rg.program_counter++);
    u8 destination = (opcode & 0x38) >> 3;

    write_register(destination, value);
    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: LD_imm16_sp() {
    u8 low_byte_sp   = rg.stack_pointer & 0xff;
    u8 high_byte_sp  = (rg.stack_pointer >> 8) & 0xff;

    u8 low_byte_pc   = mmu.get().read_from_bytes(rg.program_counter++);
    u8 high_byte_pc  = mmu.get().read_from_bytes(rg.program_counter++);
    u16 destination_address = (high_byte_pc << 8) | low_byte_pc;

    mmu.get().write_to_bytes(destination_address, low_byte_sp);
    mmu.get().write_to_bytes(destination_address + 1, high_byte_sp);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: LD_a_r16() {
    u8 source = (opcode & 0x30) >> 4; // bits 5-4
    u16 address = 0;

    switch(source) {
        case 0: address = rg.bc; break;
        case 1: address = rg.de; break;
        case 2: address = rg.hl; break;
        case 3: address = rg.hl; break;
        default: break; // invalid register pair
    }

    u8 value = mmu.get().read_from_bytes(address);
    rg.a = value;

    if(source == 2) {
        rg.hl += 1; // increment HL after reading from memory
    }
    else if(source == 3) {
        rg.hl -= 1; // decrement HL after reading from memory
    }
    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: LD_r8_r8() {
    u8 source_value = read_register(opcode & 0x07);
    u8 destination  = (opcode & 0x38) >> 3;

    write_register(destination, source_value); 

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]); 
}

void CPU :: LD_imm8_a() {
    u8 operand = mmu.get().read_from_bytes(rg.program_counter++);

    u16 destination_address = 0xff00 | operand;
    mmu.get().write_to_bytes(destination_address, rg.a);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: LD_c_a() {
    u16 destination_address = 0xff00 | rg.c;
    mmu.get().write_to_bytes(destination_address, rg.a);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: LD_a_c() {
    u16 source = 0xff00 | rg.c;
    rg.a = mmu.get().read_from_bytes(source);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: LD_imm16_a() {
    u8 low_byte  = mmu.get().read_from_bytes(rg.program_counter++);
    u8 high_byte = mmu.get().read_from_bytes(rg.program_counter++);

    u16 destination_address = (high_byte << 8) | low_byte;
    mmu.get().write_to_bytes(destination_address, rg.a);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: LD_a_imm8() {
    u8 value = mmu.get().read_from_bytes(rg.program_counter++);
    u16 source_address = 0xff00 | value;

    rg.a = mmu.get().read_from_bytes(source_address);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: LD_HL_SP_imm8() {
    u8 value = mmu.get().read_from_bytes(rg.program_counter++);
    int8_t signed_value = static_cast<int8_t>(value); 

    u16 result = rg.stack_pointer + signed_value;
    mmu.get().write_to_bytes(rg.hl , result);

    // check if lower byte overflowed to upper byte(half carry)
    bool check_half_carry = ((rg.stack_pointer & 0x0f) + (value & 0x0f)) > 0x0f;
    // check if the upper byte overflowed(carry)
    bool check_carry = ((rg.stack_pointer & 0xff) + (value & 0xff)) > 0xff;

    // set all flags
    rg.set_flag(rg.Zero_flag, false);
    rg.set_flag(rg.Subtract_flag, false);
    rg.set_flag(rg.HalfCarry_flag, check_half_carry);
    rg.set_flag(rg.Carry_flag, check_carry);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: LD_SP_HL() {
    u16 source  = mmu.get().read_from_bytes(rg.hl);
    rg.stack_pointer = source;

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: LD_a_imm16() {
    u8 low_byte  = mmu.get().read_from_bytes(rg.program_counter++);
    u8 high_byte = mmu.get().read_from_bytes(rg.program_counter++);

    u16 source_address = (high_byte << 8) | low_byte;
    rg.a = mmu.get().read_from_bytes(source_address);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

// increment functions
void CPU :: INC_r16() {
    u8 operand = (opcode & 0x30) >> 4; // bits 5-4
    switch(operand) {
        case 0: rg.bc += 1; break;
        case 1: rg.de += 1; break;
        case 2: rg.hl += 1; break;
        case 3: rg.stack_pointer += 1; break;
        default: break; // invalid register pair
    }

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: INC_r8() {
    // check if half carry will occur after addtion operation
    bool check_half_carry = (opcode & 0x0f) == 0x0f;

    u8 operand = (opcode & 0x38) >> 3; // bits 5-3
    u8 value   = 0;
    switch(operand) {
        case 0: rg.b += 1; value = rg.b; break;
        case 1: rg.d += 1; value = rg.d; break;
        case 2: rg.h += 1; value = rg.h; break;
        default: break; // invalid register pair
    }

    if(operand == 3) {
        value = mmu.get().read_from_bytes(rg.hl);
        value += 1;
        mmu.get().write_to_bytes(rg.hl, value);
    }

    rg.set_flag(rg.Zero_flag, (value == 0));
    rg.set_flag(rg.Subtract_flag, false);
    rg.set_flag(rg.HalfCarry_flag, check_half_carry);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}    

// decrement functions
void CPU :: DEC_r16() {
    u8 operand = (opcode & 0x30) >> 4; // bits 5-4
    switch(operand) {
        case 0: rg.bc -= 1; break;
        case 1: rg.de -= 1; break;
        case 2: rg.hl -= 1; break;
        case 3: rg.stack_pointer -= 1; break;
        default: break; // invalid register pair
    }

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: DEC_r8() {
    // check if half carry will occur after addtion operation
    bool check_half_carry = (opcode & 0x0f) == 0x00;

    u8 operand = (opcode & 0x38) >> 3; // bits 5-3
    u8 value   = 0;
    switch(operand) {
        case 0: rg.b -= 1; value = rg.b; break;
        case 1: rg.d -= 1; value = rg.d; break;
        case 2: rg.h -= 1; value = rg.h; break;
        default: break; // invalid register pair
    }

    if(operand == 3) {
        value = mmu.get().read_from_bytes(rg.hl);
        value -= 1;
        mmu.get().write_to_bytes(rg.hl, value);
    }

    rg.set_flag(rg.Zero_flag, (value == 0));
    rg.set_flag(rg.Subtract_flag, true);
    rg.set_flag(rg.HalfCarry_flag, check_half_carry);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

// rotate function
void CPU :: RLCA() {
    u8 value = rg.a;
    u8 bit_7 = (value & 0x80) >> 7;
    rg.a     = (value << 1) | bit_7;

    // clear f register and set carry flag only if the 7th bit is set
    rg.f = 0;
    if(bit_7) {
       rg.set_flag(rg.Carry_flag, true); 
    }

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: RLA() {
    u8 value = rg.a;
    u8 bit_7 = (rg.a & 0x80) >> 7;
    u8 carry_bit = (rg.f & 0x10) >> 4;
    rg.a = (value << 1) | carry_bit;

    // clear f register and set carry flag only if the 7th bit is set
    rg.f = 0;
    if(bit_7) {
      rg.set_flag(rg.Carry_flag, true);   
    }   

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: RRCA() {
    u8 value = rg.a;
    u8 bit_0 = (value & 0x01);
    value    = (value >> 1) | (bit_0 << 7);
    rg.a     = value;

    // clear f register and set carry flag only if the 0th bit is set
    rg.f = 0;
    if(bit_0) {
       rg.set_flag(rg.Carry_flag, true); 
    }   

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: RRA() {
    u8 value     = rg.a;
    u8 bit_0     = (rg.a & 0x01);
    u8 carry_bit = (rg.f & 0x10) << 3; 
    rg.a = (value >> 1) | carry_bit;

    // clear f register and set carry flag only if the 0th bit is set
    rg.f = 0;
    if(bit_0) {
       rg.set_flag(rg.Carry_flag, true); 
    }    

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

// flip all bits
void CPU :: CPL() {
    rg.a = ~rg.a;

    rg.set_flag(rg.Subtract_flag, true);
    rg.set_flag(rg.HalfCarry_flag, true);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: CCF() {
    bool check_carry_flag = (rg.f & rg.Carry_flag) == 1;

    if(check_carry_flag) {
        rg.set_flag(rg.Carry_flag, false);
    }

    rg.set_flag(rg.Subtract_flag, false);
    rg.set_flag(rg.HalfCarry_flag, false);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);  
}

// adjust the A(accumulator) register
void CPU :: DAA() { // comeback here
    u16 value      = rg.a;
    bool set_carry = false;

    // BCD calculation for addtion and subtraction
    if(!rg.get_Subtract_flag()) {
        // lower nibble
        if(rg.get_HalfCarry_flag() || (rg.a & 0x0f) > 0x09) {
            value += 0x06;
        }
        // upper nibble
        if(rg.get_Carry_flag() || rg.a > 0x99) {
            value += 0x60;
            set_carry = true;
        }
    } 
    else {
        // lower nibble
        if(rg.get_HalfCarry_flag()) {
            value -= 0x06;
        }
        if(rg.get_Carry_flag()) {
            value -= 0x60;
            set_carry = true;
        }
    }

    rg.a = static_cast<u8>(value);

    // set flags
    if(rg.a == 0 ) {rg.set_flag(rg.Zero_flag, true);}
    if(set_carry)  {rg.set_flag(rg.Carry_flag, true);}

    // only keep the subtract flag
    rg.f &= rg.Subtract_flag;

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

// set carry flag
void CPU :: SCF() {
    rg.set_flag(rg.Carry_flag, true);

    rg.set_flag(rg.Subtract_flag, false);
    rg.set_flag(rg.HalfCarry_flag, false);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

// add functions
void CPU :: ADD_a_r8() {
    u8 operand = read_register(opcode & 0x07);
    bool check_half_carry = false;
    bool check_carry = false;
    u16 result = 0;

    switch(operand) {
        case 0: {
            check_half_carry = ((rg.a & 0x0f) + (rg.b & 0x0f)) > 0x0f;
            result = static_cast<u16>(rg.a) + static_cast<u16>(rg.b);
            check_carry = result > 0xff;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 1: {
            check_half_carry = ((rg.a & 0x0f) + (rg.c & 0x0f)) > 0x0f;
            result = static_cast<u16>(rg.a) + static_cast<u16>(rg.c);
            check_carry = result > 0xff;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 2: {
            check_half_carry = ((rg.a & 0x0f) + (rg.d & 0x0f)) > 0x0f;
            result = static_cast<u16>(rg.a) + static_cast<u16>(rg.d);
            check_carry = result > 0xff;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 3: {
            check_half_carry = ((rg.a & 0x0f) + (rg.e & 0x0f)) > 0x0f;
            result = static_cast<u16>(rg.a) + static_cast<u16>(rg.e);
            check_carry = result > 0xff;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 4: {
            check_half_carry = ((rg.a & 0x0f) + (rg.h & 0x0f)) > 0x0f;
            result = static_cast<u16>(rg.a) + static_cast<u16>(rg.h);
            check_carry = result > 0xff;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 5: {
            check_half_carry = ((rg.a & 0x0f) + (rg.l & 0x0f)) > 0x0f;
            result = static_cast<u16>(rg.a) + static_cast<u16>(rg.l);
            check_carry = result > 0xff;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 6: {
            u8 value = mmu.get().read_from_bytes(rg.hl);
            check_half_carry = ((rg.a & 0x0f) + (value & 0x0f)) > 0x0f;
            result = static_cast<u16>(rg.a) + static_cast<u16>(value);
            check_carry = result > 0xff;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 7: {
            check_half_carry = ((rg.a & 0x0f) + (rg.a & 0x0f)) > 0x0f;
            result = static_cast<u16>(rg.a) + static_cast<u16>(rg.a);
            check_carry = result > 0xff;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        default: break; // invalid register
    }

    rg.set_flag(rg.Zero_flag, (rg.a == 0));
    rg.set_flag(rg.Subtract_flag, false);
    rg.set_flag(rg.HalfCarry_flag, check_half_carry);
    rg.set_flag(rg.Carry_flag, check_carry);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: ADC_a_r8() {
    u8 operand = read_register(opcode & 0x07);
    bool check_half_carry = false;
    bool check_carry = false;
    u8 carry_bit = (rg.f & 0x10) >> 4;
    u16 result = 0;

    switch(operand) {
        case 0: {
            check_half_carry = ((rg.a & 0x0f) + (rg.b & 0x0f) + carry_bit) > 0x0f;
            result = static_cast<u16>(rg.a) + static_cast<u16>(rg.b) + static_cast<u16>(carry_bit);
            check_carry = result > 0xff;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 1: {
            check_half_carry = ((rg.a & 0x0f) + (rg.c & 0x0f) + carry_bit) > 0x0f;
            result = static_cast<u16>(rg.a) + static_cast<u16>(rg.c) + static_cast<u16>(carry_bit);
            check_carry = result > 0xff;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 2: {
            check_half_carry = ((rg.a & 0x0f) + (rg.d & 0x0f) + carry_bit) > 0x0f;
            result = static_cast<u16>(rg.a) + static_cast<u16>(rg.d) + static_cast<u16>(carry_bit);
            check_carry = result > 0xff;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 3: {
            check_half_carry = ((rg.a & 0x0f) + (rg.e & 0x0f) + carry_bit) > 0x0f;
            result = static_cast<u16>(rg.a) + static_cast<u16>(rg.e) + static_cast<u16>(carry_bit);
            check_carry = result > 0xff;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 4: {
            check_half_carry = ((rg.a & 0x0f) + (rg.h & 0x0f) + carry_bit) > 0x0f;
            result = static_cast<u16>(rg.a) + static_cast<u16>(rg.h) + static_cast<u16>(carry_bit);
            check_carry = result > 0xff;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 5: {
            check_half_carry = ((rg.a & 0x0f) + (rg.l & 0x0f) + carry_bit) > 0x0f;
            result = static_cast<u16>(rg.a) + static_cast<u16>(rg.l) + static_cast<u16>(carry_bit);
            check_carry = result > 0xff;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 6: {
            u8 value = mmu.get().read_from_bytes(rg.hl);
            check_half_carry = ((rg.a & 0x0f) + (value & 0x0f) + carry_bit) > 0x0f;
            result = static_cast<u16>(rg.a) + static_cast<u16>(value) + static_cast<u16>(carry_bit);
            check_carry = result > 0xff;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 7: {
            check_half_carry = ((rg.a & 0x0f) + (rg.a & 0x0f) + carry_bit) > 0x0f;
            result = static_cast<u16>(rg.a) + static_cast<u16>(rg.a) + static_cast<u16>(carry_bit);
            check_carry = result > 0xff;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        default: break; // invalid register
    }

    rg.set_flag(rg.Zero_flag, (rg.a == 0));
    rg.set_flag(rg.Subtract_flag, false);
    rg.set_flag(rg.HalfCarry_flag, check_half_carry);
    rg.set_flag(rg.Carry_flag, check_carry);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: ADD_a_imm8() {
    u8 value   = mmu.get().read_from_bytes(rg.program_counter++);
    bool check_half_carry = ((rg.a & 0x0f) + (value & 0x0f)) > 0x0f;
    u16 result = static_cast<u16>(rg.a) + static_cast<u16>(value);
    bool check_carry = result > 0xff;
    rg.a = static_cast<u8>(result & 0xff);

    rg.set_flag(rg.Zero_flag, (rg.a == 0));
    rg.set_flag(rg.Subtract_flag, false);
    rg.set_flag(rg.HalfCarry_flag, check_half_carry);
    rg.set_flag(rg.Carry_flag, check_carry);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: ADC_a_imm8() {
    u8 value = mmu.get().read_from_bytes(rg.program_counter++);
    u8 carry_bit = (rg.f & 0x10) >> 4;
    bool check_half_carry = ((rg.a & 0x0f) + (value & 0x0f) + carry_bit) > 0x0f;
    u16 result = static_cast<u16>(rg.a) + static_cast<u16>(value) + static_cast<u16>(carry_bit);
    bool check_carry = result > 0xff;
    rg.a = static_cast<u8>(result & 0xff);

    rg.set_flag(rg.Zero_flag, (rg.a == 0));
    rg.set_flag(rg.Subtract_flag, false);
    rg.set_flag(rg.HalfCarry_flag, check_half_carry);
    rg.set_flag(rg.Carry_flag, check_carry);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: ADD_HL_r16() {
    u8 operand = (opcode & 0x30) >> 4; // bits 5-4
    bool check_half_carry = false;
    bool check_carry = false;
    u32 result = 0;

    switch(operand) {
        case 0: {
            check_half_carry = ((rg.hl & 0x0fff) + (rg.bc & 0x0fff)) > 0x0fff;
            result = static_cast<u32>(rg.hl) + static_cast<u32>(rg.bc);
            check_carry = result > 0xffff;
            rg.hl = static_cast<u16>(result & 0xffff);
            break;
        }
        case 1: {
            check_half_carry = ((rg.hl & 0x0fff) + (rg.de & 0x0fff)) > 0x0fff;
            result = static_cast<u32>(rg.hl) + static_cast<u32>(rg.de);
            check_carry = result > 0xffff;
            rg.hl = static_cast<u16>(result & 0xffff);
            break;
        }
        case 2: {
            check_half_carry = ((rg.hl & 0x0fff) + (rg.hl & 0x0fff)) > 0x0fff;
            result = static_cast<u32>(rg.hl) + static_cast<u32>(rg.hl);
            check_carry = result > 0xffff;
            rg.hl = static_cast<u16>(result & 0xffff);
            break;
        }
        case 3: {
            check_half_carry = ((rg.hl & 0x0fff) + (rg.stack_pointer & 0x0fff)) > 0x0fff;
            result = static_cast<u32>(rg.hl) + static_cast<u32>(rg.stack_pointer);
            check_carry = result > 0xffff;
            rg.hl = static_cast<u16>(result & 0xffff);
            break;
        }
        default: break; // invalid register pair
    }

    rg.set_flag(rg.Subtract_flag, false);
    rg.set_flag(rg.HalfCarry_flag, check_half_carry);
    rg.set_flag(rg.Carry_flag, check_carry);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: ADD_SP_imm8() { 
    u8 value = mmu.get().read_from_bytes(rg.program_counter++);
    int8_t value_signed = static_cast<int8_t>(value);

    rg.stack_pointer += value_signed;

    // check if lower byte overflowed to upper byte(half carry)
    bool check_half_carry = ((rg.stack_pointer & 0x0f) + (value & 0x0f)) > 0x0f;
    // check if the upper byte overflowed(carry)
    bool check_carry = ((rg.stack_pointer & 0xff) + (value & 0xff)) > 0xff;

    // set all flags
    rg.set_flag(rg.Zero_flag, false);
    rg.set_flag(rg.Subtract_flag, false);
    rg.set_flag(rg.HalfCarry_flag, check_half_carry);
    rg.set_flag(rg.Carry_flag, check_carry);    

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]); 
}

// subtract functions
void CPU :: SUB_a_r8() {
    u8 operand = read_register(opcode & 0x07);
    bool check_half_carry = false;
    bool check_carry = false;
    u16 result = 0;

    switch(operand) {
        case 0: {
            check_half_carry = (rg.a & 0x0f) < (rg.b & 0x0f);
            check_carry = rg.a < rg.b;
            result = rg.a - rg.b;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 1: {
            check_half_carry = (rg.a & 0x0f) < (rg.c & 0x0f);
            check_carry = rg.a < rg.c;
            result = rg.a - rg.c;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 2: {
            check_half_carry = (rg.a & 0x0f) < (rg.d & 0x0f);
            check_carry = rg.a < rg.d;
            result = rg.a - rg.d;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 3: {
            check_half_carry = (rg.a & 0x0f) < (rg.e & 0x0f);
            check_carry = rg.a < rg.e;
            result = rg.a - rg.e;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 4: {
            check_half_carry = (rg.a & 0x0f) < (rg.h & 0x0f);
            check_carry = rg.a < rg.h;
            result = rg.a - rg.h;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 5: {
            check_half_carry = (rg.a & 0x0f) < (rg.l & 0x0f);
            check_carry = rg.a < rg.l;
            result = rg.a - rg.l;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 6: {
            u8 value = mmu.get().read_from_bytes(rg.hl);
            check_half_carry = (rg.a & 0x0f) < (value & 0x0f);
            check_carry = rg.a < value;
            result = rg.a - value;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 7: {
            rg.a = 0x00;
            break;
        }
        default: break; // invalid register
    }

    rg.set_flag(rg.Zero_flag, (rg.a == 0));
    rg.set_flag(rg.Subtract_flag, true);
    rg.set_flag(rg.HalfCarry_flag, check_half_carry);
    rg.set_flag(rg.Carry_flag, check_carry);

   clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: SBC_a_r8() {
    u8 operand = read_register(opcode & 0x07);
    bool check_half_carry = false;
    bool check_carry = 0;
    u8 carry_bit = (rg.f & 0x10) >> 4;
    int result = 0;

    switch(operand) {
        case 0: {
            check_half_carry = (rg.a & 0x0f) < ((rg.b & 0x0f) + carry_bit);
            check_carry = rg.a < (rg.b + carry_bit);
            result = rg.a - rg.b - carry_bit;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 1: {
            check_half_carry = (rg.a & 0x0f) < ((rg.c & 0x0f) + carry_bit);
            check_carry = rg.a < (rg.c + carry_bit);
            result = rg.a - rg.c - carry_bit;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 2: {
            check_half_carry = (rg.a & 0x0f) < ((rg.d & 0x0f) + carry_bit);
            check_carry = rg.a < (rg.d + carry_bit);
            result = rg.a - rg.d - carry_bit;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 3: {
            check_half_carry = (rg.a & 0x0f) < ((rg.e & 0x0f) + carry_bit);
            check_carry = rg.a < (rg.e + carry_bit);
            result = rg.a - rg.e - carry_bit;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 4: {
            check_half_carry = (rg.a & 0x0f) < ((rg.h & 0x0f) + carry_bit);
            check_carry = rg.a < (rg.h + carry_bit);
            result = rg.a - rg.h - carry_bit;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 5: {
            check_half_carry = (rg.a & 0x0f) < ((rg.l & 0x0f) + carry_bit);
            check_carry = rg.a < (rg.l + carry_bit);
            result = rg.a - rg.l - carry_bit;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 6: {
            u8 value = mmu.get().read_from_bytes(rg.hl);
            check_half_carry = (rg.a & 0x0f) < ((value & 0x0f) + carry_bit);
            check_carry = rg.a < (value + carry_bit);
            result = rg.a - value - carry_bit;
            rg.a = static_cast<u8>(result & 0xff);
            break;
        }
        case 7: {
            rg.a = static_cast<u8>((0x00 - carry_bit) & 0xff); // here
            if(carry_bit == 1) {
                check_half_carry = true;
                check_carry = true;
            }
            break;
        }
        default: break; // invalid register
    }

    rg.set_flag(rg.Zero_flag, (rg.a == 0));
    rg.set_flag(rg.Subtract_flag, true);
    rg.set_flag(rg.HalfCarry_flag, check_half_carry);
    rg.set_flag(rg.Carry_flag, check_carry);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: SUB_a_imm8() {
    u8 value = mmu.get().read_from_bytes(rg.program_counter++);
    bool check_half_carry = (rg.a & 0x0f) < (value & 0x0f);
    bool check_carry = rg.a < value;
    int result = rg.a - value;
    rg.a = static_cast<u8>(result & 0xff);

    rg.set_flag(rg.Zero_flag, (rg.a == 0));
    rg.set_flag(rg.Subtract_flag, true);
    rg.set_flag(rg.HalfCarry_flag, check_half_carry);
    rg.set_flag(rg.Carry_flag, check_carry);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: SBC_a_imm8() {
    u8 value = mmu.get().read_from_bytes(rg.program_counter++);
    bool check_half_carry = (rg.a & 0x0f) < ((rg.b & 0x0f) + ((rg.f & 0x10) >> 4));
    bool check_carry = rg.a < (rg.b + ((rg.f & 0x10) >> 4));
    int result = rg.a - value - ((rg.f & 0x10) >> 4);
    rg.a = static_cast<u8>(result & 0xff); 

    rg.set_flag(rg.Zero_flag, (rg.a == 0));
    rg.set_flag(rg.Subtract_flag, true);
    rg.set_flag(rg.HalfCarry_flag, check_half_carry);
    rg.set_flag(rg.Carry_flag, check_carry);
    
    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

// AND, OR, XOR, CP functions
void CPU :: AND_a_r8() {
    u8 operand = read_register(opcode & 0x07);
    switch(operand) {
        case 0: rg.a &= rg.b; break;
        case 1: rg.a &= rg.c; break;
        case 2: rg.a &= rg.d; break;
        case 3: rg.a &= rg.e; break;
        case 4: rg.a &= rg.h; break;
        case 5: rg.a &= rg.l; break;
        case 6: {
            u8 value = mmu.get().read_from_bytes(rg.hl);
            rg.a &= value;
            break;
        }
        case 7: rg.a &= rg.a; break;
        default: break; // invalid register
    }

    rg.set_flag(rg.Zero_flag, (rg.a == 0));
    rg.set_flag(rg.Subtract_flag, false);
    rg.set_flag(rg.HalfCarry_flag, true);
    rg.set_flag(rg.Carry_flag, false);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: AND_a_imm8() {
    u8 value = mmu.get().read_from_bytes(rg.program_counter++);
    rg.a &= value;

    rg.set_flag(rg.Zero_flag, (rg.a == 0));
    rg.set_flag(rg.Subtract_flag, false);
    rg.set_flag(rg.HalfCarry_flag, true);
    rg.set_flag(rg.Carry_flag, false);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: OR_a_r8() {
    u8 source_value = read_register(opcode & 0x07);
    switch(source_value) {
        case 0: rg.a |= rg.b; break;
        case 1: rg.a |= rg.c; break;
        case 2: rg.a |= rg.d; break;
        case 3: rg.a |= rg.e; break;
        case 4: rg.a |= rg.h; break;
        case 5: rg.a |= rg.l; break;
        case 6: {
            u8 value = mmu.get().read_from_bytes(rg.hl);
            rg.a |= value;
            break;
        }
        case 7: rg.a |= rg.a; break;
        default: break; // invalid register
    }

    rg.set_flag(rg.Zero_flag, (rg.a == 0));
    rg.set_flag(rg.Subtract_flag, false);
    rg.set_flag(rg.HalfCarry_flag, false);
    rg.set_flag(rg.Carry_flag, false);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: OR_a_imm8() {
    u8 value = mmu.get().read_from_bytes(rg.program_counter++);
    rg.a |= value;

    rg.set_flag(rg.Zero_flag, (rg.a == 0));
    rg.set_flag(rg.Subtract_flag, false);
    rg.set_flag(rg.HalfCarry_flag, false);
    rg.set_flag(rg.Carry_flag, false);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: XOR_a_r8() {
    u8 source_value = read_register(opcode & 0x07);
    switch(source_value) {
        case 0: rg.a ^= rg.b; break;
        case 1: rg.a ^= rg.c; break;
        case 2: rg.a ^= rg.d; break;
        case 3: rg.a ^= rg.e; break;
        case 4: rg.a ^= rg.h; break;
        case 5: rg.a ^= rg.l; break;
        case 6: {
            u8 value = mmu.get().read_from_bytes(rg.hl);
            rg.a ^= value;
            break;
        }
        case 7: rg.a ^= rg.a; break;
        default: break; // invalid register
    }

    rg.set_flag(rg.Zero_flag, (rg.a == 0));
    rg.set_flag(rg.Subtract_flag, false);
    rg.set_flag(rg.HalfCarry_flag, false);
    rg.set_flag(rg.Carry_flag, false);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: XOR_a_imm8() {
    u8 value = mmu.get().read_from_bytes(rg.program_counter++);
    rg.a ^= value;

    rg.set_flag(rg.Zero_flag, (rg.a == 0));
    rg.set_flag(rg.Subtract_flag, false);
    rg.set_flag(rg.HalfCarry_flag, false);
    rg.set_flag(rg.Carry_flag, false);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: CP_a_r8() { 
    u8 operand = read_register(opcode & 0x07);
    bool check_half_carry = false;
    bool check_carry = false;
    int result = 0;

    switch(operand) {
        case 0: {
            result = rg.a - rg.b;
            check_half_carry = (rg.a & 0x0f) < (rg.b & 0x0f);
            check_carry = rg.a < rg.b; 
            break;
        }
        case 1: {
            result = rg.a - rg.c;
            check_half_carry = (rg.a & 0x0f) < (rg.c & 0x0f);
            check_carry = rg.a < rg.c; 
            break;
        }
        case 2: {
            result = rg.a - rg.d;
            check_half_carry = (rg.a & 0x0f) < (rg.d & 0x0f);
            check_carry = rg.a < rg.d; 
            break;
        }
        case 3: {
            result = rg.a - rg.e;
            check_half_carry = (rg.a & 0x0f) < (rg.e & 0x0f);
            check_carry = rg.a < rg.e; 
            break;
        }
        case 4: {
            result = rg.a - rg.h;
            check_half_carry = (rg.a & 0x0f) < (rg.h & 0x0f);
            check_carry = rg.a < rg.h; 
            break;
        }
        case 5: {
            result = rg.a - rg.l;
            check_half_carry = (rg.a & 0x0f) < (rg.l & 0x0f);
            check_carry = rg.a < rg.l; 
            break;
        }
        case 6: {
            u8 value = mmu.get().read_from_bytes(rg.hl);
            result = rg.a - value;
            check_half_carry = (rg.a & 0x0f) < (value & 0x0f);
            check_carry = rg.a < value; 
            break;
        }
        case 7: {
            break;
        }
        default: break; // invalid register
    }

    rg.set_flag(rg.Zero_flag, ((result & 0xff) == 0));
    rg.set_flag(rg.Subtract_flag, true);
    rg.set_flag(rg.HalfCarry_flag, check_half_carry);
    rg.set_flag(rg.Carry_flag, check_carry);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: CP_imm8() { 
    u8 value = mmu.get().read_from_bytes(rg.program_counter++);
    int result = rg.a - value;
    bool check_half_carry = (rg.a & 0x0f) < (value & 0x0f);
    bool check_carry = rg.a < value;

    rg.set_flag(rg.Zero_flag, ((result & 0xff) == 0));
    rg.set_flag(rg.Subtract_flag, true);
    rg.set_flag(rg.HalfCarry_flag, check_half_carry);
    rg.set_flag(rg.Carry_flag, check_carry);

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

// RET functions
void CPU :: RET_cond() {
    if(!rg.get_Zero_flag() || rg.get_Zero_flag() || !rg.get_Carry_flag() || rg.get_Carry_flag()) {
        u8 low_byte  = mmu.get().read_from_bytes(rg.stack_pointer++);
        u8 high_byte = mmu.get().read_from_bytes(rg.stack_pointer++);

        rg.program_counter = (static_cast<u16>(high_byte) << 8) | low_byte;
        clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
    }
    else{
        clock.get().cycle_tick(8);
    }
}   

void CPU :: RET() {
    u8 low_byte  = mmu.get().read_from_bytes(rg.stack_pointer++);
    u8 high_byte = mmu.get().read_from_bytes(rg.stack_pointer++);

    rg.program_counter = (static_cast<u16>(high_byte) << 8) | low_byte;
    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: RETI() { // return from interrupt
    u8 low_byte  = mmu.get().read_from_bytes(rg.stack_pointer++);
    u8 high_byte = mmu.get().read_from_bytes(rg.stack_pointer++);

    rg.program_counter = (static_cast<u16>(high_byte) << 8) | low_byte;

    IME_flag = true;
    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

// JUMP functions
void CPU :: JR_cond_imm8() {
    if(!rg.get_Zero_flag() || rg.get_Zero_flag() || !rg.get_Carry_flag() || rg.get_Carry_flag()) {
        u8 offset = static_cast<u8>(mmu.get().read_from_bytes(rg.program_counter++));
        rg.program_counter += offset;
        clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
    }
    else {
        clock.get().cycle_tick(8);
    }
}

void CPU :: JR_imm8() {
    u8 offset = static_cast<u8>(mmu.get().read_from_bytes(rg.program_counter++));

    rg.program_counter += offset;
    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: JP_cond_imm16() {
    if(!rg.get_Zero_flag() || rg.get_Zero_flag() || !rg.get_Carry_flag() || rg.get_Carry_flag()) {
        u8 low_byte  = mmu.get().read_from_bytes(rg.program_counter++);
        u8 high_byte = mmu.get().read_from_bytes(rg.program_counter++);

        rg.program_counter = (static_cast<u16>(high_byte) << 8) | low_byte;
        clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
    }
    else {
        clock.get().cycle_tick(12);
    }
}

void CPU :: JP_imm16() {
    u8 low_byte  = mmu.get().read_from_bytes(rg.program_counter++);
    u8 high_byte = mmu.get().read_from_bytes(rg.program_counter++);

    rg.program_counter = (static_cast<u16>(high_byte) << 8) | low_byte;
    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

void CPU :: JP_hl() { 
    u8 low_byte  = read_register(rg.l);
    u8 high_byte = read_register(rg.h);

    rg.program_counter = (static_cast<u16>(high_byte) << 8) | low_byte;
    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

// CALL functions
void CPU :: CALL_cond_i16() {
    if(!rg.get_Zero_flag() || rg.get_Zero_flag() || !rg.get_Carry_flag() || rg.get_Carry_flag()) {
        u8 low_byte  = mmu.get().read_from_bytes(rg.program_counter++);
        u8 high_byte = mmu.get().read_from_bytes(rg.program_counter++);

        u16 address = (static_cast<u16>(high_byte) << 8) | low_byte;

        // push the current program counter to the stack
        rg.stack_pointer--;
        mmu.get().write_to_bytes(rg.stack_pointer, (rg.program_counter >> 8) & 0xff); // high byte
        rg.stack_pointer--;
        mmu.get().write_to_bytes(rg.stack_pointer + 1, rg.program_counter & 0xff); // low byte

        rg.program_counter = address;
        clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
    }
    else{
        clock.get().cycle_tick(12);
    }
}

void CPU :: CALL_i16() {
    u8 low_byte = mmu.get().read_from_bytes(rg.program_counter++);
    u8 high_byte = mmu.get().read_from_bytes(rg.program_counter++);

    u16 address = (static_cast<u16>(high_byte) << 8) | low_byte;

    // push the current program counter to the stack
    rg.stack_pointer--;
    mmu.get().write_to_bytes(rg.stack_pointer, (rg.program_counter >> 8) & 0xff); // high byte
    rg.stack_pointer--;
    mmu.get().write_to_bytes(rg.stack_pointer, rg.program_counter & 0xff); // low byte

    rg.program_counter = address;
    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

// RST functions
void CPU :: RST() {
    rg.stack_pointer -= 1;
    mmu.get().write_to_bytes(rg.stack_pointer, (rg.program_counter >> 8) & 0xff); // high byte
    rg.stack_pointer -= 1;
    mmu.get().write_to_bytes(rg.stack_pointer, rg.program_counter & 0xff); // low byte

    u8 target = (opcode & 0x38) >> 3; // bits 5-3
    switch(target) {
        case 0: rg.program_counter = 0x0000; break;
        case 1: rg.program_counter = 0x0008; break;
        case 2: rg.program_counter = 0x0010; break;
        case 3: rg.program_counter = 0x0018; break;
        case 4: rg.program_counter = 0x0020; break;
        case 5: rg.program_counter = 0x0028; break;
        case 6: rg.program_counter = 0x0030; break;
        case 7: rg.program_counter = 0x0038; break;
        default: break; // invalid RST address
    }

    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

// DI
void CPU :: DI() {
    IME_enable_pending = 0;
    IME_flag = false;
    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

// EI
void CPU :: EI() {
    IME_enable_pending = 2;
    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

// invalid opcode handler
void CPU :: invalid() {
    std::cerr << opcode << " Error: Invalid opcode encountered" << std::endl;
    clock.get().cycle_tick(nonCB_opcode_cycles[opcode]);
}

/*###################################################### CB instructions ######################################################*/
void CPU :: RLC() {
    u8 source = opcode & 0x07;
    u8 register_val = read_register(source);
    u8 bit_7 = (register_val & 0x80) >> 7; 

    //rotate left carry
    register_val = (register_val << 1) | bit_7;
    write_register(source, register_val);

    rg.f &= ~(rg.Subtract_flag | rg.HalfCarry_flag | rg.Carry_flag);

    if(bit_7) {
        rg.set_flag(rg.Carry_flag, true);
    }

    clock.get().cycle_tick(CB_opcode_cycles[opcode]);
}

void CPU :: RRC() {
    u8 source = opcode & 0x07;
    u8 register_val = read_register(source);
    u8 bit_0 = (register_val & 0x01);
    
    // rotate right carry
    register_val = (register_val >> 1) | (bit_0 << 7);
    write_register(source, register_val); 

    rg.f &= ~(rg.Subtract_flag | rg.HalfCarry_flag | rg.Carry_flag);

    if(bit_0) {
        rg.set_flag(rg.Carry_flag, true);
    }

    clock.get().cycle_tick(CB_opcode_cycles[opcode]);
}

void CPU :: RL() {
    u8 source = opcode & 0x07;
    u8 register_val = read_register(source);
    u8 bit_7 = (register_val & 0x80) >> 7;

    // rotate left
    register_val = (register_val << 1) | (rg.get_Carry_flag() >> 4);
    write_register(source, register_val);

    rg.f &= ~(rg.Subtract_flag | rg.HalfCarry_flag | rg.Carry_flag);

    if(bit_7) {
        rg.set_flag(rg.Carry_flag, true);
    }

    clock.get().cycle_tick(CB_opcode_cycles[opcode]);
}

void CPU :: RR() {
    u8 source = opcode & 0x07;
    u8 register_val = read_register(source);
    u8 bit_0 = (register_val & 0x01);

    // rotate right
    register_val = (register_val >> 1) | (rg.get_Carry_flag() << 3);
    write_register(source, register_val);

    rg.f &= ~(rg.Subtract_flag | rg.HalfCarry_flag | rg.Carry_flag);

    if(bit_0) {
        rg.set_flag(rg.Carry_flag, true);
    }

    clock.get().cycle_tick(CB_opcode_cycles[opcode]);
}

void CPU :: SLA() {
    u8 source = opcode & 0x07;
    u8 register_val = read_register(source);
    u8 bit_7 = (register_val & 0x80) >> 7;

    // shift left
    register_val = (register_val << 1);
    write_register(source, register_val);

    rg.f &= ~(rg.Subtract_flag | rg.HalfCarry_flag | rg.Carry_flag);

    if(bit_7) {
        rg.set_flag(rg.Carry_flag, true);
    }

    clock.get().cycle_tick(CB_opcode_cycles[opcode]);
}

void CPU :: SRA() {
    u8 source = opcode & 0x07;
    u8 register_val = read_register(source);
    u8 bit_0 = (register_val & 0x01);

    // shift right
    register_val = (register_val >> 1);
    write_register(source, register_val);

    rg.f &= ~(rg.Subtract_flag | rg.HalfCarry_flag | rg.Carry_flag);

    if(bit_0) {
        rg.set_flag(rg.Carry_flag, true);
    }
    
    clock.get().cycle_tick(CB_opcode_cycles[opcode]);
}

void CPU :: SWAP() {
    u8 source = opcode & 0x07;
    u8 register_val = read_register(source);
    u8 low_bits  = register_val & 0x0f;
    u8 high_bits = (register_val & 0xf0) >> 4;

    register_val = (low_bits << 4) | high_bits; 

    clock.get().cycle_tick(CB_opcode_cycles[opcode]);
}

void CPU :: SRL() {
    u8 source = opcode & 0x07;
    u8 register_val = read_register(source);
    u8 bit_0 = (register_val & 0x01);

    // shift right
    register_val = (register_val >> 1);
    write_register(source, register_val);

    rg.f &= ~(rg.Subtract_flag | rg.HalfCarry_flag | rg.Carry_flag);

    if(bit_0) {
        rg.set_flag(rg.Carry_flag, true);
    }

    clock.get().cycle_tick(CB_opcode_cycles[opcode]);
}

void CPU :: BIT_b3_r8() {
    u8 source = opcode & 0x07;
    u8 register_val = read_register(source);
    u8 bit_idx = (opcode & 0x38) >> 3;

    // isolate bit_idx
    bool bit_is_off = (register_val & bit_idx) == 0;

    rg.set_flag(rg.Zero_flag, bit_is_off);
    rg.set_flag(rg.Subtract_flag, false);
    rg.set_flag(rg.HalfCarry_flag, true);

    clock.get().cycle_tick(CB_opcode_cycles[opcode]);
}

void CPU :: RES_b3_r8() {
    u8 source = opcode & 0x07;
    u8 register_val = read_register(source);
    u8 bit_idx = (opcode & 0x38) >> 3;

    // reset bit_idx
    register_val &= ~(1 << bit_idx);
    write_register(source, register_val);

    clock.get().cycle_tick(CB_opcode_cycles[opcode]);
}

void CPU :: SET_b3_r8() {
    u8 source = opcode & 0x07;
    u8 register_val = read_register(source);
    u8 bit_idx = (opcode & 0x38) >> 3;

    //set bit_idx
    register_val |= (1 < bit_idx);
    write_register(source, register_val);

    clock.get().cycle_tick(CB_opcode_cycles[opcode]);
}