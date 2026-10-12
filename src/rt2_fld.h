/* rt2_fld.h -- fields: attributes, loading and storing numbers,
   MOVE, numeric editing, comparison */



typedef struct {
    int type, digits, scale, flags;
    const char *pic;            /* expanded picture of edited items */
} CobAttr;

typedef struct {
    long size;
    unsigned char *data;
    const CobAttr *attr;
} CobFld;

/* figurative constants */

static const CobAttr cob_alnum_attr = { CT_ALNUM, 0, 0, 0, 0 };
static int cob_binary_trunc = 1;
static int cob_little = 0;
static int cob_size_err = 0;    /* last store had a size error */

static int cob_isnum(const CobFld *f)
{
    int t = f->attr->type;
    return t >= CT_NUMDISP && t <= CT_DBL ? 1 :
           (t == CT_NUMEDIT);
}

#define COB_EBCDIC ('0' == 0xF0)

static int cob_zone(int neg, int signd)
{
    if (COB_EBCDIC) return neg ? 0xD0 : (signd ? 0xC0 : 0xF0);
    return neg ? 0x70 : 0x30;
}

/* multiply a decimal integer by m and add a: used for binary */
static void cd_mul_add(CobDec *r, int m, int a)
{
    int i, c = a, v;
    for (i = 0; i < r->n; i++) {
        v = r->d[i] * m + c;
        r->d[i] = (unsigned char)(v % 10);
        c = v / 10;
    }
    while (c && r->n < CD_MAX) {
        r->d[r->n++] = (unsigned char)(c % 10);
        c /= 10;
    }
}

/* parse text such as " -12,345.60CR" into a number */
static void cob_parse_num(CobDec *r, const unsigned char *s, long n,
                          int dpc)
{
    long i;
    int seen = 0, fr = 0, neg = 0, k = 0;
    unsigned char t[CD_MAX * 2];
    cd_zero(r);
    for (i = 0; i < n; i++) {
        int c = s[i];
        if (c == '-') neg = 1;
        else if ((c == 'C' || c == 'D') && i + 1 < n &&
                 (s[i + 1] == 'R' || s[i + 1] == 'B')) {
            neg = 1;
            i++;
        } else if (c == (dpc ? ',' : '.')) seen = 1;
        else if (c >= '0' && c <= '9') {
            if (k < CD_MAX * 2) t[k++] = (unsigned char)(c - '0');
            if (seen) fr++;
        }
    }
    if (k > CD_MAX) {
        fr -= k - CD_MAX;
        k = CD_MAX;
    }
    for (i = 0; i < k; i++) r->d[k - 1 - i] = t[i];
    r->n = k;
    r->scale = fr < 0 ? 0 : fr;
    r->neg = neg;
    cd_norm(r);
}

