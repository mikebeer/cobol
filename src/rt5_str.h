/* rt5_str.h -- STRING, UNSTRING and INSPECT */

/* ---- STRING ---- */

static CobFld *st_dst = 0, *st_ptr = 0;
static long st_pos = 0;
static int st_ovf = 0;

static void cob_str_begin(CobFld *dst, CobFld *ptr)
{
    st_dst = dst;
    st_ptr = ptr;
    st_ovf = 0;
    st_pos = ptr ? cob_toi(ptr) - 1 : 0;
    if (st_pos < 0 || st_pos > dst->size) st_ovf = 1;
}

/* mode 0: whole source; 1: up to the delimiter */
static void cob_str_add(const CobFld *src, const CobFld *del, int mode)
{
    long n = src->size, k;
    const unsigned char *s = src->data;
    if (st_ovf) return;
    if (mode == 1 && del && del->size > 0) {
        long i, dn = del->size;
        for (i = 0; i + dn <= n; i++)
            if (!memcmp(s + i, del->data, (size_t)dn)) { n = i; break;
                }
    }
    for (k = 0; k < n; k++) {
        if (st_pos >= st_dst->size) { st_ovf = 1; return; }
        st_dst->data[st_pos++] = s[k];
    }
}

static int cob_str_end(void)
{
    if (st_ptr && !st_ovf) cob_seti(st_ptr, st_pos + 1);
    else if (st_ptr && st_ovf) {
        /* the pointer is unchanged on invalid starting values */
        if (st_pos >= 0 && st_pos <= st_dst->size)
            cob_seti(st_ptr, st_pos + 1);
    }
    return st_ovf;
}

/* ---- UNSTRING ---- */

static const CobFld *un_src = 0;
static CobFld *un_ptr = 0;
static long un_pos = 0, un_cnt = 0;
static int un_ovf = 0, un_nd = 0;
static const CobFld *un_del[16];
static int un_all[16];

static void cob_unstr_begin(const CobFld *src, CobFld *ptr)
{
    un_src = src;
    un_ptr = ptr;
    un_ovf = 0;
    un_nd = 0;
    un_cnt = 0;
    un_pos = ptr ? cob_toi(ptr) - 1 : 0;
    if (un_pos < 0 || un_pos >= src->size) un_ovf = 1;
}

static void cob_unstr_delim(const CobFld *d, int all)
{
    if (un_nd < 16) {
        un_del[un_nd] = d;
        un_all[un_nd++] = all;
    }
}

static void cob_unstr_into(CobFld *dst, CobFld *dlm, CobFld *cnt)
{
    long start = un_pos, end = un_src->size, i, dl = 0;
    int matched = -1, k;
    const unsigned char *s = un_src->data;
    if (un_ovf) return;
    if (un_nd > 0) {
        for (i = start; i < un_src->size && matched < 0; i++) {
            for (k = 0; k < un_nd; k++) {
                long n = un_del[k]->size;
                if (n > 0 && i + n <= un_src->size &&
                    !memcmp(s + i, un_del[k]->data, (size_t)n)) {
                    matched = k;
                    end = i;
                    dl = n;
                    break;
                }
            }
        }
    }
    {
        CobFld f;
        f.size = end - start;
        f.data = (unsigned char *)s + start;
        f.attr = &cob_alnum_attr;
        cob_move(&f, dst);
        if (cnt) cob_seti(cnt, end - start);
    }
    if (matched >= 0) {
        if (dlm) {
            CobFld d;
            d.size = dl;
            d.data = (unsigned char *)s + end;
            d.attr = &cob_alnum_attr;
            cob_move(&d, dlm);
        }
        un_pos = end + dl;
        if (un_all[matched]) {
            long n = un_del[matched]->size;
            while (n > 0 && un_pos + n <= un_src->size &&
                   !memcmp(s + un_pos, un_del[matched]->data,
                           (size_t)n))
                un_pos += n;
        }
    } else {
        if (dlm) cob_fig(dlm, FG_SPACE);
        un_pos = un_src->size;
    }
    un_cnt++;
    if (un_pos >= un_src->size && matched < 0) un_pos = un_src->size;
}

static int cob_unstr_end(CobFld *tally)
{
    if (tally) {
        CobDec a, b, r;
        cob_ld(&a, tally);
        cd_from_long(&b, un_cnt);
        cd_add(&r, &a, &b);
        cob_st(&r, tally, CR_NONE, 0);
    }
    if (un_ptr) cob_seti(un_ptr, un_pos + 1);
    if (!un_ovf && un_nd > 0 && un_pos < un_src->size) {
        /* data remains and no field is left */
        un_ovf = 1;
    }
    return un_ovf;
}

/* ---- INSPECT ---- */

#define IN_MAX 32
typedef struct {
    int op;                     /* 0 chars 1 all 2 leading 3 first */
    CobFld *cnt;
    const CobFld *pat, *rep, *bf, *af;
    int kind;                   /* 0 tally, 1 replace, 2 convert */
    int active;
    long hits;
} InspC;

