#include <stdio.h>
#include <string.h>

/*
TODO - C-Strings
? A C-string is an array of chars terminated by a null byte '\0'.
! Example:
!   char s[] = "Hi"; // {'H','i','\0'}
! Always allocate space for the terminator.

TODO - Ways to Declare Strings
? String literal stored in read-only segment on most systems.
! Example:
!   char *s = "Hello, World!"; // ❌ UB if you try to modify *s

? Compiler enforces read-only; you cannot write through s.
! Example:
!   const char *s = "Hello, World!"; // compiler stops writes like *s = 'h';

? Array on stack (or static storage if global).
? The compiler allocates enough space for all characters + '\0'.
? For "Hello, World!" → 13 visible characters + 1 null terminator = 14 bytes.
! Example:
!   char s[] = "Hello, World!"; 
!   // ✅ mutable for indices [0–12] (the characters).
!   // ✅ index [13] is '\0' and can be reassigned, 
!      but then another terminator must exist.
!   // ❌ never write past [13] (out-of-bounds).
  
! Rule of thumb:
!   Use const char * for string literals you won’t modify.
!   Use char [] for mutable buffers you intend to change.

TODO - Pointers
? Pointers are variables that store the memory address of another variable.
! Example:
!   int x = 42;
!   int *p = &x;   // p holds address of x

? Dereferencing a pointer gives the object it points to.
! Example:
!   int y = *p;    // y == 42

? Address-of: '&x' gives the address of x. Type of &x is "pointer to type of x".
! Example:
!   int *p2 = &x;

? Pointer types ('int *', 'char *', etc.) control arithmetic and dereference.
! Example:
!   char c[3] = {'A','B','\0'}; char *pc = c;   // pc+1 points to 'B'
!   int  a[3] = {1,2,3};        int  *pi = a;   // pi+1 points to 2

? Dereference: '*p' accesses the object pointed to by p (an lvalue).
! Example:
!   *pi = 10; // writes 10 into a[0]

TODO - Pointer Arithmetic
? 'p+1' moves by sizeof(*p) bytes; with 'char *' it moves by 1 byte.
! Example:
!   char s2[] = "AB";
!   char *q = s2;     // points to 'A'
!   q = q + 1;        // now points to 'B'

TODO - Arrays & decay: In most expressions, 'arr' decays to '(&arr[0])' of type 'T *'.
! Example:
!   char arr[] = "Hi";
!   char *r = arr;           // arr decays to &arr[0]
!   BUT: sizeof(arr) != sizeof(r). sizeof(arr) is 3; sizeof(r) is pointer size.

TODO - Const correctness: 'const char *p' means you cannot modify the chars via p.
? 'char *' must not point to a string literal if you plan to modify through it.
! Example:
!   const char *safe = "Hello, World!"; // reads OK, writes forbidden
!   char *danger = "Hello, World!";  // writing via danger is UB

TODO - Null terminator: Strings end at '\0'. Functions like strlen/printf("%s") rely on it.
! Example:
!   const char *t = "Hi"; printf("%zu\n", strlen(t)); // prints 2

TODO - Common Undefined Behavior (UB): 
? writing through a pointer to read-only memory; 
? walking past array bounds; 
? using uninitialized/NULL pointers.
! Example (DON'T DO):
!   char *lit = "Hi"; lit[0] = 'h';     // UB (read-only)
!   int *u; *u = 5;                     // UB (uninitialized pointer)
!   char small[3] = "Hi"; small[3] = '!'; // UB (out-of-bounds)
*/

// TODO - Class Demo

static void demo_decay(void) {
    char arr[] = "Hi!";
    char *p = arr; // decay to &arr[0]
    printf("[demo_decay] sizeof(arr)=%zu, sizeof(p)=%zu\n", sizeof(arr), sizeof(p));
    printf("[demo_decay] arr[1]=%c, p[1]=%c\n", arr[1], p[1]);
}