static void cob_ld(CobDec *r, const CobFld *f)
{
    const CobAttr *a = f->attr;
    const unsigned char *p = f->data;
    long n = f->size, i;
    int neg = 0, fl = a->flags;
    cd_zero(r);
    switch (a->type) {
    case CT_NUMDISP: {
        long b = 0, e = n;
        int k = 0;
        if (fl & CF_SEP) {
            if (fl & CF_LEAD) {
                neg = p[0] == '-';
                b = 1;
            } else {
                neg = p[n - 1] == '-';
                e = n - 1;
            }
        }
        for (i = b; i < e; i++) {
            int c = p[i], d;
            if (COB_EBCDIC) {
                d = c & 0x0F;
                if ((c & 0xF0) == 0xD0) neg = 1;
            } else {
                d = c - '0';
                if (c >= 0x70 && c <= 0x79) {
                    d = c - 0x70;
                    neg = 1;
                } else if (d < 0 || d > 9) d = 0;
            }
            if (d > 9) d = 0;
            if (e - 1 - i < CD_MAX) r->d[e - 1 - i] = (unsigned char)d;
            k++;
        }
        r->n = k > CD_MAX ? CD_MAX : k;
        r->scale = a->scale;
        r->neg = neg;
        cd_norm(r);
        if (a->scale < 0) {
            cd_shift(r, -a->scale);
            r->scale = 0;
        }
        break;
    }
    case CT_PACKED: {
        unsigned char dg[CD_MAX + 2];
        int m = 0, sg;
        for (i = 0; i < n && m < CD_MAX; i++) {
            dg[m++] = (unsigned char)(p[i] >> 4);
            dg[m++] = (unsigned char)(p[i] & 15);
        }
        sg = m > 0 ? dg[m - 1] : 0;
        neg = sg == 0x0D || sg == 0x0B;
        m--;
        for (i = 0; i < m; i++)
            r->d[m - 1 - i] = dg[i] > 9 ? 0 : dg[i];
        r->n = m > 0 ? m : 0;
        r->scale = a->scale;
        r->neg = neg;
        cd_norm(r);
        if (a->scale < 0) {
            cd_shift(r, -a->scale);
            r->scale = 0;
        }
        break;
    }
    case CT_BIN: case CT_BIN5: case CT_PTR: case CT_OBJ: {
        unsigned char b[16];
        int sgn = (fl & CF_SIGNED) != 0, c = 1;
        if (n > 16) n = 16;
        for (i = 0; i < n; i++)
            b[i] = a->type == CT_BIN || !cob_little ? p[i] :
                   p[n - 1 - i];
        if (sgn && n > 0 && (b[0] & 0x80)) {
            neg = 1;
            for (i = n - 1; i >= 0; i--) {      /* two's complement */
                int v = (unsigned char)~b[i] + c;
                b[i] = (unsigned char)v;
                c = v > 255;
            }
        }
        for (i = 0; i < n; i++) cd_mul_add(r, 256, b[i]);
        r->scale = 0;
        cd_norm(r);
        r->scale = a->scale;
        r->neg = neg && r->n;
        if (a->scale < 0) {
            cd_shift(r, -a->scale);
            r->scale = 0;
        }
        break;
    }
    case CT_FLT: {
        float x;
        memcpy(&x, p, sizeof x);
        cd_from_double(r, (double)x);
        break;
    }
    case CT_DBL: {
        double x;
        memcpy(&x, p, sizeof x);
        cd_from_double(r, x);
        break;
    }
    default:
        cob_parse_num(r, p, n, (fl & CF_DPC) != 0);
        break;
    }
}

static long cob_ndigits(const CobDec *d)    /* integer digits */
{
    return d->n > d->scale ? d->n - d->scale : 0;
}

/* the number as a string of exactly nd digits (integer then
   fraction already scaled by the caller) */
static void cob_dstr(const CobDec *d, unsigned char *out, int nd)
{
    int i;
    for (i = 0; i < nd; i++)
        out[nd - 1 - i] = (unsigned char)(i < d->n ? d->d[i] : 0);
}

static void cob_edit(const CobDec *v, CobFld *f);

/* store; mode = rounding; returns 1 on size error (field then holds
   the truncated value unless nostore) */
