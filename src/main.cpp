#include <bitset>
#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <stack>
#include <stdexcept>
#include <utility>
#include <vector>

#define PROGRAMS_START 0x200
#define INSTRUCTIONS_SPAN 2

struct Chip {
    std::vector<uint8_t> memory;
    std::stack<uint16_t> stack;
    uint16_t registers[15];
    uint16_t PC;

    void dump_into_memory(std::vector<uint8_t>);
    uint16_t next_instruction();
    void eval_instruction(uint16_t);
    void execute();
};

void Chip::dump_into_memory(const std::vector<uint8_t> bytes) {
    if(bytes.size() % INSTRUCTIONS_SPAN != 0 || bytes.size() == 0)
        throw std::runtime_error("malformed byte stream");

    for(size_t index = 0; index < bytes.size(); index++)
        this->memory.push_back(bytes.at(index));

    this->PC = 0;
}

uint16_t Chip::next_instruction() {
    if(PC + 1 >= static_cast<uint16_t>(this->memory.size()))
        return '\0';

    const auto high_byte = this->memory.at(this->PC);
    const auto low_byte = this->memory.at(this->PC + 1);

    this->PC += INSTRUCTIONS_SPAN;
    return (high_byte << 0x8) + low_byte;
}

void Chip::eval_instruction(const uint16_t instruction) {
    const uint16_t kind = (instruction & 0xf000) >> 12;
    const uint16_t target = (instruction & 0x0f00) >> 8;
    const uint16_t value = instruction & 0x00ff;
    const uint16_t next_address = (instruction & 0x0fff) - PROGRAMS_START;

    switch(kind) {
        case 0x0: {
            switch(instruction) {
                case 0x00EE: // RET
                    if(stack.empty()) return;
                    this->PC = stack.top();
                    stack.pop();
                    break;
                default: break; // SYS addr
            }
            break;
        }
        case 0x1: // JP addr
            this->PC = next_address;
            break;
        case 0x2: // CALL addr
            this->stack.push(this->PC);
            this->PC = next_address;
            break;
        case 0x3: // SE Vx, byte
            if(this->registers[target] == value) this->PC++;
            break;
        case 0x6: // LD Vx, byte
            this->registers[target] = value;
            break;
        case 0x7: // ADD Vx, byte
            this->registers[target] += value;
            break;
        default: throw std::runtime_error("not implemented yet\n");
    }
}

void Chip::execute() {
    for(size_t index = 0; index < sizeof(registers) / sizeof(int16_t); index++)
        registers[index] = 0;

    uint16_t instruction;
    while((instruction = this->next_instruction()) != '\0')
        this->eval_instruction(instruction);
}

struct Reader {
    std::ifstream target;
    size_t index = 0;

    Reader(std::string);
    std::vector<uint8_t> extract_bytes();
};

Reader::Reader(const std::string file_path) {
    const auto extension = file_path.substr(file_path.find_last_of('.'));
    if(extension != ".ch8")
        throw std::runtime_error(std::format("file extension not supported: {}\n", extension));

    this->target.open(file_path, std::ios::binary);
    if(!this->target.is_open())
        throw std::runtime_error(std::format("could not open rom: {}\n", file_path));
}

std::vector<uint8_t> Reader::extract_bytes() {
    std::vector<uint8_t> bytes;
    char byte;
    while(this->target.get(byte)) bytes.push_back(static_cast<uint8_t>(byte));
    return bytes;
}

int32_t main(int32_t argc, char* argv[]) {
    if(argc < 2) {
        std::cout << std::format("Usage: {} [ROM]\n", argv[0]);
        exit(1);
    }

    const auto file_path = std::string(argv[1]);
    Reader reader(file_path);
    const auto bytes = reader.extract_bytes();

    Chip machine;
    machine.dump_into_memory(bytes);
    machine.execute();

    return 0;
}
