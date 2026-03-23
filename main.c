#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdint.h>

typedef struct S { int t; double n; char *s; struct S **l; int len; } S;
typedef long (*ffi_fn)(long, long, long, long, long, long);  // Proper prototype

#define NUM 1
#define SYM 2
#define LST 3
#define T_TRUE 4
#define T_FALSE 5
#define T_NIL 6
#define FFI_FN 7

char *buf = NULL;
size_t buf_size = 4096;
char *p, *env_keys[256];
S *env_vals[256], nil_cell = {.t = T_NIL, .n = 0.0, .s = NULL, .l = NULL, .len = 0};
int env_len = 0;

// FFI function cache
typedef struct {
    char *lib_name;
    char *fn_name;
    void *lib_handle;
    ffi_fn fn;
} ffi_cache_entry;

#define MAX_FFI_CACHE 64
ffi_cache_entry ffi_cache[MAX_FFI_CACHE];
int ffi_cache_len = 0;

S *n(double x) { S *s = malloc(sizeof(S)); s->t = NUM; s->n = x; return s; }
S *m(char *x) {
    S *s = malloc(sizeof(S));
    s->t = SYM;
    size_t len = strlen(x);
    s->s = malloc(len + 1);
    memcpy(s->s, x, len);
    s->s[len] = '\0';
    return s;
}
S *l(int c) { S *s = malloc(sizeof(S)); s->t = LST; s->l = malloc(c*sizeof(S*)); s->len = c; return s; }
S *t() { S *s = malloc(sizeof(S)); s->t = T_TRUE; return s; }
S *f() { S *s = malloc(sizeof(S)); s->t = T_FALSE; return s; }
S *nil() { return &nil_cell; }

S *ffi_cell(char *lib, char *fn, ffi_fn func) {
    S *s = malloc(sizeof(S));
    s->t = FFI_FN;
    s->s = malloc(strlen(lib) + strlen(fn) + 2);
    snprintf(s->s, strlen(lib) + strlen(fn) + 2, "%s:%s", lib, fn);
    s->n = (double)(intptr_t)func;
    return s;
}

void env_set(char *k, S *v) {
    for (int i = 0; i < env_len; i++) if (!strcmp(env_keys[i], k)) { env_vals[i] = v; return; }
    env_keys[env_len] = k;
    env_vals[env_len++] = v;
}

S *env_get(char *k) {
    for (int i = 0; i < env_len; i++) if (!strcmp(env_keys[i], k)) return env_vals[i];
    return nil();
}

// Load FFI function from library
ffi_fn ffi_load(char *lib_name, char *fn_name) {
    // Check cache first
    for (int i = 0; i < ffi_cache_len; i++) {
        if (!strcmp(ffi_cache[i].lib_name, lib_name) &&
            !strcmp(ffi_cache[i].fn_name, fn_name)) {
            return ffi_cache[i].fn;
        }
    }

    if (ffi_cache_len >= MAX_FFI_CACHE) {
        fprintf(stderr, "FFI cache full\n");
        return NULL;
    }

    // Load library - dlopen(NULL) works for libc on Unix systems
    void *handle;

    if (!strcmp(lib_name, "libc") || !strcmp(lib_name, "c")) {
        // Use dlopen(NULL) to access already-loaded libc
        // Works on: Linux, macOS, OpenBSD, and most Unix systems
        handle = dlopen(NULL, RTLD_LAZY);
    } else {
        // Load custom library
        handle = dlopen(lib_name, RTLD_LAZY);
    }

    if (!handle) {
        fprintf(stderr, "Cannot load library %s: %s\n", lib_name, dlerror());
        return NULL;
    }

    // Load function
    ffi_fn fn = (ffi_fn)dlsym(handle, fn_name);
    if (!fn) {
        fprintf(stderr, "Cannot find function %s in %s: %s\n", fn_name, lib_name, dlerror());
        // Note: we don't dlclose here because for libc we need to keep it open
        return NULL;
    }

    // Cache it
    ffi_cache[ffi_cache_len].lib_name = malloc(strlen(lib_name) + 1);
    ffi_cache[ffi_cache_len].fn_name = malloc(strlen(fn_name) + 1);
    memcpy(ffi_cache[ffi_cache_len].lib_name, lib_name, strlen(lib_name));
    ffi_cache[ffi_cache_len].lib_name[strlen(lib_name)] = '\0';
    memcpy(ffi_cache[ffi_cache_len].fn_name, fn_name, strlen(fn_name));
    ffi_cache[ffi_cache_len].fn_name[strlen(fn_name)] = '\0';
    ffi_cache[ffi_cache_len].lib_handle = handle;
    ffi_cache[ffi_cache_len].fn = fn;
    ffi_cache_len++;

    return fn;
}

