#ifndef __PSEH_H__
#define __PSEH_H__

#if !defined(PSEH_DISABLE_HANDLING)
#include <windows.h>
#include <setjmp.h>

#if defined(__cplusplus)
	#define PSEH_NULL nullptr
#else
	#include <stddef.h>
	#define PSEH_NULL NULL
#endif

#if defined(PSEH_C11)
	#define PSEH_THREADLOCAL _Thread_local
#elif defined(PSEH_C99)
	#define PSEH_THREADLOCAL
#else
	#define PSEH_THREADLOCAL thread_local
#endif

#if defined(__SEH__) && !defined(PSEH_FORCE_VEH)
	#define PSEH_USE_NATIVE_SEH 1
#endif

typedef struct pseh_t_exception {
	DWORD code;
	DWORD flags;
	PVOID address;
	ULONG_PTR info[EXCEPTION_MAXIMUM_PARAMETERS];
	DWORD paramCount;
	CONTEXT context;
} pseh_t_exception;

#if !defined(PSEH_USE_NATIVE_SEH) // MinGW GCC path
	extern PSEH_THREADLOCAL pseh_t_exception * pseh_exception_output;
	extern PVOID pseh_installed_handle;
	extern PSEH_THREADLOCAL char pseh_handle_exception;
	extern PSEH_THREADLOCAL char pseh_exception_occurred;
	extern PSEH_THREADLOCAL jmp_buf pseh_jmpbuf;

	LONG CALLBACK pseh_exception_func(PEXCEPTION_POINTERS ExceptionInfo);

	int pseh_install(void);

	int pseh_uninstall(void);

	#define PSEH_QUIT() {\
		memcpy(pseh_jmpbuf, pseh_jmpbuf_prev, sizeof(jmp_buf)); \
		pseh_exception_output = pseh_exception_output_prev; \
		pseh_handle_exception = pseh_handle_exception_prev; \
	}

	#define PSEH_TRY(output_pointer,code,_,except) {\
		BOOL pseh_handle_exception_prev = pseh_handle_exception; \
		pseh_t_exception * pseh_exception_output_prev = pseh_exception_output; \
		jmp_buf pseh_jmpbuf_prev; \
		memcpy(pseh_jmpbuf_prev, pseh_jmpbuf, sizeof(jmp_buf)); \
		pseh_exception_output = (output_pointer); \
		if (setjmp(pseh_jmpbuf) == 0) { \
			pseh_handle_exception = 1; \
			code \
			pseh_handle_exception = 0; \
		} else { \
			pseh_handle_exception = 0; \
			pseh_exception_occurred = 0; \
			except \
		} \
		PSEH_QUIT(); \
	}
#else // MSVC / clang with -fasync-exceptions
	#define PSEH_QUIT() {}
	
	static __inline int pseh_seh_filter(PEXCEPTION_POINTERS pseh_msvcexc, pseh_t_exception * output_pointer) {
		if (output_pointer != PSEH_NULL) {
			DWORD i;
			output_pointer->code = pseh_msvcexc->ExceptionRecord->ExceptionCode;
			output_pointer->flags = pseh_msvcexc->ExceptionRecord->ExceptionFlags;
			output_pointer->address = pseh_msvcexc->ExceptionRecord->ExceptionAddress;
			output_pointer->paramCount = pseh_msvcexc->ExceptionRecord->NumberParameters;
			for (i = 0; i < pseh_msvcexc->ExceptionRecord->NumberParameters; ++i) {
				output_pointer->info[i] = pseh_msvcexc->ExceptionRecord->ExceptionInformation[i];
			}
			output_pointer->context = *pseh_msvcexc->ContextRecord;
		}
		return EXCEPTION_EXECUTE_HANDLER;
	}

	#define PSEH_TRY(output_pointer,code,_,except) {\
		__try { \
			code \
		} \
		__except(pseh_seh_filter(GetExceptionInformation(), (output_pointer))) {\
			except \
		} \
	}
#endif


#define PSEH_RETURN(val) {\
	PSEH_QUIT(); \
	return val; \
}

#define PSEH_CONTINUE() {\
	PSEH_QUIT(); \
	continue; \
}

#define PSEH_BREAK() {\
	PSEH_QUIT(); \
	break; \
}

#define PSEH_RAISE(exc_pointer) RaiseException((exc_pointer)->code,(exc_pointer)->flags,(exc_pointer)->paramCount,(exc_pointer)->info);

#else // PSEH_DISABLE_HANDLING

typedef struct pseh_t_exception {
	char disabled;
} pseh_t_exception;

#define PSEH_TRY(output_pointer,code,_,except) {\
	code \
}

#endif
#endif
