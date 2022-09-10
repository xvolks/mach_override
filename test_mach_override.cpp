#include <cstdio>
#include <cassert>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <CoreServices/CoreServices.h>
#include "mach_override.h"

#define	assertStrEqual( EXPECTED, ACTUAL ) if( strcmp( (EXPECTED), (ACTUAL) ) != 0 ) { printf( "EXPECTED: %s\nACTUAL: %s\n", (EXPECTED), (ACTUAL)); assert( strcmp( (EXPECTED), (ACTUAL) ) == 0 ); }
#define	assertIntEqual( EXPECTED, ACTUAL ) if( (EXPECTED) != (ACTUAL) ) { printf( "EXPECTED: %d\nACTUAL: %d\n", (EXPECTED), (ACTUAL)); assert( (EXPECTED) == (ACTUAL) ); }

//------------------------------------------------------------------------------
#pragma mark Test Local Override by Pointer

struct NaiveTrace {
    const char* _message;
    explicit NaiveTrace(const char * message) {
        this->_message = message;
        std::cout << "enter " << message << std::endl;
    }
    ~NaiveTrace() {
        std::cout << "exit " << this->_message << std::endl;
    }
};

const char* localFunction() {
	asm("nop;nop;nop;nop;");
	return __FUNCTION__;
}
const char* (*localOriginalPtr)() = localFunction;

void testLocalFunctionOverrideByPointer() {
    NaiveTrace _("testLocalFunctionOverrideByPointer()");
	//	Test original.
	assertStrEqual( "localFunction", localOriginalPtr() );

	//	Override local function by pointer.
	kern_return_t err;
	
	MACH_OVERRIDE( const char*, localFunction, (), err ) {
		//	Test calling through the reentry island back into the original
		//	implementation.
		assertStrEqual( "localFunction", localFunction_reenter() );
		
		return "localFunctionOverride";
	} END_MACH_OVERRIDE(localFunction);
	assert( !err );
	
	//	Test override took effect.
	assertStrEqual( "localFunctionOverride", localOriginalPtr() );
}

//------------------------------------------------------------------------------
#pragma mark Test System Override by Pointer

char* (*strerrorPtr)(int) = strerror;
const char* strerrReturnValue = "Unknown error: 0";

void testSystemFunctionOverrideByPointer() {
    NaiveTrace _("testSystemFunctionOverrideByPointer()");

    SInt32 sysv;
	if (Gestalt( gestaltSystemVersion, &sysv ) == noErr && sysv >= 0x1070)
		strerrReturnValue = "Undefined error: 0";

	//	Test original.
	assertStrEqual( strerrReturnValue, strerrorPtr( 0 ) );
	
	//	Override system function by pointer.
	kern_return_t err;
	MACH_OVERRIDE( char*, strerror, (int errnum), err ) {
		//	Test calling through the reentry island back into the original
		//	implementation.
		assertStrEqual( strerrReturnValue, strerror_reenter( 0 ) );
		
		return (char *)"strerrorOverride";
	} END_MACH_OVERRIDE(strerror);
	assert( !err );
	
	//	Test override took effect.
	assertStrEqual( "strerrorOverride", strerrorPtr( 0 ) );
}

//------------------------------------------------------------------------------
#pragma mark Test System Override by Name

/* The following is commented out because it does not compile.
int strerror_rOverride( int errnum, char *strerrbuf, size_t buflen );
int (*strerror_rPtr)( int, char*, size_t ) = strerror_r;
int (*gReentry_strerror_r)( int, char*, size_t );

void testSystemFunctionOverrideByName() {
	//	Test original.
	assertIntEqual( ERANGE, strerror_rPtr( 0, NULL, 0 ) );
	
	//	Override local function by pointer.
	kern_return_t err = mach_override( (char*)"_strerror_r",
									   NULL,
									   (void*)&strerror_rOverride,
									   (void**)&gReentry_strerror_r );
	
	//	Test override took effect.
	assertIntEqual( 0, strerror_rPtr( 0, NULL, 0 ) );
}

int strerror_rOverride( int errnum, char *strerrbuf, size_t buflen ) {
	assertIntEqual( ERANGE, gReentry_strerror_r( 0, NULL, 0 ) );
	
	return 0;
}
*/

