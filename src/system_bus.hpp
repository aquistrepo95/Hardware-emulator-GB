#ifndef SYSTEM_BUS_HPP
#define SYSTEM_BUS_HPP
#include <cstdint>
#include "types.hpp"

class SystemBus {
    public:
        // virtual destructor
        virtual ~SystemBus() = default;

        // this function will return true if the object(IO) is responsible for the memory address
        virtual bool respond_to_operation(u16 address) const  = 0;

        // read or write to IO devices
        virtual u8 read_from_IO(u16 address) = 0;
        virtual void write_to_IO(u16 address, u8 value) = 0;

};


#endif // SYSTEM_BUS_HPP