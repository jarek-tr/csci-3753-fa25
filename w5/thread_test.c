// thread_test.c
// Threads, Thread Safety, Reentrancy, and Semaphores — with pause sections
//
// Build (teaching):
//   gcc -pthread -O0 -g -Wall -Wextra -o thread_test thread_test.c
// Run:
//   ./thread_test
//
// Sections (each pauses):
//   1) Counter race (no lock)
//   2) Counter fixed (mutex)
//   3) Non-reentrant function (sequential + threaded overwrite)
//   4) Reentrant function (caller-provided buffers)
//   5) Bounds-safety clinic (strings & snprintf)
//   6) Semaphores via portable counting-semaphore (producer/consumer)

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

/* ====================== PART 1: Counter race ====================== */
static long counter = 0;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static void *inc_no_lock(void *arg) {
    (void)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        long tmp = counter;
        if ((i & 0x3FF) == 0) sched_yield();
        busy_spin(50);
        tmp = tmp + 1;
        if ((i & 0x7FF) == 0) sched_yield();
        counter = tmp;
    }
    return NULL;
}

/* ====================== PART 2: Counter with mutex ====================== */
static void *inc_with_lock(void *arg) {
    (void)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        pthread_mutex_lock(&g_lock);
        counter++;
        pthread_mutex_unlock(&g_lock);
    }
    return NULL;
}

/* =========== PART 3 & 4: Reentrancy (bad vs good) ============ */
static char *upper_not_reentrant(const char *s) {
    static char buf[64];
    size_t n = strlen(s);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    for (size_t i = 0; i < n; i++) {
        buf[i] = (char)toupper((unsigned char)s[i]);
        if ((i & 7) == 0) busy_spin(200);
    }
    buf[n] = '\0';
    return buf;
}

static void upper_reentrant(const char *s, char *out, size_t outcap) {
    if (!outcap) return;
    size_t n = strlen(s);
    if (n >= outcap) n = outcap - 1;
    for (size_t i = 0; i < n; i++)
        out[i] = (char)toupper((unsigned char)s[i]);
    out[n] = '\0';
}

/* ---- helpers for Part 5 ---- */
static void chomp_newline(char *s) { size_t n = strlen(s); if (n && s[n-1] == '\n') s[n-1] = '\0'; }

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

/* =================== Portable counting semaphore ================== */
typedef struct {
    int count;
    pthread_mutex_t m;
    pthread_cond_t  cv;
} semc_t;

static void semc_init(semc_t *s, int initial) {
    s->count = initial;
    pthread_mutex_init(&s->m, NULL);
    pthread_cond_init(&s->cv, NULL);
}
static void semc_destroy(semc_t *s) {
    pthread_mutex_destroy(&s->m);
    pthread_cond_destroy(&s->cv);
}
static void semc_wait(semc_t *s) {
    pthread_mutex_lock(&s->m);
    while (s->count == 0) pthread_cond_wait(&s->cv, &s->m);
    s->count--;
    pthread_mutex_unlock(&s->m);
}
static void semc_post(semc_t *s) {
    pthread_mutex_lock(&s->m);
    s->count++;
    pthread_cond_signal(&s->cv);
    pthread_mutex_unlock(&s->m);
}

/* ====================== PART 6: Bounded buffer ==================== */
#define QSIZE 4
#define PRODUCE_COUNT 12
typedef struct {
    int data[QSIZE];
    int head, tail;
    pthread_mutex_t qlock;
    semc_t empty, full;
} queue_t;

