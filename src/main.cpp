#include <bitset>
#include <cstdint>
#include <format>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <vector>

struct Chip {
    std::vector<char> memory;
    int16_t PC;
    int16_t registers[15];
    void dump_into_memory(std::vector<char>);
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

void Chip::eval_instruction(const uint16_t instruction) {
    const auto kind = (instruction & 0xf000) >> 12;
    switch(kind) {
        case 0x6: // LD Vx, byte
        {
            const auto target = (instruction & 0x0f00) >> 8;
            const auto value = instruction & 0x00ff;
            this->registers[target] = value;
            break;
        }
        default: throw std::runtime_error("not implemented yet\n");
    }
}

void Chip::execute() {
    while(this->PC < memory.size()) {
        const auto high_byte = this->memory.at(this->PC);
        const auto low_byte = this->memory.at(this->PC + 1);
        const uint16_t instruction = (high_byte << 0x8) + low_byte;
        this->eval_instruction(instruction);
        this->PC += 2;
    }
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
        return '\0';
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
    while((current_byte = this->next_byte()) != '\0') bytes.push_back(current_byte);
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
