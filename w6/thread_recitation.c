// thread_recitation.c
// Threads, Thread Safety, Reentrancy, and Semaphores (single counter) — with pause sections
//
// Build (teaching):
//   gcc -pthread -O0 -g -Wall -Wextra -o thread_recitation thread_recitation.c
// Run:
//   ./thread_recitation
//
// Sections (each pauses):
//   1) Counter race (no lock)
//   2) Counter fixed with mutex (mutual exclusion)
//   3) Non-reentrant function bug (sequential + threaded overwrite)
//   4) Reentrant function fix (caller buffers)
//   5) Bounds-safety mini-clinic (fgets + snprintf)
//   6) Semaphores with a single counter
//        6a) Binary semaphore (count=1) used like a mutex → correct
//        6b) Counting semaphore with 3 permits (count=3) → shows lost updates
//
// Notes:
//   • Data race: same memory, at least one write, no sync.
//   • Mutex: exclusive entry to critical section.
//   • Reentrancy: no shared hidden state (safe under concurrency).
//   • Semaphore: a counter you can wait()/post() on to gate entry.

#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ============================ Settings ============================ */
#ifndef THREADS
#define THREADS    8
#endif
#ifndef ITERATIONS
#define ITERATIONS 100000
#endif

/* ============================ Utilities =========================== */
static void wait_for_enter(const char *title) {
    if (title && *title) printf("\n===== %s =====\n", title);
    printf("Press ENTER to continue...\n");
    int c; while ((c = getchar()) != '\n' && c != EOF) {}
}
static void busy_spin(int n) { for (volatile int i = 0; i < n; ++i) {} }

/* ====================== Shared counter + mutex ==================== */
static long counter = 0;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

/* ====================== PART 1: Counter race ====================== */
// counter++ is load→add→store, not atomic → lost updates under contention.
static void *inc_no_lock(void *arg) {
    (void)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        long tmp = counter;                  // racy read
        if ((i & 0x3FF) == 0) sched_yield(); // encourage overlap
        busy_spin(50);
        tmp = tmp + 1;                        // racy modify
        if ((i & 0x7FF) == 0) sched_yield();
        counter = tmp;                        // racy write
    }
    return NULL;
}

/* =================== PART 2: Counter with mutex =================== */
// Make the critical section exclusive; no two threads update at once.
static void *inc_with_lock(void *arg) {
    (void)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        pthread_mutex_lock(&g_lock);
        counter++;
        pthread_mutex_unlock(&g_lock);
    }
    return NULL;
}

/* =========== PARTS 3–4: Reentrancy (bad vs good) ================== */
// ❌ Non-reentrant: returns pointer to ONE shared static buffer.
static char *upper_not_reentrant(const char *s) {
    static char buf[64]; // shared across all threads + calls (last writer wins)
    size_t n = strlen(s);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    for (size_t i = 0; i < n; i++) {
        buf[i] = (char)toupper((unsigned char)s[i]);
        if ((i & 7) == 0) busy_spin(200); // widen the overlap
    }
    buf[n] = '\0';
    return buf;
}

// ✅ Reentrant: caller supplies output buffer → no shared hidden state.
static void upper_reentrant(const char *s, char *out, size_t outcap) {
    if (!outcap) return;
    size_t n = strlen(s);
    if (n >= outcap) n = outcap - 1;
    for (size_t i = 0; i < n; i++)
        out[i] = (char)toupper((unsigned char)s[i]);
    out[n] = '\0';
}

/* ------------------ Part 5 helpers (bounds safety) ---------------- */
static void chomp_newline(char *s) { size_t n = strlen(s); if (n && s[n-1] == '\n') s[n-1] = '\0'; }

// Safe, bounded line read with truncation detection + flush.
static void read_label_bounded(char *dst, size_t cap) {
    printf("Enter a short label (<= %zu chars):\n> ", cap ? cap - 1 : 0);
    if (!fgets(dst, (int)cap, stdin)) { fprintf(stderr, "error: no input.\n"); dst[0] = '\0'; return; }
    bool truncated = strchr(dst, '\n') == NULL;
    if (truncated) {
        fprintf(stderr, "[warn] input longer than %zu chars; truncated and flushing.\n", cap - 1);
        int c; while ((c = getchar()) != '\n' && c != EOF) {}
    }
    chomp_newline(dst);
}

/* ================= Portable counting semaphore (semc_*) =========== */
/* We avoid POSIX sem_t because macOS deprecates unnamed semaphores.  */
typedef struct {
    int count;
    pthread_mutex_t m;
    pthread_cond_t  cv;
} semc_t;

static void semc_init(semc_t *s, int initial) { s->count = initial; pthread_mutex_init(&s->m, NULL); pthread_cond_init(&s->cv, NULL); }
static void semc_destroy(semc_t *s) { pthread_mutex_destroy(&s->m); pthread_cond_destroy(&s->cv); }
static void semc_wait(semc_t *s) { pthread_mutex_lock(&s->m); while (s->count == 0) pthread_cond_wait(&s->cv, &s->m); s->count--; pthread_mutex_unlock(&s->m); }
static void semc_post(semc_t *s) { pthread_mutex_lock(&s->m); s->count++; pthread_cond_signal(&s->cv); pthread_mutex_unlock(&s->m); }

