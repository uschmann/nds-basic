/*
 * basic.c - small line-numbered BASIC interpreter (see basic.h)
 *
 * Supported:
 *   PRINT/? LET INPUT IF..THEN..ELSE GOTO GOSUB RETURN FOR..TO..STEP NEXT
 *   ON..GOTO/GOSUB DIM READ DATA RESTORE REM ' END STOP CLS PAUSE LOCATE
 *   COLOR RANDOMIZE  -  commands RUN LIST NEW CLR
 *   functions ABS INT SQR SIN COS TAN ATN EXP LOG RND SGN LEN VAL ASC
 *   CHR$ STR$ LEFT$ RIGHT$ MID$ INSTR INKEY$ PI
 */
#include "basic.h"

#include <ctype.h>
#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_LINES    1000
#define MAX_VARS     256
#define MAX_NAME     15
#define STR_MAX      255
#define LINE_MAX_LEN 255
#define ARENA_SIZE   8192
#define GOSUB_DEPTH  64
#define FOR_DEPTH    32
#define EXPR_DEPTH   48
#define MAX_ELEMENTS 20000

enum {
	ERR_NONE = 0, ERR_SYNTAX, ERR_TYPE, ERR_UNDEF_LINE, ERR_RETURN,
	ERR_NEXT, ERR_DATA, ERR_DIV0, ERR_QUANTITY, ERR_SUBSCRIPT, ERR_MEMORY,
	ERR_STRLEN, ERR_REDIM, ERR_STACK, ERR_COMPLEX, ERR_NOSTORAGE,
	ERR_FILENAME, ERR_NOTFOUND, ERR_IO, ERR_NOGFX, ERR_NOSOUND
};

static const char *errText[] = {
	"", "SYNTAX", "TYPE MISMATCH", "UNDEF'D STATEMENT", "RETURN WITHOUT GOSUB",
	"NEXT WITHOUT FOR", "OUT OF DATA", "DIVISION BY ZERO", "ILLEGAL QUANTITY",
	"BAD SUBSCRIPT", "OUT OF MEMORY", "STRING TOO LONG", "REDIM'D ARRAY",
	"STACK OVERFLOW", "FORMULA TOO COMPLEX", "NO STORAGE", "BAD FILE NAME",
	"FILE NOT FOUND", "DISK", "NO GRAPHICS", "NO SOUND"
};

typedef struct { int num; char *text; } Line;

typedef struct {
	char   name[MAX_NAME + 2];
	char   isStr, isArr;
	int    d1, d2;          /* array dimensions (sizes), d2 = 0 for 1-D */
	float  n;
	char  *s;
	float *na;
	char **sa;
} Var;

typedef struct { int vi; int idx; } VarRef;
typedef struct { int isStr; float n; const char *s; } Val;
typedef struct { int line; int off; } RetPos;
typedef struct { int vi; float limit, step; int line; int off; } ForEntry;

static const BasicIO *io;
static BasicState state = BASIC_IDLE;

static Line  lines[MAX_LINES];
static int   nlines;
static Var   vars[MAX_VARS];
static int   nvars;

static char        immbuf[LINE_MAX_LEN + 1];
static int         curLine;          /* index into lines, -1 = immediate */
static const char *P;                /* parse pointer                    */
static int         jumped;           /* control flow changed P           */
static jmp_buf     errjmp;
static char        lastError[64];
static char        storageDir[128];   /* "" = no storage available     */
static char        lastFile[16];      /* name of last SAVE / LOAD       */

static void st_save(void);
static void st_load(void);
static void st_dir(void);
static void st_delete(void);
static int         exprDepth;

static RetPos   gosubStack[GOSUB_DEPTH]; static int gosubSp;
static ForEntry forStack[FOR_DEPTH];     static int forSp;

static int         dataLine, dataIn;
static const char *dataPos;

static const char *inputPtr;         /* var list of a pending INPUT      */
static const char *inputPrompt;
static int         waitFrames;
static int         col;              /* output column for PRINT , TAB    */
static unsigned    rndState = 12345;
static int         gcolor = 1;        /* default drawing colour         */
static signed char sprites[BASIC_SPRITES][64]; /* 8x8, -1 = transparent */
static BasicObject objects[BASIC_OBJECTS];
static BasicSound  sounds[BASIC_SOUNDS];
static int         waitSound;         /* SOUND n,1: wait until finished */

static char arena[ARENA_SIZE];
static int  arenaTop;

/* ------------------------------------------------------------------ */
/* helpers                                                             */
/* ------------------------------------------------------------------ */

static void error(int code) __attribute__((noreturn));
static void error(int code) { longjmp(errjmp, code); }

static void out(char c) {
	io->putch(c);
	if (c == '\n') col = 0;
	else if (++col >= io->width) col = 0;
}
static void outs(const char *s) { while (*s) out(*s++); }

static const char *lineText(int li) { return li < 0 ? immbuf : lines[li].text; }

static char *arena_alloc(int n) {
	if (arenaTop + n > ARENA_SIZE) error(ERR_MEMORY);
	char *p = arena + arenaTop;
	arenaTop += n;
	return p;
}

static void skipsp(void) { while (*P == ' ') P++; }

/* keyword at P? must not be followed by a letter (so END != ENDE) */
static int kw_at(const char *p, const char *kw) {
	size_t n = strlen(kw);
	if (strncmp(p, kw, n) != 0) return 0;
	if (isalpha((unsigned char)kw[n - 1]) && isalpha((unsigned char)p[n])) return 0;
	return (int)n;
}
static int match_kw(const char *kw) {
	skipsp();
	int n = kw_at(P, kw);
	if (n) P += n;
	return n != 0;
}
static void expect(char c) {
	skipsp();
	if (*P != c) error(ERR_SYNTAX);
	P++;
}
static int at_stmt_end(void) {
	skipsp();
	return *P == 0 || *P == ':' || kw_at(P, "ELSE");
}

static Val num(float n) { Val v; v.isStr = 0; v.n = n; v.s = NULL; return v; }
static Val str(const char *s) { Val v; v.isStr = 1; v.n = 0; v.s = s; return v; }
static float need_num(Val v) { if (v.isStr) error(ERR_TYPE); return v.n; }
static const char *need_str(Val v) { if (!v.isStr) error(ERR_TYPE); return v.s; }
static char *arena_str(const char *s, int len) {
	if (len > STR_MAX) error(ERR_STRLEN);
	char *d = arena_alloc(len + 1);
	memcpy(d, s, len);
	d[len] = 0;
	return d;
}

/* parse a number at s; returns 1 on success */
static int parse_number_at(const char *s, float *outv, const char **end) {
	double v = 0, frac = 0.1;
	int any = 0;
	while (isdigit((unsigned char)*s)) { v = v * 10 + (*s - '0'); s++; any = 1; }
	if (*s == '.') {
		s++;
		while (isdigit((unsigned char)*s)) { v += (*s - '0') * frac; frac *= 0.1; s++; any = 1; }
	}
	if (!any) return 0;
	if (*s == 'E' || *s == 'e') {
		const char *q = s + 1;
		int sign = 1, e = 0, digits = 0;
		if (*q == '+') q++; else if (*q == '-') { sign = -1; q++; }
		while (isdigit((unsigned char)*q)) { e = e * 10 + (*q - '0'); q++; digits = 1; if (e > 99) e = 99; }
		if (digits) { v *= pow(10.0, sign * e); s = q; }
	}
	*outv = (float)v;
	if (end) *end = s;
	return 1;
}

static void fmt_num(char *b, float n) {
	if (n == 0) { strcpy(b, "0"); return; }
	if (fabsf(n) < 1e9f && n == (float)(int)n) { sprintf(b, "%d", (int)n); return; }
	sprintf(b, "%.7g", (double)n);
}

