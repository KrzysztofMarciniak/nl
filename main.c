#include <ctype.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NUM 1
#define SYM 2
#define LST 3
#define T_TRUE 4
#define T_FALSE 5
#define T_NIL 6
#define FFI_FN 7
#define T_STR 8

typedef struct S {
        int t;
        double n;
        char* s;
        struct S** l;
        int len;
} S;

typedef long (*ffi_fn)(long, long, long, long, long, long);

static char* buf       = NULL;
static size_t buf_size = 4096;
static char* p;
static char* env_keys[256];
static S* env_vals[256];
static int env_len = 0;

static S nil_cell = {.t = T_NIL, .n = 0.0, .s = NULL, .l = NULL, .len = 0};

#define MAX_FFI_CACHE 64

typedef struct {
        char* lib_name;
        char* fn_name;
        void* lib_handle;
        ffi_fn fn;
} ffi_cache_entry;

static ffi_cache_entry ffi_cache[MAX_FFI_CACHE];
static int ffi_cache_len = 0;

static S* n(double x) {
        S* s   = malloc(sizeof(S));
        s->t   = NUM;
        s->n   = x;
        s->s   = NULL;
        s->l   = NULL;
        s->len = 0;
        return s;
}

static S* sym(const char* x) {
        S* s       = malloc(sizeof(S));
        s->t       = SYM;
        s->n       = 0.0;
        s->l       = NULL;
        s->len     = 0;
        size_t len = strlen(x);
        s->s       = malloc(len + 1);
        memcpy(s->s, x, len + 1);
        return s;
}

static S* str_cell(const char* x) {
        S* s       = malloc(sizeof(S));
        s->t       = T_STR;
        s->n       = 0.0;
        s->l       = NULL;
        s->len     = 0;
        size_t len = strlen(x);
        s->s       = malloc(len + 1);
        memcpy(s->s, x, len + 1);
        return s;
}

static S* list_cell(int c) {
        S* s   = malloc(sizeof(S));
        s->t   = LST;
        s->n   = 0.0;
        s->s   = NULL;
        s->l   = malloc(c * sizeof(S*));
        s->len = c;
        return s;
}

static S* t(void) {
        S* s   = malloc(sizeof(S));
        s->t   = T_TRUE;
        s->n   = 0.0;
        s->s   = NULL;
        s->l   = NULL;
        s->len = 0;
        return s;
}

static S* f(void) {
        S* s   = malloc(sizeof(S));
        s->t   = T_FALSE;
        s->n   = 0.0;
        s->s   = NULL;
        s->l   = NULL;
        s->len = 0;
        return s;
}

static S* nil(void) { return &nil_cell; }

static void env_set(char* k, S* v) {
        for (int i = 0; i < env_len; i++) {
                if (!strcmp(env_keys[i], k)) {
                        env_vals[i] = v;
                        return;
                }
        }
        env_keys[env_len]   = k;
        env_vals[env_len++] = v;
}

static S* env_get(char* k) {
        for (int i = 0; i < env_len; i++) {
                if (!strcmp(env_keys[i], k)) return env_vals[i];
        }
        return nil();
}

static ffi_fn ffi_load(char* lib_name, char* fn_name) {
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

        void* handle = NULL;
        if (!strcmp(lib_name, "libc") || !strcmp(lib_name, "c")) {
                handle = RTLD_DEFAULT;
        } else {
                handle = dlopen(lib_name, RTLD_LAZY | RTLD_GLOBAL);
        }

        if (!handle) {
                fprintf(stderr, "Cannot load library %s: %s\n", lib_name,
                        dlerror());
                return NULL;
        }

        dlerror();
        ffi_fn fn = (ffi_fn)dlsym(handle, fn_name);
        char* err = dlerror();
        if (err) {
                fprintf(stderr, "Cannot find function %s in %s: %s\n", fn_name,
                        lib_name, err);
                return NULL;
        }

        ffi_cache[ffi_cache_len].lib_name = malloc(strlen(lib_name) + 1);
        ffi_cache[ffi_cache_len].fn_name  = malloc(strlen(fn_name) + 1);
        memcpy(ffi_cache[ffi_cache_len].lib_name, lib_name,
               strlen(lib_name) + 1);
        memcpy(ffi_cache[ffi_cache_len].fn_name, fn_name, strlen(fn_name) + 1);
        ffi_cache[ffi_cache_len].lib_handle = handle;
        ffi_cache[ffi_cache_len].fn         = fn;
        ffi_cache_len++;

        return fn;
}

