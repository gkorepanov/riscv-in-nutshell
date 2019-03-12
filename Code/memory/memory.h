#ifndef _MEMORY_H_
#define _MEMORY_H_

#include "../infra/common.h"

class Memory {
private:
    std::vector<uint8> data;
    size_t size;
    Addr start_PC;

    uint32 read(Addr addr, size_t num_bytes) const;
    void write(uint32 value, Addr addr, size_t num_bytes);

    void load(Instruction& instr) const {
        uint32 value = read(instr.get_memory_addr(), instr.get_memory_size());
        instr.set_rd_v(value);
    }

    void store(const Instruction& instr) {
        write(instr.get_rs2_v(), instr.get_memory_addr(), instr.get_memory_size());
    }   
    
public:
    Memory(const std::string& executable_filename);
    Addr get_start_PC() const { return start_PC; }

    void load_store(Instruction& instr) {
        if (instr.is_load())
            this->load(instr);
        else if (instr.is_store())
            this->store(instr);
    }
};

#endif