static unsigned rnd_next(void) {
	rndState = rndState * 1103515245u + 12345u;
	return (rndState >> 8) & 0xFFFFFF;
}

/* ------------------------------------------------------------------ */
/* program lines                                                       */
/* ------------------------------------------------------------------ */

static int find_line_exact(int num) {
	int lo = 0, hi = nlines - 1;
	while (lo <= hi) {
		int mid = (lo + hi) / 2;
		if (lines[mid].num == num) return mid;
		if (lines[mid].num < num) lo = mid + 1; else hi = mid - 1;
	}
	return -1;
}

int basic_find_index(int num) {
	int lo = 0, hi = nlines;
	while (lo < hi) {
		int mid = (lo + hi) / 2;
		if (lines[mid].num < num) lo = mid + 1; else hi = mid;
	}
	return lo;
}

static int store_line(int num, const char *text) {
	int idx = find_line_exact(num);
	if (!*text) {                         /* delete */
		if (idx < 0) return BASIC_ENTER_DELETED;
		free(lines[idx].text);
		memmove(&lines[idx], &lines[idx + 1], (nlines - idx - 1) * sizeof(Line));
		nlines--;
		return BASIC_ENTER_DELETED;
	}
	char *copy = strdup(text);
	if (!copy) return BASIC_ENTER_ERROR;
	if (idx >= 0) {
		free(lines[idx].text);
		lines[idx].text = copy;
		return BASIC_ENTER_STORED;
	}
	if (nlines >= MAX_LINES) { free(copy); return BASIC_ENTER_ERROR; }
	idx = basic_find_index(num);
	memmove(&lines[idx + 1], &lines[idx], (nlines - idx) * sizeof(Line));
	lines[idx].num = num;
	lines[idx].text = copy;
	nlines++;
	return BASIC_ENTER_STORED;
}

static void clear_vars(void) {
	for (int i = 0; i < nvars; i++) {
		Var *v = &vars[i];
		free(v->s);
		free(v->na);
		if (v->sa) {
			for (int k = 0; k < v->d1 * (v->d2 ? v->d2 : 1); k++) free(v->sa[k]);
			free(v->sa);
		}
	}
	memset(vars, 0, sizeof(vars));
	nvars = 0;
	gosubSp = forSp = 0;
	dataLine = 0; dataIn = 0; dataPos = NULL;
}

static void clear_program(void) {
	for (int i = 0; i < nlines; i++) free(lines[i].text);
	nlines = 0;
	clear_vars();
}

/* ------------------------------------------------------------------ */
/* variables                                                           */
/* ------------------------------------------------------------------ */

static int find_var(const char *name, int isArr) {
	for (int i = 0; i < nvars; i++)
		if (vars[i].isArr == isArr && strcmp(vars[i].name, name) == 0) return i;
	return -1;
}

static int new_var(const char *name, int isArr) {
	if (nvars >= MAX_VARS) error(ERR_MEMORY);
	Var *v = &vars[nvars];
	memset(v, 0, sizeof(*v));
	strcpy(v->name, name);
	v->isStr = name[strlen(name) - 1] == '$';
	v->isArr = isArr;
	return nvars++;
}

static int dim_array(const char *name, int s1, int s2) {
	if (find_var(name, 1) >= 0) error(ERR_REDIM);
	if (s1 < 0 || s2 < 0) error(ERR_QUANTITY);
	long count = (long)(s1 + 1) * (s2 ? s2 + 1 : 1);
	if (count > MAX_ELEMENTS) error(ERR_MEMORY);
	int vi = new_var(name, 1);
	Var *v = &vars[vi];
	v->d1 = s1 + 1;
	v->d2 = s2 ? s2 + 1 : 0;
	if (v->isStr) v->sa = calloc(count, sizeof(char *));
	else          v->na = calloc(count, sizeof(float));
	if (!v->sa && !v->na) { nvars--; error(ERR_MEMORY); }
	return vi;
}

static void read_name(char *name) {
	skipsp();
	if (!isalpha((unsigned char)*P)) error(ERR_SYNTAX);
	int n = 0;
	while (isalnum((unsigned char)*P)) {
		if (n < MAX_NAME) name[n++] = *P;
		P++;
	}
	if (*P == '$') { name[n++] = '$'; P++; }
	name[n] = 0;
}

static Val expr(void);

static VarRef parse_varref(void) {
	char name[MAX_NAME + 2];
	VarRef r;
	read_name(name);
	if (*P == '(') {
		P++;
		int i1 = (int)need_num(expr()), i2 = -1;
		skipsp();
		if (*P == ',') { P++; i2 = (int)need_num(expr()); }
		expect(')');
		int vi = find_var(name, 1);
		if (vi < 0) vi = dim_array(name, 10, i2 >= 0 ? 10 : 0);
		Var *v = &vars[vi];
		if ((i2 >= 0) != (v->d2 != 0)) error(ERR_SUBSCRIPT);
		if (i1 < 0 || i1 >= v->d1) error(ERR_SUBSCRIPT);
		if (i2 >= 0 && i2 >= v->d2) error(ERR_SUBSCRIPT);
		r.vi = vi;
		r.idx = i2 >= 0 ? i1 * v->d2 + i2 : i1;
	} else {
		int vi = find_var(name, 0);
		if (vi < 0) vi = new_var(name, 0);
		r.vi = vi;
		r.idx = -1;
	}
	return r;
}

static Val get_var(VarRef r) {
	Var *v = &vars[r.vi];
	if (v->isStr) {
		const char *s = r.idx < 0 ? v->s : v->sa[r.idx];
		return str(s ? s : "");
	}
	return num(r.idx < 0 ? v->n : v->na[r.idx]);
}

static void set_var(VarRef r, Val val) {
	Var *v = &vars[r.vi];
	if (v->isStr) {
		const char *s = need_str(val);
		char *d = strdup(s);
		if (!d) error(ERR_MEMORY);
		char **slot = r.idx < 0 ? &v->s : &v->sa[r.idx];
		free(*slot);
		*slot = d;
	} else {
		float n = need_num(val);
		if (r.idx < 0) v->n = n; else v->na[r.idx] = n;
	}
}

/* ------------------------------------------------------------------ */
/* expressions                                                         */
/* ------------------------------------------------------------------ */

static Val parse_unary(void);

static const char *fnNames[] = {
	"ABS(", "INT(", "SQR(", "SIN(", "COS(", "TAN(", "ATN(", "EXP(", "LOG(",
	"RND(", "SGN(", "LEN(", "VAL(", "ASC(", "CHR$(", "STR$(", "LEFT$(",
	"RIGHT$(", "MID$(", "INSTR(", "POINT(", "SPRHIT(", NULL
};