static long s_to_long(S* s) {
        if (!s) return 0;
        switch (s->t) {
                case NUM:
                        return (long)s->n;
                case SYM:
                        return (long)(intptr_t)s->s;
                case T_STR:
                        return (long)(intptr_t)s->s;
                case T_TRUE:
                        return 1;
                case T_FALSE:
                        return 0;
                case T_NIL:
                        return 0;
                default:
                        return 0;
        }
}

static char* xstrdup(const char* s) {
        size_t len = strlen(s ? s : "");
        char* out  = malloc(len + 1);
        if (!out) return NULL;
        memcpy(out, s ? s : "", len + 1);
        return out;
}

static void append_chunk(char** dst, size_t* cap, const char* src) {
        size_t need = strlen(*dst) + strlen(src) + 1;
        if (need > *cap) {
                while (need > *cap) *cap *= 2;
                *dst = realloc(*dst, *cap);
        }
        strcat(*dst, src);
}

static char* stringify(S* s) {
        if (!s) return NULL;
        char tmp[128];

        switch (s->t) {
                case NUM:
                        snprintf(tmp, sizeof(tmp), "%g", s->n);
                        return xstrdup(tmp);
                case SYM:
                        return xstrdup(s->s ? s->s : "");
                case T_STR:
                        return xstrdup(s->s ? s->s : "");
                case T_TRUE:
                        return xstrdup("true");
                case T_FALSE:
                        return xstrdup("false");
                case T_NIL:
                        return xstrdup("nil");
                case LST: {
                        size_t cap = 64;
                        char* out  = malloc(cap);
                        out[0]     = '\0';
                        append_chunk(&out, &cap, "(");
                        for (int i = 0; i < s->len; i++) {
                                if (i) append_chunk(&out, &cap, " ");
                                char* part = stringify(s->l[i]);
                                append_chunk(&out, &cap, part ? part : "");
                                free(part);
                        }
                        append_chunk(&out, &cap, ")");
                        return out;
                }
                case FFI_FN:
                        snprintf(tmp, sizeof(tmp), "#<ffi:%s>",
                                 s->s ? s->s : "");
                        return xstrdup(tmp);
                default:
                        return xstrdup("");
        }
}

static void print_escaped_str(const char* s) {
        putchar('"');
        for (; *s; s++) {
                switch (*s) {
                        case '\n':
                                fputs("\\n", stdout);
                                break;
                        case '\t':
                                fputs("\\t", stdout);
                                break;
                        case '\r':
                                fputs("\\r", stdout);
                                break;
                        case '"':
                                fputs("\\\"", stdout);
                                break;
                        case '\\':
                                fputs("\\\\", stdout);
                                break;
                        default:
                                putchar(*s);
                                break;
                }
        }
        putchar('"');
}

static void print(S* s, bool in_repl_mode) {
        if (!s) return;
        switch (s->t) {
                case NUM:
                        printf("%g", s->n);
                        break;
                case SYM:
                        if (in_repl_mode) printf("%s", s->s ? s->s : "");
                        break;
                case T_STR:
                        print_escaped_str(s->s ? s->s : "");
                        break;
                case T_TRUE:
                        printf("true");
                        break;
                case T_FALSE:
                        printf("false");
                        break;
                case T_NIL:
                        printf("nil");
                        break;
                case FFI_FN:
                        printf("#<ffi:%s>", s->s ? s->s : "");
                        break;
                case LST:
                        putchar('(');
                        for (int i = 0; i < s->len; i++) {
                                if (i) putchar(' ');
                                print(s->l[i], in_repl_mode);
                        }
                        putchar(')');
                        break;
        }
}

