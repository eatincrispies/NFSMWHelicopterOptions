A pseudo-reimplementation of Microsoft's SEH for MinGW/MSVC. Should work with C and C++. Should be thread-safe.

This is primarily for making MSVC projects that depend on SEH compatible with MinGW GCC. However, it requires some manual labor. The syntax is close, though.

MSVC and MinGW clang with -fasync-exceptions will use Microsoft's SEH instead, for reliability. Define PSEH_FORCE_VEH if you want to force the MinGW GCC path instead.

You may also define PSEH_DISABLE_HANDLING, to not handle any exceptions.

## Notes

* **C99:** Define `PSEH_C99`. **NOT THREAD-SAFE!**
* **C11/C17:** Define `PSEH_C11`
* **C++11/C23:** Should work as-is

## How to use

1. Include `pseh.h` (or `pseh.c`, if you wanna inline pseh)

2. Call `pseh_install()` to install the generic handler. 0 = success. Only do this once in the program.

3. Now you can handle exceptions like this:

```c
pseh_t_exception e;
PSEH_TRY(&e,{
	// Code that could raise an exception
},except,{
	// Handle your exception here
});
```

If the except path is triggered, `e` contains information about the exception. See `pseh_exception_func` in `pseh.c` for more information about the contents.

4. Call `pseh_uninstall()` to uninstall the generic handler. 0 = success. Do this on DLL unload for example. Only do this once in the program.

## Caveats

* Inside PSEH_TRY, use PSEH_CONTINUE/PSEH_BREAK/PSEH_RETURN for actions that would break out of the block. If you use something else that would break out of the block, use PSEH_QUIT beforehand. You should use regular continue/break/return inside of the except block, though.
* If you intend to use local variables that are changed in PSEH_TRY inside or after the except block, mark them as volatile.