static Val call_fn(int f) {
	Val a = expr(), b;
	float x;
	const char *s;
	int n, len;
	char buf[32];
	switch (f) {
	case 0: x = fabsf(need_num(a)); break;
	case 1: x = floorf(need_num(a)); break;
	case 2: x = need_num(a); if (x < 0) error(ERR_QUANTITY); x = sqrtf(x); break;
	case 3: x = sinf(need_num(a)); break;
	case 4: x = cosf(need_num(a)); break;
	case 5: x = tanf(need_num(a)); break;
	case 6: x = atanf(need_num(a)); break;
	case 7: x = expf(need_num(a)); break;
	case 8: x = need_num(a); if (x <= 0) error(ERR_QUANTITY); x = logf(x); break;
	case 9: x = need_num(a);
		if (x < 0) { rndState = (unsigned)(-x * 1000); }
		x = (float)rnd_next() / 16777216.0f;
		break;
	case 10: x = need_num(a); x = x > 0 ? 1 : x < 0 ? -1 : 0; break;
	case 11: x = (float)strlen(need_str(a)); break;
	case 12: s = need_str(a); while (*s == ' ') s++;
		if (!parse_number_at(s[0] == '-' || s[0] == '+' ? s + 1 : s, &x, NULL)) x = 0;
		else if (s[0] == '-') x = -x;
		break;
	case 13: s = need_str(a); if (!*s) error(ERR_QUANTITY); x = (unsigned char)s[0]; break;
	case 14: n = (int)need_num(a); if (n < 0 || n > 255) error(ERR_QUANTITY);
		expect(')'); buf[0] = (char)n; return str(arena_str(buf, 1));
	case 15: fmt_num(buf, need_num(a)); expect(')'); return str(arena_str(buf, strlen(buf)));
	case 16: case 17:
		s = need_str(a); expect(','); n = (int)need_num(expr()); expect(')');
		if (n < 0) error(ERR_QUANTITY);
		len = strlen(s); if (n > len) n = len;
		return str(arena_str(f == 16 ? s : s + len - n, n));
	case 18:
		s = need_str(a); expect(','); n = (int)need_num(expr());
		skipsp(); len = 255;
		if (*P == ',') { P++; len = (int)need_num(expr()); }
		expect(')');
		if (n < 1 || len < 0) error(ERR_QUANTITY);
		{
			int sl = strlen(s);
			if (n > sl) return str("");
			if (len > sl - n + 1) len = sl - n + 1;
			return str(arena_str(s + n - 1, len));
		}
	case 19: {
		s = need_str(a); expect(','); b = expr(); const char *t = need_str(b);
		const char *hit = strstr(s, t);
		x = hit ? (float)(hit - s + 1) : 0;
		break;
	}
	case 21: {   /* SPRHIT(a,b): -1 if the visible sprites a and b overlap */
		int oa = (int)need_num(a);
		expect(',');
		int ob = (int)need_num(expr());
		if (oa < 0 || ob < 0 || oa >= BASIC_OBJECTS || ob >= BASIC_OBJECTS) error(ERR_QUANTITY);
		const BasicObject *A = &objects[oa], *B = &objects[ob];
		int sa = 8 * A->scale, sb = 8 * B->scale;
		x = (A->visible && B->visible && oa != ob &&
		     A->x < B->x + sb && B->x < A->x + sa && A->y < B->y + sb && B->y < A->y + sa) ? -1 : 0;
		break;
	}
	case 20: {
		if (!io->pget) error(ERR_NOGFX);
		int px = (int)need_num(a);
		expect(',');
		int py = (int)need_num(expr());
		x = (px < 0 || py < 0 || px >= io->gw || py >= io->gh) ? -1 : (float)io->pget(px, py);
		break;
	}
	default: error(ERR_SYNTAX);
	}
	expect(')');
	return num(x);
}

static Val parse_factor(void) {
	skipsp();
	char c = *P;
	if (isdigit((unsigned char)c) || c == '.') {
		float v;
		if (!parse_number_at(P, &v, &P)) error(ERR_SYNTAX);
		return num(v);
	}
	if (c == '"') {
		const char *start = ++P;
		while (*P && *P != '"') P++;
		const char *e = P;
		if (*P == '"') P++;
		return str(arena_str(start, e - start));
	}
	if (c == '(') {
		P++;
		Val v = expr();
		expect(')');
		return v;
	}
	if (isalpha((unsigned char)c)) {
		for (int i = 0; fnNames[i]; i++) {
			size_t n = strlen(fnNames[i]);
			if (strncmp(P, fnNames[i], n) == 0) { P += n; return call_fn(i); }
		}
		if (kw_at(P, "INKEY$")) {
			P += 6;
			int k = io->inkey ? io->inkey() : 0;
			if (!k) return str("");
			char ch = (char)k;
			return str(arena_str(&ch, 1));
		}
		if (kw_at(P, "JOY") && !isalnum((unsigned char)P[3]) && P[3] != '$' && P[3] != '(') {
			P += 3;
			return num(io->joy ? (float)io->joy() : 0);
		}
		if (kw_at(P, "PI") && !isalnum((unsigned char)P[2]) && P[2] != '$' && P[2] != '(') {
			P += 2;
			return num(3.14159265f);
		}
		return get_var(parse_varref());
	}
	error(ERR_SYNTAX);
}

static Val parse_unary(void) {
	skipsp();
	if (*P == '-') { P++; return num(-need_num(parse_unary())); }
	if (*P == '+') { P++; return num(need_num(parse_unary())); }
	Val a = parse_factor();
	skipsp();
	if (*P == '^') {
		P++;
		float e = need_num(parse_unary());
		float b = need_num(a);
		if (b == 0 && e < 0) error(ERR_DIV0);
		return num(powf(b, e));
	}
	return a;
}

static Val parse_mul(void) {
	Val a = parse_unary();
	for (;;) {
		skipsp();
		if (*P == '*') { P++; a = num(need_num(a) * need_num(parse_unary())); }
		else if (*P == '/') {
			P++;
			float d = need_num(parse_unary());
			if (d == 0) error(ERR_DIV0);
			a = num(need_num(a) / d);
		} else if (match_kw("MOD")) {
			int d = (int)need_num(parse_unary());
			if (d == 0) error(ERR_DIV0);
			a = num((float)((int)need_num(a) % d));
		} else return a;
	}
}

static Val parse_add(void) {
	Val a = parse_mul();
	for (;;) {
		skipsp();
		if (*P == '+') {
			P++;
			Val b = parse_mul();
			if (a.isStr) {
				const char *s2 = need_str(b);
				int l1 = strlen(a.s), l2 = strlen(s2);
				if (l1 + l2 > STR_MAX) error(ERR_STRLEN);
				char *d = arena_alloc(l1 + l2 + 1);
				memcpy(d, a.s, l1);
				memcpy(d + l1, s2, l2 + 1);
				a = str(d);
			} else a = num(a.n + need_num(b));
		} else if (*P == '-') {
			P++;
			a = num(need_num(a) - need_num(parse_mul()));
		} else return a;
	}
}

static Val parse_cmp(void) {
	Val a = parse_add();
	for (;;) {
		skipsp();
		int op;
		if      (P[0] == '<' && P[1] == '>') { op = 1; P += 2; }
		else if (P[0] == '<' && P[1] == '=') { op = 2; P += 2; }
		else if (P[0] == '=' && P[1] == '<') { op = 2; P += 2; }
		else if (P[0] == '>' && P[1] == '=') { op = 3; P += 2; }
		else if (P[0] == '=' && P[1] == '>') { op = 3; P += 2; }
		else if (P[0] == '<') { op = 4; P++; }
		else if (P[0] == '>') { op = 5; P++; }
		else if (P[0] == '=') { op = 6; P++; }
		else return a;
		Val b = parse_add();
		if (a.isStr != b.isStr) error(ERR_TYPE);
		int c = a.isStr ? strcmp(a.s, b.s) : (a.n < b.n ? -1 : a.n > b.n ? 1 : 0);
		int r = 0;
		switch (op) {
		case 1: r = c != 0; break;
		case 2: r = c <= 0; break;
		case 3: r = c >= 0; break;
		case 4: r = c < 0; break;
		case 5: r = c > 0; break;
		case 6: r = c == 0; break;
		}
		a = num(r ? -1.0f : 0.0f);
	}
}

static Val parse_not(void) {
	if (match_kw("NOT")) return num((float)~(int)need_num(parse_not()));
	return parse_cmp();
}

