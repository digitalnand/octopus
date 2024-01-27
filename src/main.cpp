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
#define INSTRUCTIONS_SPAN 2
#define SCALE_FACTOR 10

#define DEFAULT_COLOR sf::Color(30, 144, 255)
#define EMPTY_COLOR sf::Color::Black

std::map<uint16_t, sf::Keyboard::Key> KEYMAP = {
    {5, sf::Keyboard::W},
    {7, sf::Keyboard::A},
    {8, sf::Keyboard::S},
    {9, sf::Keyboard::D},
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

struct InputHandler {
    sf::RenderWindow& active_window;
    InputHandler(sf::RenderWindow& window) : active_window(window) {};
    uint16_t wait_for_keypress();
    bool check_for_keypress(uint16_t);
};

uint16_t InputHandler::wait_for_keypress() {
    sf::Keyboard::Key pressed_key = sf::Keyboard::Unknown;

    while(true) {
        sf::Event event;
        this->active_window.pollEvent(event);
        if(event.type == sf::Event::KeyPressed) {
            pressed_key = event.key.code;
            break;
        }
    }

    for(const auto& [key_code, key] : KEYMAP) {
        if(pressed_key == key)
            return key_code;
    }

    return 0;
}

bool InputHandler::check_for_keypress(const uint16_t key_code) {
    return sf::Keyboard::isKeyPressed(KEYMAP[key_code]);
}

struct CPU {
    GPU& gpu;
    InputHandler input;

    std::vector<uint8_t> memory;
    std::stack<uint16_t> stack;
    uint16_t registers[16];
    uint16_t PC = 0;
    uint16_t I = 0;
    bool DT = 0;

    CPU(GPU& graphics_handler, InputHandler& input_handler) : gpu(graphics_handler), input(input_handler) {};
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

    this->PC += INSTRUCTIONS_SPAN;
    return (high_byte << 0x8) + low_byte;
}

void CPU::execute(const uint16_t instruction) {
    const uint16_t kind = (instruction & 0xf000) >> 12;
    switch(kind) {
        case 0x0:
        {
            switch(instruction) {
                case 0x00E0: // CLS
                    this->gpu.clear_framebuffer();
                break;

                case 0x00EE: // RET
                    if(this->stack.empty()) {
                        throw std::runtime_error("could not return from subroutine, stack was empty\n");
                    }
                    this->PC = this->stack.top();
                    this->stack.pop();
                break;

                default: break; // SYS address
            }
        } break;

        case 0x1: { // JP address
            const uint16_t address = (instruction & 0x0fff) - PROGRAMS_START;
            this->PC = address;
        } break;

        case 0x2: { // CALL address
            if(this->stack.size() > 0xf) {
                throw std::runtime_error("stack overflow\n");
            }
            const uint16_t address = (instruction & 0x0fff) - PROGRAMS_START;
            this->stack.push(this->PC);
            this->PC = address;
        } break;

        case 0x3: { // SE Vx, value
            const uint16_t target = (instruction & 0x0f00) >> 8;
            const uint16_t value = instruction & 0x00ff;
            if(this->registers[target] == value) {
                this->PC += INSTRUCTIONS_SPAN;
            }
        } break;

        case 0x4: { // SNE Vx, value
            const uint16_t target = (instruction & 0x0f00) >> 8;
            const uint16_t value = instruction & 0x00ff;
            if(this->registers[target] != value) {
                this->PC += INSTRUCTIONS_SPAN;
            }
        } break;

        case 0x6: { // LD Vx, value
            const uint16_t target = (instruction & 0x0f00) >> 8;
            const uint16_t value = instruction & 0x00ff;
            this->registers[target] = value;
        } break;

        case 0x7: { // ADD Vx, value
            const uint16_t target = (instruction & 0x0f00) >> 8;
            const uint16_t value = instruction & 0x00ff;
            this->registers[target] += value;
        } break;

        case 0x8:
        {
            const uint16_t nibble = instruction & 0x000f;
            switch(nibble) {
                case 0x0: { // LD Vx, Vy
                    const uint16_t target = (instruction & 0x0f00) >> 8;
                    const uint16_t source = (instruction & 0x00f0) >> 4;
                    this->registers[target] = this->registers[source];
                } break;

                case 0x5: { // SUB Vx, Vy
                    const uint16_t target = (instruction & 0x0f00) >> 8;
                    const uint16_t source = (instruction & 0x00f0) >> 4;
                    this->registers[0xf] = (this->registers[target] > this->registers[source]) ? 1 : 0;
                    this->registers[target] -= this->registers[source];
                } break;

                default: throw std::runtime_error("not implemented yet\n");
            }
        } break;

        case 0xA: { // LD I, address
            const uint16_t address = (instruction & 0x0fff) - PROGRAMS_START;
            this->I = address;
        } break;

        case 0xD: { // DRW Vx, Vy, length
            const auto x = this->registers[(instruction & 0x0f00) >> 8];
            const auto y = this->registers[(instruction & 0x00f0) >> 4];
            const uint16_t length = instruction & 0x000f;

            std::vector<uint8_t> sprite;
            for(size_t byte = 0; byte < length; byte++) {
                sprite.push_back(this->memory[this->I + byte]);
            }

            const auto overlapping = this->gpu.copy_to_framebuffer(x, y, sprite);
            this->registers[0xf] = (overlapping) ? 1 : 0;
        } break;

        case 0xE:
        {
            const uint16_t low_byte = instruction & 0x00ff;
            switch(low_byte) {
                case 0xA1: { // SKNP Vx
                    const auto key_code = this->registers[(instruction & 0x0f00) >> 8];
                    if(!this->input.check_for_keypress(key_code)) {
                        this->PC += INSTRUCTIONS_SPAN;
                    }
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
                } break;

                case 0x0A: { // LD Vx, K
                    const uint16_t target = (instruction & 0x0f00) >> 8;
                    this->registers[target] = this->input.wait_for_keypress();
                } break;

                case 0x15: { // LD DT, Vx
                    const uint16_t source = (instruction & 0x0f00) >> 8;
                    this->DT = this->registers[source];
                } break;

                case 0x65: { // LD Vx, [I]
                    const uint16_t end = (instruction & 0x0f00) >> 8;
                    for(size_t index = 0; index <= end; index++) {
                        this->registers[index] = this->memory[this->I + index];
                    }
                } break;

                default: throw std::runtime_error("not implemented yet\n");
            }
        } break;

        default: throw std::runtime_error("not implemented yet\n");
    }
}

void CPU::cycle() {
    const uint16_t instruction = this->fetch_instruction();
    if(instruction == '\0') {
        return;
    }
    this->execute(instruction);
    this->DT = 0;
}

int32_t main(int32_t argc, char* argv[]) {
    if(argc < 2) {
        std::cout << std::format("Usage: {} [ROM]\n", argv[0]);
        exit(1);
    }

    const auto file_path = std::string(argv[1]);

    sf::RenderWindow screen(sf::VideoMode(64 * SCALE_FACTOR, 32 * SCALE_FACTOR), "octopus");

    GPU graphics_handler(screen);
    InputHandler input_handler(screen);

    CPU processor(graphics_handler, input_handler);
    processor.init();
    processor.dump_into_memory(file_path);

    while(screen.isOpen()) {
        sf::Event event;
        while(screen.pollEvent(event))
            if(event.type == sf::Event::Closed) screen.close();

        processor.cycle();
        graphics_handler.draw();

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return 0;
}