static InspC in_c[IN_MAX];
static int in_n = 0;
static CobFld *in_t = 0;

static void cob_insp_begin(CobFld *t)
{
    in_t = t;
    in_n = 0;
}

static void in_add(int kind, int op, CobFld *cnt, const CobFld *pat,
                   const CobFld *rep, const CobFld *bf,
                       const CobFld *af)
{
    InspC *c;
    if (in_n >= IN_MAX) cob_fatal("INSPECT: too many phrases");
    c = &in_c[in_n++];
    c->kind = kind;
    c->op = op;
    c->cnt = cnt;
    c->pat = pat;
    c->rep = rep;
    c->bf = bf;
    c->af = af;
    c->active = 1;
    c->hits = 0;
}

static void cob_insp_t(int op, CobFld *cnt, const CobFld *pat,
                       const CobFld *bf, const CobFld *af)
{
    in_add(0, op, cnt, pat, 0, bf, af);
}

static void cob_insp_r(int op, const CobFld *pat, const CobFld *rep,
                       const CobFld *bf, const CobFld *af)
{
    in_add(1, op, 0, pat, rep, bf, af);
}

static void cob_insp_c(const CobFld *from, const CobFld *to,
                       const CobFld *bf, const CobFld *af)
{
    in_add(2, 1, 0, from, to, bf, af);
}

static long in_find(const unsigned char *s, long n, long from,
                    const CobFld *p)
{
    long i, k = p->size;
    if (k == 0) return -1;
    for (i = from; i + k <= n; i++)
        if (!memcmp(s + i, p->data, (size_t)k)) return i;
    return -1;
}

static void cob_insp_run(void)
{
    long n = in_t->size, i;
    unsigned char *s = in_t->data;
    unsigned char *copy = (unsigned char *)malloc((size_t)(n + 1));
    int phase;
    if (!copy) cob_fatal("out of memory");
    memcpy(copy, s, (size_t)n);
    for (phase = 0; phase < 2; phase++) {
        int j;
        InspC *c;
        long start[IN_MAX], end[IN_MAX];
        int any = 0;
        for (j = 0; j < in_n; j++) {
            c = &in_c[j];
            c->active = 1;
            start[j] = 0;
            end[j] = n;
            if ((phase == 0 && c->kind != 0) ||
                (phase == 1 && c->kind == 0)) continue;
            any = 1;
            if (c->af) {
                long p = in_find(copy, n, 0, c->af);
                start[j] = p < 0 ? n : p + c->af->size;
            }
            if (c->bf) {
                long p = in_find(copy, n, start[j], c->bf);
                if (p >= 0) end[j] = p;
            }
        }
        if (!any) continue;
        for (i = 0; i < n; ) {
            int hit = 0;
            for (j = 0; j < in_n && !hit; j++) {
                long m = 0;
                c = &in_c[j];
                if ((phase == 0 && c->kind != 0) ||
                    (phase == 1 && c->kind == 0)) continue;
                if (i < start[j] || i >= end[j]) {
                    if (c->op == 2 && i >= start[j]) c->active = 0;
                    continue;
                }
                if (c->kind == 2) {
                    long q;
                    for (q = 0; q < c->pat->size; q++)
                        if (c->pat->data[q] == s[i]) {
                            long r = q < c->rep->size ? q : 0;
                            s[i] = c->rep->data[r];
                            break;
                        }
                    hit = 1;
                    m = 1;
                } else if (c->op == 0) {
                    m = 1;
                } else {
                    long k = c->pat->size;
                    if (k > 0 && i + k <= end[j] &&
                        !memcmp(copy + i, c->pat->data, (size_t)k)) {
                        if (c->op == 2 && !c->active) { m = 0; }
                        else if (c->op == 3 && c->hits) m = 0;
                        else m = k;
                    } else if (c->op == 2) c->active = 0;
                }
                if (m > 0 && c->kind != 2) {
                    c->hits++;
                    if (c->kind == 1) {
                        long q;
                        if (c->op == 0)
                            for (q = 0; q < m; q++)
                                s[i + q] = c->rep->data[q %
                                    (c->rep->size ? c->rep->size : 1)];
                        else
                            for (q = 0; q < m; q++)
                                s[i + q] = q < c->rep->size ?
                                    c->rep->data[q] : ' ';
                    }
                    i += m;
                    hit = 1;
                    if (c->op == 0 || c->kind == 1) { }
                    break;
                }
                if (hit && c->kind == 2) { i++; break; }
            }
            if (!hit) {
                /* leading phrases stop at the first non-match */
                for (j = 0; j < in_n; j++)
                    if (in_c[j].op == 2) in_c[j].active = 0;
                i++;
            }
        }
        for (j = 0; j < in_n; j++) {
            c = &in_c[j];
            if (c->kind == 0 && phase == 0 && c->cnt) {
                CobDec a, b, r;
                cob_ld(&a, c->cnt);
                cd_from_long(&b, c->hits);
                cd_add(&r, &a, &b);
                cob_st(&r, c->cnt, CR_NONE, 0);
            }
        }
    }
    free(copy);
}