static Val parse_and(void) {
	Val a = parse_not();
	while (match_kw("AND")) {
		Val b = parse_not();
		a = num((float)((int)need_num(a) & (int)need_num(b)));
	}
	return a;
}

static Val expr(void) {
	if (++exprDepth > EXPR_DEPTH) error(ERR_COMPLEX);
	Val a = parse_and();
	while (match_kw("OR")) {
		Val b = parse_and();
		a = num((float)((int)need_num(a) | (int)need_num(b)));
	}
	exprDepth--;
	return a;
}

/* ------------------------------------------------------------------ */
/* control flow                                                        */
/* ------------------------------------------------------------------ */

static int parse_linenum(void) {
	skipsp();
	if (!isdigit((unsigned char)*P)) error(ERR_SYNTAX);
	long n = 0;
	while (isdigit((unsigned char)*P)) { n = n * 10 + (*P - '0'); if (n > 99999) n = 99999; P++; }
	return (int)n;
}

static void goto_line(int num) {
	int idx = find_line_exact(num);
	if (idx < 0) error(ERR_UNDEF_LINE);
	curLine = idx;
	P = lines[idx].text;
	jumped = 1;
}

static void next_line(void) {
	if (curLine < 0 || curLine + 1 >= nlines) { state = BASIC_IDLE; return; }
	curLine++;
	P = lines[curLine].text;
}

static void skip_to_eol(void) { P += strlen(P); }

static void skip_to_stmt_end(void) {
	int q = 0;
	while (*P && (q || *P != ':')) { if (*P == '"') q = !q; P++; }
}

static void push_gosub(void) {
	if (gosubSp >= GOSUB_DEPTH) error(ERR_STACK);
	gosubStack[gosubSp].line = curLine;
	gosubStack[gosubSp].off = (int)(P - lineText(curLine));
	gosubSp++;
}

/* find keyword outside of string literals, at a word boundary */
static const char *find_kw(const char *s, const char *kw) {
	int q = 0;
	const char *start = s;
	for (; *s; s++) {
		if (*s == '"') { q = !q; continue; }
		if (q) continue;
		if (s > start && isalnum((unsigned char)s[-1])) continue;
		if (kw_at(s, kw)) return s;
	}
	return NULL;
}

/* ------------------------------------------------------------------ */
/* graphics: PLOT LINE RECT CIRCLE PAINT GCLS GCOLOR                   */
/* ------------------------------------------------------------------ */

static void need_gfx(void) { if (!io->pset) error(ERR_NOGFX); }

static int int_arg(void) { return (int)need_num(expr()); }

static void comma_arg(int *v) { expect(','); *v = int_arg(); }

/* optional ",colour" (-1 = erase, 0-15) */
static int opt_color(void) {
	skipsp();
	if (*P != ',') return gcolor;
	P++;
	int c = int_arg();
	if (c < -1 || c > 15) error(ERR_QUANTITY);
	return c;
}

static int opt_fill(void) {
	skipsp();
	if (*P != ',') return 0;
	P++;
	return int_arg() != 0;
}

static void pset(int x, int y, int c) {
	if (x >= 0 && y >= 0 && x < io->gw && y < io->gh) io->pset(x, y, c);
}

static void hline(int x1, int x2, int y, int c) {
	if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
	if (y < 0 || y >= io->gh) return;
	if (x1 < 0) x1 = 0;
	if (x2 >= io->gw) x2 = io->gw - 1;
	for (int x = x1; x <= x2; x++) io->pset(x, y, c);
}

static void draw_line(int x1, int y1, int x2, int y2, int c) {
	int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
	int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
	int err = dx + dy;
	for (int guard = 0; guard < 4096; guard++) {
		pset(x1, y1, c);
		if (x1 == x2 && y1 == y2) break;
		int e2 = 2 * err;
		if (e2 >= dy) { err += dy; x1 += sx; }
		if (e2 <= dx) { err += dx; y1 += sy; }
	}
}

static void st_plot(void) {
	int x = int_arg(), y;
	need_gfx();
	comma_arg(&y);
	pset(x, y, opt_color());
}

static void st_line(void) {
	int x1 = int_arg(), y1, x2, y2;
	need_gfx();
	comma_arg(&y1); comma_arg(&x2); comma_arg(&y2);
	draw_line(x1, y1, x2, y2, opt_color());
}

static void st_rect(void) {
	int x1 = int_arg(), y1, x2, y2;
	need_gfx();
	comma_arg(&y1); comma_arg(&x2); comma_arg(&y2);
	int c = opt_color(), f = opt_fill();
	if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
	if (f) {
		for (int y = y1 < 0 ? 0 : y1; y <= y2 && y < io->gh; y++) hline(x1, x2, y, c);
	} else {
		hline(x1, x2, y1, c);
		hline(x1, x2, y2, c);
		draw_line(x1, y1, x1, y2, c);
		draw_line(x2, y1, x2, y2, c);
	}
}

static void st_circle(void) {
	int cx = int_arg(), cy, r;
	need_gfx();
	comma_arg(&cy); comma_arg(&r);
	if (r < 0 || r > 1000) error(ERR_QUANTITY);
	int c = opt_color(), f = opt_fill();
	int x = r, y = 0, err = 1 - r;
	while (x >= y) {
		if (f) {
			hline(cx - x, cx + x, cy + y, c);
			hline(cx - x, cx + x, cy - y, c);
			hline(cx - y, cx + y, cy + x, c);
			hline(cx - y, cx + y, cy - x, c);
		} else {
			pset(cx + x, cy + y, c); pset(cx - x, cy + y, c);
			pset(cx + x, cy - y, c); pset(cx - x, cy - y, c);
			pset(cx + y, cy + x, c); pset(cx - y, cy + x, c);
			pset(cx + y, cy - x, c); pset(cx - y, cy - x, c);
		}
		y++;
		if (err < 0) err += 2 * y + 1;
		else { x--; err += 2 * (y - x) + 1; }
	}
}

/* scanline flood fill of the area with the colour found at (x,y) */
static void st_paint(void) {
	static short stack[4096][2];
	int x = int_arg(), y;
	need_gfx();
	if (!io->pget) error(ERR_NOGFX);
	comma_arg(&y);
	int c = opt_color();
	if (x < 0 || y < 0 || x >= io->gw || y >= io->gh) return;
	int target = io->pget(x, y);
	if (target == c) return;
	int sp = 0;
	stack[sp][0] = (short)x; stack[sp][1] = (short)y; sp++;
	while (sp > 0) {
		sp--;
		x = stack[sp][0]; y = stack[sp][1];
		if (io->pget(x, y) != target) continue;
		int lx = x, rx = x;
		while (lx > 0 && io->pget(lx - 1, y) == target) lx--;
		while (rx < io->gw - 1 && io->pget(rx + 1, y) == target) rx++;
		for (int i = lx; i <= rx; i++) io->pset(i, y, c);
		for (int ny = y - 1; ny <= y + 1; ny += 2) {
			if (ny < 0 || ny >= io->gh) continue;
			for (int i = lx; i <= rx; i++) {
				if (io->pget(i, ny) != target) continue;
				if (i > lx && io->pget(i - 1, ny) == target) continue;
				if (sp >= 4096) error(ERR_COMPLEX);
				stack[sp][0] = (short)i; stack[sp][1] = (short)ny; sp++;
			}
		}
	}
}

static int sprite_slot(void) {
	int n = int_arg();
	if (n < 0 || n >= BASIC_SPRITES) error(ERR_QUANTITY);
	return n;
}

/* STAMP n,x,y[,scale] - draw 8x8 sprite n into the graphics layer,
 * transparent pixels are skipped */
