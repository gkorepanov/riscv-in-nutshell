#include "instruction.h"
#include "../rf/rf.h"

// simple macro aliases
#define I(name) \
ISA_entry_generated_ ## name

#define F(format) \
Instruction::Format:: ## format

// Contains fields which are generated by macro below
struct ISAEntryGenerated {
    std::name;
    uint32 match;
    uint32 mask;
    Instruction::Executor function;
};

#define DECLARE_INSN(name, match, mask) \
static const ISAEntryGenerated I(name) = \
{ #name, match, mask, &Instruction::execute_ ## name};
#include <riscv.opcode.gen.h>
#undef DECLARE_INSN

// contains fields which are defined in the table below
struct ISAEntry {
    ISAEntryGenerated generated_entry;
    Format format;
    size_t memory_size;

    bool match(uint32 raw) const { 
        return (raw & this->generated_entry.mask) == this->generated_entry.match;
    }
};

// ISA table describing instructions
static const std::vector<ISAEntry> ISA_table = {
//   name        format  memsize
   { I(lui),     F(U),   0        },
   { I(srai),    F(I),   0        }
}


const ISAEntry find_entry(uint32 raw) {
    for (const auto& x : ISA_table) {
        if (x.match(raw))
            return x;
    }
    assert(0);
}


Instruction::Instruction(uint32 bytes, Addr PC) :
    PC(PC)
{
    ISAEntry entry = find_entry(bytes);

    this->name  = entry.generated_entry.name;
    this->format = entry.format;
    this->executor  = entry.generated_entry.function;
    this->memory_size = entry.memory_size;

    Decoder decoder(bytes, this->format);
    this->rs1   = decoder.get_rs1();
    this->rs2   = decoder.get_rs2();
    this->rd    = decoder.get_rd();
    this->imm_v = decoder.get_immediate();

    this->generate_disasm();
}


void Instruction::generate_disasm() {
    // might pretty-format in future but
    // for debug stage this is perfect
    std::ostringstream oss;
    oss << this->name  << " ";
    oss << this->rs1   << " ";
    oss << this->rs2   << " ";
    oss << this->rd    << " ";
    oss << this->imm_v;
}


void Instruction::execute() {
    (this->*function)();
    this->complete = true;
}

