#ifndef __PSEH_C__
#define __PSEH_C__
#include "pseh.h"

#if !defined(PSEH_DISABLE_HANDLING)
#if !defined(PSEH_USE_NATIVE_SEH)
	PSEH_THREADLOCAL pseh_t_exception * pseh_exception_output = PSEH_NULL;
	PVOID pseh_installed_handle = PSEH_NULL;
	PSEH_THREADLOCAL char pseh_handle_exception = 0;
	PSEH_THREADLOCAL char pseh_exception_occurred = 0;
	PSEH_THREADLOCAL jmp_buf pseh_jmpbuf;

	LONG CALLBACK pseh_exception_func(PEXCEPTION_POINTERS ExceptionInfo) {
		if (pseh_handle_exception == 0) return EXCEPTION_CONTINUE_SEARCH;
		
		pseh_handle_exception = 0;
		pseh_exception_occurred = 1;
		
		// Copy exception information
		pseh_exception_output->code = ExceptionInfo->ExceptionRecord->ExceptionCode;
		pseh_exception_output->flags = ExceptionInfo->ExceptionRecord->ExceptionFlags;
		pseh_exception_output->address = ExceptionInfo->ExceptionRecord->ExceptionAddress;
		pseh_exception_output->paramCount = ExceptionInfo->ExceptionRecord->NumberParameters;
		for (DWORD i = 0; i < ExceptionInfo->ExceptionRecord->NumberParameters; ++i) {
			pseh_exception_output->info[i] = ExceptionInfo->ExceptionRecord->ExceptionInformation[i];
		}
		pseh_exception_output->context = *ExceptionInfo->ContextRecord;
		
		longjmp(pseh_jmpbuf, 1);
		return EXCEPTION_CONTINUE_SEARCH; // This should never be reached
	}

	int pseh_install(void) {
		if (pseh_installed_handle != PSEH_NULL) return 1;
		pseh_installed_handle = AddVectoredExceptionHandler(1, pseh_exception_func);
		if (pseh_installed_handle != PSEH_NULL) return 0;
		return 1;
	}

	int pseh_uninstall(void) {
		if (pseh_installed_handle == PSEH_NULL) return 1;
		RemoveVectoredExceptionHandler(pseh_installed_handle);
		return 0;
	}
#else
	int pseh_install(void) { return 0; }
	int pseh_uninstall(void) { return 0; }
#endif

#else // PSEH_DISABLE_HANDLING
	int pseh_install(void) { return 0; }
	int pseh_uninstall(void) { return 0; }
#endif

#endif