static void st_stamp(void) {
	int n = sprite_slot(), x, y, sc = 1;
	need_gfx();
	comma_arg(&x); comma_arg(&y);
	skipsp();
	if (*P == ',') { P++; sc = int_arg(); if (sc < 1 || sc > 8) error(ERR_QUANTITY); }
	const signed char *d = sprites[n];
	for (int py = 0; py < 8; py++)
		for (int px = 0; px < 8; px++) {
			int c = d[py * 8 + px];
			if (c < 0) continue;
			for (int j = 0; j < sc; j++)
				for (int i = 0; i < sc; i++) pset(x + px * sc + i, y + py * sc + j, c);
		}
}

static void hide_objects(void) {
	for (int i = 0; i < BASIC_OBJECTS; i++) objects[i].visible = 0;
}

/* SPRITE o,n,x,y[,s] - show hardware sprite object o with graphic slot n */
static void st_sprite(void) {
	int o = int_arg(), n, x, y, sc = 1;
	if (o < 0 || o >= BASIC_OBJECTS) error(ERR_QUANTITY);
	comma_arg(&n); comma_arg(&x); comma_arg(&y);
	skipsp();
	if (*P == ',') { P++; sc = int_arg(); }
	if (n < 0 || n >= BASIC_SPRITES || sc < 1 || sc > BASIC_OBJ_SCALE) error(ERR_QUANTITY);
	if (x < -9999) x = -9999; else if (x > 9999) x = 9999;
	if (y < -9999) y = -9999; else if (y > 9999) y = 9999;
	BasicObject *b = &objects[o];
	if (!b->visible || b->slot != n || b->scale != sc) b->dirty = 1;
	b->visible = 1;
	b->slot = (unsigned char)n;
	b->scale = (unsigned char)sc;
	b->x = x;
	b->y = y;
}

/* SPROFF [o] - hide sprite object o, without argument all */
static void st_sproff(void) {
	if (at_stmt_end()) { hide_objects(); return; }
	int o = int_arg();
	if (o < 0 || o >= BASIC_OBJECTS) error(ERR_QUANTITY);
	objects[o].visible = 0;
}

static void slot_changed(int n) {
	for (int i = 0; i < BASIC_OBJECTS; i++)
		if (objects[i].slot == n) objects[i].dirty = 1;
}

/* SPRSET n,x,y,c - set one pixel of sprite n (c = -1 transparent) */
static void st_sprset(void) {
	int n = sprite_slot(), x, y, c;
	comma_arg(&x); comma_arg(&y); comma_arg(&c);
	if (x < 0 || x > 7 || y < 0 || y > 7 || c < -1 || c > 15) error(ERR_QUANTITY);
	sprites[n][y * 8 + x] = (signed char)c;
	slot_changed(n);
}

/* SOUND n[,w] - play sound slot n (0-9) in the background, -1 stops;
 * w <> 0 waits until the sound has finished */
static void st_sound(void) {
	int n = int_arg(), w = 0;
	skipsp();
	if (*P == ',') { P++; w = int_arg(); }
	if (n < -1 || n >= BASIC_SOUNDS) error(ERR_QUANTITY);
	if (!io->sound) error(ERR_NOSOUND);
	io->sound(n);
	if (w && n >= 0 && io->sound_busy) waitSound = 1;
}

/* ------------------------------------------------------------------ */
/* statements                                                          */
/* ------------------------------------------------------------------ */

static void print_value(Val v) {
	if (v.isStr) { outs(v.s); return; }
	char b[32];
	fmt_num(b, v.n);
	if (v.n >= 0) out(' ');
	outs(b);
}

static void st_print(void) {
	int newline = 1;
	for (;;) {
		if (at_stmt_end()) break;
		if (*P == ';') { P++; newline = 0; continue; }
		if (*P == ',') {
			P++;
			do out(' '); while (col % 8);
			newline = 0;
			continue;
		}
		newline = 1;
		if (strncmp(P, "TAB(", 4) == 0 || strncmp(P, "SPC(", 4) == 0) {
			int isTab = P[0] == 'T';
			P += 4;
			int n = (int)need_num(expr());
			expect(')');
			if (n < 0 || n > 255) error(ERR_QUANTITY);
			if (isTab) { n %= io->width; while (col != n) out(' '); }
			else while (n-- > 0) out(' ');
			newline = 1;
			continue;
		}
		print_value(expr());
	}
	if (newline) out('\n');
}

static void st_let(void) {
	VarRef r = parse_varref();
	expect('=');
	set_var(r, expr());
}

static void st_if(void) {
	float c = need_num(expr());
	skipsp();
	if (!match_kw("THEN") && !kw_at(P, "GOTO")) error(ERR_SYNTAX);
	if (c == 0) {
		const char *e = find_kw(P, "ELSE");
		if (!e) { skip_to_eol(); jumped = 1; return; }
		P = e + 4;
	}
	skipsp();
	if (isdigit((unsigned char)*P)) goto_line(parse_linenum());
	else jumped = 1;  /* continue with the statement after THEN/ELSE */
}

static void st_for(void) {
	char name[MAX_NAME + 2];
	read_name(name);
	if (name[strlen(name) - 1] == '$') error(ERR_TYPE);
	int vi = find_var(name, 0);
	if (vi < 0) vi = new_var(name, 0);
	expect('=');
	float start = need_num(expr());
	if (!match_kw("TO")) error(ERR_SYNTAX);
	float limit = need_num(expr());
	float step = 1;
	if (match_kw("STEP")) step = need_num(expr());
	vars[vi].n = start;
	/* drop an existing loop on the same variable (and everything above) */
	for (int i = forSp - 1; i >= 0; i--)
		if (forStack[i].vi == vi) { forSp = i; break; }
	if (forSp >= FOR_DEPTH) error(ERR_STACK);
	ForEntry *f = &forStack[forSp++];
	f->vi = vi; f->limit = limit; f->step = step;
	f->line = curLine;
	f->off = (int)(P - lineText(curLine));
}

static void st_next(void) {
	for (;;) {
		int fi = forSp - 1;
		skipsp();
		if (isalpha((unsigned char)*P)) {
			char name[MAX_NAME + 2];
			read_name(name);
			int vi = find_var(name, 0);
			while (fi >= 0 && forStack[fi].vi != vi) fi--;
		}
		if (fi < 0) error(ERR_NEXT);
		ForEntry *f = &forStack[fi];
		float v = vars[f->vi].n + f->step;
		vars[f->vi].n = v;
		if (f->step >= 0 ? v <= f->limit : v >= f->limit) {
			forSp = fi + 1;
			curLine = f->line;
			P = lineText(curLine) + f->off;
			jumped = 1;
			return;
		}
		forSp = fi;
		skipsp();
		if (*P == ',') { P++; continue; }
		return;
	}
}

static void st_dim(void) {
	do {
		char name[MAX_NAME + 2];
		read_name(name);
		expect('(');
		int s1 = (int)need_num(expr()), s2 = 0;
		skipsp();
		if (*P == ',') { P++; s2 = (int)need_num(expr()); if (s2 < 1) error(ERR_QUANTITY); }
		expect(')');
		dim_array(name, s1, s2);
		skipsp();
	} while (*P == ',' && P++);
}

static void st_input(void) {
	skipsp();
	inputPrompt = NULL;
	if (*P == '"') {
		Val p = parse_factor();
		skipsp();
		if (*P == ';' || *P == ',') P++;
		else error(ERR_SYNTAX);
		/* keep a persistent copy for REDO */
		static char promptBuf[STR_MAX + 1];
		strcpy(promptBuf, p.s);
		inputPrompt = promptBuf;
	}
	if (inputPrompt) outs(inputPrompt);
	outs("? ");
	inputPtr = P;
	state = BASIC_INPUT;
	jumped = 1;
}

