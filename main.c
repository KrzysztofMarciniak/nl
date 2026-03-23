#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

typedef struct S { int t; double n; char *s; struct S **l; int len; } S;
#define NUM 1
#define SYM 2
#define LST 3
#define T_TRUE 4
#define T_FALSE 5
#define T_NIL 6

char buf[4096], *p, *env_keys[256];
S *env_vals[256], nil_cell = {T_NIL};
int env_len = 0;

S *n(double x) { S *s = malloc(sizeof(S)); s->t = NUM; s->n = x; return s; }
S *m(char *x) { S *s = malloc(sizeof(S)); s->t = SYM; s->s = malloc(strlen(x)+1); strcpy(s->s, x); return s; }
S *l(int c) { S *s = malloc(sizeof(S)); s->t = LST; s->l = malloc(c*sizeof(S*)); s->len = c; return s; }
S *t() { S *s = malloc(sizeof(S)); s->t = T_TRUE; return s; }
S *f() { S *s = malloc(sizeof(S)); s->t = T_FALSE; return s; }
S *nil() { return &nil_cell; }

void env_set(char *k, S *v) {
    for (int i = 0; i < env_len; i++) if (!strcmp(env_keys[i], k)) { env_vals[i] = v; return; }
    env_keys[env_len] = k;
    env_vals[env_len++] = v;
}

S *env_get(char *k) {
    for (int i = 0; i < env_len; i++) if (!strcmp(env_keys[i], k)) return env_vals[i];
    return nil();
}

S *read_atom(), *read_list();

S *read_atom() {
    while (*p && isspace(*p)) p++;
    if (!*p) return nil();
    if (*p == '(') return read_list();

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

    S *fn = eval(e->l[0]);
    if (fn->t == SYM) {
        char *op = fn->s;
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
    else if (s->t == LST) {
        printf("(");
        for (int i = 0; i < s->len; i++) { if (i) printf(" "); print(s->l[i]); }
        printf(")");
    }
}

int main(int argc, char **argv) {
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

    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            FILE *f = fopen(argv[i], "r");
            if (!f) continue;
            while (fgets(buf, sizeof(buf), f)) {
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
        while (fgets(buf, sizeof(buf), stdin)) {
            p = buf;
            S *e = read_atom();
            S *res = eval(e);
            print(res);
            printf("\n");
        }
    }
    return 0;
}
