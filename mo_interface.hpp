#ifndef MO_INTERFACE_HPP
#define MO_INTERFACE_HPP

#include <libkern/OSAtomic.h>
#include <mach-o/dyld.h>
#include <mach/mach_init.h>
#include <mach/vm_map.h>
#include <mach/vm_statistics.h>
#include <sys/mman.h>

#include <CoreServices/CoreServices.h>

#if defined(__ppc__) || defined(__POWERPC__) || defined(__arm64__)
#define ISLAND uint32_t
#else
#define ISLAND unsigned char
#endif

#define	err_cannot_override	(err_max_system|0x40)


/**************************
*
*	Constants
*
**************************/
#pragma mark	-
#pragma mark	(Constants)
#define	kAllocateHigh		1
#define	kAllocateNormal		0

/**************************
*
*	Data Types
*
**************************/
#pragma mark	-
#pragma mark	(Data Types)

struct BranchIsland;

class MO {
private:
    static MO *mo;

protected:
    // virtual ISLAND* get_island();
    MO() = default;

public:
    virtual ~MO() noexcept {};

    static MO *get();

    virtual mach_error_t
    allocateBranchIsland(
		BranchIsland	**island,
		int				allocateHigh,
		void *originalFunctionAddress) = 0;

	virtual mach_error_t
    freeBranchIsland(
		BranchIsland	*island ) = 0;

    /****************************************************************************************
	Dynamically overrides the function implementation referenced by
	originalFunctionAddress with the implentation pointed to by overrideFunctionAddress.
	Optionally returns a pointer to a "reentry island" which, if jumped to, will resume
	the original implementation.
	
	@param	originalFunctionAddress			->	Required address of the function to
												override (with overrideFunctionAddress).
	@param	overrideFunctionAddress			->	Required address to the overriding
												function.
	@param	originalFunctionReentryIsland	<-	Optional pointer to pointer to the
												reentry island. Can be NULL.
	@result									<-	err_cannot_override if the original
												function's implementation begins with
												the 'mfctr' instruction.

	************************************************************************************/
    virtual mach_error_t
    mach_override_ptr(
        void *originalFunctionAddress,
        const void *overrideFunctionAddress,
        void **originalFunctionReentryIsland) = 0;

    virtual mach_error_t
    setBranchIslandTarget(
        BranchIsland	*island,
        const void		*branchTo,
        ISLAND*			instruction ) = 0;

};

#endif