static void read_data_item(char *buf, int *quoted) {
	for (;;) {
		if (dataLine >= nlines) error(ERR_DATA);
		const char *s = dataPos ? dataPos : lines[dataLine].text;
		if (!dataIn) {
			const char *f = find_kw(s, "DATA");
			if (!f) { dataLine++; dataPos = NULL; continue; }
			s = f + 4;
			dataIn = 1;
		}
		while (*s == ' ') s++;
		int n = 0;
		*quoted = 0;
		if (*s == '"') {
			s++;
			*quoted = 1;
			while (*s && *s != '"' && n < STR_MAX) buf[n++] = *s++;
			if (*s == '"') s++;
			while (*s == ' ') s++;
		} else {
			while (*s && *s != ',' && *s != ':' && n < STR_MAX) buf[n++] = *s++;
			while (n > 0 && buf[n - 1] == ' ') n--;
		}
		buf[n] = 0;
		if (*s == ',') dataPos = s + 1;
		else {
			dataIn = 0;
			if (*s == ':') dataPos = s + 1;
			else { dataLine++; dataPos = NULL; }
		}
		return;
	}
}

static void st_read(void) {
	do {
		VarRef r = parse_varref();
		char buf[STR_MAX + 1];
		int quoted;
		read_data_item(buf, &quoted);
		if (vars[r.vi].isStr) set_var(r, str(buf));
		else {
			float v;
			const char *s = buf, *e;
			int neg = 0;
			if (*s == '-') { neg = 1; s++; } else if (*s == '+') s++;
			if (quoted || !parse_number_at(s, &v, &e) || *e) error(ERR_SYNTAX);
			set_var(r, num(neg ? -v : v));
		}
		skipsp();
	} while (*P == ',' && P++);
}

static void st_on(void) {
	int k = (int)need_num(expr());
	int isSub;
	if (match_kw("GOTO")) isSub = 0;
	else if (match_kw("GOSUB")) isSub = 1;
	else error(ERR_SYNTAX);
	int target = -1;
	for (int i = 1;; i++) {
		int ln = parse_linenum();
		if (i == k) target = ln;
		skipsp();
		if (*P == ',') P++; else break;
	}
	if (target < 0) return;
	if (isSub) push_gosub();
	goto_line(target);
}

static void st_list(void) {
	int from = 0, to = 99999;
	skipsp();
	if (isdigit((unsigned char)*P)) { from = to = parse_linenum(); }
	skipsp();
	if (*P == '-') {
		P++;
		skipsp();
		to = isdigit((unsigned char)*P) ? parse_linenum() : 99999;
	}
	for (int i = 0; i < nlines; i++) {
		if (lines[i].num < from || lines[i].num > to) continue;
		char b[16];
		sprintf(b, "%d ", lines[i].num);
		outs(b);
		outs(lines[i].text);
		out('\n');
	}
}

static void st_run(void) {
	int start = -1;
	skipsp();
	if (isdigit((unsigned char)*P)) start = parse_linenum();
	clear_vars();
	hide_objects();
	if (nlines == 0) { state = BASIC_IDLE; return; }
	if (start >= 0) goto_line(start);
	else { curLine = 0; P = lines[0].text; jumped = 1; }
}

static void exec_statement(void) {
	skipsp();
	if (match_kw("PRINT") || match_kw("?")) { st_print(); return; }
	if (match_kw("LET"))      { st_let(); return; }
	if (match_kw("IF"))       { st_if(); return; }
	if (match_kw("GOTO"))     { goto_line(parse_linenum()); return; }
	if (match_kw("GOSUB"))    { int n = parse_linenum(); push_gosub(); goto_line(n); return; }
	if (match_kw("RETURN")) {
		if (gosubSp == 0) error(ERR_RETURN);
		gosubSp--;
		curLine = gosubStack[gosubSp].line;
		P = lineText(curLine) + gosubStack[gosubSp].off;
		jumped = 1;
		return;
	}
	if (match_kw("FOR"))      { st_for(); return; }
	if (match_kw("NEXT"))     { st_next(); return; }
	if (match_kw("REM") || match_kw("'")) { skip_to_eol(); return; }
	if (match_kw("DATA"))     { skip_to_stmt_end(); return; }
	if (match_kw("ELSE"))     { skip_to_eol(); return; }   /* reached after a true THEN */
	if (match_kw("END"))      { state = BASIC_IDLE; jumped = 1; return; }
	if (match_kw("EXIT")) {
		state = BASIC_IDLE;
		jumped = 1;
		if (io->exit) io->exit();
		return;
	}
	if (match_kw("STOP")) {
		char b[32];
		if (curLine >= 0) sprintf(b, "\nBREAK IN %d\n", lines[curLine].num);
		else strcpy(b, "\nBREAK\n");
		outs(b);
		state = BASIC_IDLE;
		jumped = 1;
		return;
	}
	if (match_kw("CLS"))      { if (io->cls) io->cls(); col = 0; return; }
	if (match_kw("DIM"))      { st_dim(); return; }
	if (match_kw("INPUT"))    { st_input(); return; }
	if (match_kw("READ"))     { st_read(); return; }
	if (match_kw("RESTORE"))  { dataLine = 0; dataIn = 0; dataPos = NULL; return; }
	if (match_kw("ON"))       { st_on(); return; }
	if (match_kw("RUN"))      { st_run(); return; }
	if (match_kw("LIST"))     { st_list(); return; }
	if (match_kw("NEW"))      { clear_program(); hide_objects(); state = BASIC_IDLE; jumped = 1; return; }
	if (match_kw("CLR"))      { clear_vars(); return; }
	if (match_kw("SAVE"))     { st_save(); return; }
	if (match_kw("LOAD"))     { st_load(); return; }
	if (match_kw("DIR"))      { st_dir(); return; }
	if (match_kw("DELETE"))   { st_delete(); return; }
	if (match_kw("PLOT"))     { st_plot(); return; }
	if (match_kw("LINE"))     { st_line(); return; }
	if (match_kw("RECT"))     { st_rect(); return; }
	if (match_kw("CIRCLE"))   { st_circle(); return; }
	if (match_kw("PAINT"))    { st_paint(); return; }
	if (match_kw("GCLS"))     { need_gfx(); if (io->gcls) io->gcls(); return; }
	if (match_kw("SPRITE"))   { st_sprite(); return; }
	if (match_kw("SPROFF"))   { st_sproff(); return; }
	if (match_kw("STAMP"))    { st_stamp(); return; }
	if (match_kw("SPRSET"))   { st_sprset(); return; }
	if (match_kw("SOUND"))    { st_sound(); return; }
	if (match_kw("GCOLOR")) {
		int c = int_arg();
		if (c < -1 || c > 15) error(ERR_QUANTITY);
		gcolor = c;
		return;
	}
	if (match_kw("PAUSE")) {
		int n = (int)need_num(expr());
		if (n < 0) error(ERR_QUANTITY);
		waitFrames = n;
		return;
	}
	if (match_kw("LOCATE")) {
		int r = (int)need_num(expr());
		expect(',');
		int c = (int)need_num(expr());
		if (r < 0 || c < 0) error(ERR_QUANTITY);
		if (io->locate) io->locate(c, r);
		col = c;
		return;
	}
	if (match_kw("COLOR")) {
		int c = (int)need_num(expr());
		if (c < 0 || c > 15) error(ERR_QUANTITY);
		if (io->color) io->color(c);
		return;
	}
	if (match_kw("RANDOMIZE")) {
		if (at_stmt_end()) rndState ^= rnd_next();
		else rndState = (unsigned)need_num(expr());
		return;
	}
	if (isalpha((unsigned char)*P)) { st_let(); return; }
	error(ERR_SYNTAX);
}

