#include <MacTypes.h>
#include "../mach_override.h"
#include "../mo_interface.hpp"
#include "../udis86.h"

#if defined(__i386__)
#define kOriginalInstructionsSize 16
#else
#define kOriginalInstructionsSize 32
#endif



class MO_x86 : public MO
{
protected:
    // virtual ISLAND *get_island() = 0;
    virtual mach_error_t
    setBranchIslandTarget(
        BranchIsland *island,
        const void *branchTo,
        ISLAND *instructions) = 0;

    static Boolean
    eatKnownInstructions(
        unsigned char *code,
        uint64_t *newInstruction,
        int *howManyEaten,
        unsigned char *originalInstructions,
        int *originalInstructionCount,
        uint8_t *originalInstructionSizes);

    static void
    fixupInstructions(
        void *originalFunction,
        void *escapeIsland,
        void *instructionsToFix,
        int instructionCount,
        uint8_t *instructionSizes);

    mach_error_t makeIslandExecutable(void *address)
    {
        mach_error_t err = err_none;
        uintptr_t page = (uintptr_t)address & ~(uintptr_t)(PAGE_SIZE - 1);
        int e = err_none;
        e |= mprotect((void *)page, PAGE_SIZE, PROT_EXEC | PROT_READ);
        e |= msync((void *)page, PAGE_SIZE, MS_INVALIDATE);
        if (e)
        {
            err = err_cannot_override;
        }
        return err;
    }

public:
    MO_x86() : MO() {}
    virtual ~MO_x86() = default;

    virtual mach_error_t allocateBranchIsland(
        BranchIsland **island,
        int allocateHigh,
        void *originalFunctionAddress) = 0;
    virtual mach_error_t freeBranchIsland(
        BranchIsland *island) = 0;

    virtual mach_error_t
    mach_override_ptr(
        void *originalFunctionAddress,
        const void *overrideFunctionAddress,
        void **originalFunctionReentryIsland) = 0;

    virtual void
    atomic_mov64(
        uint64_t *targetAddress,
        uint64_t value ) = 0;

};
