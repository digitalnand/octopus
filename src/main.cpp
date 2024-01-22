#include <bitset>
#include <cstdint>
#include <format>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <vector>

enum InstructionKind {
    END_OF_FILE,
    LD_BYTE
};

struct Instruction {
    InstructionKind kind;
    int16_t parameter_a = 0;
    int16_t parameter_b = 0;
};

struct Machine {
    int16_t registers[15];
    void eval(Instruction);
};

void Machine::eval(Instruction instruction) {
    switch(instruction.kind) {
        case LD_BYTE: {
            const auto target_register = instruction.parameter_a;
            const auto value = instruction.parameter_b;
            this->registers[target_register] = value;
            break;
        }
        default: std::unreachable();
    }
}

struct Reader {
    std::ifstream target;
    size_t index = 0;

    Reader(std::string);
    std::bitset<8> next_byte();
    Instruction next_instruction();
};

Reader::Reader(std::string file_path) {
    const auto extension = file_path.substr(file_path.find_last_of('.'));
    if(extension != ".ch8")
        throw std::runtime_error(std::format("file extension not supported: {}\n", extension));

    this->target.open(file_path, std::ios::binary);
    if(!this->target.is_open())
        throw std::runtime_error(std::format("file could not be opened: {}\n", file_path));
}

std::bitset<8> Reader::next_byte() {
    char buffer;
    this->target.get(buffer);

    if(this->target.eof())
        return {};
    if(this->target.fail())
        throw std::runtime_error("malformed file\n");

    index++;
    this->target.clear();
    this->target.seekg(index, std::ios::beg);
    return std::bitset<8>(buffer);
}

std::vector<std::bitset<4>> extract_nibbles(std::bitset<8> byte) {
    std::vector<std::bitset<4>> output;
    const auto first_nibble = byte.to_string().substr(0, 4);
    output.push_back(std::bitset<4>(first_nibble));

    const auto second_nibble = byte.to_string().substr(4);
    output.push_back(std::bitset<4>(second_nibble));
    return output;
}

InstructionKind get_instruction_kind(int16_t value) {
    switch(value) {
        case 6: return LD_BYTE;
        default: throw std::runtime_error("not implemented yet");
    }
}

Instruction Reader::next_instruction() {
    const auto high_byte = this->next_byte();
    if(this->target.eof())
        return Instruction{END_OF_FILE};

    const auto high_half = extract_nibbles(high_byte);
    const auto instruction_kind = get_instruction_kind(high_half.front().to_ulong());

    switch(instruction_kind) {
        case LD_BYTE: {
            const int16_t target_register = high_half.back().to_ulong();
            const int16_t value = this->next_byte().to_ulong();
            return Instruction{instruction_kind, target_register, value};
        }
        default: std::unreachable();
    }
}

int32_t main() {
    Reader reader("files/test.ch8");
    Machine machine;

    Instruction instruction;
    while((instruction = reader.next_instruction()).kind != END_OF_FILE) {
        machine.eval(instruction);
    }

    return 0;
}