static void step(void) {
	arenaTop = 0;
	exprDepth = 0;
	skipsp();
	if (*P == 0) { next_line(); return; }
	if (*P == ':') { P++; return; }
	jumped = 0;
	exec_statement();
	if (jumped || state != BASIC_RUNNING) return;
	if (!at_stmt_end()) error(ERR_SYNTAX);
}

static void report_error(int code) {
	if (curLine >= 0 && curLine < nlines)
		snprintf(lastError, sizeof(lastError), "?%s ERROR IN %d", errText[code], lines[curLine].num);
	else
		snprintf(lastError, sizeof(lastError), "?%s ERROR", errText[code]);
	if (col) out('\n');
	outs(lastError);
	out('\n');
	state = BASIC_IDLE;
}

/* ------------------------------------------------------------------ */
/* public API                                                          */
/* ------------------------------------------------------------------ */

void basic_init(const BasicIO *bio) {
	io = bio;
	clear_program();
	memset(sprites, -1, sizeof sprites);
	for (int k = 0; k < BASIC_SOUNDS; k++) basic_sound_clear(k);
	state = BASIC_IDLE;
	lastError[0] = 0;
}

BasicState basic_state(void) { return state; }
const char *basic_last_error(void) { return lastError; }
void basic_seed(unsigned s) { rndState = s ? s : 1; }
int basic_line_count(void) { return nlines; }
int basic_line_number(int i) { return (i >= 0 && i < nlines) ? lines[i].num : -1; }
const char *basic_line_text(int i) { return (i >= 0 && i < nlines) ? lines[i].text : ""; }
int basic_current_line(void) { return (curLine >= 0 && curLine < nlines) ? lines[curLine].num : -1; }

/* uppercase everything outside of string literals, trim */
static void normalize(const char *src, char *dst) {
	int q = 0, n = 0;
	while (*src == ' ') src++;
	for (; *src && n < LINE_MAX_LEN; src++) {
		char c = *src;
		if (c == '"') q = !q;
		if (!q) c = (char)toupper((unsigned char)c);
		if (c == '\t') c = ' ';
		dst[n++] = c;
	}
	while (n > 0 && dst[n - 1] == ' ') n--;
	dst[n] = 0;
}

/* store a normalized "123 TEXT" line */
static int enter_numbered(const char *buf, int *lineNum) {
	const char *s = buf;
	long n = 0;
	while (isdigit((unsigned char)*s)) { n = n * 10 + (*s - '0'); if (n > 99999) break; s++; }
	if (n < 1 || n > 65535) return BASIC_ENTER_ERROR;
	while (*s == ' ') s++;
	if (lineNum) *lineNum = (int)n;
	return store_line((int)n, s);
}

/* ------------------------------------------------------------------ */
/* storage: SAVE LOAD DIR DELETE                                       */
/* ------------------------------------------------------------------ */

static void message(const char *m) {
	if (io->message) io->message(m);
	else { outs(m); out('\n'); }
}

/* parse "NAME" -> NAME.BAS; without argument the last file name is used */
static void parse_filename(char *name, int optional) {
	if (!storageDir[0]) error(ERR_NOSTORAGE);
	if (at_stmt_end()) {
		if (optional && lastFile[0]) { strcpy(name, lastFile); return; }
		error(ERR_FILENAME);
	}
	const char *s = need_str(expr());
	int n = 0, base = 0, dot = 0;
	for (; *s; s++) {
		char c = (char)toupper((unsigned char)*s);
		if (c == '.') { if (dot || !base) error(ERR_FILENAME); dot = 1; }
		else if (!isalnum((unsigned char)c) && c != '_' && c != '-') error(ERR_FILENAME);
		else if (!dot && ++base > 8) error(ERR_FILENAME);
		if (n >= 12) error(ERR_FILENAME);
		name[n++] = c;
	}
	name[n] = 0;
	if (!base) error(ERR_FILENAME);
	if (!dot) strcat(name, ".BAS");
}

static void file_path(char *path, const char *name) {
	snprintf(path, 160, "%.127s/%.15s", storageDir, name);
}

static void st_save(void) {
	char name[16], path[160], msg[40];
	parse_filename(name, 1);
	file_path(path, name);
	FILE *f = fopen(path, "w");
	if (!f) error(ERR_IO);
	int ok = 1;
	for (int i = 0; i < nlines && ok; i++)
		ok = fprintf(f, "%d %s\n", lines[i].num, lines[i].text) > 0;
	/* sprites: "#SPR n" + 64 chars, '.' = transparent, 0-F = colour */
	for (int n = 0; n < BASIC_SPRITES && ok; n++) {
		char hex[65];
		int used = 0;
		for (int k = 0; k < 64; k++) {
			int c = sprites[n][k];
			hex[k] = c < 0 ? '.' : "0123456789ABCDEF"[c];
			used |= c >= 0;
		}
		hex[64] = 0;
		if (used) ok = fprintf(f, "#SPR %d %s\n", n, hex) > 0;
	}
	/* sounds: "#SND n instrument tempo note:len ..." */
	for (int n = 0; n < BASIC_SOUNDS && ok; n++) {
		const BasicSound *sd = &sounds[n];
		if (!sd->count) continue;
		ok = fprintf(f, "#SND %d %d %d", n, sd->inst, sd->tempo) > 0;
		for (int k = 0; k < sd->count && ok; k++) ok = fprintf(f, " %d:%d", sd->note[k], sd->len[k]) > 0;
		if (ok) ok = fputc('\n', f) != EOF;
	}
	if (fclose(f) != 0) ok = 0;
	if (!ok) error(ERR_IO);
	strcpy(lastFile, name);
	snprintf(msg, sizeof msg, "SAVED %s", name);
	message(msg);
}

