#include "mo_x86.hpp"

Boolean
MO_x86::eatKnownInstructions(
	unsigned char	*code,
	uint64_t		*newInstruction,
	int				*howManyEaten,
	unsigned char	*originalInstructions,
	int				*originalInstructionCount,
	uint8_t			*originalInstructionSizes )
{
	Boolean allInstructionsKnown = true;
	int totalEaten = 0;
	int remainsToEat = 5; // a JMP instruction takes 5 bytes
	int instructionIndex = 0;
	ud_t ud_obj;

	if (howManyEaten) *howManyEaten = 0;
	if (originalInstructionCount) *originalInstructionCount = 0;
	ud_init(&ud_obj);
#if defined(__i386__)
	ud_set_mode(&ud_obj, 32);
#else
	ud_set_mode(&ud_obj, 64);
#endif
	ud_set_input_buffer(&ud_obj, code, 64); // Assume that 'code' points to at least 64bytes of data.
	while (remainsToEat > 0) {
		if (!ud_disassemble(&ud_obj)) {
		    allInstructionsKnown = false;
		    fprintf(stderr, "mach_override: some instructions unknown! Need to update libudis86\n");
		    break;
		}

		// At this point, we've matched curInstr
		int eaten = ud_insn_len(&ud_obj);
		remainsToEat -= eaten;
		totalEaten += eaten;

		if (originalInstructionSizes) originalInstructionSizes[instructionIndex] = eaten;
		instructionIndex += 1;
		if (originalInstructionCount) *originalInstructionCount = instructionIndex;
	}


	if (howManyEaten) *howManyEaten = totalEaten;

	if (originalInstructions) {
		Boolean enoughSpaceForOriginalInstructions = (totalEaten < kOriginalInstructionsSize);

		if (enoughSpaceForOriginalInstructions) {
			memset(originalInstructions, 0x90 /* NOP */, kOriginalInstructionsSize); // fill instructions with NOP
			bcopy(code, originalInstructions, totalEaten);
		} else {
			// printf ("Not enough space in island to store original instructions. Adapt the island definition and kOriginalInstructionsSize\n");
			return false;
		}
	}

	if (allInstructionsKnown) {
		// save last 3 bytes of first 64bits of codre we'll replace
		uint64_t currentFirst64BitsOfCode = *((uint64_t *)code);
		currentFirst64BitsOfCode = OSSwapInt64(currentFirst64BitsOfCode); // back to memory representation
		currentFirst64BitsOfCode &= 0x0000000000FFFFFFLL;

		// keep only last 3 instructions bytes, first 5 will be replaced by JMP instr
		*newInstruction &= 0xFFFFFFFFFF000000LL; // clear last 3 bytes
		*newInstruction |= (currentFirst64BitsOfCode & 0x0000000000FFFFFFLL); // set last 3 bytes
	}

	return allInstructionsKnown;
}

void
MO_x86::fixupInstructions(
    void		*originalFunction,
    void		*escapeIsland,
    void		*instructionsToFix,
	int			instructionCount,
	uint8_t		*instructionSizes )
{
	int	index;
	for (index = 0;index < instructionCount;index += 1)
	{
		if (*(uint8_t*)instructionsToFix == 0xE9) // 32-bit jump relative
		{
			uint32_t offset = (uintptr_t)originalFunction - (uintptr_t)escapeIsland;
			uint32_t *jumpOffsetPtr = (uint32_t*)((uintptr_t)instructionsToFix + 1);
			*jumpOffsetPtr += offset;
		}

		originalFunction = (void*)((uintptr_t)originalFunction + instructionSizes[index]);
		escapeIsland = (void*)((uintptr_t)escapeIsland + instructionSizes[index]);
		instructionsToFix = (void*)((uintptr_t)instructionsToFix + instructionSizes[index]);
    }
}
