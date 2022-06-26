// mach_override.c semver:1.2.0
//   Copyright (c) 2003-2012 Jonathan 'Wolf' Rentzsch: http://rentzsch.com
//   Some rights reserved: http://opensource.org/licenses/mit
//   https://github.com/rentzsch/mach_override

#include "mo_interface.hpp"

#include "mach_override.h"
#if defined(__i386__)
#include "x86/i386/mo_i386.hpp"
MO* MO::mo = new MO_i386();
#elif defined(__x86_64__)
#include "x86/amd64/mo_amd64.hpp"
MO* MO::mo = new MO_amd64();
#elif defined(__arm64__)
#include "arm64/mo_arm64.hpp"
MO* MO::mo = new MO_arm64();
#elif defined(__ppc__)
#include "ppc/mo_ppc.hpp"
MO* MO::mo = new MO_ppc();
#else
MO* MO::mo = nullptr;
#endif

MO* MO::get() {
    return mo;
}


/*******************************************************************************
*
*	Interface
*
*******************************************************************************/
#pragma mark	-
#pragma mark	(Interface)



/*******************************************************************************
*
*	Implementation
*
*******************************************************************************/
#pragma mark	-
#pragma mark	(Implementation)
