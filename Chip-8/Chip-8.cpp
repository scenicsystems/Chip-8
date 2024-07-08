#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <bitset>
#include <SDL.h>
#include <stdio.h>
#include <algorithm>
#include <chrono>
#include <thread>
#include <stack>
#include <random>
#include <cmath>
#include <map>

const int SCREEN_SCALE = 6;

const int SCREEN_WIDTH = (SCREEN_SCALE * 64);
const int SCREEN_HEIGHT = (SCREEN_SCALE * 32);

const int CLOCK_RATE = 60;      // stores clock rate in hz (frames per second)

char* openRomFile(int &romSize, std::string fileName)
{
    std::ifstream file{};
    file.open(fileName, std::ios::in | std::ios::binary | std::ios::ate);

    if (!file.is_open())
    {
        std::cout << "ERROR: ROM file '" << fileName << "' could not be opened.\n";
        romSize = -1;
        return {};
    }

    std::streampos size{ file.tellg() };
    romSize = size;
    char* memoryBlock{ new char[size] };
    file.seekg(0, std::ios::beg);
    file.read(memoryBlock, size);
    file.close();

    std::cout << "ROM file loaded.\n";

    return memoryBlock;
}

int retrieveCartData(std::vector<int> &cartData, std::string romName)
{
    const int OPCODE_LENGTH_IN_BYTES = 1;
    int romSize{};

    char* memoryBlock{ openRomFile(romSize, romName)};
    if (romSize == -1)      // invalid rom size
    {
        return -1;
    }
    
    int opcodeCount = romSize / OPCODE_LENGTH_IN_BYTES;

    std::string tempString{};
    int tempValue{};

    // this loop grabs the raw data from the rom file and segements it into OPCODE_LENGTH_IN_BYTES-sized pieces to be loaded into the cartData vector for later use.
    for (int i{ 0 }; i < romSize; i++)
    {
        unsigned int rawData{ (unsigned char)memoryBlock[i] };

        if ((i + 1) % OPCODE_LENGTH_IN_BYTES != 0)
        {
            if (rawData > 1)
            {

            }
            tempValue = (rawData * 0x100);
        }
        else
        {
            tempValue += rawData;
            cartData.push_back(tempValue);
            std::cout << i << ": " << std::hex << tempValue << "\n";
            tempValue = 0;
        }
    }
    delete[] memoryBlock;
    return opcodeCount;
}

void printScreenArray(bool screen[64][32])
{
    for (int y{ 0 }; y < 32; y++)
    {
        for (int x{ 0 }; x < 64; x++)
        {
            if (screen[x][y] == true)
            {
                std::cout << "1";
            }
            else
            {
                std::cout << "0";
            }
        }
        std::cout << "\n";
    }
}

bool drawToScreen(int x, int y, std::vector<int> sprite, bool screenArray[64][32], SDL_Renderer* renderer)
{
    int yOffset{ 0 };
    bool collision{ false };
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    for (int spriteByte{ 0 }; spriteByte < sprite.size(); spriteByte += 0x1)
    {
        std::string binary{ std::bitset<8>(sprite.at(spriteByte)).to_string() };

        for (int spriteBit{ 0 }; spriteBit < 8; spriteBit++)
        {
            if (binary[spriteBit] == '1')
            {
                int xRenderPixel{ ((x + spriteBit) % SCREEN_WIDTH) };
                int yRenderPixel{ ((y + yOffset) % SCREEN_HEIGHT)};

                screenArray[xRenderPixel][yRenderPixel] = (screenArray[xRenderPixel][yRenderPixel] ^ true);
                if (screenArray[xRenderPixel][yRenderPixel] == false)
                {
                    collision = true;
                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                    for (int i{ 0 }; i < SCREEN_SCALE; i++)
                    {
                        for (int n{ 0 }; n < SCREEN_SCALE; n++)
                        {
                            SDL_RenderDrawPoint(renderer, (xRenderPixel * SCREEN_SCALE) + n, (((y + yOffset) * SCREEN_SCALE)) + i);
                        }
                    }
                    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                }

                else
                {
                    screenArray[xRenderPixel][y + yOffset] = true;
                    for (int i{ 0 }; i < SCREEN_SCALE; i++)
                    {
                        for (int n{ 0 }; n < SCREEN_SCALE; n++)
                        {
                            SDL_RenderDrawPoint(renderer, (xRenderPixel * SCREEN_SCALE) + n, (((y + yOffset) * SCREEN_SCALE)) + i);
                        }
                        
                    }
                }

            }
        }
        yOffset++;
    }

    return collision;
}

