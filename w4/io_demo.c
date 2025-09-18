// io_demo.c
// Recitation: Practical Input/Output in C (argv, fgets, strtok, strtol, fopen/fprintf)
// + Part 6: Bounds checking clinic
//
// Build:  gcc -O2 -Wall -Wextra -o io_demo io_demo.c
// Run:    ./io_demo [output_path] [-a]

#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ---------------------------- Small utilities ---------------------------- */

static void chomp_newline(char *s) {
    size_t n = strlen(s);
    if (n && s[n - 1] == '\n') s[n - 1] = '\0';
}

static void trim_spaces(char *s) {
    size_t i = 0;
    while (isspace((unsigned char)s[i])) i++;
    if (i) memmove(s, s + i, strlen(s + i) + 1);

    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
}

static bool parse_int_strict(const char *token, long *out) {
    if (!token || !*token) return false;
    errno = 0;
    char *end = NULL;
    long val = strtol(token, &end, 10);
    if (end == token) return false;
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end != '\0') return false;
    if (errno == ERANGE) return false;
    *out = val;
    return true;
}

/* --------- Bounds helpers: input flushing & safe snprintf checks ---------- */

// Flush the remainder of the current stdin line after a truncated fgets.
static void flush_stdin_line(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { /* discard */ }
}

// Wait-for-enter gate between parts.
static void wait_for_enter(void) {
    printf("\nPress ENTER to continue...\n");
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { /* discard extra */ }
}

/* ---------------------------- File helpers ------------------------------- */

static FILE* open_output(const char *path, bool append_mode) {
    const char *mode = append_mode ? "a" : "w";
    FILE *f = fopen(path, mode);
    if (!f) {
        fprintf(stderr, "error: cannot open '%s' (%s)\n", path, strerror(errno));
        return NULL;
    }
    return f;
}

/* ---------------------------- Pretty printing ---------------------------- */

static void show_argv(int argc, char **argv) {
    printf("=== Part 1: Command-line arguments (space-delimited by your shell) ===\n");
    printf("argc = %d (includes program name)\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("  argv[%d] = \"%s\"\n", i, argv[i]);
    }
    puts("");
}

/* ---------------------------- Main exercise ------------------------------ */

