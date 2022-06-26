#if defined(__x86_64__)

#include "mo_amd64.hpp"

// #define kOriginalInstructionsSize 32

#define kJumpAddress    kOriginalInstructionsSize + 6
#define kMaxJumpOffset  (0x7fffffffUL)

ISLAND kIslandTemplate[] = {
	// kOriginalInstructionsSize nop instructions so that we
	// should have enough space to host original instructions
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
	// Now the real jump instruction
	0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
};

struct BranchIsland {
	char	instructions[sizeof(kIslandTemplate)];
	int		allocatedHigh;
};


ISLAND* 
MO_amd64::get_island()
{
    return kIslandTemplate;
}

mach_error_t
MO_amd64::mach_override_ptr(
	void *originalFunctionAddress,
    const void *overrideFunctionAddress,
    void **originalFunctionReentryIsland )
{
	assert( originalFunctionAddress );
	assert( overrideFunctionAddress );

	// this addresses overriding such functions as AudioOutputUnitStart()
	// test with modified DefaultOutputUnit project
    for(;;){
        if(*(uint16_t*)originalFunctionAddress==0x25FF)    // jmp qword near [rip+0x????????]
            originalFunctionAddress=*(void**)((char*)originalFunctionAddress+6+*(int32_t *)((uint16_t*)originalFunctionAddress+1));
        else break;
    }

	long	*originalFunctionPtr = (long*) originalFunctionAddress;
	mach_error_t	err = err_none;

	int eatenCount = 0;
	int originalInstructionCount = 0;
	unsigned char originalInstructions[kOriginalInstructionsSize];
	uint8_t originalInstructionSizes[kOriginalInstructionsSize];
	uint64_t jumpRelativeInstruction = 0; // JMP

	Boolean overridePossible = eatKnownInstructions ((unsigned char *)originalFunctionPtr,
										&jumpRelativeInstruction, &eatenCount,
										originalInstructions, &originalInstructionCount,
										originalInstructionSizes );
	if (eatenCount > kOriginalInstructionsSize) {
		//printf ("Too many instructions eaten\n");
		overridePossible = false;
	}
	if (!overridePossible) err = err_cannot_override;
	if (err) fprintf(stderr, "err = %x %s:%d\n", err, __FILE__, __LINE__);

	//	Make the original function implementation writable.
	if( !err ) {
		err = vm_protect( mach_task_self(),
				(vm_address_t) originalFunctionPtr, 8, false,
				(VM_PROT_ALL | VM_PROT_COPY) );
		if( err )
			err = vm_protect( mach_task_self(),
					(vm_address_t) originalFunctionPtr, 8, false,
					(VM_PROT_DEFAULT | VM_PROT_COPY) );
	}
	if (err) fprintf(stderr, "err = %x %s:%d\n", err, __FILE__, __LINE__);

	//	Allocate and target the escape island to the overriding function.
	BranchIsland	*escapeIsland = NULL;
	if( !err )
		err = allocateBranchIsland( &escapeIsland, kAllocateHigh, originalFunctionAddress );
    if (err) fprintf(stderr, "err = %x %s:%d\n", err, __FILE__, __LINE__);


	if( !err )
		err = setBranchIslandTarget( escapeIsland, overrideFunctionAddress, 0 );

	if (err) fprintf(stderr, "err = %x %s:%d\n", err, __FILE__, __LINE__);
	// Build the jump relative instruction to the escape island

	if (!err) {
//		uint32_t addressOffset = ((char*)escapeIsland - (char*)originalFunctionPtr - 5);

		// TODO: On 64-bit, move to opcode FF/4 (jmp 64-bit absolute indirect)
		// instead of E9 (jmp 32-bit relative to RIP). Then we should update
		// allocateBranchIsland to simply allocate any page in the address space.
		// See the 64-bit version of kIslandTemplate array.
		int64_t addressOffset64 = ((char*)escapeIsland - (char*)originalFunctionPtr - 5);
		int32_t addressOffset = addressOffset64;
		assert(addressOffset64 == addressOffset);

		addressOffset = OSSwapInt32(addressOffset);

		jumpRelativeInstruction |= 0xE900_0000_0000_0000LL;
		jumpRelativeInstruction |= ((uint64_t)addressOffset & 0xffffffff) << 24;
		jumpRelativeInstruction = OSSwapInt64(jumpRelativeInstruction);
	}

	//	Optionally allocate & return the reentry island. This may contain relocated
	//  jmp instructions and so has all the same addressing reachability requirements
	//  the escape island has to the original function, except the escape island is
	//  technically our original function.
	BranchIsland	*reentryIsland = NULL;
	if( !err && originalFunctionReentryIsland ) {
		err = allocateBranchIsland( &reentryIsland, kAllocateHigh, escapeIsland);
		if( !err )
			*originalFunctionReentryIsland = reentryIsland;
	}

	// Atomically:
	//	o If the reentry island was allocated:
	//		o Insert the original instructions into the reentry island.
	//		o Target the reentry island at the first non-replaced
	//        instruction of the original function.
	//	o Replace the original first instructions with the jump relative.
	//
	// Note that on i386, we do not support someone else changing the code under our feet
	if ( !err ) {
		fixupInstructions(originalFunctionPtr, reentryIsland, originalInstructions,
					originalInstructionCount, originalInstructionSizes );

		if( reentryIsland )
			err = setBranchIslandTarget( reentryIsland,
										 (void*) ((char *)originalFunctionPtr+eatenCount), originalInstructions );
		// try making islands executable before planting the jmp
        if( !err )
            err = makeIslandExecutable(escapeIsland);
        if( !err && reentryIsland )
            err = makeIslandExecutable(reentryIsland);
		if ( !err )
			atomic_mov64((uint64_t *)originalFunctionPtr, jumpRelativeInstruction);
		mach_error_t prot_err = err_none;
		prot_err = vm_protect( mach_task_self(),
				       (vm_address_t) originalFunctionPtr, 8, false,
				       (VM_PROT_READ | VM_PROT_EXECUTE) );
		if(prot_err) fprintf(stderr, "err = %x %s:%d\n", prot_err, __FILE__, __LINE__);
	}

	//	Clean up on error.
	if( err ) {
		if( reentryIsland )
			freeBranchIsland( reentryIsland );
		if( escapeIsland )
			freeBranchIsland( escapeIsland );
	}

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
MO_amd64::allocateBranchIsland(
		BranchIsland	**island,
		int				allocateHigh,
		void *originalFunctionAddress)
{
	assert( island );

	mach_error_t	err = err_none;

	if( allocateHigh ) {
		assert( sizeof( BranchIsland ) <= PAGE_SIZE );
		vm_address_t page = 0;
		// This logic is more complex due to the 32-bit limit of the jump out
		// of the original function. Once that limitation is removed, we can
		// use vm_allocate with VM_FLAGS_ANYWHERE as in the i386 code above.
		const uint64_t kPageMask = ~(PAGE_SIZE - 1);
		vm_address_t first = (uint64_t)originalFunctionAddress - kMaxJumpOffset;
		first = (first & kPageMask) + PAGE_SIZE; // Align up to the next page start
		if (first > (uint64_t)originalFunctionAddress) first = 0;
		vm_address_t last = (uint64_t)originalFunctionAddress + kMaxJumpOffset;
		if (last < (uint64_t)originalFunctionAddress) last = ULONG_MAX;

		page = first;
		int allocated = 0;
		vm_map_t task_self = mach_task_self();

		while( !err && !allocated && page < last ) {

			err = vm_allocate( task_self, &page, PAGE_SIZE, 0 );
			if( err == err_none )
				allocated = 1;
			else if( err == KERN_NO_SPACE || err == KERN_INVALID_ADDRESS) {
                // This memory region is not suitable, skip it:
                vm_size_t region_size;
                mach_msg_type_number_t int_count = VM_REGION_BASIC_INFO_COUNT_64;
                vm_region_basic_info_data_64_t vm_region_info;
                mach_port_t object_name;
                // The call will move 'page' to the beginning of the region:
                err = vm_region_64(task_self, &page, &region_size,
                            VM_REGION_BASIC_INFO_64, (vm_region_info_t)&vm_region_info,
                            &int_count, &object_name);
                if (err == KERN_SUCCESS)
                    page += region_size;
                else
                    break;
                    err = err_none;
			}
		}
		if( allocated )
			*island = (BranchIsland*) page;
		else if( !allocated && !err )
			err = KERN_NO_SPACE;
	} else {
		void *block = malloc( sizeof( BranchIsland ) );
		if( block )
			*island = (BranchIsland*) block;
		else
			err = KERN_NO_SPACE;
	}
	if( !err )
		(**island).allocatedHigh = allocateHigh;

	return err;
}

/*******************************************************************************
	Implementation: Deallocates memory for a branch island.

	@param	island	->	The island to deallocate.
	@result			<-	mach_error_t

	***************************************************************************/

mach_error_t
MO_amd64::freeBranchIsland(
		BranchIsland	*island )
{
	assert( island );
	assert( (*(long*)&island->instructions[0]) == kIslandTemplate[0] );
	assert( island->allocatedHigh );

	mach_error_t	err = err_none;

	if( island->allocatedHigh ) {
		assert( sizeof( BranchIsland ) <= PAGE_SIZE );
		err = vm_deallocate(mach_task_self(), (vm_address_t) island, PAGE_SIZE );
	} else {
		free( island );
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
MO_amd64::setBranchIslandTarget(
        BranchIsland	*island,
        const void		*branchTo,
        ISLAND*			instructions )
{
    // Copy over the template code.
    bcopy( kIslandTemplate, island->instructions, sizeof( kIslandTemplate ) );

    // Copy original instructions.
    if (instructions) {
        bcopy (instructions, island->instructions, kOriginalInstructionsSize);
    }

    //	Fill in the address.
    *((uint64_t *)(island->instructions + kJumpAddress)) = (uint64_t)branchTo;
    msync( island->instructions, sizeof( kIslandTemplate ), MS_INVALIDATE );

    return err_none;
}

void MO_amd64::atomic_mov64(
		uint64_t *targetAddress,
		uint64_t value )
{
    *targetAddress = value;
}


#endif