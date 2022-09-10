// TODO
#if defined(__arm64__)
#include "mo_arm64.hpp"
#include <iostream>
#include <stdatomic.h>
#include <mach/mach_vm.h>


#define NOP 0xd503201f
#define LDR_X18_PC_PLUS_8 0x58000052
#define BR_X18 0xd61f0240
#define BADCOFFE 0xBADC0FFE
#define DEADBEEF 0xDEADBEEF

ISLAND kIslandTemplate[] = {
	NOP,  // nop
	NOP,  // nop
	NOP,  // nop
	NOP,  // nop
  	LDR_X18_PC_PLUS_8,  // ldr x18, [pc + 8] #  ldr x10, =0xDEADBEEF_BADC0FFEE
	BR_X18,  // br x18
	BADCOFFE,  // BADC0FFE
	DEADBEEF,  // DEADBEEF
};

//	0x1F2003D5, //  nop

// Jump over 16 nops and ldr + br x10
uint64_t kJumpAddress=			(4 * 4 + 4 * 2);
#define kOriginalInstructionsSize 16
//#define kAddressLo			(16 * 8 + 8 * 2)
//#define kAddressHi			(kAddressLo + 8)
//#define kInstructionHi		10
//#define kInstructionLo		11
//#define kMaxJumpOffset  (0x7fffffffUL)

struct BranchIsland{
	char	instructions[sizeof(ISLAND) * sizeof(kIslandTemplate)];
	int		allocatedHigh;
};

void MO_arm64::atomic_mov64(
        ISLAND *targetAddress,
        ISLAND *value,
        size_t count)
{
    for (size_t i = 0; i < count; i++) {
        targetAddress[i] = value[i];
    }
}

mach_error_t makeIslandExecutable(void *address)
{
    auto err = err_none;
    uintptr_t page = (uintptr_t)address & ~(uintptr_t)(PAGE_SIZE - 1);
    int e = err_none;
    e |= mprotect((void *)page, PAGE_SIZE, PROT_EXEC | PROT_READ);
    e |= msync((void *)page, PAGE_SIZE, MS_SYNC | MS_INVALIDATE);
    if (e)
    {
        err = err_cannot_override;
    }
    return err;
}