static int cob_st(const CobDec *v0, CobFld *f, int mode, int nostore)
{
    const CobAttr *a = f->attr;
    CobDec v = *v0;
    int fl = a->flags, nd = a->digits, err = 0, ints;
    long n = f->size, i;
    unsigned char *p = f->data;
    if (a->type == CT_FLT || a->type == CT_DBL) {
        double x = cd_to_double(&v);
        if (a->type == CT_FLT) {
            float y = (float)x;
            memcpy(p, &y, sizeof y);
        } else {
            memcpy(p, &x, sizeof x);
        }
        return 0;
    }
    if (a->type == CT_NUMEDIT) {
        cd_setscale(&v, a->scale, mode);
        err = cob_ndigits(&v) > a->digits - a->scale;
        if (err && nostore) return 1;
        cob_edit(&v, f);
        cob_size_err = err;
        return err;
    }
    if (a->type == CT_ALNUM || a->type == CT_GROUP ||
        a->type == CT_ALNEDIT) {
        return 0;
    }
    if (a->type == CT_PTR || a->type == CT_OBJ) nd = 18;
    if (a->scale < 0) {
        v.scale += -a->scale;
        cd_setscale(&v, 0, mode);
    } else {
        cd_setscale(&v, a->scale, mode);
    }
    ints = nd - (a->scale > 0 ? a->scale : 0);
    if (cob_ndigits(&v) > ints &&
        !((a->type == CT_BIN || a->type == CT_BIN5) &&
          !cob_binary_trunc)) {
        err = 1;
        if (nostore) return 1;
        /* keep the low-order digits */
        v.n = ints + (v.scale > 0 ? v.scale : 0);
        if (v.n < 0) v.n = 0;
        cd_norm(&v);
    }
    if (!(fl & CF_SIGNED)) v.neg = 0;
    if (v.n == 0) v.neg = 0;
    switch (a->type) {
    case CT_NUMDISP: {
        long b = 0, e = n;
        if (fl & CF_SEP) {
            if (fl & CF_LEAD) {
                p[0] = (unsigned char)(v.neg ? '-' : '+');
                b = 1;
            } else {
                p[n - 1] = (unsigned char)(v.neg ? '-' : '+');
                e = n - 1;
            }
        }
        {
            long cnt = e - b;
            for (i = 0; i < cnt; i++) {
                int d = i < v.n ? v.d[i] : 0;
                p[e - 1 - i] = (unsigned char)(0x30 + d);
                if (COB_EBCDIC)
                    p[e - 1 - i] = (unsigned char)(0xF0 + d);
            }
            if (!(fl & CF_SEP) && (fl & CF_SIGNED)) {
                long pos = (fl & CF_LEAD) ? b : e - 1;
                int d = p[pos] & 0x0F;
                p[pos] = (unsigned char)(cob_zone(v.neg, 1) + d);
            }
        }
        break;
    }
    case CT_PACKED: {
        int m = (int)n * 2, k, d, sg;
        sg = v.neg ? 0x0D : ((fl & CF_SIGNED) ? 0x0C : 0x0F);
        for (i = 0; i < n; i++) p[i] = 0;
        p[n - 1] = (unsigned char)sg;
        for (k = 0; k < m - 1; k++) {
            d = k < v.n ? v.d[k] : 0;
            i = n - 1 - (k + 1) / 2;
            if ((k + 1) % 2) p[i] = (unsigned char)(p[i] | (d << 4));
            else p[i] = (unsigned char)(p[i] | d);
        }
        break;
    }
    case CT_BIN: case CT_BIN5: case CT_PTR: case CT_OBJ: {
        unsigned char b[16];
        CobDec t = v;
        int c, j;
        if (n > 16) n = 16;
        for (i = 0; i < n; i++) b[i] = 0;
        /* repeated division by 256 of the integer value */
        t.scale = 0;
        for (i = n - 1; i >= 0 && t.n > 0; i--) {
            int rem = 0, k2;
            for (k2 = t.n - 1; k2 >= 0; k2--) {
                int cur = rem * 10 + t.d[k2];
                t.d[k2] = (unsigned char)(cur / 256);
                rem = cur % 256;
            }
            b[i] = (unsigned char)rem;
            cd_norm(&t);
        }
        if (t.n > 0 && !cob_binary_trunc) err = 1;
        if (v.neg) {
            c = 1;
            for (j = (int)n - 1; j >= 0; j--) {
                int w = (unsigned char)~b[j] + c;
                b[j] = (unsigned char)w;
                c = w > 255;
            }
        }
        for (i = 0; i < n; i++)
            p[i] = a->type == CT_BIN || !cob_little ? b[i] :
                   b[n - 1 - i];
        break;
    }
    default:
        break;
    }
    cob_size_err = err;
    return err;
}

