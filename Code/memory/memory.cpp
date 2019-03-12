#include "memory.h"
#include "../infra/elf/elf.h"

Memory::Memory(const std::string& executable_filename):
    size(MAX_VAL8),
    data(size, 0)
{
    ElfLoader elf_loader(executable_filename);
    elf_loader.load_data(data);

    // TODO: set start_PC correctly
}


Addr Memory::get_start_PC() const {
    return this->start_PC;
}


uint32 read(Addr addr, size_t num_bytes) const {
    assert(num_bytes <= 4);
    assert(num_bytes >= 0);

    uint32 value = 0;
    for (uint i = 0; i < num_bytes; ++i) {
        uint8 byte = this->read_byte(addr + i);
        value |= static_cast<uint32>(byte) << (8*i);
    }

    return value;
}


void write(uint32 value, Addr addr, size_t num_bytes) {
    assert(num_bytes <= 4);
    assert(num_bytes >= 0);

    for (uint i = 0; i < num_bytes; ++i) {
        uint8 byte = static_cast<uint8>(value >> 8*i); 
        this->store_byte(byte, addr + i);
    }
}