static S* read_atom(void);
static S* read_list(void);

static S* read_list(void) {
        p++;
        while (*p && isspace((unsigned char)*p)) p++;

        int cap = 10, cnt = 0;
        S** cells = malloc(cap * sizeof(S*));

        while (*p && *p != ')') {
                if (cnt >= cap) {
                        cap *= 2;
                        cells = realloc(cells, cap * sizeof(S*));
                }
                cells[cnt++] = read_atom();
                while (*p && isspace((unsigned char)*p)) p++;
        }

        if (*p == ')') p++;

        S* res = list_cell(cnt);
        if (cnt > 0) memcpy(res->l, cells, cnt * sizeof(S*));
        free(cells);
        return res;
}

static S* read_atom(void) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) return nil();
        if (*p == '(') return read_list();

        if (*p == '"') {
                p++;
                size_t cap = 64, len = 0;
                char* w = malloc(cap);
                while (*p && *p != '"') {
                        char c;
                        if (*p == '\\' && *(p + 1)) {
                                p++;
                                switch (*p) {
                                        case 'n':
                                                c = '\n';
                                                break;
                                        case 't':
                                                c = '\t';
                                                break;
                                        case 'r':
                                                c = '\r';
                                                break;
                                        case '"':
                                                c = '"';
                                                break;
                                        case '\\':
                                                c = '\\';
                                                break;
                                        default:
                                                c = *p;
                                                break;
                                }
                                p++;
                        } else {
                                c = *p++;
                        }
                        if (len + 2 > cap) {
                                cap *= 2;
                                w = realloc(w, cap);
                        }
                        w[len++] = c;
                }
                if (*p == '"') p++;
                w[len] = '\0';
                S* res = str_cell(w);
                free(w);
                return res;
        }

        char w[256], *wp = w;
        while (*p && !isspace((unsigned char)*p) && *p != ')') *wp++ = *p++;
        *wp = '\0';

        if (isdigit((unsigned char)w[0]) ||
            (w[0] == '-' && isdigit((unsigned char)w[1])))
                return n(atof(w));
        if (!strcmp(w, "true")) return t();
        if (!strcmp(w, "false")) return f();
        if (!strcmp(w, "nil")) return nil();
        return sym(w);
}

static S* eval(S* e);

static S* eval_concat(S* e) {
        size_t cap = 64;
        char* out  = malloc(cap);
        out[0]     = '\0';

        for (int i = 1; i < e->len; i++) {
                S* arg     = eval(e->l[i]);
                char* part = NULL;

                if (arg->t == T_STR || arg->t == SYM) {
                        part = xstrdup(arg->s ? arg->s : "");
                } else if (arg->t == NUM) {
                        char tmp[128];
                        snprintf(tmp, sizeof(tmp), "%g", arg->n);
                        part = xstrdup(tmp);
                } else if (arg->t == T_TRUE) {
                        part = xstrdup("true");
                } else if (arg->t == T_FALSE) {
                        part = xstrdup("false");
                } else if (arg->t == T_NIL) {
                        part = xstrdup("nil");
                } else {
                        part = stringify(arg);
                }

                if (!part) part = xstrdup("");
                size_t need = strlen(out) + strlen(part) + 1;
                if (need > cap) {
                        while (need > cap) cap *= 2;
                        out = realloc(out, cap);
                }
                strcat(out, part);
                free(part);
        }

        S* res = str_cell(out);
        free(out);
        return res;
}

