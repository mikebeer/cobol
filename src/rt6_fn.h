/* rt6_fn.h -- intrinsic functions */

typedef struct {
    unsigned char *buf;
    long cap;
    CobAttr a;
    CobFld f;
} FnSlot;

static FnSlot fn_ring[32];
static int fn_k = 0;
static const CobFld *fn_trim_lead = 0;

static CobFld *fn_alnum(const unsigned char *s, long n)
{
    FnSlot *r = &fn_ring[fn_k++ & 31];
    if (n < 1) n = 0;
    if (r->cap < n + 1) {
        free(r->buf);
        r->cap = n + 64;
        r->buf = (unsigned char *)malloc((size_t)r->cap);
        if (!r->buf) cob_fatal("out of memory");
    }
    if (n) memcpy(r->buf, s, (size_t)n);
    r->a.type = CT_ALNUM;
    r->a.digits = 0;
    r->a.scale = 0;
    r->a.flags = 0;
    r->a.pic = 0;
    r->f.size = n;
    r->f.data = r->buf;
    r->f.attr = &r->a;
    return &r->f;
}

static CobFld *fn_dec(const CobDec *d)
{
    return cob_dtof(d);
}

static CobFld *fn_int(long v)
{
    CobDec d;
    cd_from_long(&d, v);
    return fn_dec(&d);
}

static CobFld *fn_dbl(double v)
{
    CobDec d;
    cd_from_double(&d, v);
    /* keep a sensible number of digits */
    if (d.scale > 30) cd_setscale(&d, 30, CR_AWAY);
    return fn_dec(&d);
}

static void fn_ld(CobDec *d, const CobFld *f)
{
    cob_ld(d, f);
}

static double fn_d(const CobFld *f)
{
    CobDec d;
    cob_ld(&d, f);
    return cd_to_double(&d);
}

static void cd_floor(CobDec *r, const CobDec *x)
{
    CobDec t = *x, i, one;
    cd_setscale(&t, 0, CR_NONE);
    i = t;
    if (x->neg && x->n) {
        CobDec back = t;
        back.scale = 0;
        if (cd_cmp(&back, x) != 0) {
            cd_from_long(&one, 1);
            cd_sub(&i, &t, &one);
        }
    }
    *r = i;
}

/* ---- dates ---- */

