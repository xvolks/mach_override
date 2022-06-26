#include "../mo_x86.hpp"

#if defined(__x86_64__)

class MO_amd64 : public MO_x86
{
private:
    virtual ISLAND *get_island();
public:
    MO_amd64() = default;
    ~MO_amd64() = default;

    virtual mach_error_t
    mach_override_ptr(
        void *originalFunctionAddress,
        const void *overrideFunctionAddress,
        void **originalFunctionReentryIsland);

    virtual mach_error_t
    allocateBranchIsland(
      BranchIsland	**island,
      int				allocateHigh,
      void *originalFunctionAddress);

    virtual mach_error_t
    freeBranchIsland(
  		BranchIsland	*island );

    virtual mach_error_t
    setBranchIslandTarget(
      BranchIsland	*island,
      const void		*branchTo,
      ISLAND*			instruction );

    virtual void
    atomic_mov64(
        uint64_t *targetAddress,
        uint64_t value );

};

#endif