static void st_load(void) {
	char name[16], path[160], msg[40];
	char raw[512], buf[LINE_MAX_LEN + 1];
	parse_filename(name, 0);
	file_path(path, name);
	FILE *f = fopen(path, "r");
	if (!f) error(ERR_NOTFOUND);
	clear_program();
	int spritesSeen = 0, soundsSeen = 0;
	while (fgets(raw, sizeof raw, f)) {
		raw[strcspn(raw, "\r\n")] = 0;
		if (!strncmp(raw, "#SND ", 5)) {
			if (!soundsSeen++) for (int k = 0; k < BASIC_SOUNDS; k++) basic_sound_clear(k);
			char *e;
			long n = strtol(raw + 5, &e, 10);
			if (n < 0 || n >= BASIC_SOUNDS) continue;
			BasicSound *sd = &sounds[n];
			long inst = strtol(e, &e, 10), tempo = strtol(e, &e, 10);
			sd->inst = (unsigned char)(inst >= 0 && inst < SND_INSTRUMENTS ? inst : SND_SQ50);
			sd->tempo = (unsigned char)(tempo >= 40 && tempo <= 250 ? tempo : 120);
			while (sd->count < BASIC_SOUND_LEN) {
				long nt = strtol(e, &e, 10);
				if (*e != ':') break;
				long ln = strtol(e + 1, &e, 10);
				if (nt != 0 && (nt < 24 || nt > 108)) nt = 0;
				if (ln < 1 || ln > 16) ln = 4;
				sd->note[sd->count] = (unsigned char)nt;
				sd->len[sd->count] = (unsigned char)ln;
				sd->count++;
			}
			continue;
		}
		if (!strncmp(raw, "#SPR ", 5)) {
			/* file has sprites: they replace all slots */
			if (!spritesSeen++) {
				memset(sprites, -1, sizeof sprites);
				for (int k = 0; k < BASIC_OBJECTS; k++) objects[k].dirty = 1;
			}
			char *e;
			long n = strtol(raw + 5, &e, 10);
			while (*e == ' ') e++;
			if (n < 0 || n >= BASIC_SPRITES || strlen(e) < 64) continue;
			for (int k = 0; k < 64; k++) {
				char c = (char)toupper((unsigned char)e[k]);
				sprites[n][k] = (signed char)(isdigit((unsigned char)c) ? c - '0' :
				                (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1);
			}
			continue;
		}
		normalize(raw, buf);
		if (isdigit((unsigned char)buf[0])) enter_numbered(buf, NULL);
	}
	fclose(f);
	strcpy(lastFile, name);
	snprintf(msg, sizeof msg, "LOADED %s (%d LINE%s)", name, nlines, nlines == 1 ? "" : "S");
	message(msg);
	/* the program changed under our feet: stop */
	state = BASIC_IDLE;
	curLine = -1;
	jumped = 1;
}

static int cmp_names(const void *a, const void *b) {
	return strcmp((const char *)a, (const char *)b);
}

int basic_list_files(char names[][16], int max) {
	int count = 0;
	if (!storageDir[0]) return -1;
	DIR *d = opendir(storageDir);
	if (!d) return -1;
	struct dirent *e;
	while ((e = readdir(d)) && count < max) {
		size_t l = strlen(e->d_name);
		if (l < 5 || l > 12 || strcasecmp(e->d_name + l - 4, ".BAS")) continue;
		for (size_t k = 0; k <= l; k++) names[count][k] = (char)toupper((unsigned char)e->d_name[k]);
		count++;
	}
	closedir(d);
	qsort(names, count, 16, cmp_names);
	return count;
}

static void st_dir(void) {
	static char names[BASIC_MAX_FILES][16];
	if (!storageDir[0]) error(ERR_NOSTORAGE);
	int count = basic_list_files(names, BASIC_MAX_FILES);
	if (count < 0) error(ERR_IO);
	for (int i = 0; i < count; i++) {
		char path[160], line[40];
		struct stat st;
		file_path(path, names[i]);
		long size = stat(path, &st) == 0 ? (long)st.st_size : 0;
		snprintf(line, sizeof line, "%-12.12s %6ld", names[i], size);
		outs(line);
		out('\n');
	}
	char b[24];
	snprintf(b, sizeof b, "%d FILE%s", count, count == 1 ? "" : "S");
	outs(b);
	out('\n');
}

static void st_delete(void) {
	char name[16], path[160], msg[40];
	parse_filename(name, 0);
	file_path(path, name);
	if (remove(path) != 0) error(ERR_NOTFOUND);
	if (!strcmp(name, lastFile)) lastFile[0] = 0;
	snprintf(msg, sizeof msg, "DELETED %s", name);
	message(msg);
}

int basic_sprite_get(int n, int x, int y) {
	if (n < 0 || n >= BASIC_SPRITES || x < 0 || x > 7 || y < 0 || y > 7) return -1;
	return sprites[n][y * 8 + x];
}

void basic_sprite_set(int n, int x, int y, int c) {
	if (n < 0 || n >= BASIC_SPRITES || x < 0 || x > 7 || y < 0 || y > 7 || c < -1 || c > 15) return;
	if (sprites[n][y * 8 + x] == c) return;
	sprites[n][y * 8 + x] = (signed char)c;
	slot_changed(n);
}

BasicObject *basic_object(int o) {
	return (o >= 0 && o < BASIC_OBJECTS) ? &objects[o] : NULL;
}

BasicSound *basic_sound(int n) {
	return (n >= 0 && n < BASIC_SOUNDS) ? &sounds[n] : NULL;
}

void basic_sound_clear(int n) {
	if (n < 0 || n >= BASIC_SOUNDS) return;
	memset(&sounds[n], 0, sizeof sounds[n]);
	sounds[n].inst = SND_SQ50;
	sounds[n].tempo = 120;
}

void basic_set_storage(const char *dir) {
	if (dir) { strncpy(storageDir, dir, sizeof storageDir - 1); storageDir[sizeof storageDir - 1] = 0; }
	else storageDir[0] = 0;
}

int basic_has_storage(void) { return storageDir[0] != 0; }

int basic_enter(const char *src, int *lineNum) {
	char buf[LINE_MAX_LEN + 1];
	if (state != BASIC_IDLE) return BASIC_ENTER_ERROR;
	normalize(src, buf);
	if (!buf[0]) return BASIC_ENTER_EMPTY;
	if (isdigit((unsigned char)buf[0])) return enter_numbered(buf, lineNum);
	strcpy(immbuf, buf);
	curLine = -1;
	P = immbuf;
	gosubSp = forSp = 0;
	waitFrames = 0;
	waitSound = 0;
	lastError[0] = 0;
	state = BASIC_RUNNING;
	return BASIC_ENTER_EXEC;
}

int basic_run_slice(int maxStatements) {
	volatile int done = 0;
	if (state != BASIC_RUNNING) return 0;
	int code = setjmp(errjmp);
	if (code) { report_error(code); return 0; }
	if (waitSound) {
		if (io->sound_busy && io->sound_busy()) return 0;
		waitSound = 0;
	}
	while (done < maxStatements && state == BASIC_RUNNING && waitFrames == 0 && !waitSound) {
		step();
		done++;
	}
	return done;
}

void basic_frame(void) {
	if (waitFrames > 0) waitFrames--;
}

void basic_input(const char *line) {
	if (state != BASIC_INPUT) return;
	state = BASIC_RUNNING;
	int code = setjmp(errjmp);
	if (code) { report_error(code); return; }
	arenaTop = 0;
	exprDepth = 0;
	col = 0;
	P = inputPtr;
	const char *q = line;
	for (;;) {
		VarRef r = parse_varref();
		char buf[STR_MAX + 1];
		int n = 0;
		while (*q == ' ') q++;
		if (*q == '"') {
			q++;
			while (*q && *q != '"' && n < STR_MAX) buf[n++] = *q++;
			if (*q == '"') q++;
			while (*q == ' ') q++;
		} else {
			while (*q && *q != ',' && n < STR_MAX) buf[n++] = *q++;
			while (n > 0 && buf[n - 1] == ' ') n--;
		}
		buf[n] = 0;
		if (*q == ',') q++;
		if (vars[r.vi].isStr) set_var(r, str(buf));
		else {
			float v = 0;
			const char *s = buf, *e = s;
			int neg = 0;
			if (*s == '-') { neg = 1; s++; } else if (*s == '+') s++;
			if (buf[0] && (!parse_number_at(s, &v, &e) || *e)) {
				outs("?REDO FROM START\n");
				if (inputPrompt) outs(inputPrompt);
				outs("? ");
				state = BASIC_INPUT;
				return;
			}
			set_var(r, num(neg ? -v : v));
		}
		skipsp();
		if (*P == ',') { P++; continue; }
		break;
	}
	/* P now points behind the variable list; step() continues there */
}

void basic_break(void) {
	if (state == BASIC_IDLE) return;
	char b[40];
	if (curLine >= 0 && curLine < nlines) sprintf(b, "BREAK IN %d", lines[curLine].num);
	else strcpy(b, "BREAK");
	if (col) out('\n');
	outs(b);
	out('\n');
	strcpy(lastError, b);
	state = BASIC_IDLE;
	waitFrames = 0;
	waitSound = 0;
}