/* ---- numeric editing ---- */

static int pic_isdig(int c)
{
    return c == '9' || c == 'Z' || c == '*';
}

/* end index of a floating string that starts at pic[i]: the sign or
   currency character c with insertion characters in between */
static long pic_runend(const char *pic, long i, char cm)
{
    long j = i, last = i, len = (long)strlen(pic);
    int c = pic[i];
    while (j + 1 < len && (pic[j + 1] == c || pic[j + 1] == cm ||
           pic[j + 1] == 'B' || pic[j + 1] == '0' ||
           pic[j + 1] == '/')) {
        j++;
        if (pic[j] == c) last = j;
    }
    return last;
}

/* edit v (already at the picture's scale) into f using its picture */
static void cob_edit(const CobDec *v, CobFld *f)
{
    const char *pic = f->attr->pic;
    int fl = f->attr->flags;
    char dp = (fl & CF_DPC) ? ',' : '.';
    char cm = (fl & CF_DPC) ? '.' : ',';
    long len = (long)strlen(pic), i, o = 0;
    int nd = 0, dec = 0, afterdp = 0, k, sig = 0, neg = v->neg;
    int zero = v->n == 0, hasnine = 0, di = 0, fsym = 0, star;
    int dig[CD_MAX + 4];
    unsigned char *out = f->data;
    star = strchr(pic, '*') != 0;
    for (i = 0; i < len; i++) {                 /* digit positions */
        int c = pic[i];
        if (c == 'V' || c == dp) { afterdp = 1; continue; }
        if (c == '9') hasnine = 1;
        if (pic_isdig(c)) {
            nd++;
            if (afterdp) dec++;
        } else if ((c == '+' || c == '-' || c == '$') &&
                   pic_runend(pic, i, cm) > i) {
            long e = pic_runend(pic, i, cm), j;
            for (j = i + 1; j <= e; j++)
                if (pic[j] == c) {
                    nd++;
                    if (afterdp) dec++;
                }
            i = e;
        }
    }
    if (nd > CD_MAX) nd = CD_MAX;
    {
        unsigned char ds[CD_MAX + 4];
        CobDec t = *v;
        cd_setscale(&t, dec, CR_NONE);
        cob_dstr(&t, ds, nd);
        for (k = 0; k < nd; k++) dig[k] = ds[k];
    }
    if ((fl & CF_BLANK) && zero) {
        memset(out, ' ', (size_t)f->size);
        return;
    }
    afterdp = 0;
    for (i = 0; i < len && o < f->size; i++) {
        int c = pic[i], d;
        if (c == 'V') { afterdp = 1; continue; }
        if (c == 'P') continue;
        if (c == dp) {
            out[o++] = (unsigned char)dp;
            afterdp = 1;
        } else if (c == '9' || c == 'Z' || c == '*') {
            d = dig[di++];
            if (c == '9' || d != 0 || sig) {
                if (!sig && fsym && o > 0 && out[o - 1] == ' ')
                    out[o - 1] = (unsigned char)fsym;
                fsym = 0;
                sig = 1;
                out[o++] = (unsigned char)('0' + d);
            } else {
                out[o++] = (unsigned char)(c == '*' ? '*' : ' ');
            }
        } else if (c == cm) {
            out[o++] = (unsigned char)(sig ? cm : (star ? '*' : ' '));
        } else if (c == 'B') {
            out[o++] = ' ';
        } else if (c == '0' || c == '/') {
            out[o++] = (unsigned char)c;
        } else if ((c == 'C' && pic[i + 1] == 'R') ||
                   (c == 'D' && pic[i + 1] == 'B')) {
            out[o++] = (unsigned char)(neg ? c : ' ');
            out[o++] = (unsigned char)(neg ? pic[i + 1] : ' ');
            i++;
        } else if (c == '+' || c == '-' || c == '$') {
            int sym = c == '$' ? '$' :
                      (neg ? '-' : (c == '+' ? '+' : 0));
            if (pic_runend(pic, i, cm) > i) {
                /* a floating run: the first character is a slot */
                long e = pic_runend(pic, i, cm);
                fsym = sym;
                out[o++] = ' ';
                while (i < e && o < f->size) {
                    int cc;
                    i++;
                    cc = pic[i];
                    if (cc == c) {
                        d = dig[di++];
                        if (d != 0 || sig) {
                            if (!sig && fsym && o > 0 &&
                                out[o - 1] == ' ')
                                out[o - 1] = (unsigned char)fsym;
                            fsym = 0;
                            sig = 1;
                            out[o++] = (unsigned char)('0' + d);
                        } else {
                            out[o++] = ' ';
                        }
                    } else if (cc == cm) {
                        out[o++] = (unsigned char)(sig ? cm : ' ');
                    } else if (cc == 'B') {
                        out[o++] = ' ';
                    } else {
                        out[o++] = (unsigned char)cc;
                    }
                }
            } else {
                out[o++] = (unsigned char)(sym ? sym : ' ');
            }
        } else {
            out[o++] = (unsigned char)c;
        }
    }
    while (o < f->size) out[o++] = ' ';
    if (zero && !hasnine) {
        for (i = 0; i < f->size; i++)
            out[i] = (unsigned char)(star &&
                     out[i] != (unsigned char)dp ? '*' : ' ');
        if (star)
            for (i = 0; i < len && i < f->size; i++)
                if (pic[i] == dp) out[i] = (unsigned char)dp;
    }
}

