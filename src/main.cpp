#include <bitset>
#include <cstdint>
#include <format>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <vector>

#define PROGRAMS_START 0x200

struct Chip {
    std::vector<char> memory;
    int16_t PC;
    int16_t registers[15];

    void dump_into_memory(std::vector<char>);
    uint16_t next_instruction();
    void eval_instruction(uint16_t);
    void execute();
};

void Chip::dump_into_memory(const std::vector<char> bytes) {
    if(bytes.size() % 2 != 0 && bytes.size() > 0)
        throw std::runtime_error("malformed byte stream");

    for(size_t index = 0; index < bytes.size(); index++)
        this->memory.push_back(bytes.at(index));

    this->PC = 0;
}

uint16_t Chip::next_instruction() {
    if(PC + 1 >= this->memory.size()) return '\0';
    const auto high_byte = this->memory.at(this->PC);
    const auto low_byte = this->memory.at(this->PC + 1);
    this->PC += 2;
    return (high_byte << 0x8) + low_byte;
}

void Chip::eval_instruction(const uint16_t instruction) {
    const uint16_t kind = (instruction & 0xf000) >> 12;
    const uint16_t target = (instruction & 0x0f00) >> 8;
    const uint16_t value = instruction & 0x00ff;
    const uint16_t next_address = (instruction & 0x0fff) - PROGRAMS_START;

    switch(kind) {
        case 0x1: // JP addr
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
    char next_byte();
    std::vector<char> extract_bytes();
};

Reader::Reader(const std::string file_path) {
    const auto extension = file_path.substr(file_path.find_last_of('.'));
    if(extension != ".ch8")
        throw std::runtime_error(std::format("file extension not supported: {}\n", extension));

    this->target.open(file_path, std::ios::binary);
    if(!this->target.is_open())
        throw std::runtime_error(std::format("could not open rom: {}\n", file_path));
}

char Reader::next_byte() {
    char byte;
    this->target.get(byte);

    if(this->target.eof())
        return '\x3';
    if(this->target.fail())
        throw std::runtime_error("could not read next byte of rom\n");

    index++;
    this->target.clear();
    this->target.seekg(index, std::ios::beg);
    return byte;
}

std::vector<char> Reader::extract_bytes() {
    std::vector<char> bytes;
    char current_byte;
    while((current_byte = this->next_byte()) != '\x3')
        bytes.push_back(current_byte);
    return bytes;
}

int32_t main() {
    Reader reader("files/test.ch8");
    Chip machine;

    const auto bytes = reader.extract_bytes();
    machine.dump_into_memory(bytes);
    machine.execute();

    return 0;
}