void
fixupInstructions(
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

int blu() {
    int v=3;
    int w = 42-v;
    std::cout << "Adding " << std::dec << v << " + " << w << std::endl;
    return v+w;
}


mach_error_t
MO_arm64::mach_override_ptr(
	void *originalFunctionAddress,
    const void *overrideFunctionAddress,
    void **originalFunctionReentryIsland )
{
 	assert( originalFunctionAddress );
	assert( overrideFunctionAddress );

    // this addresses overriding such functions as AudioOutputUnitStart()
    // test with modified DefaultOutputUnit project
    // ported from amd64 to arm64
    for(;;){
        if(*(uint32_t*)originalFunctionAddress==0x40011FD6)    // br x10
			// TODO: understand this calculus
            originalFunctionAddress=*(void**)((char*)originalFunctionAddress+6+*(int32_t *)((uint16_t*)originalFunctionAddress+1));
        else
            break;
    }

    auto	*originalFunctionPtr = (ISLAND*) originalFunctionAddress;
	auto	err = err_none;

	//	Ensure first instruction isn't 'mfctr'.
	#define	kMFCTRMask			0xfc1fffff
	#define	kMFCTRInstruction	0x7c0903a6

	ISLAND	originalInstruction = *originalFunctionPtr;
	if( !err && ((originalInstruction & kMFCTRMask) == kMFCTRInstruction) )
		err = err_cannot_override;

    auto allocated_mem = this->vm_alloc_helper();
    err &= allocated_mem;

    //	Allocate and target the escape island to the overriding function.
	BranchIsland	*escapeIsland = nullptr;
	if( !err )
		err = allocateBranchIsland( allocated_mem, &escapeIsland, kAllocateHigh, originalFunctionAddress );
    if (err) fprintf(stderr, "err = %x %s:%d\n", err, __FILE__, __LINE__);

	if( !err )
		err = setBranchIslandTarget( escapeIsland, overrideFunctionAddress, 0 );

	//	Build the branch absolute instruction to the escape island.
	ISLAND	branchAbsoluteInstruction[4]; // Set to 0 just to silence warning.
	if( !err ) {
        branchAbsoluteInstruction[0] = LDR_X18_PC_PLUS_8;
        branchAbsoluteInstruction[1] = BR_X18;
        
        branchAbsoluteInstruction[2] = (ISLAND)((uint64_t)blu & 0xFFFFFFFFUL);
        branchAbsoluteInstruction[3] = (ISLAND)((uint64_t)blu >> 32 & 0xFFFFFFFFUL);
//        branchAbsoluteInstruction[2] = (ISLAND)((uint64_t)escapeIsland & 0xFFFFFFFFUL);
//        branchAbsoluteInstruction[3] = (ISLAND)((uint64_t)escapeIsland >> 32 & 0xFFFFFFFFUL);


//		long escapeIslandAddress = ((long) escapeIsland) & 0x3FFFFFF;
//		branchAbsoluteInstruction = 0x48000002 | escapeIslandAddress;
	}

	//	Optionally allocate & return the reentry island. This may contain relocated
	//  jmp instructions and so has all the same addressing reachability requirements
	//  the escape island has to the original function, except the escape island is
	//  technically our original function.
	BranchIsland	*reentryIsland = nullptr;
//	if( !err && originalFunctionReentryIsland ) {
//		err = allocateBranchIsland(0, &reentryIsland, kAllocateHigh, escapeIsland);
//		if( !err ) {
//            if ( !err )
//                *originalFunctionReentryIsland = reentryIsland;
//        }
//	}

	//	Atomically:
	//	o If the reentry island was allocated:
	//		o Insert the original instruction into the reentry island.
	//		o Target the reentry island at the 2nd instruction of the
	//		  original function.
	//	o Replace the original instruction with the branch absolute.
	if( !err ) {
		int escapeIslandEngaged = false;
        auto eatenBytes = 16;
		do {
			if( reentryIsland )
				err = setBranchIslandTarget( reentryIsland,
						(void*) (originalFunctionPtr + eatenBytes), originalFunctionPtr );
            if( !err )
                err = makeIslandExecutable(escapeIsland);

//            if( !err )
//                err = makeIslandExecutable(reentryIsland);
			if( !err ) {
//                std::cout << "8 aligned " << (uint64_t) originalFunctionPtr % 8 << std::endl;
                err = rw_mem(originalFunctionPtr, err);
                atomic_mov64(reinterpret_cast<uint32_t *>(originalFunctionPtr), branchAbsoluteInstruction, 4);
                err = rx_mem(originalFunctionPtr, err);
                escapeIslandEngaged = true;
//                rw_mem(originalFunctionPtr + eatenBytes, err);
//                escapeIslandEngaged = swap(originalFunctionPtr, branchAbsoluteInstruction, originalInstruction);
//                rx_mem(originalFunctionPtr, err);
			}
		} while( !err && !escapeIslandEngaged );
	}

	//	Clean up on error.
	if( err ) {
		if( reentryIsland )
			freeBranchIsland( reentryIsland );
		if( escapeIsland )
			freeBranchIsland( escapeIsland );
	}

    std::cout << "Will return with result " << err << std::endl;
	return err;
}

bool MO_arm64::swap(ISLAND* address, ISLAND newValue, ISLAND oldValue) {
    if (*address == oldValue) {
        *address = newValue;
        return true;
    }
    return false;
}

mach_error_t MO_arm64::rw_mem(const ISLAND *originalFunctionPtr, mach_error_t err) {
    if( !err ) {
        std::cout << "rw_mem, address: " << std::hex << originalFunctionPtr << std::endl;
        uintptr_t page = (uintptr_t)originalFunctionPtr & ~(uintptr_t)(PAGE_SIZE - 1);
        std::cout << "rw_mem, PAGE_SIZE: " << PAGE_SIZE<< ", page: " << std::hex << page << std::endl;
        // TODO check if we are across a page boundary
        err = mach_vm_protect( mach_task_self(),
                (vm_address_t) page,
                PAGE_SIZE,
                false,
                VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY );
    }
    if (err) {
        fprintf(stderr, "vm_protect err = %x %s:%d\n", err, __FILE__, __LINE__);
    } else {
//        auto merr = msync((void *)originalFunctionPtr, PAGE_SIZE, MS_INVALIDATE | MS_SYNC);
//        if (merr) {
//            fprintf(stderr, "msync err = %x %s:%d\n", err, __FILE__, __LINE__);
//        }
        std::cout << "rw_mem: SUCCESS" << std::endl;
    }
    return err;
}

mach_error_t MO_arm64::rx_mem(const ISLAND *originalFunctionPtr, mach_error_t err) {
    if( !err ) {
        std::cout << "rx_mem: " << std::hex << originalFunctionPtr << std::endl;
        uintptr_t page = (uintptr_t)originalFunctionPtr & ~(uintptr_t)(PAGE_SIZE - 1);
        std::cout << "rx_mem, PAGE_SIZE: " << PAGE_SIZE<< ", page: " << std::hex << page << std::endl;
        // TODO check if we are across a page boundary
        err = mach_vm_protect( mach_task_self(),
                (vm_address_t) page,
                PAGE_SIZE,
                true,
                VM_PROT_READ | VM_PROT_EXECUTE);
    }
    if (err) fprintf(stderr, "err = %x %s:%d\n", err, __FILE__, __LINE__);
    else msync((void *)originalFunctionPtr, kOriginalInstructionsSize, MS_INVALIDATE);
    return err;
}


/*******************************************************************************
	Implementation: Allocates memory for a branch island.

	@param	island			<-	The allocated island.
	@param	allocateHigh	->	Whether to allocate the island at the end of the
								address space (for use with the branch absolute
								instruction).
	@result					<-	mach_error_t

	***************************************************************************/

mach_error_t
MO_arm64::allocateBranchIsland(
        vm_address_t    allocated_mem,
		BranchIsland	**island,
		int				allocateHigh,
		void *originalFunctionAddress)
{
	assert( island );

	auto	err = err_none;

	vm_address_t page = 0;
	int allocated = allocated_mem != 0;
//    std::cout << "PAGE_SIZE: " << 0x4000 << std::endl;
    if (!allocated && allocateHigh) {
        assert(sizeof(BranchIsland) <= PAGE_SIZE);
        vm_map_t task_self;
        vm_address_t first;
        vm_alloc_helper(task_self, first, err);
        if (err == err_none) {
            *island = (BranchIsland *) first;
        }
    } else if (allocated) {
        page = allocated_mem;
        *island = (BranchIsland *) page;
    } else {
        void *block = malloc(sizeof(BranchIsland));
        if (block)
            *island = (BranchIsland *) block;
        else
            err = KERN_NO_SPACE;
    }
	if( !err )
		(**island).allocatedHigh = allocateHigh;

	return err;
}

void MO_arm64::vm_alloc_helper(vm_map_t &task_self, vm_address_t &first, mach_error_t &err) {
    task_self= mach_task_self();
    first= 0;// (uint64_t)originalFunctionAddress + kMaxJumpOffset;
    err = vm_allocate( task_self, &first, PAGE_SIZE, VM_FLAGS_ANYWHERE );
}

vm_address_t MO_arm64::vm_alloc_helper() {
    vm_map_t task_self;
    vm_address_t first;
    auto err = err_none;
    vm_alloc_helper(task_self, first, err);
    std::cout << "err = " << err << std::endl;
    return first;
}

/*******************************************************************************
	Implementation: Deallocates memory for a branch island.

	@param	island	->	The island to deallocate.
	@result			<-	mach_error_t

	***************************************************************************/

mach_error_t
MO_arm64::freeBranchIsland(
		BranchIsland	*island )
{
	assert( island );
//	assert( (*(ISLAND*)&island->instructions[0]) == kIslandTemplate[0] );
//	assert( island->allocatedHigh );

	mach_error_t	err = err_none;

	if( island->allocatedHigh ) {
		assert( sizeof( BranchIsland ) <= PAGE_SIZE );
		err = vm_deallocate(mach_task_self(), (vm_address_t) island, PAGE_SIZE );
//		free( island );
	}

	return err;
}

/*******************************************************************************
	Implementation: Sets the branch island's target, with an optional
	instruction.

	@param	island		->	The branch island to insert target into.
	@param	branchTo	->	The address of the target.
	@param	instruction	->	Optional instruction to execute prior to branch. Set
							to zero for nop.
	@result				<-	mach_error_t

	***************************************************************************/

mach_error_t
MO_arm64::setBranchIslandTarget(
        BranchIsland *island,
        const void *branchTo,
        ISLAND *instruction) {
    size_t size = sizeof(kIslandTemplate);
    std::cout << "sizeof(ISLAND): " <<  sizeof(ISLAND) << std::endl;
    std::cout << "sizeof(kIslandTemplate): " <<  sizeof(kIslandTemplate) << std::endl;
    std::cout << "setBranchIslandTarget: TODO, size: " << size << std::endl;
    //	Copy over the template code. param order is reversed vs. bcopy
    memmove(island->instructions, kIslandTemplate, size);
    //	Fill in the address.
    *((uint64_t *)(island->instructions + kJumpAddress)) = (uint64_t)branchTo;

//    ((short*)island->instructions)[kAddressLo] = ((long) branchTo) & 0x0000FFFF;
//    ((short*)island->instructions)[kAddressHi]
//            = (((long) branchTo) >> 16) & 0x0000FFFF;

    //	Fill in the (optional) instructions.
    if( instruction ) {
//        char* dst = island->instructions;
//        char* src = (char*)instruction;
//        for (auto i=0; i<kOriginalInstructionsSize; ++i) {
//            std::cout << "Copy instruction #" << i << std::endl;
//            std::cout << "Value: " << *src << std::endl;
//            *dst = *src;
//            dst++;
//            src++;
//        }
//        bcopy(instruction, island->instructions, kOriginalInstructionsSize);
        memmove(island->instructions, instruction, kOriginalInstructionsSize);
    }

    //MakeDataExecutable( island->instructions, sizeof( kIslandTemplate ) );
    msync( island->instructions, sizeof( kIslandTemplate ), MS_SYNC | MS_INVALIDATE );
    return err_none;
}

#endif