/* =================== PART 6: Semaphores (single counter) ===========
   6a) Binary semaphore (count=1) used like a mutex → correct result.
   6b) Counting semaphore with 3 permits (count=3): up to 3 threads can
       be inside the “critical region” simultaneously → counter++ is no
       longer mutually exclusive, so lost updates reappear.
*/
static semc_t sem_bin;   // initialized with 1 for 6a
static semc_t sem_three; // initialized with 3 for 6b

static void *inc_with_sem_binary(void *arg) {
    (void)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        semc_wait(&sem_bin);   // like lock()
        counter++;             // safe (exclusive entry)
        semc_post(&sem_bin);   // like unlock()
    }
    return NULL;
}

static void *inc_with_sem_three(void *arg) {
    (void)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        semc_wait(&sem_three); // allows up to 3 threads in at once
        // ⚠ Not mutually exclusive when count>1 → counter++ races again
        long tmp = counter;
        busy_spin(30);         // widen the window to show the bug
        counter = tmp + 1;
        semc_post(&sem_three);
    }
    return NULL;
}

/* ========== Part 5 worker (moved to file scope to avoid linker error) ===== */
typedef struct { const char *tag; const char *name; } bounds_args_t;
static void *fn_bounds(void *arg) {
    bounds_args_t *a = (bounds_args_t*)arg;
    char local[24]; // per-thread local buffer (no sharing)
    int need2 = snprintf(local, sizeof(local), "[%s:%s]", a->tag, a->name);
    if (need2 >= (int)sizeof(local)) fprintf(stderr, "[warn] local truncated for \"%s\"\n", a->name);
    printf("thread-banner: %s\n", local);
    return NULL;
}

/* ================= Top-level thread workers for reentrancy ========= */
typedef struct { const char *in; const char **out; } args_bad_t;
static void *thread_fn_bad(void *arg) {
    args_bad_t *a = (args_bad_t*)arg;
    sched_yield();
    const char *p = upper_not_reentrant(a->in); // returns same static pointer
    sched_yield();
    *(a->out) = p;
    return NULL;
}

typedef struct { const char *in; char *out; size_t cap; } args_ok_t;
static void *thread_fn_ok(void *arg) {
    args_ok_t *a2 = (args_ok_t*)arg;
    upper_reentrant(a2->in, a2->out, a2->cap);
    return NULL;
}

