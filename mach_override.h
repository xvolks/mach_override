// mach_override.h semver:1.2.0
//   Copyright (c) 2003-2012 Jonathan 'Wolf' Rentzsch: http://rentzsch.com
//   Some rights reserved: http://opensource.org/licenses/mit
//   https://github.com/rentzsch/mach_override

#ifndef		_mach_override_
#define		_mach_override_

#include <sys/types.h>
#include <mach/error.h>
#include "mo_interface.hpp"

__BEGIN_DECLS

__END_DECLS

/****************************************************************************************
	If you're using C++ this macro will ease the tedium of typedef'ing, naming, keeping
	track of reentry islands and defining your override code. See test_mach_override.cp
	for example usage.

	************************************************************************************/
 
#ifdef	__cplusplus
#define MACH_OVERRIDE( ORIGINAL_FUNCTION_RETURN_TYPE, ORIGINAL_FUNCTION_NAME, ORIGINAL_FUNCTION_ARGS, ERR )		\
{																												\
	static ORIGINAL_FUNCTION_RETURN_TYPE (*ORIGINAL_FUNCTION_NAME##_reenter)ORIGINAL_FUNCTION_ARGS;				\
	static bool ORIGINAL_FUNCTION_NAME##_overriden = false;													    \
	class mach_override_class__##ORIGINAL_FUNCTION_NAME {														\
	public:																										\
		static kern_return_t override(void *originalFunctionPtr) {												\
			kern_return_t result = err_none;																	\
			if (!ORIGINAL_FUNCTION_NAME##_overriden) {															\
                std::cout << "Will override and store result at address " << &result << std::endl;              \
				ORIGINAL_FUNCTION_NAME##_overriden = true;														\
                result = MO::get()->mach_override_ptr( (void*)originalFunctionPtr,						        \
											(void*)mach_override_class__##ORIGINAL_FUNCTION_NAME::replacement,	\
											(void**)&ORIGINAL_FUNCTION_NAME##_reenter );						\
			}																									\
			return result;																						\
		}																										\
		static ORIGINAL_FUNCTION_RETURN_TYPE replacement ORIGINAL_FUNCTION_ARGS {

#define END_MACH_OVERRIDE( ORIGINAL_FUNCTION_NAME )																\
		}																										\
	};																											\
																												\
	err = mach_override_class__##ORIGINAL_FUNCTION_NAME::override((void*)ORIGINAL_FUNCTION_NAME);				\
}
#endif

#endif	//	_mach_override_
