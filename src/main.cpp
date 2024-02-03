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

#define PROGRAMS_OFFSET 0x200
#define OPCODE_SPAN 2

#define WIDTH 64
#define HEIGHT 32
#define SCALE_FACTOR 10
#define DEFAULT_COLOR sf::Color(30, 144, 255)
#define EMPTY_COLOR sf::Color::Black

#define KEY_UP 1
#define KEY_DOWN 0

#define CLOCK_HZ (1000 / 60)

#ifdef DEBUG
#define debug_log(...) std::printf(__VA_ARGS__)
#else
#define debug_log(...)
#endif

#define FONT_LENGTH 5
const uint8_t fontset[80] =
{
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

struct GPU {
    sf::RenderWindow& active_screen;
    sf::Image framebuffer;
    sf::Texture graphics;
    sf::Sprite drawable_graphics;

    GPU(sf::RenderWindow&);
    uint8_t copy_to_framebuffer(const uint8_t, const uint8_t, const std::vector<uint8_t>);
    void clear_framebuffer();
    void draw();
};

GPU::GPU(sf::RenderWindow& screen) : active_screen(screen) {
    this->framebuffer.create(WIDTH, HEIGHT);
    this->graphics.create(WIDTH, HEIGHT);

    this->drawable_graphics.setScale(SCALE_FACTOR, SCALE_FACTOR);
    this->drawable_graphics.setTexture(this->graphics);
}

uint8_t GPU::copy_to_framebuffer(const uint8_t default_x, const uint8_t default_y, const std::vector<uint8_t> sprite) {
    uint8_t overlapping = 0;

    for(int8_t pixel_y = 0; pixel_y < static_cast<int8_t>(sprite.size()); pixel_y++) {
        const auto byte = sprite[pixel_y];
        for(int8_t bit_index = 8; bit_index >= 0; bit_index--) {
            const int8_t pixel_x = 8 - bit_index;
            const int8_t current_pixel = (byte >> bit_index) & 0x01;
            if(current_pixel == 0) continue;

            const int8_t x = (default_x + pixel_x) % WIDTH;
            const int8_t y = (default_y + pixel_y) % HEIGHT;

            if(this->framebuffer.getPixel(x, y) == EMPTY_COLOR) {
                this->framebuffer.setPixel(x, y, DEFAULT_COLOR);
            } else {
                this->framebuffer.setPixel(x, y, EMPTY_COLOR);
                overlapping = 1;
            }
        }
    }

    return overlapping;
}

void GPU::clear_framebuffer() {
    for(int8_t y = 0; y < HEIGHT; y++) {
        for(int8_t x = 0; x < WIDTH; x++) {
            this->framebuffer.setPixel(x, y, EMPTY_COLOR);
        }
    }
}

void GPU::draw() {
    this->graphics.update(this->framebuffer);
    this->active_screen.draw(this->drawable_graphics);
    this->active_screen.display();
}

struct CPU {
    GPU& gpu;

    uint8_t ram[4096];
    std::stack<uint16_t> stack;

    uint8_t v[16];
    uint16_t pc;
    uint16_t i;
    uint8_t dt;
    uint8_t st;

    bool blocked;
    std::map<uint8_t, uint8_t> keys;

    CPU(GPU& graphics_handler) : gpu(graphics_handler) {};
    void init();
    void dump_into_memory(const std::string);
    uint16_t fetch_opcode();
    void execute(uint16_t);
    void cycle();
};

void CPU::init() {
    std::memset(this->ram, 0, sizeof this->ram);
    std::memcpy(this->ram, fontset, sizeof fontset);

    std::memset(this->v, 0, 0xf);
    this->pc = PROGRAMS_OFFSET;
    this->i = 0;

    this->dt = 0;
    this->st = 0;

    this->blocked = false;
    this->keys = {
        {0x0, KEY_DOWN}, {0x1, KEY_DOWN}, {0x2, KEY_DOWN}, {0x3, KEY_DOWN},
        {0x4, KEY_DOWN}, {0x5, KEY_DOWN}, {0x6, KEY_DOWN}, {0x7, KEY_DOWN},
        {0x8, KEY_DOWN}, {0x9, KEY_DOWN}, {0xa, KEY_DOWN}, {0xb, KEY_DOWN},
        {0xc, KEY_DOWN}, {0xd, KEY_DOWN}, {0xe, KEY_DOWN}, {0xf, KEY_DOWN},
    };

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

    size_t index = 0;
    char byte;
    while(file.get(byte)) {
        this->ram[this->pc + index] = static_cast<uint8_t>(byte);
        index++;
    }

    file.close();
}

uint16_t CPU::fetch_opcode() {
    const auto high_byte = this->ram[this->pc];
    const auto low_byte = this->ram[this->pc + 1];

    this->pc += OPCODE_SPAN;
    return (high_byte << 0x8) + low_byte;
}

void CPU::execute(const uint16_t opcode) {
    const uint8_t kind = (opcode & 0xf000) >> 12;
    debug_log("opcode: %x\n", opcode);
    switch(kind) {
        case 0x0:
        {
            switch(opcode) {
                case 0x00E0: // CLS
                    this->gpu.clear_framebuffer();
                    debug_log("CLS\n");
                break;

                case 0x00EE: // RET
                    if(this->stack.empty()) throw std::runtime_error("could not return from subroutine, stack was empty\n");
                    this->pc = this->stack.top();
                    this->stack.pop();
                    debug_log("RET %x\n", this->pc);
                break;

                default: break; // SYS address
            }
        } break;

        case 0x1: { // JP address
            const uint16_t address = opcode & 0x0fff;
            this->pc = address;
            debug_log("JP %x\n", address);
        } break;

        case 0x2: { // CALL address
            if(this->stack.size() > 0xf) throw std::runtime_error("stack overflow\n");
            const uint16_t address = opcode & 0x0fff;
            this->stack.push(this->pc);
            this->pc = address;
            debug_log("CALL %x\n", address);
        } break;

        case 0x3: { // SE Vx, value
            const uint8_t target = (opcode & 0x0f00) >> 8;
            const uint8_t value = opcode & 0x00ff;
            if(this->v[target] == value) this->pc += OPCODE_SPAN;
            debug_log("SE V%x, %x\n", target, value);
        } break;

        case 0x4: { // SNE Vx, value
            const uint8_t target = (opcode & 0x0f00) >> 8;
            const uint8_t value = opcode & 0x00ff;
            if(this->v[target] != value) this->pc += OPCODE_SPAN;
            debug_log("SNE V%x, %x\n", target, value);
        } break;

        case 0x5: { // SE Vx, Vy
            const uint8_t target = (opcode & 0x0f00) >> 8;
            const uint8_t source = (opcode & 0x00f0) >> 4;
            if(this->v[target] == this->v[source]) this->pc += OPCODE_SPAN;
            debug_log("SE V%x, V%x\n", target, source);
        } break;

        case 0x6: { // LD Vx, value
            const uint8_t target = (opcode & 0x0f00) >> 8;
            const uint8_t value = opcode & 0x00ff;
            this->v[target] = value;
            debug_log("LD V%x, %x\n", target, value);
        } break;

        case 0x7: { // ADD Vx, value
            const uint8_t target = (opcode & 0x0f00) >> 8;
            const uint8_t value = opcode & 0x00ff;
            this->v[target] += value;
            debug_log("ADD V%x, %x\n", target, value);
        } break;

        case 0x8:
        {
            const uint8_t nibble = opcode & 0x000f;
            switch(nibble) {
                case 0x0: { // LD Vx, Vy
                    const uint8_t target = (opcode & 0x0f00) >> 8;
                    const uint8_t source = (opcode & 0x00f0) >> 4;
                    this->v[target] = this->v[source];
                    debug_log("LD V%x, V%x\n", target, source);
                } break;

                case 0x1: { // OR Vx, Vy
                    const uint8_t target = (opcode & 0x0f00) >> 8;
                    const uint8_t source = (opcode & 0x00f0) >> 4;
                    this->v[target] |= this->v[source];
                    debug_log("OR V%x, V%x\n", target, source);
                } break;

                case 0x2: { // AND Vx, Vy
                    const uint8_t target = (opcode & 0x0f00) >> 8;
                    const uint8_t source = (opcode & 0x00f0) >> 4;
                    this->v[target] &= this->v[source];
                    debug_log("AND V%x, V%x\n", target, source);
                } break;

                case 0x3: { // XOR Vx, Vy
                    const uint8_t target = (opcode & 0x0f00) >> 8;
                    const uint8_t source = (opcode & 0x00f0) >> 4;
                    this->v[target] ^= this->v[source];
                    debug_log("XOR V%x, V%x\n", target, source);
                } break;

                case 0x4: { // ADD Vx, Vy
                    const uint8_t target = (opcode & 0x0f00) >> 8;
                    const uint8_t source = (opcode & 0x00f0) >> 4;
                    const auto carry = ((this->v[target] + this->v[source]) >= 0xff);
                    this->v[target] += this->v[source];
                    this->v[0xf] = carry;
                    debug_log("ADD V%x, V%x\n", target, source);
                } break;

                case 0x5: { // SUB Vx, Vy
                    const uint8_t target = (opcode & 0x0f00) >> 8;
                    const uint8_t source = (opcode & 0x00f0) >> 4;
                    const auto not_borrow = (this->v[target] >= this->v[source]);
                    this->v[target] = this->v[target] - this->v[source];
                    this->v[0xf] = not_borrow;
                    debug_log("SUB V%x, V%x\n", target, source);
                } break;

                case 0x6: { // SHR Vx, Vy
                    const uint8_t target = (opcode & 0x0f00) >> 8;
                    const auto least_significant_bit = this->v[target] & 0x001;
                    this->v[target] >>= 1;
                    this->v[0xf] = least_significant_bit;
                    debug_log("SHR V%x\n", target);
                } break;

                case 0x7: { // SUBN Vx, Vy
                    const uint8_t target = (opcode & 0x0f00) >> 8;
                    const uint8_t source = (opcode & 0x00f0) >> 4;
                    this->v[target] = this->v[source] - this->v[target];
                    this->v[0xf] = (this->v[source] > this->v[target]);
                    debug_log("SUBN V%x, V%x\n", target, source);
                } break;

                case 0xE: { // SHL Vx, Vy
                    const uint8_t target = (opcode & 0x0f00) >> 8;
                    const auto most_significant_bit = this->v[target] & 0x80;
                    this->v[target] <<= 1;
                    this->v[0xf] = (most_significant_bit > 0) ? 1 : 0;
                    debug_log("SHL V%x\n", target);
                } break;

                default: throw std::runtime_error("not implemented yet\n");
            }
        } break;

        case 0x9: { // SNE Vx, Vy
            const uint8_t target = (opcode & 0x0f00) >> 8;
            const uint8_t source = (opcode & 0x00f0) >> 4;
            if(this->v[target] != this->v[source]) this->pc += OPCODE_SPAN;
            debug_log("SNE V%x, V%x\n", target, source);
        } break;

        case 0xA: { // LD I, address
            const uint16_t address = opcode & 0x0fff;
            this->i = address;
            debug_log("LD I, %x\n", address);
        } break;

        case 0xB: { // JP V0, address
            const uint16_t address = opcode & 0x0fff;
            this->pc = address + this->v[0];
            debug_log("JP V0, %x\n", address);
        } break;

        case 0xC: { // RND Vx, byte
            const uint8_t target = (opcode & 0x0f00) >> 8;
            const uint8_t value = opcode & 0x00ff;
            this->v[target] = (std::rand() % 256) & value;
            debug_log("RND V%x, %x\n", target, value);
        } break;

        case 0xD: { // DRW Vx, Vy, length
            const uint8_t x = (opcode & 0x0f00) >> 8;
            const uint8_t y = (opcode & 0x00f0) >> 4;
            const uint8_t length = opcode & 0x000f;

            std::vector<uint8_t> sprite;
            for(int8_t byte = 0; byte < length; byte++) {
                sprite.push_back(this->ram[this->i + byte]);
            }

            this->v[0xf] = this->gpu.copy_to_framebuffer(this->v[x], this->v[y], sprite);
            debug_log("DRW V%x, V%x, %x\n", x, y, length);
        } break;

        case 0xE:
        {
            const uint8_t source = (opcode & 0x0f00) >> 8;
            auto& key_state = this->keys[this->v[source]];

            const uint8_t low_byte = opcode & 0x00ff;
            switch(low_byte) {
                case 0x9E: { // SKP Vx
                    if(key_state == KEY_UP) this->pc += OPCODE_SPAN;
                    debug_log("SKP V%x\n", source);
                } break;

                case 0xA1: { // SKNP Vx
                    if(key_state == KEY_DOWN) this->pc += OPCODE_SPAN;
                    debug_log("SKNP V%x\n", source);
                } break;

                default: throw std::runtime_error("unknown opcode\n");
            }
        } break;

        case 0xF:
        {
            const uint8_t low_byte = opcode & 0x00ff;
            switch(low_byte) {
                case 0x07: { // LD Vx, DT
                    const uint8_t target = (opcode & 0x0f00) >> 8;
                    this->v[target] = this->dt;
                    debug_log("LD V%x, DT\n", target);
                } break;

                case 0x0A: { // LD Vx, K
                    const uint8_t target = (opcode & 0x0f00) >> 8;
                    this->blocked = true;
                    for(const auto& [code, state] : this->keys) {
                        if(state == KEY_DOWN) continue;
                        this->v[target] = code;
                        this->blocked = false;
                    }
                    debug_log("LD V%x, K\n", target);
                } break;

                case 0x15: { // LD DT, Vx
                    const uint8_t source = (opcode & 0x0f00) >> 8;
                    this->dt = this->v[source];
                    debug_log("LD DT, V%x\n", source);
                } break;

                case 0x18: { // LD ST, Vx
                    const uint8_t source = (opcode & 0x0f00) >> 8;
                    this->st = this->v[source];
                    debug_log("LD ST, V%x\n", source);
                } break;

                case 0x1E: { // ADD I, Vx
                    const uint8_t source = (opcode & 0x0f00) >> 8;
                    this->i += this->v[source];
                    debug_log("ADD I, V%x\n", source);
                } break;

                case 0x29: { // LD F, VX
                    const uint8_t source = (opcode & 0x0f00) >> 8;
                    this->i = this->v[source] * FONT_LENGTH;
                    debug_log("LD F, V%x\n", source);
                } break;

                case 0x33: { // LD B, Vx
                    const uint8_t source = (opcode & 0x0f00) >> 8;
                    const uint8_t hundreds = this->v[source] / 100;
                    const uint8_t tens = (this->v[source] - hundreds * 100) / 10;
                    const uint8_t ones = this->v[source] - hundreds * 100 - tens * 10;
                    this->ram[this->i] = hundreds;
                    this->ram[this->i + 1] = tens;
                    this->ram[this->i + 2] = ones;
                    debug_log("LD B, V%x\n", source);
                } break;

                case 0x55: { // LD [I], VX
                    const uint8_t end = (opcode & 0x0f00) >> 8;
                    for(size_t index = 0; index <= end; index++) {
                        this->ram[this->i + index] = this->v[index];
                    }
                    debug_log("LD I [V0...V%x]\n", end);
                } break;

                case 0x65: { // LD Vx, [I]
                    const uint8_t end = (opcode & 0x0f00) >> 8;
                    for(size_t index = 0; index <= end; index++) {
                        this->v[index] = this->ram[this->i + index];
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
    const auto opcode = this->fetch_opcode();
    if(opcode == '\0') return;

    this->execute(opcode);
    if(this->blocked) this->pc -= 2; // go back to the last instruction
}

int8_t get_key_code(sf::Keyboard::Key key) {
    switch(key) {
        case sf::Keyboard::Num1: return 0x1;
        case sf::Keyboard::Num2: return 0x2;
        case sf::Keyboard::Num3: return 0x3;
        case sf::Keyboard::Num4: return 0xc;
        case sf::Keyboard::Q:    return 0x4;
        case sf::Keyboard::W:    return 0x5;
        case sf::Keyboard::E:    return 0x6;
        case sf::Keyboard::R:    return 0xd;
        case sf::Keyboard::A:    return 0x7;
        case sf::Keyboard::S:    return 0x8;
        case sf::Keyboard::D:    return 0x9;
        case sf::Keyboard::F:    return 0xe;
        case sf::Keyboard::Z:    return 0xa;
        case sf::Keyboard::X:    return 0x0;
        case sf::Keyboard::C:    return 0xb;
        case sf::Keyboard::V:    return 0xf;
        default: return -1;
    }
}

int32_t main(int32_t argc, char* argv[]) {
    if(argc < 2) {
        std::cout << std::format("Usage: {} [ROM]\n", argv[0]);
        return 1;
    }

    const auto file_path = std::string(argv[1]);

    sf::RenderWindow screen(sf::VideoMode(WIDTH * SCALE_FACTOR, HEIGHT * SCALE_FACTOR), "octopus");

    GPU graphics_handler(screen);

    CPU processor(graphics_handler);
    processor.init();
    processor.dump_into_memory(file_path);

    auto clock_previous = std::chrono::steady_clock::now();

    while(screen.isOpen()) {
        const auto clock_now = std::chrono::steady_clock::now();

        sf::Event event;
        while(screen.pollEvent(event)) {
            switch(event.type) {
                case sf::Event::Closed:
                    screen.close();
                break;

                case sf::Event::KeyPressed: {
                    const auto key_code = get_key_code(event.key.code);
                    if(!processor.keys.contains(key_code)) break;
                    processor.keys[key_code] = KEY_UP;
                } break;

                case sf::Event::KeyReleased: {
                    const auto key_code = get_key_code(event.key.code);
                    if(!processor.keys.contains(key_code)) break;
                    processor.keys[key_code] = KEY_DOWN;
                } break;

                default: break;
            }
        }

        processor.cycle();
        graphics_handler.draw();

        const auto clock_rate = std::chrono::milliseconds(CLOCK_HZ).count();
        if(std::chrono::duration_cast<std::chrono::milliseconds>(clock_now - clock_previous).count() > clock_rate) {
            if(processor.dt > 0) processor.dt--;
            if(processor.st > 0) {
                processor.st--;
                if(processor.st == 0) std::cout << "beep\n";
            }
            
            clock_previous = clock_now;
        }
    }

    return 0;
}