// Convert S cell to native type for FFI call
long s_to_long(S *s) {
    if (!s) return 0;
    if (s->t == NUM) return (long)s->n;
    if (s->t == SYM) return (long)(intptr_t)s->s;
    if (s->t == T_TRUE) return 1;
    if (s->t == T_FALSE) return 0;
    return 0;
}

S *read_atom(), *read_list();

S *read_atom() {
    while (*p && isspace(*p)) p++;
    if (!*p) return nil();
    if (*p == '(') return read_list();

    // Handle string literals (quoted with ")
    if (*p == '"') {
        p++;  // skip opening quote
        char w[256], *wp = w;
        // Read until closing quote
        while (*p && *p != '"') {
            if (*p == '\\' && *(p+1)) {
                // Handle escape sequences
                p++;
                if (*p == 'n') *wp++ = '\n';
                else if (*p == 't') *wp++ = '\t';
                else if (*p == 'r') *wp++ = '\r';
                else if (*p == '"') *wp++ = '"';
                else if (*p == '\\') *wp++ = '\\';
                else *wp++ = *p;
                p++;
            } else {
                *wp++ = *p++;
            }
        }
        if (*p == '"') p++;  // skip closing quote
        *wp = 0;
        return m(w);
    }

    // Handle regular symbols/numbers
    char w[256], *wp = w;
    while (*p && !isspace(*p) && *p != ')') *wp++ = *p++;
    *wp = 0;

    if (isdigit(w[0]) || (w[0] == '-' && isdigit(w[1]))) return n(atof(w));
    if (!strcmp(w, "true")) return t();
    if (!strcmp(w, "false")) return f();
    if (!strcmp(w, "nil")) return nil();
    return m(w);
}

S *read_list() {
    p++;
    while (*p && isspace(*p)) p++;
    int cap = 10, cnt = 0;
    S **cells = malloc(cap * sizeof(S*));
    while (*p && *p != ')') {
        if (cnt >= cap) cells = realloc(cells, (cap *= 2) * sizeof(S*));
        cells[cnt++] = read_atom();
        while (*p && isspace(*p)) p++;
    }
    p++;
    S *res = l(cnt);
    memcpy(res->l, cells, cnt * sizeof(S*));
    free(cells);
    return res;
}

S *eval(S *e);