static S* eval(S* e) {
        if (!e || e->t == T_NIL) return nil();
        if (e->t == NUM || e->t == T_TRUE || e->t == T_FALSE || e->t == T_STR)
                return e;
        if (e->t == SYM) return env_get(e->s);
        if (e->t != LST || !e->len) return nil();

        if (e->l[0]->t == SYM) {
                char* op = e->l[0]->s;

                if (!strcmp(op, "define")) {
                        if (e->len < 3 || e->l[1]->t != SYM) return nil();
                        S* v = eval(e->l[2]);
                        env_set(e->l[1]->s, v);
                        return e->l[1];
                }

                if (!strcmp(op, "if")) {
                        if (e->len < 3) return nil();
                        S* cond = eval(e->l[1]);
                        if (cond->t != T_FALSE && cond->t != T_NIL) {
                                return eval(e->len > 2 ? e->l[2] : nil());
                        }
                        return eval(e->len > 3 ? e->l[3] : nil());
                }

                if (!strcmp(op, "lambda")) {
                        if (e->len < 3) return nil();
                        S* res    = list_cell(2);
                        res->l[0] = e->l[1];
                        res->l[1] = e->l[2];
                        return res;
                }

                if (!strcmp(op, "ffi")) {
                        if (e->len < 3) return nil();

                        S* lib_cell = e->l[1];
                        S* fn_cell  = e->l[2];

                        if (!((lib_cell->t == T_STR) || (lib_cell->t == SYM)) ||
                            !((fn_cell->t == T_STR) || (fn_cell->t == SYM))) {
                                fprintf(stderr,
                                        "FFI: library and function names must "
                                        "be strings\n");
                                return nil();
                        }

                        char* lib_name = lib_cell->s;
                        char* fn_name  = fn_cell->s;
                        ffi_fn fn      = ffi_load(lib_name, fn_name);
                        if (!fn) return nil();

                        long args[6] = {0};
                        for (int i = 0; i < 6 && i + 3 < e->len; i++) {
                                S* arg  = eval(e->l[i + 3]);
                                args[i] = s_to_long(arg);
                        }

                        long result = fn(args[0], args[1], args[2], args[3],
                                         args[4], args[5]);
                        return n((double)result);
                }

                if (!strcmp(op, "strlen")) {
                        if (e->len != 2) return n(0);
                        S* arg = eval(e->l[1]);
                        if (arg->t == T_STR || arg->t == SYM)
                                return n((double)strlen(arg->s ? arg->s : ""));
                        return n(0);
                }

                if (!strcmp(op, "concat")) {
                        if (e->len < 2) return str_cell("");
                        return eval_concat(e);
                }

                if (!strcmp(op, "+") || !strcmp(op, "-") || !strcmp(op, "*") ||
                    !strcmp(op, "/")) {
                        double acc = (!strcmp(op, "*")) ? 1.0 : 0.0;
                        for (int i = 1; i < e->len; i++) {
                                S* arg   = eval(e->l[i]);
                                double v = (arg->t == NUM) ? arg->n : 0.0;
                                if (!strcmp(op, "+"))
                                        acc += v;
                                else if (!strcmp(op, "-"))
                                        acc = (i == 1) ? v : acc - v;
                                else if (!strcmp(op, "*"))
                                        acc *= v;
                                else if (!strcmp(op, "/"))
                                        acc = (i == 1) ? v : acc / v;
                        }
                        return n(acc);
                }

                if (!strcmp(op, "=")) {
                        if (e->len < 3) return f();
                        S* first = eval(e->l[1]);
                        double v = (first->t == NUM) ? first->n : 0.0;
                        for (int i = 2; i < e->len; i++) {
                                S* cur    = eval(e->l[i]);
                                double v2 = (cur->t == NUM) ? cur->n : 0.0;
                                if (v2 != v) return f();
                        }
                        return t();
                }

                if (!strcmp(op, "<")) {
                        if (e->len < 3) return f();
                        S* first = eval(e->l[1]);
                        double v = (first->t == NUM) ? first->n : 0.0;
                        for (int i = 2; i < e->len; i++) {
                                S* cur    = eval(e->l[i]);
                                double v2 = (cur->t == NUM) ? cur->n : 0.0;
                                if (v >= v2) return f();
                                v = v2;
                        }
                        return t();
                }

                if (!strcmp(op, ">")) {
                        if (e->len < 3) return f();
                        S* first = eval(e->l[1]);
                        double v = (first->t == NUM) ? first->n : 0.0;
                        for (int i = 2; i < e->len; i++) {
                                S* cur    = eval(e->l[i]);
                                double v2 = (cur->t == NUM) ? cur->n : 0.0;
                                if (v <= v2) return f();
                                v = v2;
                        }
                        return t();
                }

                if (!strcmp(op, "list")) {
                        S* res = list_cell(e->len - 1);
                        for (int i = 1; i < e->len; i++)
                                res->l[i - 1] = eval(e->l[i]);
                        return res;
                }

                if (!strcmp(op, "car")) {
                        if (e->len != 2) return nil();
                        S* arg = eval(e->l[1]);
                        if (arg->t != LST || arg->len <= 0) return nil();
                        return arg->l[0];
                }

                if (!strcmp(op, "cdr")) {
                        if (e->len != 2) return nil();
                        S* arg = eval(e->l[1]);
                        if (arg->t != LST || arg->len <= 1) return nil();
                        S* res = list_cell(arg->len - 1);
                        for (int j = 1; j < arg->len; j++)
                                res->l[j - 1] = arg->l[j];
                        return res;
                }
        }

        S* fn = eval(e->l[0]);
        if (fn->t == LST && fn->len == 2) {
                S* params = fn->l[0];
                S* body   = fn->l[1];
                for (int i = 0; i < params->len && i + 1 < e->len; i++) {
                        if (params->l[i]->t == SYM)
                                env_set(params->l[i]->s, eval(e->l[i + 1]));
                }
                return eval(body);
        }

        return nil();
}

