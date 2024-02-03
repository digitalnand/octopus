#include <chrono>
#include <cstdint>
#include <format>
#include <iostream>

#include "octopus.h"

#define KEY_UP 1
#define KEY_DOWN 0

#define CLOCK_HZ (1000 / 60)

int8_t get_key_code(const sf::Keyboard::Key);

int32_t main(int32_t argc, char* argv[]) {
    if(argc < 2) {
        std::cout << std::format("Usage: {} [ROM]\n", argv[0]);
        return 1;
    }

    const auto file_path = std::string(argv[1]);

    GPU graphics_handler;
    CPU processor(graphics_handler);
    processor.init();
    processor.dump_into_memory(file_path);

    auto& screen = graphics_handler.init();
    auto clock_previous = std::chrono::steady_clock::now();

    while(true) {
        const auto clock_now = std::chrono::steady_clock::now();

        sf::Event event;
        while(screen.pollEvent(event)) {
            switch(event.type) {
                case sf::Event::Closed: exit(0); break;
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
            processor.tick();
            clock_previous = clock_now;
        }
    }

    return 0;
}

int8_t get_key_code(const sf::Keyboard::Key key) {
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

#undef KEY_UP
#undef KEY_DOWN
#undef CLOCK_HZ