void clearScreen(bool (&screenArray)[64][32], SDL_Renderer* renderer)
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    for (int x{ 0 }; x < SCREEN_WIDTH; x++)
    {
        for (int y{ 0 }; y < SCREEN_HEIGHT; y++)
        {
            screenArray[x][y] = false;
            SDL_RenderDrawPoint(renderer, x, y);
        }
    }

    // update screen, reset render draw color to white
    SDL_RenderPresent(renderer);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
}

int returnFromSubroutine(std::stack<int16_t> &stackPointer)
{
    if (stackPointer.size() == 0)
    {
        std::cout << "ERROR: stack pointer access failure";
    }
    else
    {
        int currentInstruction = (stackPointer.top());
        stackPointer.pop();
        return currentInstruction;
    }
}

int jumpToAddress(int opcode)
{
    return ((opcode & 0x0FFF) - 2);
}

void printOpcode(int opcode, std::string text="Unknown opcode")
{
    std::cout << std::hex << opcode << "\t" << text;
    return;
}

int main(int argc, char* args[])
{
    // Main program loop flag
    bool quit{ false };

    // Initializing registers
    uint16_t IRegister{0};
    signed char VRegister[16]{0};
    uint8_t delayTimer{0};
    uint8_t soundTimer{0};
    uint16_t programCounter{0};
    std::stack<int16_t> stackPointer{};

    const int CART_MEMORY_START{ 0x200 };
    const int CART_MEMORY_END{ 0xFFF };
    const int NUMBER_OF_KEYS{ 16 };
    const int EXECUTIONS_PER_FRAME{ 8 };
    const int OPCODE_LENGTH_IN_BYTES{ 2 };

    // Preloading CHIP-8 memory with onboard sprites 0 through F:
    std::vector<int> memory{                      0xF0, 0x90, 0x90, 0x90, 0xF0,     // 0
                                                  0x20, 0x60, 0x20, 0x20, 0x70,     // 1   
                                                  0xF0, 0x10, 0xF0, 0x80, 0xF0,     // 2
                                                  0xF0, 0x10, 0xF0, 0x10, 0xF0,     // 3
                                                  0x90, 0x90, 0xF0, 0x10, 0x10,     // 4
                                                  0xF0, 0x80, 0xF0, 0x10, 0xF0,     // 5
                                                  0xF0, 0x80, 0xF0, 0x90, 0xF0,     // 6
                                                  0xF0, 0x10, 0x20, 0x40, 0x40,     // 7
                                                  0xF0, 0x90, 0xF0, 0x90, 0xF0,     // 8
                                                  0xF0, 0x90, 0xF0, 0x10, 0xF0,     // 9
                                                  0xF0, 0x90, 0xF0, 0x90, 0x90,     // A
                                                  0xE0, 0x90, 0xE0, 0x90, 0xE0,     // B
                                                  0xF0, 0x80, 0x80, 0x80, 0xF0,     // C
                                                  0xE0, 0x90, 0x90, 0x90, 0xE0,     // D
                                                  0xF0, 0x80, 0xF0, 0x80, 0xF0,     // E
                                                  0xF0, 0x80, 0xF0, 0x80, 0x80 };   // F

    const int FONT_MEMORY_END{ (int)memory.size() };

    // Filling rest of memory up until 0x200 with blank 0s
    for (int i{ FONT_MEMORY_END }; i < CART_MEMORY_START; i++)
    {
        memory.push_back(0);
    }

    std::cout << "Enter the filename of the ROM you'd like to load: ";
    std::string romName{};
    std::cin >> romName;

    // Loads rest of memory with game cart
    int opcodeCount{ retrieveCartData(memory, romName) };
    if (opcodeCount == -1)
    {
        std::cout << "ERROR: could not load game cart\n";
        return 0;
    }

    // Setting up GUI
    SDL_Window* gWindow;
    SDL_Renderer* gRenderer;
    SDL_Event e;

    // Initializing RNG
    std::mt19937 mt{ std::random_device{}() };
    std::uniform_int_distribution<> rng{ 0x0, 0xFF };

    SDL_Init(SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer(SCREEN_WIDTH, SCREEN_HEIGHT, 0, &gWindow, &gRenderer);
    SDL_SetRenderDrawColor(gRenderer, 255, 255, 255, 255);

    // Screen array for maintaining screen data
    bool screenArray[64][32]{ false };

    int currentInstruction{ 0x200 };

    int sleepTimeInMilliseconds{ static_cast<int>(1000*(1.0 / CLOCK_RATE)) };

    enum keyMap {
        KEY_PRESS_0,
        KEY_PRESS_1,
        KEY_PRESS_2,
        KEY_PRESS_3,
        KEY_PRESS_4,
        KEY_PRESS_5,
        KEY_PRESS_6,
        KEY_PRESS_7,
        KEY_PRESS_8,
        KEY_PRESS_9,
        KEY_PRESS_A,
        KEY_PRESS_B,
        KEY_PRESS_C,
        KEY_PRESS_D,
        KEY_PRESS_E,
        KEY_PRESS_F,
    };

    // map for mapping SDL key press events to our keyPresses array
    std::map<int, int> Keysym_To_Key{
        {SDLK_1, KEY_PRESS_1},
        {SDLK_UP, KEY_PRESS_2},
        {SDLK_3, KEY_PRESS_3},
        {SDLK_LEFT, KEY_PRESS_4},
        {SDLK_5, KEY_PRESS_5},
        {SDLK_RIGHT, KEY_PRESS_6},
        {SDLK_7, KEY_PRESS_7},
        {SDLK_DOWN, KEY_PRESS_8},
        {SDLK_9, KEY_PRESS_9},
        {SDLK_0, KEY_PRESS_0},
        {SDLK_a, KEY_PRESS_A},
        {SDLK_b, KEY_PRESS_B},
        {SDLK_c, KEY_PRESS_C},
        {SDLK_d, KEY_PRESS_D},
        {SDLK_e, KEY_PRESS_E},
        {SDLK_f, KEY_PRESS_F}
    };

    bool keyPresses[NUMBER_OF_KEYS]{ false };

    while (!quit)
    {
            // outer loop increments visual frames. each frame corresponds to the CLOCK_RATE, with each frame happening every 1/CLOCK_RATE seconds
            for (int frameCount{0}; quit != true; frameCount++)
            {
                // wait timer
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepTimeInMilliseconds));

                // if valid key press is detected
                while (SDL_PollEvent(&e) != 0)
                {
                    if (e.type == SDL_QUIT)
                    {
                        quit = true;
                    }
                    else if (e.type == SDL_KEYDOWN)
                    {
                        keyPresses[Keysym_To_Key[e.key.keysym.sym]] = true;
                    }
                }

                if (delayTimer != 0)
                {
                    delayTimer -= 1;
                }

                // inner loop for executing instructions. each frame has the intreprter execute EXECUTIONS_PER_FRAME instructions
                for (int i{ 0 }; currentInstruction < CART_MEMORY_END && i <= EXECUTIONS_PER_FRAME; currentInstruction += OPCODE_LENGTH_IN_BYTES)
                {
                    int currentOpcode{ (memory.at(currentInstruction) * 0x100) + memory.at(currentInstruction + 1) };

                    std::cout << currentInstruction << "\t";

                    switch (currentOpcode & 0xF000) // checking first bit
                    {
                    case 0x0000:
                    {
                        switch (currentOpcode)
                        {
                        case 0x00E0:    // CLS
                        {
                            clearScreen(screenArray, gRenderer);
                            printOpcode(currentOpcode, "Clearing screen");
                            break;
                        }
                        case 0x00EE:    // RET
                        {
                            currentInstruction = returnFromSubroutine(stackPointer);
                            printOpcode(currentOpcode, ("Returning from subroutine, jumping to " + std::to_string(currentInstruction)));
                            break;
                        }
                        default:        // Invalid opcode
                        {
                            printOpcode(currentOpcode);
                            break;
                        }
                        }
                        break;
                    }

                    case 0x1000:    // JP nnn
                    {
                        int jumpAddress{ jumpToAddress(currentOpcode) };
                        printOpcode(currentOpcode, ("Jumping to address " + std::to_string(jumpAddress + OPCODE_LENGTH_IN_BYTES)));
                        currentInstruction = jumpAddress;
                        break;
                    }
                    case 0x2000:    // CALL nnn
                    {
                        int jumpAddress{ jumpToAddress(currentOpcode) };
                        stackPointer.push(currentInstruction);
                        currentInstruction = jumpAddress;
                        printOpcode(currentOpcode, ("Calling subroutine and jumping to " + std::to_string(jumpAddress + OPCODE_LENGTH_IN_BYTES)));
                        break;
                    }
                    case 0x3000:    // SE Vx, nn
                    {
                        int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                        int value{ currentOpcode & 0x00FF };
                        if (VRegister[Vx] == value)
                        {
                            currentInstruction += OPCODE_LENGTH_IN_BYTES;
                            printOpcode(currentOpcode, "Skipping next instruction");
                        }
                        else
                        {
                            printOpcode(currentOpcode, "Not skipping next instruction");
                        }
                        break;
                    }
                    case 0x4000:    // SNE Vx, nn
                    {
                        int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                        int value{ currentOpcode & 0x00FF };
                        if (VRegister[Vx] != value)
                        {
                            currentInstruction += OPCODE_LENGTH_IN_BYTES;
                            printOpcode(currentOpcode, "Skipping next instruction");
                        }
                        else
                        {
                            printOpcode(currentOpcode, "Not skipping next instruction");
                        }
                        break;
                    }
                    case 0x5000:    // SE Vx, Vy
                    {
                        int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                        int Vy{ (currentOpcode & 0x00F0) / 0x0010 };
                        if (VRegister[Vx] == VRegister[Vy])
                        {
                            currentInstruction += OPCODE_LENGTH_IN_BYTES;
                            printOpcode(currentOpcode, "Skipping next instruction");
                        }
                        else
                        {
                            printOpcode(currentOpcode, "Not skipping next instruction");
                        }
                        break;
                    }
                    case 0x6000:    // LD Vx, nn
                    {
                        int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                        int value{ currentOpcode & 0x00FF };
                        VRegister[Vx] = value;
                        printOpcode(currentOpcode, "Putting value into register");
                        break;
                    }
                    case 0x7000:    // ADD Vx, nn
                    {
                        int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                        int value{ currentOpcode & 0x00FF };
                        VRegister[Vx] += value;
                        printOpcode(currentOpcode, "Adding value to register");
                        break;
                    }
                    case 0x8000:
                    {
                        switch (currentOpcode & 0x000F)
                        {
                        case 0x0000:    // LD Vx, Vy
                        {
                            int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                            int Vy{ (currentOpcode & 0x00F0) / 0x0010 };
                            VRegister[Vx] = VRegister[Vy];
                            printOpcode(currentOpcode, "Storing value from register Vy to Vx");
                            break;
                        }
                        case 0x0001:    // OR Vx, Vy
                        {
                            int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                            int Vy{ (currentOpcode & 0x00F0) / 0x0010 };
                            VRegister[Vx] = (VRegister[Vx] | VRegister[Vy]);
                            VRegister[0xF] = 0;
                            printOpcode(currentOpcode, "Computing bitwise OR on Vx and Vy");
                            break;
                        }
                        case 0x0002:    // AND Vx, Vy
                        {
                            int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                            int Vy{ (currentOpcode & 0x00F0) / 0x0010 };
                            VRegister[Vx] = (VRegister[Vx] & VRegister[Vy]);
                            printOpcode(currentOpcode, "Computing bitwise AND on Vx and Vy");
                            VRegister[0xF] = 0;
                            break;
                        }
                        case 0x0003:    // XOR Vx, Vy
                        {
                            int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                            int Vy{ (currentOpcode & 0x00F0) / 0x0010 };
                            VRegister[Vx] = (VRegister[Vx] ^ VRegister[Vy]);
                            printOpcode(currentOpcode, "Computing bitwise XOR on Vx and Vy");
                            VRegister[0xF] = 0;
                            break;
                        }
                        case 0x0004:    // ADD Vx, Vy
                        {
                            int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                            int Vy{ (currentOpcode & 0x00F0) / 0x0010 };
                            if ((VRegister[Vx] + VRegister[Vy]) > 0xFF)
                            {
                                VRegister[Vx] += VRegister[Vy];
                                VRegister[0xF] = 0x01;
                            }
                            else
                            {
                                VRegister[Vx] += VRegister[Vy];
                                VRegister[0xF] = 0x00;
                            }
                            printOpcode(currentOpcode, "Adding register Vx and Vy");
                            break;
                        }
                        case 0x0005:    // SUB Vx, Vy
                        {
                            int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                            int Vy{ (currentOpcode & 0x00F0) / 0x0010 };
                            if (VRegister[Vx] >= VRegister[Vy])
                            {
                                VRegister[Vx] -= VRegister[Vy];
                                VRegister[0xF] = 0x01;
                            }
                            else
                            {
                                VRegister[Vx] -= VRegister[Vy];
                                VRegister[0xF] = 0x00;
                            }
                            printOpcode(currentOpcode, "Subtracting Vy from Vx");
                            break;
                        }
                        case 0x0006:    // SHR Vx
                        {
                            int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                            int Vy{ (currentOpcode & 0x00F0) / 0x0010 };
                            VRegister[Vx] = VRegister[Vy];
                            char shift{ (VRegister[Vx] & 0b00000001) };
                            VRegister[Vx] >>= 1;
                            VRegister[0xF] = shift;

                            printOpcode(currentOpcode, "Dividing register Vx by 2");
                            break;
                        }
                        case 0x0007:    // SUBN Vx, Vy
                        {
                            int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                            int Vy{ (currentOpcode & 0x00F0) / 0x0010 };
                            if (VRegister[Vy] >= VRegister[Vx])
                            {
                                VRegister[Vx] = (VRegister[Vy] - VRegister[Vx]);
                                VRegister[0xF] = 0x01;
                            }
                            else
                            {
                                VRegister[Vx] = (VRegister[Vy] - VRegister[Vx]);
                                VRegister[0xF] = 0x00;
                            }
                            printOpcode(currentOpcode, "Setting register Vx to Vy - Vx");
                            break;
                        }
                        case 0x000E:    // SHL Vx
                        {
                            int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                            int Vy{ (currentOpcode & 0x00F0) / 0x0010 };
                            VRegister[Vx] = VRegister[Vy];
                            char shift{ (VRegister[Vx] & 0b10000000) / 128 };
                            VRegister[Vx] <<= 1;
                            VRegister[0xF] = shift;
                            printOpcode(currentOpcode, "Doubling register Vx");
                            break;
                        }
                        default:        // Invalid opcode
                        {
                            printOpcode(currentOpcode);
                            break;
                        }
                        }
                        break;
                    }
                    case 0x9000:
                    {
                        switch (currentOpcode & 0x000F)
                        {
                        case 0x0000:    // SNE Vx, VY
                        {
                            int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                            int Vy{ (currentOpcode & 0x00F0) / 0x0010 };
                            if (VRegister[Vx] != VRegister[Vy])
                            {
                                printOpcode(currentOpcode, "Skipping next instruction");
                                currentInstruction += OPCODE_LENGTH_IN_BYTES;
                            }
                            else
                            {
                                printOpcode(currentOpcode, "Not skipping next instruction");
                            }
                            break;
                        }
                        default:
                        {
                            printOpcode(currentOpcode);
                            break;
                        }
                        }
                        break;
                    }
                    case 0xA000:    // LD I, nnn
                    {
                        int value{ currentOpcode & 0x0FFF };
                        IRegister = value;
                        printOpcode(currentOpcode, "Loading value to register I");
                        break;
                    }
                    case 0xB000:    // JP V0, nnn
                    {
                        int jumpAddress{ jumpToAddress(currentOpcode) + VRegister[0] };
                        printOpcode(currentOpcode, ("Jumping to address " + std::to_string(jumpAddress + 0x0002)));
                        currentInstruction = jumpAddress;
                        break;
                    }
                    case 0xC000:    // RND Vx, nn
                    {
                        int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                        int value{ currentOpcode & 0x00FF };
                        int randomValue{ (rng(mt)) };
                        randomValue = randomValue & value;
                        VRegister[Vx] = randomValue;
                        printOpcode(currentOpcode, "Storing random number to register Vx");
                        break;
                    }
                    case 0xD000:    // DRW Vx, Vy, n
                    {
                        int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                        int Vy{ (currentOpcode & 0x00F0) / 0x0010 };

                        int xStart = VRegister[Vx];
                        int yStart = VRegister[Vy];
                        int spriteSize = (currentOpcode & 0x000F);

                        std::vector<int> spriteVector(spriteSize);

                        // load sprite into vector so we can pass it to drawToScreen
                        int n{ 0 };
                        int i{};
                        for (int i{ IRegister }; n < spriteSize; i += 0x001)
                        {
                            spriteVector.at(n) = memory[i];
                            n++;
                        }

                        // if collision is detected, set register F to 1
                        if (drawToScreen(xStart, yStart, spriteVector, screenArray, gRenderer))
                        {
                            VRegister[0xF] = 0x1;
                        }

                        // Update screen via SDL
                        SDL_RenderPresent(gRenderer);

                        printOpcode(currentOpcode, ("Drawing sprite to screen at x = " + std::to_string(xStart) + ", y = " + std::to_string(yStart)));
                        break;
                    }
                    case 0xE000:
                    {
                        switch (currentOpcode & 0x00FF)
                        {
                        case 0x009E:    // SKP Vx
                        {
                            int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                            if (keyPresses[VRegister[Vx]] == true)
                            {
                                currentInstruction += OPCODE_LENGTH_IN_BYTES;
                                printOpcode(currentOpcode, "Skipping next instruction since value is pressed");
                            }
                            else
                            {
                                printOpcode(currentOpcode, "Not skipping next instruction since value is not pressed");
                            }

                            break;
                        }
                        case 0x00A1:    // SKNP Vx
                        {
                            int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                            if (keyPresses[VRegister[Vx]] != true)
                            {
                                currentInstruction += OPCODE_LENGTH_IN_BYTES;
                                printOpcode(currentOpcode, "Skipping next instruction since value is not pressed");
                            }
                            else
                            {
                                printOpcode(currentOpcode, "Not skipping next instruction since value is pressed");
                            }
                            break;
                        }
                        default:
                        {
                            printOpcode(currentOpcode);
                            break;
                        }
                        }
                        break;
                    }
                    case 0xF000:
                    {
                        switch (currentOpcode & 0x00FF)
                        {
                        case 0x0007:    // LD Vx, DT
                        {
                            int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                            VRegister[Vx] = delayTimer;
                            printOpcode(currentOpcode, "Placing value of DT into Vx");
                            break;
                        }
                        case 0x000A:    // LD Vx, K
                        {
                            printOpcode(currentOpcode, "Waiting for key press, putting it into register Vx");
                            bool done{ false };
                            while (SDL_PollEvent(&e) != 0 && done == false)
                            {
                                if (e.type == SDL_QUIT)
                                {
                                    quit = true;
                                }
                                else if (e.type == SDL_KEYDOWN)
                                {
                                    keyPresses[Keysym_To_Key[e.key.keysym.sym]] = true;
                                    done = true;
                                }
                            }
                            break;
                        }
                        case 0x0015:    // LD DT, Vx
                        {
                            int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                            delayTimer = VRegister[Vx];
                            printOpcode(currentOpcode, "Setting DT to register Vx");
                            break;
                        }
                        case 0x0018:    // LD ST, Vx
                        {
                            int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                            soundTimer = VRegister[Vx];
                            printOpcode(currentOpcode, "Setting ST to register Vx");
                            break;
                        }
                        case 0x001E:    // ADD I, Vx
                        {
                            int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                            IRegister += VRegister[Vx];
                            printOpcode(currentOpcode, "Adding register Vx to register I");
                            break;
                        }
                        case 0x0029:    // LD F, Vx
                        {
                            int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                            IRegister = (VRegister[Vx] * 5) + 1;
                            printOpcode(currentOpcode, "Setting register I to location of sprite at Vx");
                            break;
                        }
                        case 0x0033:    // LD B, Vx
                        {
                            int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                            int hundredsDigit{ (VRegister[Vx] / 100) % 10 };
                            int tensDigit{ (VRegister[Vx] / 10) % 10 };
                            int onesDigit{ (VRegister[Vx]) % 10 };
                            memory[(IRegister)] = hundredsDigit;
                            memory[(IRegister + 0x0001)] = tensDigit;
                            memory[(IRegister + 0x0002)] = onesDigit;
                            printOpcode(currentOpcode, "Storing binary of Vx at I, I+1, and I+2");
                            break;
                        }
                        case 0x0055:    // LD [I]. Vx
                        {
                            int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                            for (int i{ 0 }; i <= Vx; i++)
                            {
                                memory[(IRegister)] = VRegister[i];
                                IRegister++;
                            }
                            printOpcode(currentOpcode, "Storing registers V0 through Vx at memory location I");
                            break;
                        }
                        case 0x0065:    // LD Vx, [I]
                        {
                            int Vx{ (currentOpcode & 0x0F00) / 0x0100 };
                            for (int i{ 0 }; i <= Vx; i++)
                            {
                                VRegister[i] = memory[(IRegister)];
                                IRegister++;
                            }
                            printOpcode(currentOpcode, "Reading registers V0 through Vx from memory location at I");
                            break;
                        }
                        default:        // Invalid opcode
                        {
                            printOpcode(currentOpcode);
                            break;
                        }
                        }
                        break;
                    }
                    default:            // Invalid opcode
                    {
                        printOpcode(currentOpcode);
                        break;
                    }
                    }

                    std::cout << "\n";
                    // DEBUG: outputting registers
                    /*for (int i{ 0 }; i < 16; i++)
                    {
                        std::cout << i << ": " << (int)VRegister[i] << "\n";
                    }
                    std::cout << "\n\n";*/

                    i++;
                }
                

                for (int i{ 0 }; i < NUMBER_OF_KEYS; i++)
                {
                    keyPresses[i] = false;
                }

            }
        }

    // cleans up SDL windows upon exit
    SDL_DestroyRenderer(gRenderer);
    SDL_DestroyWindow(gWindow);
    SDL_Quit();

    return 0;
}