/* ============================= Driver ============================= */
int main(void) {
    /* -------------------- Part 1: Race (no lock) -------------------- */
    printf("=== Part 1: Counter race (no lock) ===\n");
    pthread_t t[THREADS]; counter = 0;
    for (int i = 0; i < THREADS; i++) pthread_create(&t[i], NULL, inc_no_lock, NULL);
    for (int i = 0; i < THREADS; i++) pthread_join(t[i], NULL);
    printf("Expected: %d, got: %ld  <-- likely WRONG due to lost updates\n", THREADS * ITERATIONS, counter);
    wait_for_enter("Discuss: Why does counter++ lose updates here?");

    /* -------------------- Part 2: Mutex (correct) ------------------- */
    printf("=== Part 2: Counter with mutex (correct) ===\n");
    counter = 0;
    for (int i = 0; i < THREADS; i++) pthread_create(&t[i], NULL, inc_with_lock, NULL);
    for (int i = 0; i < THREADS; i++) pthread_join(t[i], NULL);
    printf("Expected: %d, got: %ld  ✅ exact\n", THREADS * ITERATIONS, counter);
    wait_for_enter("Discuss: What property does the mutex provide? Tradeoffs?");

    /* -------- Part 3: Non-reentrant function (sequential) ----------- */
    printf("=== Part 3: Non-reentrant function (sequential overwrite) ===\n");
    char *p1 = upper_not_reentrant("hello"); printf("First call -> %s\n", p1);
    char *p2 = upper_not_reentrant("world"); printf("Second call -> %s (overwrote first)\n", p2);
    wait_for_enter("Discuss: Why did the second call overwrite the first result?");

    /* ---- Part 3b: Non-reentrant under threads (same static buffer) - */
    printf("=== Part 3b: Non-reentrant under threads (same static buffer) ===\n");
    const char *outA = NULL, *outB = NULL;
    args_bad_t a = { "abcdef", &outA }, b = { "XYZ123", &outB };
    pthread_t A, B;
    pthread_create(&A, NULL, thread_fn_bad, &a);
    pthread_create(&B, NULL, thread_fn_bad, &b);
    pthread_join(A, NULL); pthread_join(B, NULL);
    printf("Thread A saw: %s\n", outA);
    printf("Thread B saw: %s\n", outB);
    printf("(Both point to the same static buffer; last finisher “wins”.)\n");
    wait_for_enter("Discuss: How does shared hidden state break correctness?");

    /* ----------- Part 4: Reentrant function (thread-safe) ----------- */
    printf("=== Part 4: Reentrant function (caller buffers; thread-safe) ===\n");
    char A_buf[64], B_buf[64];
    args_ok_t a2 = { "abcdef", A_buf, sizeof(A_buf) };
    args_ok_t b2 = { "XYZ123", B_buf, sizeof(B_buf) };
    pthread_create(&A, NULL, thread_fn_ok, &a2);
    pthread_create(&B, NULL, thread_fn_ok, &b2);
    pthread_join(A, NULL); pthread_join(B, NULL);
    printf("Thread-safe results: A=\"%s\", B=\"%s\"  ✅\n", A_buf, B_buf);
    wait_for_enter("Discuss: Why does caller-owned memory make it reentrant?");

    /* ------- Part 5: Bounds-safety mini-clinic (fgets/snprintf) ----- */
    printf("=== Part 5: Bounds-safety clinic (fgets/snprintf) ===\n");
    char label[16]; read_label_bounded(label, sizeof(label));
    char tag[20]; int need = snprintf(tag, sizeof(tag), "TAG:%s", label);
    if (need >= (int)sizeof(tag)) fprintf(stderr, "[warn] tag truncated (need %d, cap %zu)\n", need, sizeof(tag));
    printf("Safe tag = \"%s\"\n", tag);

    pthread_t T1, T2;
    bounds_args_t a1 = { .tag = tag, .name = "T1" };
    bounds_args_t a2b = { .tag = tag, .name = "T2" };
    pthread_create(&T1, NULL, fn_bounds, &a1);
    pthread_create(&T2, NULL, fn_bounds, &a2b);
    pthread_join(T1, NULL); pthread_join(T2, NULL);
    wait_for_enter("Discuss: Detecting truncation & avoiding shared temporaries");

    /* -------------------- Part 6: Semaphores (single counter) ------- */
    printf("=== Part 6a: Binary semaphore (count=1) used like a mutex ===\n");
    semc_init(&sem_bin, 1);     // 1 permit → exclusive entry
    counter = 0;
    for (int i = 0; i < THREADS; i++) pthread_create(&t[i], NULL, inc_with_sem_binary, NULL);
    for (int i = 0; i < THREADS; i++) pthread_join(t[i], NULL);
    printf("Expected: %d, got: %ld  ✅ exact (binary semaphore = mutual exclusion)\n", THREADS * ITERATIONS, counter);
    semc_destroy(&sem_bin);
    wait_for_enter("Discuss: How is a binary semaphore similar to a mutex? Any differences?");

    printf("=== Part 6b: Counting semaphore with 3 permits (count=3) ===\n");
    semc_init(&sem_three, 3);   // 3 permits → up to 3 inside at once
    counter = 0;
    for (int i = 0; i < THREADS; i++) pthread_create(&t[i], NULL, inc_with_sem_three, NULL);
    for (int i = 0; i < THREADS; i++) pthread_join(t[i], NULL);
    printf("Expected: %d, got: %ld  <-- likely WRONG again (not exclusive)\n", THREADS * ITERATIONS, counter);
    semc_destroy(&sem_three);
    wait_for_enter("Discuss: Why does allowing >1 permit reintroduce lost updates?");

    puts("\nAll sections complete. Thanks!");
    return 0;
}

/* =======================================================================
                            DETAILED ANSWER KEY
   =======================================================================
Part 1 — Why does counter++ lose updates?
    • counter++ is not atomic: load → add → store. Two threads can read the same
      old value and both write back, losing one increment. That’s a data race.

Part 2 — What does the mutex guarantee? Tradeoffs?
    • Mutual exclusion: only one thread enters the critical section at a time.
    • Guarantees correctness (no lost updates); costs performance under contention.

Part 3 — Why did the second call overwrite the first?
    • upper_not_reentrant() returns the same static buffer address to both calls.
      The second call overwrites the memory the first pointer refers to.

Part 3b — Why do both threaded results alias?
    • Both threads get the SAME static buffer pointer. The last finisher
      overwrites the content, so both ‘out’ pointers show the same final text.

Part 4 — Why is the reentrant version safe?
    • Each call writes into caller-provided memory; there’s no shared hidden state.
      Calls can overlap safely across threads (true reentrancy).

Part 5 — How to detect truncation & avoid shared temporaries?
    • fgets: if no newline captured, input exceeded buffer → truncated; flush the rest.
    • snprintf: if return value >= buffer size, output was truncated.
    • Avoid shared temporaries: use per-thread locals or caller-provided buffers.

Part 6a — Binary semaphore vs mutex?
    • A binary semaphore (count=1) enforces exclusive entry like a mutex, so the
      counter is correct. Differences: semaphores are more general (counting),
      and classically don’t encode ownership like a mutex does.

Part 6b — Why do 3 permits break correctness?
    • With 3 permits, up to three threads are inside the “critical region” at once.
      counter++ is still non-atomic, so interleavings reintroduce lost updates.
      Semaphores with count>1 are NOT mutual exclusion; they limit concurrency,
      not necessarily provide atomicity.
*/
