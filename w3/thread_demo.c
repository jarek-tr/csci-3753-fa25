// thread_demo.c
// Week 2: Threads, Thread Safety, Reentrant Code — with DELIBERATE race stress
//
// Build (for teaching; widens race windows):
//   gcc -pthread -O0 -g -Wall -Wextra -o thread_demo thread_demo.c
//
// Run:
//   ./thread_demo
//
// -------------------------------------------------------------------
// Learning goals:
//   1) See a race condition when many threads update a shared global.
//   2) Fix the race condition using a lock (mutex).
//   3) Understand the difference between non-reentrant and reentrant functions.
//   4) Observe a broken invariant (two values that “should” stay equal)
//      without a lock, and intact with a lock.
//   5) See non-reentrant behavior break under concurrency.
// -------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sched.h>   // sched_yield

// Increase these to make races even more obvious
#define THREADS     15
#define ITERATIONS  10000000

/* ========================= Shared state for A/A2/B ========================= */
long counter = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/* ---------------- Part A: naive increment (may look “fine” sometimes) ----- */
// ❓ Why might this *sometimes* look correct? What hidden steps are in counter++?
static void *increment_without_lock(void *arg) {
    for (int i = 0; i < ITERATIONS; i++) {
        counter++;  // data race: load, add, store (not atomic)
    }
    return NULL;
}

/* -------- Part A2: STRESSED race (widens window; almost always wrong) ------ */
// ❓ How do yields/spin widen the race window to increase overlap?
static void *increment_without_lock_stressed(void *arg) {
    for (int i = 0; i < ITERATIONS; i++) {
        long tmp = counter;          // read
        if ((i & 0x3FF) == 0) sched_yield();       // invite interleaving
        for (volatile int spin = 0; spin < 50; ++spin) { /* widen */ }
        tmp = tmp + 1;               // modify
        if ((i & 0x7FF) == 0) sched_yield();       // invite collision
        counter = tmp;               // write (may clobber another thread)
    }
    return NULL;
}