static void q_init(queue_t *q) {
    q->head = q->tail = 0;
    pthread_mutex_init(&q->qlock, NULL);
    semc_init(&q->empty, QSIZE);
    semc_init(&q->full, 0);
}
static void q_destroy(queue_t *q) {
    pthread_mutex_destroy(&q->qlock);
    semc_destroy(&q->empty);
    semc_destroy(&q->full);
}
static void q_push(queue_t *q, int v) {
    semc_wait(&q->empty);
    pthread_mutex_lock(&q->qlock);
    q->data[q->tail] = v; q->tail = (q->tail + 1) % QSIZE;
    pthread_mutex_unlock(&q->qlock);
    semc_post(&q->full);
}
static int q_pop(queue_t *q) {
    semc_wait(&q->full);
    pthread_mutex_lock(&q->qlock);
    int v = q->data[q->head]; q->head = (q->head + 1) % QSIZE;
    pthread_mutex_unlock(&q->qlock);
    semc_post(&q->empty);
    return v;
}

/* ==================== Top-level thread functions ================== */
typedef struct { const char *in; const char **out; } args_bad_t;
static void *thread_fn_bad(void *arg) {
    args_bad_t *a = (args_bad_t*)arg;
    sched_yield();
    const char *p = upper_not_reentrant(a->in);
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

typedef struct { const char *tag; const char *name; } bounds_args_t;
static void *fn_bounds(void *arg) {
    bounds_args_t *a = (bounds_args_t*)arg;
    char local[24];
    int need2 = snprintf(local, sizeof(local), "[%s:%s]", a->tag, a->name);
    if (need2 >= (int)sizeof(local)) fprintf(stderr, "[warn] local truncated for \"%s\"\n", a->name);
    printf("thread-banner: %s\n", local);
    return NULL;
}

typedef struct { queue_t *q; int count; } prod_args_t;
typedef struct { queue_t *q; int count; } cons_args_t;

static void *producer(void *arg) {
    prod_args_t *pa = (prod_args_t*)arg;
    for (int i = 1; i <= pa->count; ++i) {
        q_push(pa->q, i);
        printf("  [P] produced %2d\n", i);
        usleep(20000);
    }
    return NULL;
}
static void *consumer(void *arg) {
    cons_args_t *ca = (cons_args_t*)arg;
    for (int taken = 0; taken < ca->count; ++taken) {
        int v = q_pop(ca->q);
        printf("      [C] consumed %2d\n", v);
        usleep(35000);
    }
    return NULL;
}

/* ============================= Driver ============================= */
int main(void) {
    // Part 1
    printf("=== Part 1: Counter race (no lock) ===\n");
    pthread_t t[THREADS]; counter = 0;
    for (int i = 0; i < THREADS; i++) pthread_create(&t[i], NULL, inc_no_lock, NULL);
    for (int i = 0; i < THREADS; i++) pthread_join(t[i], NULL);
    printf("Expected: %d, got: %ld\n", THREADS * ITERATIONS, counter);
    wait_for_enter("Discuss: Why is counter++ not atomic?");

    // Part 2
    printf("=== Part 2: Counter with mutex (correct) ===\n");
    counter = 0;
    for (int i = 0; i < THREADS; i++) pthread_create(&t[i], NULL, inc_with_lock, NULL);
    for (int i = 0; i < THREADS; i++) pthread_join(t[i], NULL);
    printf("Expected: %d, got: %ld ✅\n", THREADS * ITERATIONS, counter);
    wait_for_enter("Discuss: What does the mutex guarantee?");

    // Part 3
    printf("=== Part 3: Non-reentrant function (sequential) ===\n");
    char *p1 = upper_not_reentrant("hello"); printf("First call -> %s\n", p1);
    char *p2 = upper_not_reentrant("world"); printf("Second call -> %s (overwrote first)\n", p2);
    wait_for_enter("Discuss: Why overwrite?");

    // Part 3b
    printf("=== Part 3b: Non-reentrant under threads ===\n");
    const char *outA = NULL, *outB = NULL;
    args_bad_t a = { "abcdef", &outA }, b = { "XYZ123", &outB };
    pthread_t A, B;
    pthread_create(&A, NULL, thread_fn_bad, &a);
    pthread_create(&B, NULL, thread_fn_bad, &b);
    pthread_join(A, NULL); pthread_join(B, NULL);
    printf("Thread A saw: %s\n", outA);
    printf("Thread B saw: %s\n", outB);
    wait_for_enter("Discuss: Why both results alias the same memory?");

    // Part 4
    printf("=== Part 4: Reentrant function (caller buffers) ===\n");
    char A_buf[64], B_buf[64];
    args_ok_t a2 = { "abcdef", A_buf, sizeof(A_buf) };
    args_ok_t b2 = { "XYZ123", B_buf, sizeof(B_buf) };
    pthread_create(&A, NULL, thread_fn_ok, &a2);
    pthread_create(&B, NULL, thread_fn_ok, &b2);
    pthread_join(A, NULL); pthread_join(B, NULL);
    printf("Thread-safe results: A=\"%s\", B=\"%s\" ✅\n", A_buf, B_buf);
    wait_for_enter("Discuss: Why safe now?");

    // Part 5
    printf("=== Part 5: Bounds-safety clinic ===\n");
    char label[16]; read_label_bounded(label, sizeof(label));
    char tag[20]; int need = snprintf(tag, sizeof(tag), "TAG:%s", label);
    if (need >= (int)sizeof(tag)) fprintf(stderr, "[warn] tag truncated\n");
    printf("Safe tag = \"%s\"\n", tag);

    pthread_t T1, T2;
    bounds_args_t a1 = { .tag = tag, .name = "T1" };
    bounds_args_t a2b = { .tag = tag, .name = "T2" };
    pthread_create(&T1, NULL, fn_bounds, &a1);
    pthread_create(&T2, NULL, fn_bounds, &a2b);
    pthread_join(T1, NULL); pthread_join(T2, NULL);
    wait_for_enter("Discuss: How to detect truncation & avoid shared state?");

    // Part 6
    printf("=== Part 6: Semaphores (producer/consumer) ===\n");
    queue_t q; q_init(&q);
    pthread_t prod, cons;
    prod_args_t pa = { .q = &q, .count = PRODUCE_COUNT };
    cons_args_t ca = { .q = &q, .count = PRODUCE_COUNT };
    pthread_create(&prod, NULL, producer, &pa);
    pthread_create(&cons, NULL, consumer, &ca);
    pthread_join(prod, NULL); pthread_join(cons, NULL);
    q_destroy(&q);
    printf("Producer and consumer finished ✅\n");
    wait_for_enter("Discuss: What do empty/full count? Why mutex too?");

    puts("\nAll sections complete. Thanks!");
    return 0;
}

/* =======================================================================
                            DETAILED ANSWER KEY
   =======================================================================
Part 1 — Why is counter++ not atomic?
    • counter++ compiles to: load, add, store. Two threads can both load the
      same old value, then both write back, losing one update.
    • This is a data race: shared memory written without synchronization.

Part 2 — What does the mutex guarantee?
    • A mutex ensures only one thread executes the critical section at a time.
    • The counter updates are serialized → no lost increments.
    • Tradeoff: slower (threads wait their turn) but correct.

Part 3 — Why does second call overwrite first?
    • upper_not_reentrant() uses one static buffer for all calls.
    • The pointer from the first call points to the same memory reused by the second.
    • So the contents are replaced.

Part 3b — Why do both results alias the same memory?
    • Both threads return the address of the same static buffer.
    • Whichever finishes last overwrites it, so both outA/outB see that.

Part 4 — Why is it safe now?
    • Each caller passes its own output buffer.
    • No hidden global/static state is shared → results remain separate.
    • This is reentrancy: can be safely called by multiple threads.

Part 5 — How to detect truncation & avoid shared state?
    • fgets: if no newline read, input was too long → truncated.
    • snprintf: return value ≥ buffer size → output was truncated.
    • Avoid shared buffers by giving each thread its own local or caller-owned buffer.

Part 6 — What do empty/full count? Why a mutex too?
    • empty: how many free slots in the queue remain.
    • full: how many filled slots contain items.
    • Semaphores control *when* producers/consumers can proceed.
    • Mutex still needed to protect the actual array indices and memory from races.
*/
