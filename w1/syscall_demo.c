// syscall_demo.c
// Week 1 OS Recitation: User Space vs Kernel Space
//
// Build: gcc -O2 -Wall -Wextra -o syscall_demo syscall_demo.c
// Run:   ./syscall_demo
//
// Parts:
//   A) Blocked: try to access a protected kernel resource (/dev/mem) -> should fail
//   B) Pure user space: compute on data in our own address space (no syscalls inside)
//   C) Proper: ask the kernel via system calls (open/read/close) for safe access

#include <stdio.h>    // printf (will use write() under the hood)
#include <stdlib.h>
#include <fcntl.h>    // open
#include <unistd.h>   // read, close
#include <string.h>   // memcpy, strlen

// ------------------ Part B Helpers: Pure user-space work (no syscalls inside) ------------------
// NOTE: These functions only touches CPU registers and the program's own RAM.
// It does not do I/O, allocate memory, or call the kernel.
static void reverse_in_place(char *s) {
    size_t i = 0, j = strlen(s);
    if (j == 0) return;
    j--; // point to last valid char
    while (i < j) {
        char tmp = s[i];
        s[i] = s[j];
        s[j] = tmp;
        i++; j--;
    }
}

static int sum_array(const int *a, size_t n) {
    int sum = 0;
    for (size_t i = 0; i < n; i++) {
        sum += a[i];  // pure computation on our own memory
    }
    return sum;
}

int main(void) {
    
    printf("=== Demo: User mode vs Kernel mode ===\n\n");
    
    // ---------------------------- Driver A -----------------------------------
    // Blocked direct kernel access
    
    /*
    printf("[Part A] Trying to read from a protected kernel/hardware mapping (/dev/mem)...\n");
    FILE *f = fopen("/dev/mem", "r");  // needs elevated privileges, typically blocked
    if (!f) {
        printf("-> Failed as expected. User programs cannot directly touch kernel/hardware memory.\n");
    } else {
        printf("-> Unexpectedly opened /dev/mem (your system may be configured unusually).\n");
        fclose(f);
    }
    */

    
    // ---------------------------- Driver B -----------------------------------
    // Pure user-space work (no syscalls inside)
    
    /* 
    printf("\n[Part B] Doing work entirely in user space (no syscalls inside these functions)...\n");
    char msg[] = "hello, kernel boundary!";
    int nums[] = {1, 2, 3, 4, 5};

    // These functions only read/write our process's own memory and use the CPU.
    reverse_in_place(msg);
    int s = sum_array(nums, sizeof(nums)/sizeof(nums[0]));

    // NOTE: The printf below is a syscall for output, but the *work above* did not cross into the kernel.
    printf("-> Reversed string (user memory only): \"%s\"\n", msg);
    printf("-> Sum of array (user memory only): %d\n", s);
    printf("   (No kernel calls were needed to compute those results.)\n");
    */

    
    // ---------------------------- Driver C -----------------------------------
    // Part C: Proper kernel interaction via system calls
    
    /*
    printf("\n[Part C] Asking the kernel for system-managed info using syscalls (open/read/close)...\n");
    const char *path = "/etc/hostname";   // on macOS you can switch to "/etc/hosts" if needed
    int fd = open(path, O_RDONLY);        // syscall
    if (fd < 0) {
        perror("open");
        printf("-> If this path doesn't exist on your OS, try \"/etc/hosts\".\n");
        return 0; // still a successful teaching run
    }

    char buf[128];
    ssize_t n = read(fd, buf, sizeof(buf)-1); // syscall
    if (n > 0) {
        buf[n] = '\0';
        printf("-> Kernel-provided data (%s): %s", path, buf); // printf uses write() under the hood
        if (buf[n-1] != '\n') printf("\n");
    }
    close(fd); // syscall

    printf("\nTakeaway:\n"
           "  • Part B: pure user-space (compute on your own memory) needs no kernel help.\n"
           "  • Part C: to access system-managed resources, you must ask the kernel via syscalls.\n");
    */
    
    return 0;
}
