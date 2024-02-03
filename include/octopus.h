#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
#include <stack>
#include <SFML/Graphics.hpp>

struct GPU {
    private: sf::RenderWindow active_screen;
    private: sf::Image framebuffer;
    private: sf::Texture graphics;
    private: sf::Sprite drawable_graphics;
    
    // \brief Initializes GPU's attributes and creates a screen
    public: sf::RenderWindow& init();
    // \brief Draws the collection of bits that represent a sprite in this->framebuffer, starting at default_x and default_y
    public: uint8_t copy_to_framebuffer(const uint8_t, const uint8_t, const std::vector<uint8_t>);
    // \brief Fills this->framebuffer with OFF_COLOR pixels, effectively cleaning it up
    public: void clear_framebuffer();
    // \brief Updates this->graphics texture and draws the new this->drawable_graphics to this->active_screen
    public: void draw();
};

struct CPU {
    private: GPU& gpu;

    private: uint8_t ram[4096];
    private: std::stack<uint16_t> stack;

    // \brief The chip's registers
    private: uint8_t v[16];
    // \brief The program counter register
    private: uint16_t pc;
    // \brief The index register 
    private: uint16_t i;

    // \brief The delay timer
    private: uint8_t dt;
    // \brief The sound timer
    private: uint8_t st;

    private: bool blocked;
    public: std::map<uint8_t, uint8_t> keys;

    // \brief Sets the intern this->gpu reference to the provided GPU
    public: CPU(GPU& graphics_handler) : gpu(graphics_handler) {};
    // \brief Initializes CPU's attributes
    public: void init();
    // \brief Fills this->ram with bytes from the ROM specified at rom_path
    public: void dump_into_memory(const std::string);
    // \brief Returns the current opcode that this->pc points to
    private: uint16_t fetch_opcode();
    // \brief Evaluates the provided opcode
    private: void execute(uint16_t);
    // \brief Emulates an instruction cycle. Gets a opcode from this->fetch_opcode to supply to this->execute
    public: void cycle();
    // \brief Designed to execute on every clock tick. Decrements this->dt and this->st
    public: void tick();
};
