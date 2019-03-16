#include <sstream>

#include "instruction.h"
#include "decoder.h"
#include "../rf/rf.h"

// simple macro aliases
#define I(name) \
ISA_entry_generated_ ## name

#define F(format) \
Instruction::Format::format

// Contains fields which are generated by macro below
struct ISAEntryGenerated {
    std::string name;
    uint32 match;
    uint32 mask;
    Instruction::Executor function;
};

#define DECLARE_INSN(name, match, mask) \
static const ISAEntryGenerated I(name) = \
{ #name, match, mask, &Instruction::execute_ ## name};
#include "opcodes.gen.h"
#undef DECLARE_INSN

// contains fields which are defined in the table below
struct ISAEntry {
    ISAEntryGenerated generated_entry;
    Instruction::Format format;
    size_t memory_size;

    bool match(uint32 raw) const { 
        return (raw & this->generated_entry.mask) == this->generated_entry.match;
    }
};

// ISA table describing instructions
static const std::vector<ISAEntry> ISA_table = {
//   name        format  memsize
   { I(lui),     F(U),   0        },
   { I(auipc),   F(U),   0        },
   { I(jal),     F(J),   0        },
   { I(jalr),    F(J),   0        },
   { I(beq),     F(B),   0        },
   { I(bne),     F(B),   0        },
   { I(blt),     F(B),   0        },
   { I(bge),     F(B),   0        },
   { I(bltu),    F(B),   0        },
   { I(bgeu),    F(B),   0        },
   { I(lb),      F(I),   1        },
   { I(lh),      F(I),   2        },
   { I(lw),      F(I),   4        },
   { I(lbu),     F(I),   1        },
   { I(lhu),     F(I),   2        },
   { I(lwu),     F(I),   4        },
   { I(sb),      F(S),   1        },
   { I(sh),      F(S),   2        },
   { I(sw),      F(S),   4        },
   { I(addi),    F(I),   0        },
   { I(slti),    F(I),   0        },
   { I(sltiu),   F(I),   0        },
   { I(xori),    F(I),   0        },
   { I(ori),     F(I),   0        },
   { I(andi),    F(I),   0        },
   { I(slli),    F(I),   0        },
   { I(srai),    F(I),   0        },
   { I(srli),    F(I),   0        },
   { I(add),     F(R),   0        },
   { I(sub),     F(R),   0        },
   { I(sll),     F(R),   0        },
   { I(slt),     F(R),   0        },
   { I(sltu),    F(R),   0        },
   { I(xor),     F(R),   0        },
   { I(or),      F(R),   0        },
   { I(and),     F(R),   0        },
   { I(sra),     F(R),   0        },
   { I(srl),     F(I),   0        }  // TODO: check this
};


const ISAEntry find_entry(uint32 raw) {
    for (const auto& x : ISA_table) {
        if (x.match(raw))
            return x;
    }
    assert(0);
}


Instruction::Instruction(uint32 bytes, Addr PC) :
    PC(PC),
    new_PC(PC + 4)
{
    ISAEntry entry = find_entry(bytes);

    this->name  = entry.generated_entry.name;
    this->format = entry.format;
    this->function  = entry.generated_entry.function;
    this->memory_size = entry.memory_size;

    Decoder decoder(bytes, this->format);
    this->rs1   = decoder.get_rs1();
    this->rs2   = decoder.get_rs2();
    this->rd    = decoder.get_rd();
    this->imm_v = decoder.get_immediate();
}


std::string Instruction::get_disasm() const {
    // might pretty-format in future but
    // for debug stage this is perfect
    std::ostringstream oss;
    oss << this->name  << " ";
    oss << this->rs1   << " ";
    oss << this->rs2   << " ";
    oss << this->rd    << " ";
    oss << this->imm_v;

    return oss.str();
}


void Instruction::execute() {
    (this->*function)();
    this->complete = true;
}

