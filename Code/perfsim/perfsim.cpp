#include "perfsim.hpp"

//#define NOP_BYTES 0x13

PerfSim::PerfSim(std::string executable_filename)
    : loader(executable_filename)
    , memory(loader.load_data(), 2)
{
    // setup stack
    rf.set_stack_pointer(memory.get_stack_pointer());
    rf.validate(Register::Number::s0);
    rf.validate(Register::Number::ra);
    rf.validate(Register::Number::s1);
    rf.validate(Register::Number::s2);
    rf.validate(Register::Number::s3);

    stage_registers.PC.write(new Addr(loader.get_start_PC()));
    stage_registers.PC.clock();
    stage_registers.FETCH_DECODE.clock();
    stage_registers.DECODE_EXE.clock();
    stage_registers.EXE_MEM.clock();
    stage_registers.MEM_WB.clock(); 
}

void PerfSim::step() {
    
    this->writeback_stage();
    this->memory_stage();
    this->execute_stage();
    this->decode_stage();
    this->fetch_stage();

    std::cout << "STALLS:  "
              << wires.PC_stage_reg_stall
              << wires.FD_stage_reg_stall
              << wires.DE_stage_reg_stall
              << wires.EM_stage_reg_stall
              << std::endl;

    rf.dump();
    std::cout << std::string(80, '-') << std::endl << std::endl;

    wires.memory_stage_usage = false;

    if (wires.PC_stage_reg_stall)
        wires.PC_stage_reg_stall = false;
    else
        stage_registers.PC.clock();

    if (wires.FD_stage_reg_stall)
        wires.FD_stage_reg_stall = false;
    else
        stage_registers.FETCH_DECODE.clock();

    if (wires.DE_stage_reg_stall)
        wires.DE_stage_reg_stall = false;
    else
        stage_registers.DECODE_EXE.clock();

    if (wires.EM_stage_reg_stall)
        wires.EM_stage_reg_stall = false;
    else
        stage_registers.EXE_MEM.clock();

    stage_registers.MEM_WB.clock();
    
}

void PerfSim::run(uint32 n) {
    for (uint32 i = 0; i < n; ++i)
        this->step();
}

void PerfSim::fetch_stage() {
    std::cout << "FETCH:  ";
    static uint fetch_stage_iteration = 0;
    static uint32 fetch_bytes = NO_VAL32;

    Addr* data = nullptr;
    data = stage_registers.PC.read();

    if (wires.FD_stage_reg_stall & (stage_registers.FETCH_DECODE.read() != nullptr))
        wires.PC_stage_reg_stall = true;

    // branch mispredctiion handling
    if (wires.memory_to_all_flush) {
        fetch_bytes = 0;
        fetch_stage_iteration = 0;

        Addr* true_PC = new Addr(wires.memory_to_fetch_target);
        stage_registers.PC.write(true_PC);

        stage_registers.FETCH_DECODE.write(nullptr);
        std::cout << "FLUSH" << std::endl;
        if (data != nullptr) delete data;
        return;
    }

    if (data == nullptr) {
        std::cout << "BUBBLE" << std::endl;
        return;
    }

    Addr PC = *data;
    std::cout << "PC: " << PC << std::endl;
    if (wires.memory_stage_usage) {
        wires.PC_stage_reg_stall = true;
        stage_registers.FETCH_DECODE.write(nullptr);
    } else if (fetch_stage_iteration == 0) {
        fetch_bytes = memory.read(PC, 2);
        fetch_stage_iteration = 1;
        wires.PC_stage_reg_stall = true;
        stage_registers.FETCH_DECODE.write(nullptr);
    } else {
        if (!wires.PC_stage_reg_stall) {
            fetch_bytes = (fetch_bytes & 0xffff) | (memory.read(PC+2, 2) << 16);
            Instruction* res = new Instruction(fetch_bytes, PC);
            stage_registers.FETCH_DECODE.write(res);
            fetch_stage_iteration = 0;
            Addr* next_data = new Addr(PC+4);
            stage_registers.PC.write(next_data);
            std::cout << "\t0x" << std::hex << res->get_PC() << ": "
                << res->get_disasm() << " "
                << std::endl;
            delete data;
        } else {
            stage_registers.FETCH_DECODE.write(nullptr);
        } 
    }


    std::cout << "\tfetch_stage_iteration: "
          << fetch_stage_iteration  << std::endl;
}