S *eval(S *e) {
    if (!e || e->t == T_NIL) return nil();
    if (e->t == NUM || e->t == T_TRUE || e->t == T_FALSE) return e;
    if (e->t == SYM) return env_get(e->s);
    if (e->t != LST || !e->len) return nil();

    // Handle special forms that need unevaluated arguments
    if (e->l[0]->t == SYM) {
        char *op = e->l[0]->s;
        if (!strcmp(op, "define")) {
            S *v = eval(e->l[2]);
            env_set(e->l[1]->s, v);
            return e->l[1];
        }
        if (!strcmp(op, "if")) {
            S *cond = eval(e->l[1]);
            return eval(cond->t != T_FALSE && cond->t != T_NIL ? e->l[2] : e->l[3]);
        }
        if (!strcmp(op, "lambda")) {
            S *res = l(2);
            res->l[0] = e->l[1];
            res->l[1] = e->l[2];
            res->t = LST;
            return res;
        }
        if (!strcmp(op, "ffi")) {
            // (ffi "libc" "write" 1 "hello")
            if (e->len < 3) return nil();

            // Get library and function names - they should be literal symbols (quoted strings)
            S *lib_cell = e->l[1];
            S *fn_cell = e->l[2];

            // Check if they're symbols (unevaluated string literals)
            if (lib_cell->t != SYM || fn_cell->t != SYM) {
                fprintf(stderr, "FFI: library and function names must be string literals\n");
                return nil();
            }

            char *lib_name = lib_cell->s;
            char *fn_name = fn_cell->s;

            ffi_fn fn = ffi_load(lib_name, fn_name);
            if (!fn) return nil();

            // Convert args - evaluate numbers but not string literals
            long args[6] = {0};
            for (int i = 0; i < 6 && i+3 < e->len; i++) {
                S *arg = e->l[i+3];
                // If it's a symbol, it might be a string literal - don't evaluate
                // If it's a number or expression, evaluate it
                if (arg->t == NUM) {
                    args[i] = (long)arg->n;
                } else if (arg->t == SYM) {
                    // Could be a string literal or a variable - try variable first
                    S *val = env_get(arg->s);
                    if (val->t == T_NIL) {
                        // Not found in environment, treat as string literal
                        args[i] = (long)(intptr_t)arg->s;
                    } else {
                        // Found in environment, use that value
                        args[i] = s_to_long(val);
                    }
                } else {
                    // For other types, evaluate normally
                    args[i] = s_to_long(eval(arg));
                }
            }

            // Call with up to 6 arguments
            long result = fn(args[0], args[1], args[2], args[3], args[4], args[5]);
            return n((double)result);
        }

        double sum = 0, prod = 1;
        for (int i = 1; i < e->len; i++) {
            S *arg = eval(e->l[i]);
            double v = arg->t == NUM ? arg->n : 0;

            if (!strcmp(op, "+")) sum += v;
            else if (!strcmp(op, "-")) sum = (i==1) ? v : sum - v;
            else if (!strcmp(op, "*")) prod *= v;
            else if (!strcmp(op, "/")) prod = (i==1) ? v : prod / v;
            else if (!strcmp(op, "car")) return arg->len > 0 ? arg->l[0] : nil();
            else if (!strcmp(op, "cdr")) {
                if (arg->len <= 1) return nil();
                S *res = l(arg->len - 1);
                for (int j = 1; j < arg->len; j++) res->l[j-1] = arg->l[j];
                return res;
            }
        }

        if (!strcmp(op, "+")) return n(sum);
        if (!strcmp(op, "-")) return n(sum);
        if (!strcmp(op, "*")) return n(prod);
        if (!strcmp(op, "/")) return n(prod);
        if (!strcmp(op, "=")) {
            if (e->len < 3) return f();
            double v = eval(e->l[1])->t == NUM ? eval(e->l[1])->n : 0;
            for (int i = 2; i < e->len; i++) {
                double v2 = eval(e->l[i])->t == NUM ? eval(e->l[i])->n : 0;
                if (v2 != v) return f();
            }
            return t();
        }
        if (!strcmp(op, "<")) {
            if (e->len < 3) return f();
            double v = eval(e->l[1])->t == NUM ? eval(e->l[1])->n : 0;
            for (int i = 2; i < e->len; i++) {
                double v2 = eval(e->l[i])->t == NUM ? eval(e->l[i])->n : 0;
                if (v >= v2) return f();
                v = v2;
            }
            return t();
        }
        if (!strcmp(op, ">")) {
            if (e->len < 3) return f();
            double v = eval(e->l[1])->t == NUM ? eval(e->l[1])->n : 0;
            for (int i = 2; i < e->len; i++) {
                double v2 = eval(e->l[i])->t == NUM ? eval(e->l[i])->n : 0;
                if (v <= v2) return f();
                v = v2;
            }
            return t();
        }
        if (!strcmp(op, "list")) {
            S *res = l(e->len - 1);
            for (int i = 1; i < e->len; i++) res->l[i-1] = eval(e->l[i]);
            return res;
        }
    }

    // Handle function application (when first element is not a symbol operator)
    S *fn = eval(e->l[0]);
    if (fn->t == LST && fn->len == 2) {
        S *params = fn->l[0];
        S *body = fn->l[1];
        for (int i = 0; i < params->len && i+1 < e->len; i++)
            env_set(params->l[i]->s, eval(e->l[i+1]));
        return eval(body);
    }

    return nil();
}

void print(S *s) {
    if (!s) return;
    if (s->t == NUM) printf("%g", s->n);
    else if (s->t == SYM) printf("%s", s->s);
    else if (s->t == T_TRUE) printf("true");
    else if (s->t == T_FALSE) printf("false");
    else if (s->t == T_NIL) printf("nil");
    else if (s->t == FFI_FN) printf("#<ffi:%s>", s->s);
    else if (s->t == LST) {
        printf("(");
        for (int i = 0; i < s->len; i++) { if (i) printf(" "); print(s->l[i]); }
        printf(")");
    }
}

int main(int argc, char **argv) {
    buf = malloc(buf_size);
    if (!buf) {
        fprintf(stderr, "Failed to allocate buffer\n");
        return 1;
    }

    env_set("+", m("+"));
    env_set("-", m("-"));
    env_set("*", m("*"));
    env_set("/", m("/"));
    env_set("=", m("="));
    env_set("<", m("<"));
    env_set(">", m(">"));
    env_set("list", m("list"));
    env_set("car", m("car"));
    env_set("cdr", m("cdr"));
    env_set("if", m("if"));
    env_set("define", m("define"));
    env_set("lambda", m("lambda"));
    env_set("ffi", m("ffi"));

    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            FILE *f = fopen(argv[i], "r");
            if (!f) continue;
            while (fgets(buf, buf_size, f)) {
                if (buf[0] == ';') continue;
                p = buf;
                S *e = read_atom();
                S *res = eval(e);
                print(res);
                printf("\n");
            }
            fclose(f);
        }
    } else {
        if (isatty(0)) printf("nil-lisp\n");
        while (fgets(buf, buf_size, stdin)) {
            p = buf;
            S *e = read_atom();
            S *res = eval(e);
            print(res);
            printf("\n");
        }
    }

    free(buf);
    return 0;
}
