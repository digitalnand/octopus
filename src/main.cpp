#include <bitset>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <format>
#include <fstream>
#include <iostream>
#include <stack>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include <SFML/Graphics.hpp>

#define PROGRAMS_START 0x200
#define INSTRUCTION_SPAN 2

#define KEY_UP 1
#define KEY_DOWN 0

#define SCALE_FACTOR 10
#define DEFAULT_COLOR sf::Color(30, 144, 255)
#define EMPTY_COLOR sf::Color::Black

#ifdef DEBUG
#define debug_log(...) std::printf(__VA_ARGS__)
#else
#define debug_log(...)
#endif

std::map<sf::Keyboard::Key, uint16_t> KEYMAP = {
    {sf::Keyboard::W, 5},
    {sf::Keyboard::A, 7},
    {sf::Keyboard::S, 8},
    {sf::Keyboard::D, 9},
};

struct GPU {
    sf::RenderWindow& active_screen;
    sf::Image framebuffer;
    sf::Texture graphics;
    sf::Sprite drawable_graphics;

    GPU(sf::RenderWindow&);
    bool copy_to_framebuffer(const uint16_t, const uint16_t, const std::vector<uint8_t>);
    void clear_framebuffer();
    void draw();
};

GPU::GPU(sf::RenderWindow& screen) : active_screen(screen) {
    this->framebuffer.create(64, 32);
    this->graphics.create(64, 32);

    this->drawable_graphics.setScale(SCALE_FACTOR, SCALE_FACTOR);
    this->drawable_graphics.setTexture(this->graphics);
}

bool GPU::copy_to_framebuffer(const uint16_t default_x, const uint16_t default_y, const std::vector<uint8_t> sprite) {
    bool overlapping = false;

    for(size_t pixel_y = 0; pixel_y < sprite.size(); pixel_y++) {
        const auto row = sprite[pixel_y];

        for(size_t bit_index = 8; bit_index > 0; bit_index--) {
            const size_t pixel_x = (bit_index % 8);
            const uint8_t current_pixel = (row >> pixel_x) & 0x1;

            if(current_pixel == 0)
                continue;

            const auto x = (default_x + pixel_x) % 64;
            const auto y = (default_y + pixel_y) % 32;

            if(this->framebuffer.getPixel(x, y) == EMPTY_COLOR) {
                this->framebuffer.setPixel(x, y, DEFAULT_COLOR);
            } else {
                this->framebuffer.setPixel(x, y, EMPTY_COLOR);
                overlapping = true;
            }
        }
    }

    return overlapping;
}

void GPU::clear_framebuffer() {
    for(size_t y = 0; y < 32; y++)
        for(size_t x = 0; x < 64; x++)
            this->framebuffer.setPixel(x, y, EMPTY_COLOR);
}

void GPU::draw() {
    this->graphics.update(this->framebuffer);
    this->active_screen.draw(this->drawable_graphics);
    this->active_screen.display();
}

struct CPU {
    GPU& gpu;

    std::vector<uint8_t> memory;
    std::stack<uint16_t> stack;
    uint16_t registers[16];
    uint16_t PC = 0;
    uint16_t I = 0;
    bool DT = 0;

    bool blocked = false;
    std::map<uint16_t, uint16_t> keys = {
        {5, KEY_DOWN},
        {7, KEY_DOWN},
        {8, KEY_DOWN},
        {9, KEY_DOWN}
    };

    CPU(GPU& graphics_handler) : gpu(graphics_handler) {};
    void init();
    void dump_into_memory(const std::string);
    uint16_t fetch_instruction();
    void execute(uint16_t);
    void cycle();
};

void CPU::init() {
    std::memset(this->registers, 0, 0xf);
    std::srand(std::time(NULL));
}

void CPU::dump_into_memory(const std::string file_path) {
    const auto extension = file_path.substr(file_path.find_last_of('.'));
    if(extension != ".ch8") {
        throw std::runtime_error(std::format("file extension not supported: {}\n", extension));
    }

    std::ifstream file(file_path, std::ios::binary);
    if(!file.is_open()) {
        throw std::runtime_error(std::format("could not open rom: {}\n", file_path));
    }

    char byte;
    while(file.get(byte)) {
        this->memory.push_back(byte);
    }

    file.close();
}

