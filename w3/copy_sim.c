// Simulating copy_from_user / copy_to_user (no kernel needed)
//
// Build:  gcc -O2 -Wall -Wextra -o copy_sim copy_sim.c
// Run:    ./copy_sim
//
// Big picture:
//   - Think of kbuf[] as "kernel memory" and ubuf[] as "user memory".
//   - copy_from_user_sim() copies from user -> kernel with bounds checks.
//   - copy_to_user_sim()   copies from kernel -> user with bounds checks.
//   - We demonstrate success and failure cases.

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define KBUF_SIZE 32

// Return number of bytes copied; 0 means "rejected" (like an EFAULT-style failure).
size_t copy_from_user_sim(const char *user_src, size_t user_len,
                          char *kernel_dst, size_t kernel_cap) {
    if (user_src == NULL) {
        printf("[KERNEL] Reject: user pointer is NULL.\n");
        return 0;
    }
    if (user_len > kernel_cap) {
        printf("[KERNEL] Reject: user_len (%zu) > kernel capacity (%zu).\n", user_len, kernel_cap);
        return 0;
    }
    memcpy(kernel_dst, user_src, user_len);
    return user_len;
}

size_t copy_to_user_sim(const char *kernel_src, size_t kernel_len,
                        char *user_dst, size_t user_cap) {
    if (user_dst == NULL) {
        printf("[KERNEL] Reject: user destination pointer is NULL.\n");
        return 0;
    }
    if (kernel_len > user_cap) {
        printf("[KERNEL] Reject: kernel_len (%zu) > user capacity (%zu).\n", kernel_len, user_cap);
        return 0;
    }
    memcpy(user_dst, kernel_src, kernel_len);
    return kernel_len;
}

// Three cases:
// 1. Valid user -> kernel -> user round trip
// 2. Oversized user write (gets rejected)
// 3. User read buffer too small (gets rejected)

// Walk through the code and understand how it works, understand that this
// code is not actually copying data from user to kernel or vice versa, but
// it is demonstrating the concept of copy_from_user and copy_to_user.

int main(void) {
    // "Kernel memory" (fixed size on purpose)
    char kbuf[KBUF_SIZE];
    size_t klen = 0;
    
    //!  --------------- Case 1: Valid user -> kernel -> user round trip ---------------
    char user_msg1[] = "hello kernel";            // user buffer
    char user_out1[64];                           // where kernel will copy back
    size_t in = 0;
    
    // user -> kernel (copy_from_user)
    printf("\n\n=== Case 1: Valid round trip ===\n");
    in = copy_from_user_sim(user_msg1, strlen(user_msg1) + 1, kbuf, sizeof(kbuf));
    if (in == 0) {
        printf("[USER] copy_from_user_sim failed.\n\n");
    } else {
        klen = in; // kernel now "has" the message
        // Kernel does some work (uppercases in place)
        for (size_t i = 0; i < klen; i++) {
            kbuf[i] = (char)toupper((unsigned char)kbuf[i]);
        }
        // kernel -> user (copy_to_user)
        size_t out = copy_to_user_sim(kbuf, klen, user_out1, sizeof(user_out1));
        if (out == 0) {
            printf("[USER] copy_to_user_sim failed.\n\n");
        } else {
            printf("[USER] Got back from kernel: \"%s\"\n\n", user_out1);
        }
    }
    printf("Takeaway:\n");
    printf("  • copy_from_user_sim: kernel validates size before reading user data.\n\n\n");
    
    
    //! --------------- Case 2: Oversized user write gets rejected --------------------
    printf("=== Case 2: Oversized user write (should be rejected) ===\n");
    char big_user_msg[128];
    memset(big_user_msg, 'A', sizeof(big_user_msg));
    big_user_msg[sizeof(big_user_msg) - 1] = '\0';  // length ~127 bytes > KBUF_SIZE (32)

    in = copy_from_user_sim(big_user_msg, strlen(big_user_msg) + 1, kbuf, sizeof(kbuf));
    if (in == 0) {
        printf("[USER] Kernel rejected oversized write ✅\n\n");
    } else {
        printf("[USER] Unexpected: kernel accepted oversized write (bytes=%zu)\n\n", in);
    }
    printf("Takeaway:\n");
    printf("  • copy_to_user_sim:   kernel validates size before writing to user memory.\n\n\n");


    //! --------------- Case 3: User read buffer too small (reject) -------------------
    printf("=== Case 3: User buffer too small on read (should be rejected) ===\n");
    // Put something in kernel first
    const char *small = "OK";
    in = copy_from_user_sim(small, strlen(small) + 1, kbuf, sizeof(kbuf));
    if (in == 0) {
        printf("[USER] Unexpected: failed to seed kernel buffer.\n\n");
        return 0;
    }
    klen = in;

    char tiny_user_out[2]; // too small: cannot hold "OK\0" (needs 3 bytes)
    size_t out = copy_to_user_sim(kbuf, klen, tiny_user_out, sizeof(tiny_user_out));
    if (out == 0) {
        printf("[USER] Kernel refused to overrun user buffer ✅\n\n");
    } else {
        printf("[USER] Unexpected: kernel copied %zu bytes into a tiny buffer\n\n", out);
    }
    printf("Takeaway:\n");
    printf("  • Without checks, bugs could crash the system or leak/corrupt data.\n\n\n");    

    return 0;
}