/* alphanumeric edited: B 0 / insert, A X 9 take source characters */
static void cob_aedit(const CobFld *src, CobFld *f)
{
    const char *pic = f->attr->pic;
    long i, o = 0, j = 0, len = (long)strlen(pic);
    for (i = 0; i < len && o < f->size; i++) {
        int c = pic[i];
        if (c == 'B') f->data[o++] = ' ';
        else if (c == '0' || c == '/') f->data[o++] = (unsigned char)c;
        else f->data[o++] = j < src->size ? src->data[j++] : ' ';
    }
    while (o < f->size) f->data[o++] = ' ';
}

/* ---- figurative constants and MOVE ---- */

static int cob_figch(int fig)
{
    switch (fig) {
    case FG_ZERO: return '0';
    case FG_SPACE: return ' ';
    case FG_HIGH: return COB_EBCDIC ? 0xFF : 0xFF;
    case FG_LOW: return 0;
    case FG_QUOTE: return '"';
    }
    return ' ';
}

/* MOVE a figurative constant */
static void cob_fig(CobFld *d, int fig)
{
    const CobAttr *a = d->attr;
    if (fig == FG_ZERO && cob_isnum(d) && a->type != CT_NUMEDIT) {
        CobDec z;
        cd_zero(&z);
        cob_st(&z, d, CR_NONE, 0);
        return;
    }
    if (fig == FG_ZERO && a->type == CT_NUMEDIT) {
        CobDec z;
        cd_zero(&z);
        cob_edit(&z, d);
        return;
    }
    if (a->type == CT_NAT) {
        long i;
        for (i = 0; i + 1 < d->size; i += 2) {
            d->data[i] = 0;
            d->data[i + 1] = (unsigned char)cob_figch(fig);
        }
        return;
    }
    memset(d->data, cob_figch(fig), (size_t)d->size);
}

/* MOVE ALL "lit" */
static void cob_move_all(CobFld *d, const CobFld *lit)
{
    long i;
    if (lit->size == 0) return;
    for (i = 0; i < d->size; i++)
        d->data[i] = lit->data[i % lit->size];
}