static long fn_civil_days(long y, long m, long d)     /* since 1970 */
{
    long era, yoe, doy, doe;
    y -= m <= 2;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = y - era * 400;
    doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static void fn_civil_from(long z, long *y, long *m, long *d)
{
    long era, doe, yoe, doy, mp;
    z += 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = z - era * 146097;
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    *y = yoe + era * 400;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp = (5 * doy + 2) / 153;
    *d = doy - (153 * mp + 2) / 5 + 1;
    *m = mp + (mp < 10 ? 3 : -9);
    *y += *m <= 2;
}

static long fn_int_of_date(long ymd)
{
    return fn_civil_days(ymd / 10000, ymd / 100 % 100, ymd % 100) +
           134775L;
}

static long fn_date_of_int(long n)
{
    long y, m, d;
    fn_civil_from(n - 134775L, &y, &m, &d);
    return y * 10000 + m * 100 + d;
}

static int fn_leap(long y)
{
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

static unsigned long fn_seed = 1;

#define FN_MAXARGS 64

static CobFld *cob_fn2(int id, int n, CobFld **a);

static CobFld *cob_fn(int id, int n, ...)
{
    CobFld *a[FN_MAXARGS];
    va_list ap;
    int i;
    CobDec x, y, r, t;
    if (n > FN_MAXARGS) n = FN_MAXARGS;
    va_start(ap, n);
    for (i = 0; i < n; i++) a[i] = va_arg(ap, CobFld *);
    va_end(ap);
    switch (id) {
    case FN_ABS:
        fn_ld(&x, a[0]);
        x.neg = 0;
        return fn_dec(&x);
    case FN_ACOS: return fn_dbl(acos(fn_d(a[0])));
    case FN_ASIN: return fn_dbl(asin(fn_d(a[0])));
    case FN_ATAN: return fn_dbl(atan(fn_d(a[0])));
    case FN_COS: return fn_dbl(cos(fn_d(a[0])));
    case FN_SIN: return fn_dbl(sin(fn_d(a[0])));
    case FN_TAN: return fn_dbl(tan(fn_d(a[0])));
    case FN_E:
        cd_from_str(&r, "2.7182818284590452353602874713526625", 36);
        return fn_dec(&r);
    case FN_PI:
        cd_from_str(&r, "3.1415926535897932384626433832795029", 36);
        return fn_dec(&r);
    case FN_EXP: return fn_dbl(exp(fn_d(a[0])));
    case FN_EXP10: return fn_dbl(pow(10.0, fn_d(a[0])));
    case FN_LOG: return fn_dbl(log(fn_d(a[0])));
    case FN_LOG10: return fn_dbl(log10(fn_d(a[0])));
    case FN_SQRT: {
        double v = fn_d(a[0]);
        return fn_dbl(v < 0 ? 0.0 : sqrt(v));
    }
    case FN_FACTORIAL: {
        long k = cob_toi(a[0]), j;
        cd_from_long(&r, 1);
        for (j = 2; j <= k; j++) {
            cd_from_long(&t, j);
            cd_mul(&r, &r, &t);
        }
        return fn_dec(&r);
    }
    case FN_INTEGER:
        fn_ld(&x, a[0]);
        cd_floor(&r, &x);
        return fn_dec(&r);
    case FN_INTEGER_PART:
        fn_ld(&x, a[0]);
        cd_setscale(&x, 0, CR_NONE);
        return fn_dec(&x);
    case FN_FRACTION_PART:
        fn_ld(&x, a[0]);
        y = x;
        cd_setscale(&y, 0, CR_NONE);
        cd_sub(&r, &x, &y);
        return fn_dec(&r);
    case FN_SIGN:
        fn_ld(&x, a[0]);
        return fn_int(x.n == 0 ? 0 : (x.neg ? -1 : 1));
    case FN_MOD:
        fn_ld(&x, a[0]);
        fn_ld(&y, a[1]);
        cd_mod(&r, &x, &y);
        return fn_dec(&r);
    case FN_REM:
        fn_ld(&x, a[0]);
        fn_ld(&y, a[1]);
        if (y.n == 0) { cd_zero(&r); return fn_dec(&r); }
        cd_div(&t, &x, &y, 0);
        cd_mul(&t, &t, &y);
        cd_sub(&r, &x, &t);
        return fn_dec(&r);
    case FN_MAX: case FN_MIN: case FN_ORD_MAX: case FN_ORD_MIN: {
        int best = 0;
        int ismax = id == FN_MAX || id == FN_ORD_MAX;
        for (i = 1; i < n; i++) {
            int c = cob_cmp(a[i], a[best]);
            if (ismax ? c > 0 : c < 0) best = i;
        }
        if (id == FN_ORD_MAX || id == FN_ORD_MIN)
            return fn_int(best + 1);
        return a[best];
    }
    case FN_SUM: case FN_MEAN: case FN_RANGE: case FN_MIDRANGE:
    case FN_VARIANCE: case FN_STANDARD_DEVIATION: case FN_MEDIAN: {
        double *v = (double *)malloc(sizeof(double) * (size_t)(n + 1));
        double s = 0, mn, mx, m, var = 0;
        CobDec sum;
        if (!v) cob_fatal("out of memory");
        cd_zero(&sum);
        for (i = 0; i < n; i++) {
            fn_ld(&x, a[i]);
            cd_add(&sum, &sum, &x);
            v[i] = cd_to_double(&x);
            s += v[i];
        }
        if (id == FN_SUM) { free(v); return fn_dec(&sum); }
        mn = mx = v[0];
        for (i = 1; i < n; i++) {
            if (v[i] < mn) mn = v[i];
            if (v[i] > mx) mx = v[i];
        }
        m = s / n;
        for (i = 0; i < n; i++) var += (v[i] - m) * (v[i] - m);
        var /= n;
        if (id == FN_MEAN) {
            CobDec cnt;
            cd_from_long(&cnt, n);
            cd_div(&r, &sum, &cnt, CD_DIVS);
            free(v);
            return fn_dec(&r);
        }
        if (id == FN_RANGE) {
            free(v);
            return fn_dbl(mx - mn);
        }
        if (id == FN_MIDRANGE) {
            free(v);
            return fn_dbl((mx + mn) / 2);
        }
        if (id == FN_VARIANCE) {
            free(v);
            return fn_dbl(var);
        }
        if (id == FN_STANDARD_DEVIATION) {
            free(v);
            return fn_dbl(sqrt(var));
        }
        {
            int j, k2;
            for (j = 0; j < n; j++)
                for (k2 = j + 1; k2 < n; k2++)
                    if (v[k2] < v[j]) {
                        double tmp = v[j]; v[j] = v[k2]; v[k2] = tmp;
                    }
            m = n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2;
            free(v);
            return fn_dbl(m);
        }
    }
    case FN_ORD: {
        long c = a[0]->size ? a[0]->data[0] : 0;
        return fn_int(c + 1);
    }
    case FN_CHAR: {
        unsigned char c = (unsigned char)(cob_toi(a[0]) - 1);
        return fn_alnum(&c, 1);
    }
    case FN_LENGTH: case FN_BYTE_LENGTH: case FN_STORED_CHAR_LENGTH:
    case FN_ULENGTH: {
        long k = a[0]->size;
        if (id == FN_LENGTH && a[0]->attr->type == CT_NAT) k /= 2;
        if (id == FN_STORED_CHAR_LENGTH) {
            while (k > 0 && a[0]->data[k - 1] == ' ') k--;
        }
        return fn_int(k);
    }
    case FN_UPPER_CASE: case FN_LOWER_CASE: {
        CobFld *f = fn_alnum(a[0]->data, a[0]->size);
        long k;
        for (k = 0; k < f->size; k++)
            f->data[k] = (unsigned char)(id == FN_UPPER_CASE ?
                toupper(f->data[k]) : tolower(f->data[k]));
        return f;
    }
    case FN_REVERSE: {
        CobFld *f = fn_alnum(a[0]->data, a[0]->size);
        long k;
        for (k = 0; k < f->size; k++)
            f->data[k] = a[0]->data[a[0]->size - 1 - k];
        return f;
    }
    case FN_TRIM: case FN_TRIM_LEFT: case FN_TRIM_RIGHT: {
        long b = 0, e = a[0]->size;
        int mode = id == FN_TRIM_LEFT ? 1 : id == FN_TRIM_RIGHT ? 2
            : 0;
        if (n > 1) mode = (int)cob_toi(a[1]);
        if (mode != 2)
            while (b < e && a[0]->data[b] == ' ') b++;
        if (mode != 1)
            while (e > b && a[0]->data[e - 1] == ' ') e--;
        if (e == b) return fn_alnum((const unsigned char *)" ", 1);
        return fn_alnum(a[0]->data + b, e - b);
    }
    case FN_CONCATENATE: {
        long tot = 0, o = 0;
        unsigned char *buf;
        CobFld *f;
        for (i = 0; i < n; i++) tot += a[i]->size;
        buf = (unsigned char *)malloc((size_t)tot + 1);
        if (!buf) cob_fatal("out of memory");
        for (i = 0; i < n; i++) {
            memcpy(buf + o, a[i]->data, (size_t)a[i]->size);
            o += a[i]->size;
        }
        f = fn_alnum(buf, tot);
        free(buf);
        return f;
    }
    case FN_SUBSTITUTE: case FN_SUBSTITUTE_CASE: {
        long cap = a[0]->size * 4 + 256, o = 0, p = 0;
        unsigned char *buf = (unsigned char *)malloc((size_t)cap);
        CobFld *f;
        if (!buf) cob_fatal("out of memory");
        while (p < a[0]->size) {
            int hit = 0;
            for (i = 1; i + 1 < n; i += 2) {
                long k = a[i]->size, q;
                int eq = k > 0 && p + k <= a[0]->size;
                for (q = 0; eq && q < k; q++) {
                    int c1 = a[0]->data[p + q], c2 = a[i]->data[q];
                    if (id == FN_SUBSTITUTE_CASE) {
                        c1 = toupper(c1);
                        c2 = toupper(c2);
                    }
                    if (c1 != c2) eq = 0;
                }
                if (eq) {
                    long rl = a[i + 1]->size;
                    if (o + rl + 8 > cap) {
                        cap = (o + rl) * 2 + 256;
                        buf = (unsigned char *)realloc(buf,
                                                       (size_t)cap);
                        if (!buf) cob_fatal("out of memory");
                    }
                    memcpy(buf + o, a[i + 1]->data, (size_t)rl);
                    o += rl;
                    p += k;
                    hit = 1;
                    break;
                }
            }
            if (!hit) {
                if (o + 8 > cap) {
                    cap = o * 2 + 256;
                    buf = (unsigned char *)realloc(buf, (size_t)cap);
                    if (!buf) cob_fatal("out of memory");
                }
                buf[o++] = a[0]->data[p++];
            }
        }
        f = fn_alnum(buf, o);
        free(buf);
        return f;
    }
    case FN_NUMVAL: case FN_NUMVAL_C: case FN_NUMVAL_F: {
        unsigned char tmp[256];
        long k, m = 0;
        for (k = 0; k < a[0]->size && m < 250; k++) {
            int c = a[0]->data[k];
            if (id == FN_NUMVAL_C && n > 1 && k < a[0]->size &&
                a[1]->size > 0 && c == a[1]->data[0]) continue;
            if (id == FN_NUMVAL_C && (c == ',' || c == '$')) continue;
            if (id == FN_NUMVAL_F && (c == 'E' || c == 'e')) {
                tmp[m++] = (unsigned char)'E';
                continue;
            }
            tmp[m++] = (unsigned char)c;
        }
        if (id == FN_NUMVAL_F) {
            double v = atof((const char *)(tmp[m] = 0, tmp));
            return fn_dbl(v);
        }
        cob_parse_num(&r, tmp, m, 0);
        return fn_dec(&r);
    }
    case FN_CURRENT_DATE: {
        time_t tt = time(0);
        struct tm lt, gt;
        char b[80];
        long off;
        lt = *localtime(&tt);
        gt = *gmtime(&tt);
        off = (lt.tm_hour - gt.tm_hour) * 60 + (lt.tm_min - gt.tm_min);
        if (lt.tm_yday != gt.tm_yday) {
            if (lt.tm_year > gt.tm_year ||
                (lt.tm_year == gt.tm_year && lt.tm_yday > gt.tm_yday))
                off += 1440;
            else off -= 1440;
        }
        sprintf(b, "%04d%02d%02d%02d%02d%02d00%c%02ld%02ld",
                lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
                lt.tm_hour, lt.tm_min, lt.tm_sec, off < 0 ? '-' : '+',
                (off < 0 ? -off : off) / 60, (off < 0 ? -off
                    : off) % 60);
        return fn_alnum((unsigned char *)b, 21);
    }
    case FN_WHEN_COMPILED: {
        static const char *mon[] = { "Jan", "Feb", "Mar", "Apr", "May",
            "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
        const char *d = __DATE__, *tm_ = __TIME__;
        char b[40];
        int m = 0, k;
        for (k = 0; k < 12; k++) if (!strncmp(d, mon[k], 3)) m = k + 1;
        sprintf(b, "%.4s%02d%02d%.2s%.2s%.2s00+0000", d + 7, m,
                atoi(d + 4), tm_, tm_ + 3, tm_ + 6);
        return fn_alnum((unsigned char *)b, 21);
    }
    case FN_INTEGER_OF_DATE:
        return fn_int(fn_int_of_date(cob_toi(a[0])));
    case FN_DATE_OF_INTEGER:
        return fn_int(fn_date_of_int(cob_toi(a[0])));
    case FN_INTEGER_OF_DAY: {
        long yd = cob_toi(a[0]), y = yd / 1000, d = yd % 1000;
        return fn_int(fn_int_of_date(y * 10000 + 101) + d - 1);
    }
    case FN_DAY_OF_INTEGER: {
        long ymd = fn_date_of_int(cob_toi(a[0])), y = ymd / 10000;
        long d = cob_toi(a[0]) - fn_int_of_date(y * 10000 + 101) + 1;
        return fn_int(y * 1000 + d);
    }
    case FN_YEAR_TO_YYYY: {
        long yy = cob_toi(a[0]), w = n > 1 ? cob_toi(a[1]) : 50;
        long cur_y;
        time_t tt = time(0);
        cur_y = localtime(&tt)->tm_year + 1900;
        {
            long base = cur_y + w;
            long y = base - base % 100 + yy;
            if (y > base) y -= 100;
            return fn_int(y);
        }
    }
    case FN_DATE_TO_YYYYMMDD: {
        long d = cob_toi(a[0]), w = n > 1 ? cob_toi(a[1]) : 50;
        long yy = d / 10000, rest = d % 10000;
        time_t tt = time(0);
        long cur_y = localtime(&tt)->tm_year + 1900;
        long base = cur_y + w;
        long y = base - base % 100 + yy;
        if (y > base) y -= 100;
        return fn_int(y * 10000 + rest);
    }
    case FN_DAY_TO_YYYYDDD: {
        long d = cob_toi(a[0]), w = n > 1 ? cob_toi(a[1]) : 50;
        long yy = d / 1000, rest = d % 1000;
        time_t tt = time(0);
        long cur_y = localtime(&tt)->tm_year + 1900;
        long base = cur_y + w;
        long y = base - base % 100 + yy;
        if (y > base) y -= 100;
        return fn_int(y * 1000 + rest);
    }
    case FN_TEST_DATE_YYYYMMDD: {
        long d = cob_toi(a[0]), y = d / 10000, m = d / 100 % 100;
        long dd = d % 100;
        static const int dm[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30,
                                  31, 30, 31 };
        if (y < 1601 || y > 9999) return fn_int(1);
        if (m < 1 || m > 12) return fn_int(2);
        if (dd < 1 || dd > dm[m - 1] + (m == 2 && fn_leap(y)))
            return fn_int(3);
        return fn_int(0);
    }
    case FN_TEST_DAY_YYYYDDD: {
        long d = cob_toi(a[0]), y = d / 1000, dd = d % 1000;
        if (y < 1601 || y > 9999) return fn_int(1);
        if (dd < 1 || dd > 365 + fn_leap(y)) return fn_int(2);
        return fn_int(0);
    }
    case FN_SECONDS_PAST_MIDNIGHT: {
        time_t tt = time(0);
        struct tm lt = *localtime(&tt);
        return fn_int(lt.tm_hour * 3600L + lt.tm_min * 60L
            + lt.tm_sec);
    }
    case FN_RANDOM: {
        double v;
        if (n > 0) fn_seed = (unsigned long)cob_toi(a[0]) &
            0x7fffffffUL;
        fn_seed = (fn_seed * 1103515245UL + 12345UL) & 0x7fffffffUL;
        v = (double)fn_seed / 2147483648.0;
        return fn_dbl(v);
    }
    case FN_ANNUITY: {
        double i2 = fn_d(a[0]);
        long p = cob_toi(a[1]);
        if (i2 == 0) return fn_dbl(1.0 / p);
        return fn_dbl(i2 / (1.0 - pow(1.0 + i2, -(double)p)));
    }
    case FN_PRESENT_VALUE: {
        double rate = fn_d(a[0]), s = 0;
        for (i = 1; i < n; i++)
            s += fn_d(a[i]) / pow(1.0 + rate, i);
        return fn_dbl(s);
    }
    case FN_TEST_NUMVAL: {
        long k = 0, nd = 0;
        for (k = 0; k < a[0]->size; k++) {
            int c = a[0]->data[k];
            if (isdigit(c)) nd++;
            else if (c != ' ' && c != '+' && c != '-' && c != '.' &&
                     c != 'C' && c != 'R' && c != 'D' && c != 'B')
                return fn_int(k + 1);
        }
        return fn_int(nd ? 0 : (long)a[0]->size + 1);
    }
    case FN_HEX_OF: {
        static const char hx[] = "0123456789ABCDEF";
        long k;
        CobFld *f = fn_alnum(a[0]->data, a[0]->size * 2);
        for (k = 0; k < a[0]->size; k++) {
            f->data[2 * k] = (unsigned char)hx[a[0]->data[k] >> 4];
            f->data[2 * k + 1] = (unsigned char)hx[a[0]->data[k] & 15];
        }
        return f;
    }
    case FN_MODULE_NAME: case FN_MODULE_ID:
        return fn_alnum((const unsigned char *)"COBOL", 5);
    case FN_NUMBER_OF_CALL_PARAMETERS:
        return fn_int(0);
    default:
        return cob_fn2(id, n, a);
    }
}

/* the callee sets the size and attributes of its result */
static void cob_fret(CobFld *dst, const CobFld *src)
{
    long n = src->size < 4096 ? src->size : 4096;
    memcpy(dst->data, src->data, (size_t)n);
    dst->size = n;
    dst->attr = src->attr;
}

static CobFld *cob_ufn(const char *name, int n, ...)
{
    static unsigned char ubuf[8][4096];
    static CobFld uf[8];
    static int uk = 0;
    CobFld *a[32];
    char nm[48];
    va_list ap;
    int i;
    CobFld *r = &uf[uk & 7];
    unsigned char *b = ubuf[uk & 7];
    uk++;
    if (n > 32) n = 32;
    va_start(ap, n);
    for (i = 0; i < n; i++) a[i] = va_arg(ap, CobFld *);
    va_end(ap);
    strcpy(nm, "FN:");
    cob_upname(nm + 3, name, (long)strlen(name));
    r->data = b;
    r->size = 4096;
    r->attr = 0;
    for (i = 0; i < cob_nprogs; i++)
        if (!strcmp(cob_progs[i].name, nm)) {
            cob_progs[i].fn(n, a, r);
            return r;
        }
    cob_fatal("user-defined function not found");
    return 0;
}