void PerfSim::decode_stage() {
    std::cout << "DECODE: ";

    Instruction* data = nullptr;
    data = stage_registers.FETCH_DECODE.read();


    if (wires.DE_stage_reg_stall & (data != nullptr))
        wires.FD_stage_reg_stall = true;

    // branch mispredctiion handling
    if (wires.memory_to_all_flush) {
        stage_registers.DECODE_EXE.write(nullptr);
        std::cout << "FLUSH" << std::endl;
        if (data != nullptr) delete data;
        return;
    }
    

    if (data == nullptr) {
        stage_registers.DECODE_EXE.write(nullptr);
        std::cout << "BUBBLE" << std::endl;
        return;
    }

    std::cout << "0x" << std::hex << data->get_PC() << ": "
        << data->get_disasm() << " "
        << std::endl;

    // read RF registers mask
    uint32 decode_stage_regs = \
        (1 << static_cast<uint32>(data->get_rs1()))
      | (1 << static_cast<uint32>(data->get_rs2()));

    uint32 hazards = \
        (decode_stage_regs & wires.execute_stage_regs)
      | (decode_stage_regs & wires.memory_stage_regs);

    if ((hazards >> 1) != 0) {
        wires.FD_stage_reg_stall = true;
        stage_registers.DECODE_EXE.write(nullptr);
    } else {
        this->rf.read_sources(*data);
        stage_registers.DECODE_EXE.write(data);
    }

    std::cout << "\tRegisters read: " << data->get_rs1() << " " \
              << data->get_rs2() << std::endl;
}


void PerfSim::execute_stage() {
    std::cout << "EXE:    ";

    Instruction* data = nullptr;
    data = stage_registers.DECODE_EXE.read();

    wires.execute_stage_regs = 0;

    if (wires.EM_stage_reg_stall & (data != nullptr))
        wires.DE_stage_reg_stall = true;

    // branch mispredctiion handling
    if (wires.memory_to_all_flush) {
        stage_registers.EXE_MEM.write(nullptr);
        std::cout << "FLUSH" << std::endl;
        if (data != nullptr) delete data;
        return;
    }

    if (data == nullptr) {
        stage_registers.EXE_MEM.write(nullptr);
        std::cout << "BUBBLE" << std::endl;
        return;
    }

    // actual execution takes place here
    data->execute();
    wires.execute_stage_regs = (1 << static_cast<uint32>(data->get_rd())); 
    stage_registers.EXE_MEM.write(data);

    std::cout << "0x" << std::hex << data->get_PC() << ": "
                      << data->get_disasm() << " "
                      << std::endl;
}

void PerfSim::memory_stage() {
    std::cout << "MEM:    ";
    static uint memory_stage_iteration = 0;

    Instruction* data = nullptr;
    data = stage_registers.EXE_MEM.read();

    wires.memory_to_all_flush = false;
    wires.memory_to_fetch_target = NO_VAL32;
    wires.memory_stage_regs = 0;

    if (data == nullptr) {
        stage_registers.MEM_WB.write(nullptr);
        std::cout << "BUBBLE" << std::endl;
        return;
    }

    wires.memory_stage_regs = (1 << static_cast<uint32>(data->get_rd())); 

    // jump operations
    if (data->is_jump() | data->is_branch()) {
        if (data->get_new_PC() != data->get_PC() + 4) {
            // target misprediction handling
            wires.memory_to_all_flush = true;
            wires.memory_to_fetch_target = data->get_new_PC();
        }
    }

    // memory operations
    if (data->is_load() | data->is_store()) {
        wires.memory_stage_usage = true;
        if (memory_stage_iteration == 0) {
            if (data->is_load()) {
                uint32 value = memory.read(data->get_memory_addr(), data->get_memory_size()==1 ? 1 : 2);
                data->set_rd_v(value);
            } else {
                memory.write(data->get_rs2_v(), data->get_memory_addr(), data->get_memory_size()==1 ? 1 : 2);
            }
        } else {
            if (data->is_load()) {
                uint32 value = memory.read(data->get_memory_addr()+2, data->get_memory_size()==1 ? 1 : 2);
                data->set_rd_v(data->get_rd_v() | (value << 16));
            } else {
                memory.write((data->get_rs2_v())>>16, data->get_memory_addr()+2, data->get_memory_size()==1 ? 1 : 2);
            }
        }
        if ((data->get_memory_size() == 4) & (memory_stage_iteration == 0)) {
            wires.EM_stage_reg_stall = true;
            memory_stage_iteration = 1;
            stage_registers.MEM_WB.write(nullptr);
        } else {
            memory_stage_iteration = 0;
        }
    }

    // pass data to writeback stage
    stage_registers.MEM_WB.write(data);

    std::cout << "0x" << std::hex << data->get_PC() << ": "
              << data->get_disasm() << " "
              << std::endl;

    if (wires.memory_to_all_flush)
        std::cout << "\tbranch mispredction, flush" << std::endl;

    std::cout << "\tmemory_stage_iteration: " << memory_stage_iteration << std::endl;
    std::cout << "\tregisters written to: " << data->get_rd() << std::endl;
} 


void PerfSim::writeback_stage() {
    std::cout << "WB:     ";
    Instruction* data = nullptr;
    
    data = stage_registers.MEM_WB.read();

    if (data == nullptr) {
        std::cout << "BUBBLE" << std::endl;
        return;
    }

    std::cout << "0x" << std::hex << data->get_PC() << ": "
          << data->get_disasm() << " "
          << std::endl;
    this->rf.writeback(*data);
    delete data;
}
