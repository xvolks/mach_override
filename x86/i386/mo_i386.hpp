#include "../mo_x86.hpp"

#if defined(__i386__)

#define kOriginalInstructionsSize 16

char kIslandTemplate[] = {
    // kOriginalInstructionsSize nop instructions so that we
    // should have enough space to host original instructions
    0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
    0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
    // Now the real jump instruction
    0xE9, 0xEF, 0xBE, 0xAD, 0xDE};

#define kInstructions 0
#define kJumpAddress kInstructions + kOriginalInstructionsSize + 1

class MO_i386 : public MO_x86
{
private:
    static ISLAND kIslandTemplate[];
    virtual ISLAND *get_island();
};

#endif