static void cob_just(CobFld *d, const unsigned char *s, long n)
{
    long i;
    if (d->attr->flags & CF_JUST) {
        if (n >= d->size) {
            memcpy(d->data, s + (n - d->size), (size_t)d->size);
        } else {
            memset(d->data, ' ', (size_t)(d->size - n));
            memcpy(d->data + (d->size - n), s, (size_t)n);
        }
    } else {
        if (n >= d->size) {
            memcpy(d->data, s, (size_t)d->size);
        } else {
            memcpy(d->data, s, (size_t)n);
            for (i = n; i < d->size; i++) d->data[i] = ' ';
        }
    }
}

/* the digits of a number as text (no sign), integer part only when
   the target is alphanumeric */
static long cob_dtext(const CobDec *v, unsigned char *o, int intonly)
{
    CobDec t = *v;
    long k = 0, i, n;
    if (intonly) cd_setscale(&t, 0, CR_NONE);
    n = t.n > t.scale ? t.n : t.scale;
    if (n == 0) n = 1;
    for (i = n - 1; i >= 0; i--) {
        if (k < 200) o[k++] = (unsigned char)('0' + (i < t.n ?
                                                    t.d[i] : 0));
    }
    return k;
}

static void cob_move(const CobFld *s, CobFld *d)
{
    const CobAttr *sa = s->attr, *da = d->attr;
    int sn = cob_isnum(s), dn = cob_isnum(d);
    if (s == d) return;
    if (da->type == CT_ALNEDIT) {
        if (sn) {
            unsigned char b[220];
            CobFld t;
            CobDec v;
            long k;
            cob_ld(&v, s);
            k = cob_dtext(&v, b, 1);
            t = *s;
            t.data = b;
            t.size = k;
            cob_aedit(&t, d);
        } else {
            cob_aedit(s, d);
        }
        return;
    }
    if (!sn && !dn) {
        if (sa->type == CT_NAT || da->type == CT_NAT) {
            long i, j = 0;
            if (sa->type == da->type) {
                cob_just(d, s->data, s->size);
                return;
            }
            if (da->type == CT_NAT) {
                for (i = 0; i + 1 < d->size; i += 2) {
                    d->data[i] = 0;
                    d->data[i + 1] = j < s->size ? s->data[j++] : ' ';
                }
            } else {
                for (i = 0; i < d->size; i++, j += 2)
                    d->data[i] = j + 1 < s->size ?
                                 s->data[j + 1] : ' ';
            }
            return;
        }
        cob_just(d, s->data, s->size);
        return;
    }
    if (sn && !dn) {
        unsigned char b[220];
        CobDec v;
        long k;
        cob_ld(&v, s);
        k = cob_dtext(&v, b, 1);
        if (da->type == CT_PTR || da->type == CT_OBJ) return;
        cob_just(d, b, k);
        return;
    }
    {
        CobDec v;
        if (sa->type == da->type && sa->type == CT_NUMDISP &&
            sa->digits == da->digits && sa->scale == da->scale &&
            sa->flags == da->flags && s->size == d->size) {
            memcpy(d->data, s->data, (size_t)s->size);
            return;
        }
        if (sa->type == CT_PTR || sa->type == CT_OBJ) {
            if (da->type == sa->type) {
                memcpy(d->data, s->data, (size_t)(s->size < d->size ?
                                                  s->size : d->size));
                return;
            }
        }
        cob_ld(&v, s);
        cob_st(&v, d, CR_NONE, 0);
    }
}

/* ---- comparison ---- */

static int cob_cmp_bytes(const unsigned char *a, long na,
                         const unsigned char *b, long nb)
{
    long i, n = na < nb ? na : nb;
    for (i = 0; i < n; i++)
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    for (i = n; i < na; i++)
        if (a[i] != ' ') return a[i] < ' ' ? -1 : 1;
    for (i = n; i < nb; i++)
        if (b[i] != ' ') return ' ' < b[i] ? -1 : 1;
    return 0;
}