//------------------------------------------------------------------------------
#pragma mark Test VM allocate
vm_address_t test_vm_allocate_anywhere() {
    NaiveTrace t("test_vm_allocate_anywhere()");
    vm_map_t task_self = mach_task_self();
    vm_address_t first = (uint64_t) 0;
    auto err = vm_allocate( task_self, &first, PAGE_SIZE, VM_FLAGS_ANYWHERE );
    if (err == err_none) {
        std::cout << "test_vm_allocate_anywhere: success" << std::endl;
        return first;
    } else {
        std::cout << "test_vm_allocate_anywhere: failed code " << err << std::endl;
        return 0;
    }
}

void rw_mem(vm_address_t address, mach_error_t& err)  {
    if( !err ) {
        std::cout << "rw_mem: " << std::hex << address << std::endl;
        uintptr_t page = (uintptr_t)address & ~(uintptr_t)(PAGE_SIZE - 1);
        std::cout << "rw_mem: " << std::hex << page << std::endl;

        err = vm_protect(mach_task_self(),
                         page, PAGE_SIZE, false, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY);
    }
    if (err) fprintf(stdout, "err = %x %s:%d\n", err, __FILE__, __LINE__);
}

void rx_mem(vm_address_t originalFunctionPtr, mach_error_t& err)  {
    if( !err ) {
        std::cout << "rx_mem: " << std::hex << originalFunctionPtr << std::endl;
        err = vm_protect( mach_task_self(),
                          originalFunctionPtr, 8, false,VM_PROT_READ | VM_PROT_EXECUTE );
    }
    if (err) fprintf(stdout, "err = %x %s:%d\n", err, __FILE__, __LINE__);
}

int (*fun_ptr)();

int base_function() {
    std::cout << "base_function" << std::endl;
    return 3;
}

void test_vm_protect(vm_address_t address) {
    NaiveTrace t("test_vm_protect()");
    auto err = err_none;
    rw_mem(address, err);
    auto nop = 0xd503201f;
    auto mov_x0_42 = 0xd2800540;
    auto ret = 0xd65f03c0;
    auto ptr = (uint32_t *)address;
//    *ptr = 0xDEADBEEF0BADF00D;
    *ptr++ = nop;
    *ptr++ = nop;
    *ptr++ = mov_x0_42;
    *ptr++ = ret;
    *ptr++ = nop;
    rx_mem(address, err);
    fun_ptr = base_function;
    std::cout << "The answer is = " << std::dec << fun_ptr() << std::endl;
    fun_ptr = (int (*)())address;
    auto a = fun_ptr();
    std::cout << "The answer is = " << std::dec << a << std::endl;
    assert(a == 42);
}

void test_overwrite_function() {
    NaiveTrace t("test_overwrite_function()");
    auto err = err_none;
    auto address = base_function;
    test_vm_protect(reinterpret_cast<vm_address_t>(address));
}

int bla() {
    int v=3;
    int w = 42-v;
    std::cout << "Adding " << std::dec << v << " + " << w << std::endl;
    return v+w;
}




//------------------------------------------------------------------------------
#pragma mark main

int main( int argc, const char *argv[] ) {
    auto v = base_function();
    std::cout << "The answer is = " << std::dec << v << std::endl;

    vm_address_t address = test_vm_allocate_anywhere();
    if (address) {
        test_vm_protect(address);
        mach_error_t err = vm_deallocate( mach_task_self(), address, PAGE_SIZE);
        if (err == err_none) {
            std::cout << "test_vm_deallocate: success" << std::endl;
        } else {
            std::cout << "test_vm_deallocate: failed code " << err << std::endl;
        }

    }

    test_overwrite_function();


    auto answer = base_function();
    std::cout << "The answer is = " << std::dec << answer << std::endl;
    
    kern_return_t err;
    MACH_OVERRIDE(int, base_function, (), err) {
        return 2;
    }
    END_MACH_OVERRIDE(base_function);

    auto result = base_function();
    std::cout << "The answer is = " << std::dec << result << std::endl;

//    testLocalFunctionOverrideByPointer();
//	testSystemFunctionOverrideByPointer();
	//testSystemFunctionOverrideByName();
	
	printf( "success\n" );
	return 0;
}