uint16_t CPU::fetch_instruction() {
    if(this->PC + 1 >= static_cast<uint16_t>(this->memory.size())) {
        return '\0';
    }

    const auto high_byte = this->memory.at(this->PC);
    const auto low_byte = this->memory.at(this->PC + 1);

    this->PC += INSTRUCTION_SPAN;
    return (high_byte << 0x8) + low_byte;
}

void CPU::execute(const uint16_t instruction) {
    const uint16_t kind = (instruction & 0xf000) >> 12;
    debug_log("instruction: %x\n", instruction);
    switch(kind) {
        case 0x0:
        {
            switch(instruction) {
                case 0x00E0: // CLS
                    this->gpu.clear_framebuffer();
                    debug_log("CLS\n");
                break;

                case 0x00EE: // RET
                    if(this->stack.empty()) throw std::runtime_error("could not return from subroutine, stack was empty\n");
                    this->PC = this->stack.top();
                    this->stack.pop();
                    debug_log("RET %x\n", this->PC);
                break;

                default: break; // SYS address
            }
        } break;

        case 0x1: { // JP address
            const uint16_t address = (instruction & 0x0fff) - PROGRAMS_START;
            this->PC = address;
            debug_log("JP %x\n", address);
        } break;

        case 0x2: { // CALL address
            if(this->stack.size() > 0xf) throw std::runtime_error("stack overflow\n");
            const uint16_t address = (instruction & 0x0fff) - PROGRAMS_START;
            this->stack.push(this->PC);
            this->PC = address;
            debug_log("CALL %x\n", address);
        } break;

        case 0x3: { // SE Vx, value
            const uint16_t target = (instruction & 0x0f00) >> 8;
            const uint16_t value = instruction & 0x00ff;
            if(this->registers[target] == value) this->PC += INSTRUCTION_SPAN;
            debug_log("SE V%x, %x\n", target, value);
        } break;

        case 0x4: { // SNE Vx, value
            const uint16_t target = (instruction & 0x0f00) >> 8;
            const uint16_t value = instruction & 0x00ff;
            if(this->registers[target] != value) this->PC += INSTRUCTION_SPAN;
            debug_log("SE V%x, %x\n", target, value);
        } break;

        case 0x6: { // LD Vx, value
            const uint16_t target = (instruction & 0x0f00) >> 8;
            const uint16_t value = instruction & 0x00ff;
            this->registers[target] = value;
            debug_log("LD V%x, %x\n", target, value);
        } break;

        case 0x7: { // ADD Vx, value
            const uint16_t target = (instruction & 0x0f00) >> 8;
            const uint16_t value = instruction & 0x00ff;
            this->registers[target] += value;
            debug_log("ADD V%x, %x\n", target, value);
        } break;

        case 0x8:
        {
            const uint16_t nibble = instruction & 0x000f;
            switch(nibble) {
                case 0x0: { // LD Vx, Vy
                    const uint16_t target = (instruction & 0x0f00) >> 8;
                    const uint16_t source = (instruction & 0x00f0) >> 4;
                    this->registers[target] = this->registers[source];
                    debug_log("LD V%x, V%x\n", target, source);
                } break;

                case 0x5: { // SUB Vx, Vy
                    const uint16_t target = (instruction & 0x0f00) >> 8;
                    const uint16_t source = (instruction & 0x00f0) >> 4;
                    this->registers[0xf] = (this->registers[target] > this->registers[source]) ? 1 : 0;
                    this->registers[target] -= this->registers[source];
                    debug_log("SUB V%x, V%x\n", target, source);
                } break;

                default: throw std::runtime_error("not implemented yet\n");
            }
        } break;

        case 0xA: { // LD I, address
            const uint16_t address = (instruction & 0x0fff) - PROGRAMS_START;
            this->I = address;
            debug_log("LD I, %x\n", address);
        } break;

        case 0xD: { // DRW Vx, Vy, length
            const uint16_t x = (instruction & 0x0f00) >> 8;
            const uint16_t y = (instruction & 0x00f0) >> 4;
            const uint16_t length = instruction & 0x000f;

            std::vector<uint8_t> sprite;
            for(size_t byte = 0; byte < length; byte++) {
                sprite.push_back(this->memory[this->I + byte]);
            }

            const auto overlapping = this->gpu.copy_to_framebuffer(this->registers[x], this->registers[y], sprite);
            this->registers[0xf] = (overlapping) ? 1 : 0;
            debug_log("DRW V%x, V%x, %x\n", x, y, length);
        } break;

        case 0xE:
        {
            const uint16_t low_byte = instruction & 0x00ff;
            switch(low_byte) {
                case 0xA1: { // SKNP Vx
                    const uint16_t source = (instruction & 0x0f00) >> 8;
                    auto& key_state = this->keys[this->registers[source]];
                    if(key_state == KEY_DOWN) {
                        this->PC += INSTRUCTION_SPAN;
                    } else key_state = KEY_DOWN;
                    debug_log("SKNP V%x\n", source);
                } break;

                default: throw std::runtime_error("not implemented yet\n");
            }
        } break;

        case 0xF:
        {
            const uint16_t low_byte = instruction & 0x00ff;
            switch(low_byte) {
                case 0x07: { // LD Vx, DT
                    const uint16_t target = (instruction & 0x0f00) >> 8;
                    this->registers[target] = this->DT;
                    debug_log("LD V%x, DT\n", target);
                } break;

                case 0x0A: { // LD Vx, K
                    const uint16_t target = (instruction & 0x0f00) >> 8;
                    this->blocked = true;
                    for(const auto& [code, state] : this->keys) {
                        if(state == KEY_DOWN) continue;
                        this->registers[target] = code;
                        this->blocked = false;
                    }
                    debug_log("LD V%x, %x\n", target, this->registers[target]);
                } break;

                case 0x15: { // LD DT, Vx
                    const uint16_t source = (instruction & 0x0f00) >> 8;
                    this->DT = this->registers[source];
                    debug_log("LD DT, V%x\n", source);
                } break;

                case 0x65: { // LD Vx, [I]
                    const uint16_t end = (instruction & 0x0f00) >> 8;
                    for(size_t index = 0; index <= end; index++) {
                        this->registers[index] = this->memory[this->I + index];
                    }
                    debug_log("LD [V0...V%x] I\n", end);
                } break;

                default: throw std::runtime_error("not implemented yet\n");
            }
        } break;

        default: throw std::runtime_error("not implemented yet\n");
    }
}

void CPU::cycle() {
    const auto instruction = this->fetch_instruction();
    if(instruction == '\0') return;

    this->execute(instruction);
    if(this->blocked) this->PC -= 2; // go back to the last instruction
}

int32_t main(int32_t argc, char* argv[]) {
    if(argc < 2) {
        std::cout << std::format("Usage: {} [ROM]\n", argv[0]);
        return 1;
    }

    const auto file_path = std::string(argv[1]);

    sf::RenderWindow screen(sf::VideoMode(64 * SCALE_FACTOR, 32 * SCALE_FACTOR), "octopus");

    GPU graphics_handler(screen);

    CPU processor(graphics_handler);
    processor.init();
    processor.dump_into_memory(file_path);

    while(screen.isOpen()) {
        sf::Event event;
        while(screen.pollEvent(event)) {
            switch(event.type) {
                case sf::Event::Closed:
                    screen.close();
                break;

                case sf::Event::KeyPressed: {
                    const auto key_code = KEYMAP[event.key.code];
                    if(!processor.keys.contains(key_code)) break;
                    processor.keys[key_code] = KEY_UP;
                } break;

                default: break;
            }
        }

        processor.cycle();
        graphics_handler.draw();

        processor.DT = 0;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return 0;
}