int main(void) {
    // Choose ONE declaration to activate:
    // char *s = "Hello, World!";           // ❌ UB if modified (read-only)
    // const char *s = "Hello, World!";     // ❌ compile-time error if we try to modify
   char s[] = "Hello, World!";          // ✅ 14-byte array: Safely mutable for indices [0–13]

    // Runtime check: sizeof vs strlen
    printf("[demo_info] sizeof(s)=%zu, strlen(s)=%zu\n", sizeof(s), strlen(s));

    printf("[demo_scramble] Original: %s\n", s);
    // Scramble the string (increment each char, stop at '\0')
    for (char *p = s; *p; ++p) {
        *p += 1;
    }
    printf("[demo_scramble] Scrambled: %s\n", s);

    // Unscramble the string safely (guard empty string; avoid pointer underflow)
    size_t n = strlen(s);
    if (n > 0) {
        for (char *p = s + n - 1; ; --p) {
            *p -= 1;
            if (p == s) break;
        }
    }
    printf("[demo_scramble] Unscrambled: %s\n", s);

    demo_decay(); // shows sizeof array vs pointer and indexing equivalence

    // ---------------------------------------------------
    // Uncomment ONE of these at a time to see what happens
    // ---------------------------------------------------

    // 1. Compile error: modifying through const pointer
    // const char *cs = "Hello";
    // cs[0] = 'h'; // ❌ error: assignment of read-only location

    // 2. Runtime crash: modifying string literal (read-only memory)
    // char *lit = "Hello";
    // lit[0] = 'h'; // ❌ UB: segfault on most systems

    // 3. Out-of-bounds write: corrupts memory (may crash later)
    // char small[3] = "Hi"; // "H","i","\0"
    // small[3] = '!'; // ❌ UB: index 3 is past end

    // 4. Out-of-bounds read: prints garbage or crashes
    // char a[2] = {'A','B'};
    // printf("a[2]=%c\n", a[2]); // ❌ UB: index past end

    // 5. NULL dereference: runtime crash
    // int *p = NULL;
    // *p = 42; // ❌ UB: segfault

    // 6. Uninitialized pointer: runtime crash or garbage
    // int *q;
    // *q = 7; // ❌ UB: q points nowhere valid

    // 7. No null terminator: strlen reads past end
    // char bad[3] = {'O','K','!'}; 
    // printf("len=%zu\n", strlen(bad)); // ❌ UB: runs off into memory

    return 0;
}

/*
TODO: Compilation, Warnings, and Linking (GCC & Clang)
? Goal: build cleanly (no warnings), run safely (sanitizers in dev), and optimize (release).

? Quick one-file builds
? Debug + warnings
!   gcc   -std=c17 -Wall -Wextra -pedantic -O0 -g demo.c -o demo_gcc
!   clang -std=c17 -Wall -Wextra -pedantic -O0 -g demo.c -o demo_clang

? Treat warnings as errors (use in CI, turn off while teaching if too strict)
!   gcc   -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g demo.c -o demo_gcc
!   clang -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g demo.c -o demo_clang

? Sanitized debug builds (highly recommended while learning)
!   # Address & UndefinedBehavior sanitizers (compile *and* link with the same flags)
!   gcc   -std=c17 -O1 -g -fno-omit-frame-pointer \
!         -fsanitize=address,undefined -fno-sanitize-recover=all \
!         demo.c -o demo_gcc_asan_ubsan

!   clang -std=c17 -O1 -g -fno-omit-frame-pointer \
!         -fsanitize=address,undefined -fno-sanitize-recover=all \
!         demo.c -o demo_clang_asan_ubsan

? Notes:
!   • Keep -O1 (or -O0) with sanitizers; very high -O can hinder diagnostics.
!   • On Windows, Clang (LLVM) supports sanitizers better than GCC/MinGW. On Linux/macOS both are solid.

? Release build (fast, no debug)
!   gcc   -std=c17 -O2 -DNDEBUG demo.c -o demo_gcc_release
!   clang -std=c17 -O2 -DNDEBUG demo.c -o demo_clang_release

? Multi-file pattern
!   gcc   -std=c17 -Wall -Wextra -pedantic -O0 -g -c foo.c -o foo.o
!   gcc   -std=c17 -Wall -Wextra -pedantic -O0 -g -c bar.c -o bar.o
!   gcc   foo.o bar.o -o app_gcc

!   clang -std=c17 -Wall -Wextra -pedantic -O0 -g -c foo.c -o foo.o
!   clang -std=c17 -Wall -Wextra -pedantic -O0 -g -c bar.c -o bar.o
!   clang foo.o bar.o -o app_clang

? Extra useful warnings (optional)
!   -Wshadow -Wconversion -Wsign-conversion -Wpointer-arith -Wstrict-prototypes -Wvla

? Libraries / linking
   ! stdio/string are in libc (linked automatically).
   ! For math (e.g., sin, pow): add -lm (Linux; macOS links libm via libSystem so -lm is optional).
   ! When using sanitizers, the same -fsanitize flags must appear at link time.

? Platform notes
!   • macOS uses Apple Clang by default (clang); Homebrew can install GCC/LLVM if needed.
!   • Windows: easiest path is Clang or GCC via MSYS2/MinGW or WSL (Linux). Sanitizers are best with Clang.
!   • Linux: both GCC and Clang are first-class.

? IDEs
!   • Configure your IDE to pass the same flags (e.g., -std=c17 -Wall -Wextra -pedantic).
!   • Enable “treat warnings as errors” in CI to keep codebases healthy.

? Pro tips
!   • Rebuild with both compilers; each catches different issues.
!   • Use sanitized builds for all pointer/string exercises.
*/
