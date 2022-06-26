#if defined(__arm64__)

#include "../mo_interface.hpp"

class MO_arm64 : public MO {
protected:
    // virtual ISLAND* get_island();
    mach_error_t
    setBranchIslandTarget(
            BranchIsland *island,
            const void *branchTo,
            ISLAND *instruction) override;

    static bool
    swap(ISLAND* address, ISLAND newValue, ISLAND oldValue);
    static mach_error_t rw_mem(const ISLAND *originalFunctionPtr, mach_error_t err);
    static mach_error_t rx_mem(const ISLAND *originalFunctionPtr, mach_error_t err);

public:
    MO_arm64() : MO() {};

    ~MO_arm64() noexcept override = default;

    virtual void atomic_mov64(
            ISLAND *targetAddress,
            ISLAND *value,
            size_t count);

    // Unused here
    mach_error_t allocateBranchIsland(
            BranchIsland **island,
            int allocateHigh,
            void *originalFunctionAddress) override {}

    virtual mach_error_t allocateBranchIsland(
            vm_address_t allocated_mem,
            BranchIsland **island,
            int allocateHigh,
            void *originalFunctionAddress);

    mach_error_t freeBranchIsland(
            BranchIsland *island) override;

    mach_error_t mach_override_ptr(
            void *originalFunctionAddress,
            const void *overrideFunctionAddress,
            void **originalFunctionReentryIsland) override;

    static void vm_alloc_helper(vm_map_t &task_self, vm_address_t &first, mach_error_t &err) ;

    static vm_address_t vm_alloc_helper() ;
};

#endif