int main(int argc, char **argv) {
    const char *out_path = "output.txt";
    bool append_mode = false;

    if (argc >= 2 && strcmp(argv[1], "-a") != 0) {
        out_path = argv[1];
    }
    if ((argc >= 3 && strcmp(argv[2], "-a") == 0) ||
        (argc == 2 && strcmp(argv[1], "-a") == 0)) {
        append_mode = true;
        if (argc == 2) out_path = "output.txt";
    }

    show_argv(argc, argv);
    wait_for_enter();

    // ---------------------- Part 2: fgets + strtok ------------------------
    printf("=== Part 2: fgets (read a line) + strtok (split by SPACE) ===\n");
    printf("Type a short sentence (tokens will be split by spaces):\n> ");

    char line[256];
    if (!fgets(line, sizeof(line), stdin)) {
        fprintf(stderr, "error: no input received (EOF?)\n");
        return 1;
    }

    // Detect truncation: if newline wasn't captured, input > sizeof(line)-1
    bool truncated = (strchr(line, '\n') == NULL);
    if (truncated) {
        fprintf(stderr, "[warn] input longer than %zu chars; truncating & flushing\n",
                sizeof(line) - 1);
        flush_stdin_line();
    }

    chomp_newline(line);
    trim_spaces(line);

    printf("Raw line: \"%s\"%s\n", line, truncated ? "  (truncated)" : "");

    // Tokenize by spaces (collapse consecutive delimiters)
    int token_count = 0;
    char *tokens[64] = {0};
    {
        char *save = NULL;
        char *t = strtok_r(line, " ", &save);
        while (t && token_count < (int)(sizeof(tokens)/sizeof(tokens[0]))) {
            tokens[token_count++] = t;
            t = strtok_r(NULL, " ", &save);
        }
        if (t != NULL) {
            fprintf(stderr, "[warn] too many tokens; kept first %zu\n",
                    sizeof(tokens)/sizeof(tokens[0]));
        }
    }

    printf("Token count: %d\n", token_count);
    for (int i = 0; i < token_count; i++) {
        printf("  token[%d] = \"%s\"\n", i, tokens[i]);
    }
    puts("");
    wait_for_enter();

    // ---------------------- Part 3: Integers with strtol ------------------
    printf("=== Part 3: Detect integers among tokens using strtol ===\n");
    long sum = 0;
    int ints_found = 0;
    for (int i = 0; i < token_count; i++) {
        long val = 0;
        if (parse_int_strict(tokens[i], &val)) {
            printf("  numeric token: \"%s\" -> %ld\n", tokens[i], val);
            sum += val;
            ints_found++;
        } else {
            printf("  non-numeric token: \"%s\"\n", tokens[i]);
        }
    }
    if (ints_found > 0) {
        printf("Sum of numeric tokens = %ld\n", sum);
    } else {
        printf("No numeric tokens found.\n");
    }
    puts("");
    wait_for_enter();

    // ---------------------- Part 4: Write a report file -------------------
    printf("=== Part 4: Write a report with fopen/fprintf/fclose ===\n");
    printf("Output path: %s (%s)\n", out_path, append_mode ? "append" : "write");

    FILE *out = open_output(out_path, append_mode);
    if (!out) return 2;

    if (fprintf(out, "REPORT\n") < 0) {
        fprintf(stderr, "error: write failed for '%s'\n", out_path);
        fclose(out);
        return 2;
    }
    fprintf(out, "argv_count=%d\n", argc);
    for (int i = 0; i < argc; i++) {
        fprintf(out, "argv[%d]=%s\n", i, argv[i]);
    }
    fprintf(out, "line_tokens=%d\n", token_count);
    for (int i = 0; i < token_count; i++) {
        fprintf(out, "token[%d]=%s\n", i, tokens[i]);
    }
    fprintf(out, "numeric_tokens=%d\n", ints_found);
    if (ints_found > 0) fprintf(out, "sum=%ld\n", sum);
    fprintf(out, "---- END REPORT ----\n");

    fclose(out);
    printf("Wrote report to %s ✅\n\n", out_path);
    wait_for_enter();

    // ---------------------- Part 5: Read report back ----------------------
    printf("=== Part 5: Read the report back with fgets ===\n");
    FILE *in = fopen(out_path, "r");
    if (!in) {
        fprintf(stderr, "error: cannot reopen '%s' (%s)\n", out_path, strerror(errno));
        return 2;
    }
    char buf[256];
    while (fgets(buf, sizeof(buf), in)) {
        printf("  %s", buf); // fgets keeps newline
    }
    fclose(in);
    puts("");
    wait_for_enter();

    // ---------------------- Part 6: Bounds checking clinic ----------------
    printf("=== Part 6: Bounds checking clinic (fgets + snprintf) ===\n");

    // (A) Bounded fgets with truncation detection & flushing
    // Ask for a short label (max 15 chars).
    char label[16];  // room for 15 chars + '\0'
    printf("Enter a short label (<=15 chars):\n> ");
    if (!fgets(label, sizeof(label), stdin)) {
        fprintf(stderr, "error: no input for label\n");
        return 1;
    }
    bool lab_trunc = (strchr(label, '\n') == NULL);
    if (lab_trunc) {
        fprintf(stderr, "[warn] label truncated to %zu chars; flushing rest\n",
                sizeof(label) - 1);
        flush_stdin_line();
    }
    chomp_newline(label);

    // (B) Safe formatting into small buffers with snprintf
    // Make a short tag like: "TAG:<label>"
    char tag[20];
    int need = snprintf(tag, sizeof(tag), "TAG:%s", label);
    // snprintf returns the number of chars it *wanted* to write (excluding '\0')
    if (need >= (int)sizeof(tag)) {
        fprintf(stderr, "[warn] tag truncated (need %d, cap %zu)\n", need, sizeof(tag));
    }
    printf("tag = \"%s\"\n", tag);

    // Build a tiny path like "tmp/<label>.txt" in a very small buffer.
    char tiny_path[24];
    need = snprintf(tiny_path, sizeof(tiny_path), "tmp/%s.txt", label);
    if (need >= (int)sizeof(tiny_path)) {
        fprintf(stderr, "[warn] path truncated (need %d, cap %zu)\n", need, sizeof(tiny_path));
    }
    printf("tiny_path = \"%s\"\n", tiny_path);

    // (C) Token array bounds already demonstrated above:
    // we never write past tokens[64] and warn if there were more tokens.

    puts("\nBounds tips:\n"
         "  • With fgets: if no newline is captured, input was too long; flush the rest.\n"
         "  • With snprintf: check return value; if >= buffer size, it truncated.\n"
         "  • For arrays: always compare an index against the array length before writing.\n");

    return 0;
}