/* ---------------- Part B: with lock (correct) ------------------------------ */
// ❓ What property does the lock enforce around counter++?
static void *increment_with_lock(void *arg) {
    for (int i = 0; i < ITERATIONS; i++) {
        pthread_mutex_lock(&lock);
        counter++;
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

/* --------------- Bonus invariant demo: (a == b) should hold ---------------- */
typedef struct { long a, b; } pair_t;
pair_t pair_vals = {0, 0};

// ❓ Why can (a == b) break without a lock, even if each thread tries to keep them in sync?
static void *touch_pair_without_lock(void *arg) {
    for (int i = 0; i < ITERATIONS; i++) {
        long ta = pair_vals.a;
        long tb = pair_vals.b;
        if ((i & 0x1FF) == 0) sched_yield();
        ta++; tb++;
        if ((i & 0x3FF) == 0) sched_yield();
        pair_vals.a = ta;
        pair_vals.b = tb;
        if (pair_vals.a != pair_vals.b) {
            return (void*)1; // signal invariant broken
        }
    }
    return NULL;
}

static void *touch_pair_with_lock(void *arg) {
    for (int i = 0; i < ITERATIONS; i++) {
        pthread_mutex_lock(&lock);
        pair_vals.a++;
        if ((i & 0x3FF) == 0) { for (volatile int s = 0; s < 50; ++s) {} }
        pair_vals.b++;
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

/* =============================== Reentrancy ================================ */
// NOT reentrant: uses a static buffer shared by all calls
// ❓ Why is this unsafe if two threads call it at the same time?
static char *not_reentrant_upper(const char *s) {
    static char buf[64];  // shared single buffer
    size_t n = strlen(s);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    for (size_t i = 0; i < n; i++) {
        buf[i] = (char)toupper((unsigned char)s[i]);
        if ((i & 7) == 0) { for (volatile int z = 0; z < 200; ++z) {} } // widen overlap
    }
    buf[n] = '\0';
    return buf; // all callers see the same pointer
}

// Reentrant: caller provides the output buffer
// ❓ How does caller-owned memory prevent interference between calls?
static void reentrant_upper(const char *s, char *out, size_t outcap) {
    size_t n = strlen(s);
    if (n >= outcap) n = outcap - 1;
    for (size_t i = 0; i < n; i++) out[i] = (char)toupper((unsigned char)s[i]);
    out[n] = '\0';
}

// Thread target to *race* the non-reentrant version
typedef struct { const char *in; const char **outptr; } nr_args_t;
static void *call_not_reentrant(void *arg) {
    nr_args_t *a = (nr_args_t*)arg;
    sched_yield();
    const char *p = not_reentrant_upper(a->in);
    sched_yield();
    *(a->outptr) = p;  // every thread “returns” the same static pointer
    return NULL;
}

/* ================================ Driver =================================== */
int main(void) {
    // ---- Part A: naive (may or may not show wrong) ----
    {
        pthread_t ts[THREADS];
        counter = 0;
        printf("=== Part A: Counter without lock (may look okay) ===\n");
        for (int i = 0; i < THREADS; i++) pthread_create(&ts[i], NULL, increment_without_lock, NULL);
        for (int i = 0; i < THREADS; i++) pthread_join(ts[i], NULL);
        printf("Expected %d, got %ld\n\n", THREADS * ITERATIONS, counter);
    }

    // ---- Part A2: stressed race (should be wrong) ----
    {
        pthread_t ts[THREADS];
        counter = 0;
        printf("=== Part A2: STRESSED counter without lock (should be wrong) ===\n");
        for (int i = 0; i < THREADS; i++) pthread_create(&ts[i], NULL, increment_without_lock_stressed, NULL);
        for (int i = 0; i < THREADS; i++) pthread_join(ts[i], NULL);
        printf("Expected %d, got %ld  <-- race likely caused lost updates\n\n",
               THREADS * ITERATIONS, counter);
    }

    // ---- Part B: with lock (correct) ----
    {
        pthread_t ts[THREADS];
        counter = 0;
        printf("=== Part B: Counter WITH lock (should be exact) ===\n");
        for (int i = 0; i < THREADS; i++) pthread_create(&ts[i], NULL, increment_with_lock, NULL);
        for (int i = 0; i < THREADS; i++) pthread_join(ts[i], NULL);
        printf("Expected %d, got %ld ✅\n\n", THREADS * ITERATIONS, counter);
    }

    // ---- Bonus: invariant break demo ----
    {
        pthread_t ts[THREADS];
        pair_vals.a = pair_vals.b = 0;
        printf("=== Bonus A: Invariant (a==b) WITHOUT lock (should break) ===\n");
        int broke = 0;
        for (int i = 0; i < THREADS; i++) pthread_create(&ts[i], NULL, touch_pair_without_lock, NULL);
        for (int i = 0; i < THREADS; i++) {
            void *ret = NULL;
            pthread_join(ts[i], &ret);
            if ((long)ret == 1) broke = 1;
        }
        printf("Invariant a==b broken? %s (a=%ld, b=%ld)\n\n", broke ? "YES" : "NO",
               pair_vals.a, pair_vals.b);

        printf("=== Bonus B: Invariant WITH lock (should hold) ===\n");
        pair_vals.a = pair_vals.b = 0;
        for (int i = 0; i < THREADS; i++) pthread_create(&ts[i], NULL, touch_pair_with_lock, NULL);
        for (int i = 0; i < THREADS; i++) pthread_join(ts[i], NULL);
        printf("Invariant a==b holds?  %s (a=%ld, b=%ld) ✅\n\n",
               (pair_vals.a == pair_vals.b) ? "YES" : "NO",
               pair_vals.a, pair_vals.b);
    }

    // ---- Part C: reentrancy ----
    printf("=== Part C1: Sequential calls (non-reentrant overwrites) ===\n");
    char *bad1 = not_reentrant_upper("hello");
    printf("First call (not reentrant): %s\n", bad1);
    char *bad2 = not_reentrant_upper("world");
    printf("Second call (not reentrant): %s (overwrote first)\n", bad2);
    char r1[16], r2[16];
    reentrant_upper("hello", r1, sizeof(r1));
    reentrant_upper("world", r2, sizeof(r2));
    printf("Reentrant calls preserved: \"%s\" and \"%s\"\n\n", r1, r2);

    printf("=== Part C2: THREADS race on non-reentrant function (garbled likely) ===\n");
    pthread_t tA, tB;
    const char *outA = NULL, *outB = NULL;
    nr_args_t a = {.in = "abcdef", .outptr = &outA};
    nr_args_t b = {.in = "XYZ123", .outptr = &outB};
    pthread_create(&tA, NULL, call_not_reentrant, &a);
    pthread_create(&tB, NULL, call_not_reentrant, &b);
    pthread_join(tA, NULL);
    pthread_join(tB, NULL);
    printf("Thread A saw: %s\n", outA);
    printf("Thread B saw: %s\n", outB);
    printf("(Both point to the same static buffer; last finisher “wins.”)\n\n");

    puts("Takeaway:\n"
         "  • A may look OK by chance; A2 stresses the race so it fails.\n"
         "  • Locks fix the counter and preserve invariants.\n"
         "  • Non-reentrant code breaks under concurrency; reentrant code is safe.");
    return 0;
}

/* =======================================================================
                               ANSWER KEY
   =======================================================================

Part A (naive) & Part A2 (stressed)
-----------------------------------
Q: Why might Part A sometimes look correct? What hidden steps are in counter++?
A: Luck and timing. With low contention the interleavings may not collide.
   counter++ is really load → add → store, not atomic; races can lose updates.

Q: How do yields/spin widen the race window in A2?
A: They insert delays between load/modify/store so threads overlap more often,
   making lost updates much more likely and visible.

Part B (with lock)
------------------
Q: What property does the lock enforce around counter++?
A: Mutual exclusion: only one thread executes the critical section at a time,
   making the read–modify–write sequence effectively atomic and race-free.

Bonus invariant (a==b)
----------------------
Q: Why can (a == b) break without a lock?
A: The updates to a and b are separate writes. Interleavings can let one thread
   see partially updated state and overwrite, leaving a != b.

Reentrancy (Part C)
-------------------
Q: Why is the static buffer in the non-reentrant version unsafe with threads?
A: It’s shared global state. All calls return the same pointer; concurrent calls
   overwrite each other’s results (and even sequential calls overwrite prior output).

Q: How does caller-owned memory prevent interference?
A: Each call writes to distinct memory provided by the caller, eliminating shared
   state and making the function reentrant/thread-safe by design.

Build/Run Notes
---------------
• Use -O0 (no optimization) for predictable demos of races.
• If a race still “looks fine,” increase THREADS or ITERATIONS.

*/