static int cob_cmp(const CobFld *a, const CobFld *b)
{
    int an = cob_isnum(a), bn = cob_isnum(b);
    if (an && bn) {
        CobDec x, y;
        cob_ld(&x, a);
        cob_ld(&y, b);
        return cd_cmp(&x, &y);
    }
    if (an != bn) {
        /* number against characters: compare as display text */
        const CobFld *n = an ? a : b;
        unsigned char t[220];
        CobDec v;
        long k;
        int r;
        cob_ld(&v, n);
        k = cob_dtext(&v, t, 1);
        r = an ? cob_cmp_bytes(t, k, b->data, b->size) :
                 cob_cmp_bytes(a->data, a->size, t, k);
        return r;
    }
    if (a->attr->type == CT_NAT || b->attr->type == CT_NAT) {
        long i, na = a->size, nb = b->size, n = na < nb ? na : nb;
        for (i = 0; i < n; i++)
            if (a->data[i] != b->data[i])
                return a->data[i] < b->data[i] ? -1 : 1;
        return 0;
    }
    return cob_cmp_bytes(a->data, a->size, b->data, b->size);
}

/* compare against a figurative constant */
static int cob_cmp_fig(const CobFld *a, int fig)
{
    long i;
    int ch;
    if (cob_isnum(a) && fig == FG_ZERO) {
        CobDec v;
        cob_ld(&v, a);
        return v.n == 0 ? 0 : (v.neg ? -1 : 1);
    }
    ch = cob_figch(fig);
    for (i = 0; i < a->size; i++)
        if (a->data[i] != (unsigned char)ch)
            return a->data[i] < (unsigned char)ch ? -1 : 1;
    return 0;
}

/* compare against ALL "lit" */
static int cob_cmp_all(const CobFld *a, const CobFld *lit)
{
    long i;
    if (lit->size == 0) return 0;
    for (i = 0; i < a->size; i++) {
        unsigned char c = lit->data[i % lit->size];
        if (a->data[i] != c) return a->data[i] < c ? -1 : 1;
    }
    return 0;
}

/* numeric value of a field as long (subscripts, counts) */
static long cob_toi(const CobFld *f)
{
    CobDec v;
    int ok;
    cob_ld(&v, f);
    return cd_to_long(&v, &ok);
}

static void cob_seti(CobFld *f, long n)
{
    CobDec v;
    cd_from_long(&v, n);
    cob_st(&v, f, CR_NONE, 0);
}

/* class conditions */
static int cob_is_numeric(const CobFld *f)
{
    long i;
    const CobAttr *a = f->attr;
    if (a->type == CT_NUMDISP) {
        long b = 0, e = f->size;
        if (a->flags & CF_SEP) {
            if (a->flags & CF_LEAD) b = 1; else e--;
        }
        for (i = b; i < e; i++) {
            int c = f->data[i];
            if (COB_EBCDIC) {
                if ((c & 0x0F) > 9) return 0;
            } else if (!((c >= '0' && c <= '9') ||
                         (c >= 0x70 && c <= 0x79 &&
                          (a->flags & CF_SIGNED)))) return 0;
        }
        return 1;
    }
    if (cob_isnum(f)) return 1;
    for (i = 0; i < f->size; i++)
        if (f->data[i] < '0' || f->data[i] > '9') return 0;
    return f->size > 0;
}

static int cob_is_alpha(const CobFld *f)
{
    long i;
    for (i = 0; i < f->size; i++)
        if (!isalpha(f->data[i]) && f->data[i] != ' ') return 0;
    return 1;
}

static int cob_is_upper(const CobFld *f)
{
    long i;
    for (i = 0; i < f->size; i++)
        if (!isupper(f->data[i]) && f->data[i] != ' ') return 0;
    return 1;
}

static int cob_is_lower(const CobFld *f)
{
    long i;
    for (i = 0; i < f->size; i++)
        if (!islower(f->data[i]) && f->data[i] != ' ') return 0;
    return 1;
}

static int cob_sign(const CobFld *f)    /* -1, 0, 1 */
{
    CobDec v;
    cob_ld(&v, f);
    return v.n == 0 ? 0 : (v.neg ? -1 : 1);
}