int main(int argc, char** argv) {
        env_set("+", sym("+"));
        env_set("-", sym("-"));
        env_set("*", sym("*"));
        env_set("/", sym("/"));
        env_set("=", sym("="));
        env_set("<", sym("<"));
        env_set(">", sym(">"));
        env_set("list", sym("list"));
        env_set("car", sym("car"));
        env_set("cdr", sym("cdr"));
        env_set("if", sym("if"));
        env_set("define", sym("define"));
        env_set("lambda", sym("lambda"));
        env_set("ffi", sym("ffi"));
        env_set("strlen", sym("strlen"));
        env_set("concat", sym("concat"));

        if (argc > 1) {
                for (int i = 1; i < argc; i++) {
                        FILE* f = fopen(argv[i], "r");
                        if (!f) continue;

                        fseek(f, 0, SEEK_END);
                        long fsize = ftell(f);
                        fseek(f, 0, SEEK_SET);

                        buf = malloc((size_t)fsize + 1);
                        if (!buf) {
                                fprintf(stderr,
                                        "Failed to allocate buffer for %s\n",
                                        argv[i]);
                                fclose(f);
                                continue;
                        }

                        buf_size    = (size_t)fsize + 1;
                        size_t read = fread(buf, 1, (size_t)fsize, f);
                        buf[read]   = '\0';

                        p = buf;
                        while (*p) {
                                while (*p && isspace((unsigned char)*p) &&
                                       *p != '\n')
                                        p++;
                                if (*p == ';') {
                                        while (*p && *p != '\n') p++;
                                        if (*p == '\n') p++;
                                        continue;
                                }
                                if (*p == '\n') {
                                        p++;
                                        continue;
                                }
                                if (!*p) break;

                                S* e   = read_atom();
                                S* res = eval(e);
                                print(res, false);
                        }

                        free(buf);
                        fclose(f);
                }
        } else {
                buf_size = 4096;
                buf      = malloc(buf_size);
                if (!buf) {
                        fprintf(stderr, "Failed to allocate buffer\n");
                        return 1;
                }

                if (isatty(0)) printf("nil-lisp v0.1\n");
                while (fgets(buf, (int)buf_size, stdin)) {
                        p      = buf;
                        S* e   = read_atom();
                        S* res = eval(e);
                        print(res, true);
                        putchar('\n');
                }

                free(buf);
        }

        return 0;
}
