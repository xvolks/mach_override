#if defined(__ppc__) || defined(__POWERPC__)

#include "../mo_interface.hpp"

long kIslandTemplate[] = {
	0x9001FFFC,	//	stw		r0,-4(SP)
	0x3C00DEAD,	//	lis		r0,0xDEAD
	0x6000BEEF,	//	ori		r0,r0,0xBEEF
	0x7C0903A6,	//	mtctr	r0
	0x8001FFFC,	//	lwz		r0,-4(SP)
	0x60000000,	//	nop		; optionally replaced
	0x4E800420 	//	bctr
};

#define kAddressHi			3
#define kAddressLo			5
#define kInstructionHi		10
#define kInstructionLo		11

typedef	struct	{
	char	instructions[sizeof(kIslandTemplate)];
	int		allocatedHigh;
}	BranchIsland;


class MO_ppc : public MO_Interface {
    private:
        virtual ISLAND* get_island();
        mach_error_t
        setBranchIslandTarget(
            BranchIsland *island,
            const void *branchTo,
            long instruction);

    public:
        MO_ppc();
        virtual ~MO_ppc();
        virtual mach_error_t allocateBranchIsland(
            BranchIsland **island,
            int allocateHigh,
            void *originalFunctionAddress);
        virtual mach_error_t freeBranchIsland(
            BranchIsland *island);
};

#endif