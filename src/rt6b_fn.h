/* rt6b_fn.h -- more intrinsic functions: formatted dates and times,
   BASECONVERT, booleans, national and miscellaneous */

static long fn_wday(long intdate)
{
    return (intdate - 1) % 7 + 1;       /* 1 = Monday */
}

/* ISO week date of an integer date */
static void fn_isoweek(long idate, long *wy, long *ww, long *wd)
{
    long th, y;
    *wd = fn_wday(idate);
    th = idate + 4 - *wd;
    y = fn_date_of_int(th) / 10000;
    *wy = y;
    *ww = (th - fn_int_of_date(y * 10000 + 101)) / 7 + 1;
}

static int fn_digits(const char *s, int n, long *v)
{
    int i;
    *v = 0;
    for (i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
        *v = *v * 10 + (s[i] - '0');
    }
    return 1;
}

/* does the format start with a date part; length of it */
static int fn_datefmt(const char *f, int *kind)
{
    *kind = 0;
    if (!strncmp(f, "YYYY-MM-DD", 10)) { *kind = 1; return 10; }
    if (!strncmp(f, "YYYYMMDD", 8)) { *kind = 2; return 8; }
    if (!strncmp(f, "YYYY-DDD", 8)) { *kind = 3; return 8; }
    if (!strncmp(f, "YYYYDDD", 7)) { *kind = 4; return 7; }
    if (!strncmp(f, "YYYY-Www-D", 10)) { *kind = 5; return 10; }
    if (!strncmp(f, "YYYYWwwD", 8)) { *kind = 6; return 8; }
    return 0;
}

static int fn_timefmt(const char *f, int *kind, int *fr)
{
    int n;
    *kind = 0; *fr = 0;
    if (!strncmp(f, "hh:mm:ss", 8)) { *kind = 1; n = 8; }
    else if (!strncmp(f, "hhmmss", 6)) { *kind = 2; n = 6; }
    else return 0;
    if (f[n] == '.' || f[n] == ',') {
        int k = n + 1;
        while (f[k] == 's') { k++; (*fr)++; }
        n = k;
    }
    return n;
}

/* format date, time and offset by a format string */
static CobFld *fn_fmt_dt(const char *f, long idate, long secs,
                         long fracn, int hasoff, long off)
{
    char b[96];
    int dk = 0, tk = 0, fr = 0, n = 0;
    int dl = idate > 0 ? fn_datefmt(f, &dk) : 0;
    long y = 0, m = 0, d = 0;
    const char *p = f;
    if (idate > 0) {
        long ymd = fn_date_of_int(idate);
        y = ymd / 10000; m = ymd / 100 % 100; d = ymd % 100;
    }
    if (dl) {
        if (dk == 1) n += sprintf(b + n, "%04ld-%02ld-%02ld", y, m, d);
        else if (dk == 2) n += sprintf(b + n, "%04ld%02ld%02ld", y, m,
            d);
        else if (dk == 3 || dk == 4) {
            long dd = idate - fn_int_of_date(y * 10000 + 101) + 1;
            n += sprintf(b + n, dk == 3 ? "%04ld-%03ld" : "%04ld%03ld",
                         y, dd);
        } else {
            long wy, ww, wd;
            fn_isoweek(idate, &wy, &ww, &wd);
            n += sprintf(b + n, dk == 5 ? "%04ld-W%02ld-%ld" :
                         "%04ldW%02ld%ld", wy, ww, wd);
        }
        p += dl;
        if (*p == 'T') { b[n++] = 'T'; p++; }
    }
    {
        int tl = fn_timefmt(p, &tk, &fr);
        if (tl) {
            long hh = secs / 3600, mm = secs / 60 % 60, ss = secs % 60;
            long fv = fracn;
            int i;
            if (tk == 1) n += sprintf(b + n, "%02ld:%02ld:%02ld", hh,
                                      mm, ss);
            else n += sprintf(b + n, "%02ld%02ld%02ld", hh, mm, ss);
            if (fr) {
                b[n++] = p[tk == 1 ? 8 : 6];
                for (i = 0; i < fr; i++) {
                    long dv = 1, k2;
                    for (k2 = 0; k2 < fr - 1 - i; k2++) dv *= 10;
                    b[n++] = (char)('0' + (fv / dv) % 10);
                }
            }
            p += tl;
        }
    }
    if (*p == 'Z') {
        if (hasoff && off != 0) {
            long ao = off < 0 ? -off : off;
            n += sprintf(b + n, "%c%02ld%02ld", off < 0 ? '-' : '+',
                         ao / 60, ao % 60);
        } else b[n++] = 'Z';
    } else if (*p == '+') {
        long ao = off < 0 ? -off : off;
        n += sprintf(b + n, "%c%02ld%s%02ld", off < 0 ? '-' : '+',
                     ao / 60, p[3] == ':' ? ":" : "", ao % 60);
    }
    return fn_alnum((unsigned char *)b, n);
}

/* parse a formatted date; returns the integer date or 0 */
static long fn_parse_date(const char *f, const char *s, int *used)
{
    int kind;
    int dl = fn_datefmt(f, &kind);
    long y, m, d, w, dd;
    if (!dl) return 0;
    *used = dl;
    switch (kind) {
    case 1:
        if (!fn_digits(s, 4, &y) || !fn_digits(s + 5, 2, &m) ||
            !fn_digits(s + 8, 2, &d)) return 0;
        break;
    case 2:
        if (!fn_digits(s, 4, &y) || !fn_digits(s + 4, 2, &m) ||
            !fn_digits(s + 6, 2, &d)) return 0;
        break;
    case 3: case 4:
        if (!fn_digits(s, 4, &y) ||
            !fn_digits(s + (kind == 3 ? 5 : 4), 3, &dd)) return 0;
        if (dd < 1 || dd > (fn_leap(y) ? 366 : 365)) return 0;
        return fn_int_of_date(y * 10000 + 101) + dd - 1;
    default: {
        long jan4, wd4;
        int o = kind == 5 ? 1 : 0;
        if (!fn_digits(s, 4, &y) || !fn_digits(s + 5 + o - 1 + 0, 0,
            &w))
            return 0;
        if (!fn_digits(s + 5 + o, 2, &w) ||
            !fn_digits(s + 7 + 2 * o, 1, &dd)) return 0;
        if (w < 1 || w > 53 || dd < 1 || dd > 7) return 0;
        jan4 = fn_int_of_date(y * 10000 + 104);
        wd4 = fn_wday(jan4);
        return jan4 - (wd4 - 1) + (w - 1) * 7 + (dd - 1);
    }
    }
    if (m < 1 || m > 12 || d < 1) return 0;
    {
        static const int dm[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30,
                                  31, 30, 31 };
        long lim = dm[m - 1] + (m == 2 && fn_leap(y));
        if (d > lim) return 0;
    }
    return fn_int_of_date(y * 10000 + m * 100 + d);
}

/* parse a formatted time to seconds; -1 when invalid */
static long fn_parse_time(const char *f, const char *s, int *used)
{
    int kind, fr;
    int tl = fn_timefmt(f, &kind, &fr);
    long h, m, sec;
    if (!tl) return -1;
    *used = tl;
    if (kind == 1) {
        if (!fn_digits(s, 2, &h) || !fn_digits(s + 3, 2, &m) ||
            !fn_digits(s + 6, 2, &sec)) return -1;
    } else if (!fn_digits(s, 2, &h) || !fn_digits(s + 2, 2, &m) ||
               !fn_digits(s + 4, 2, &sec)) return -1;
    if (h > 23 || m > 59 || sec > 60) return -1;
    return h * 3600 + m * 60 + sec;
}

static void fn_str0(char *b, int cap, const CobFld *f)
{
    long n = f->size < cap - 1 ? f->size : cap - 1;
    memcpy(b, f->data, (size_t)n);
    while (n > 0 && (b[n - 1] == ' ' || b[n - 1] == 0)) n--;
    b[n] = 0;
}

static int fn_bdigit(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    return -1;
}

static CobFld *cob_fn2(int id, int n, CobFld **a)
{
    switch (id) {
    case FN_BASECONVERT: {
        char in[260];
        unsigned char out[1100];
        int from = (int)cob_toi(a[1]), to = (int)cob_toi(a[2]);
        int neg = 0, no = 1, i, j;
        char res[1100];
        char *p;
        static const char dg[] =
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        fn_str0(in, 260, a[0]);
        p = in;
        while (*p == ' ') p++;
        if (*p == '-') { neg = 1; p++; }
        if (from < 2 || from > 36 || to < 2 || to > 36)
            return fn_alnum((const unsigned char *)" ", 1);
        out[0] = 0;
        for (; *p; p++) {
            int dv = fn_bdigit(*p), carry;
            if (dv < 0 || dv >= from) break;
            carry = dv;
            for (i = 0; i < no; i++) {
                int t = out[i] * from + carry;
                out[i] = (unsigned char)(t % to);
                carry = t / to;
            }
            while (carry && no < 1090) {
                out[no++] = (unsigned char)(carry % to);
                carry /= to;
            }
        }
        j = 0;
        if (neg) res[j++] = '-';
        for (i = no - 1; i >= 0; i--) res[j++] = dg[out[i]];
        return fn_alnum((unsigned char *)res, j);
    }
    case FN_FORMATTED_CURRENT_DATE: {
        char f[64];
        time_t tt = time(0);
        struct tm lt, gt;
        long off = 0, id_;
        int hasz = 0;
        fn_str0(f, 64, a[0]);
        lt = *localtime(&tt);
        gt = *gmtime(&tt);
        id_ = fn_int_of_date((lt.tm_year + 1900) * 10000L +
                             (lt.tm_mon + 1) * 100 + lt.tm_mday);
        off = (lt.tm_hour - gt.tm_hour) * 60 + (lt.tm_min - gt.tm_min);
        if (lt.tm_yday != gt.tm_yday)
            off += (lt.tm_year > gt.tm_year ||
                    (lt.tm_year == gt.tm_year &&
                     lt.tm_yday > gt.tm_yday)) ? 1440 : -1440;
        hasz = strchr(f, 'Z') != 0 || strchr(f, '+') != 0;
        return fn_fmt_dt(f, id_, lt.tm_hour * 3600L + lt.tm_min * 60 +
                         lt.tm_sec, 0, hasz, off);
    }
    case FN_FORMATTED_DATE: {
        char f[64];
        fn_str0(f, 64, a[0]);
        return fn_fmt_dt(f, cob_toi(a[1]), 0, 0, 0, 0);
    }
    case FN_FORMATTED_TIME: {
        char f[64];
        long off = n > 2 ? cob_toi(a[2]) : 0;
        fn_str0(f, 64, a[0]);
        return fn_fmt_dt(f, 0, cob_toi(a[1]), 0, n > 2, off);
    }
    case FN_FORMATTED_DATETIME: {
        char f[64];
        long off = n > 3 ? cob_toi(a[3]) : 0;
        CobDec t, w;
        long secs, frac = 0;
        int okk;
        fn_str0(f, 64, a[0]);
        fn_ld(&t, a[2]);
        w = t;
        cd_setscale(&w, 0, CR_NONE);
        secs = cd_to_long(&w, &okk);
        {
            CobDec one, fp, hund;
            cd_from_long(&one, 1);
            cd_sub(&fp, &t, &w);
            cd_from_long(&hund, 100);
            cd_mul(&fp, &fp, &hund);
            cd_setscale(&fp, 0, CR_NONE);
            frac = cd_to_long(&fp, &okk);
        }
        return fn_fmt_dt(f, cob_toi(a[1]), secs, frac, n > 3, off);
    }
    case FN_INTEGER_OF_FORMATTED_DATE: {
        char f[64], s[64];
        int used = 0;
        fn_str0(f, 64, a[0]);
        fn_str0(s, 64, a[1]);
        return fn_int(fn_parse_date(f, s, &used));
    }
    case FN_SECONDS_FROM_FORMATTED_TIME: {
        char f[64], s[64];
        int used = 0, dk;
        char *fp = f, *sp = s;
        long v;
        fn_str0(f, 64, a[0]);
        fn_str0(s, 64, a[1]);
        used = fn_datefmt(f, &dk);
        if (used) { fp += used; sp += used; if (*fp == 'T') {
            fp++; sp++; } }
        v = fn_parse_time(fp, sp, &used);
        return fn_int(v < 0 ? 0 : v);
    }
    case FN_TEST_FORMATTED_DATETIME: {
        char f[64], s[64];
        int used = 0, dk, bad = 0;
        char *fp = f, *sp = s;
        fn_str0(f, 64, a[0]);
        fn_str0(s, 64, a[1]);
        if (fn_datefmt(f, &dk)) {
            if (!fn_parse_date(f, s, &used)) bad = 1;
            fp += used; sp += used;
            if (*fp == 'T') {
                if (*sp != 'T') bad = 1;
                fp++; sp++;
            }
        }
        if (!bad && fn_timefmt(fp, &dk, &used) &&
            fn_parse_time(fp, sp, &used) < 0) bad = 1;
        return fn_int(bad ? 1 : 0);
    }
    case FN_COMBINED_DATETIME: {
        CobDec s, d, sc;
        fn_ld(&s, a[1]);
        cd_from_long(&d, cob_toi(a[0]));
        cd_from_long(&sc, 100000L);
        cd_div(&s, &s, &sc, 5);
        cd_add(&d, &d, &s);
        return fn_dec(&d);
    }
    case FN_TEST_NUMVAL_C: case FN_TEST_NUMVAL_F: {
        char s[260];
        int i = 0, dg = 0, pt = 0, sg = 0, ex = 0;
        fn_str0(s, 260, a[0]);
        while (s[i] == ' ') i++;
        if (s[i] == '+' || s[i] == '-') { sg = 1; i++;
            while (s[i] == ' ') i++; }
        if (id == FN_TEST_NUMVAL_C && s[i] == '$') {
            i++; while (s[i] == ' ') i++;
        }
        for (; s[i]; i++) {
            if (s[i] >= '0' && s[i] <= '9') dg++;
            else if (s[i] == '.' && !pt) pt = 1;
            else if (id == FN_TEST_NUMVAL_C && s[i] == ',') { }
            else if (id == FN_TEST_NUMVAL_F && (s[i] == 'E' ||
                                                 s[i] == 'e') && !ex
                     && dg) {
                ex = 1;
                if (s[i + 1] == '+' || s[i + 1] == '-') i++;
            } else break;
        }
        while (s[i] == ' ') i++;
        if (!sg && (s[i] == '+' || s[i] == '-')) i++;
        else if (!sg && id == FN_TEST_NUMVAL_C &&
                 (!strncmp(s + i, "CR", 2) || !strncmp(s + i, "DB",
                     2)))
            i += 2;
        while (s[i] == ' ') i++;
        if (s[i] || !dg) return fn_int(s[i] ? i + 1 : (int)a[0]->size
            + 1);
        return fn_int(0);
    }
    case FN_HIGHEST_ALGEBRAIC: case FN_LOWEST_ALGEBRAIC: {
        CobDec v;
        int i, dgt = a[0]->attr->digits, sc = a[0]->attr->scale;
        int sgd = (a[0]->attr->flags & CF_SIGNED) != 0;
        memset(&v, 0, sizeof v);
        if (dgt <= 0) dgt = 1;
        if (id == FN_LOWEST_ALGEBRAIC && !sgd) return fn_int(0);
        for (i = 0; i < dgt && i < CD_MAX; i++) v.d[i] = 9;
        v.n = dgt;
        v.scale = sc;
        v.neg = id == FN_LOWEST_ALGEBRAIC;
        return fn_dec(&v);
    }
    case FN_BOOLEAN_OF_INTEGER: {
        long v = cob_toi(a[0]), len = cob_toi(a[1]), i;
        CobFld *f;
        if (len < 1) len = 1;
        f = fn_alnum((const unsigned char *)"", 0);
        f = fn_alnum(a[0]->data, len);
        for (i = len - 1; i >= 0; i--) {
            f->data[i] = (unsigned char)('0' + (v & 1));
            v >>= 1;
        }
        return f;
    }
    case FN_INTEGER_OF_BOOLEAN: {
        long v = 0, i;
        for (i = 0; i < a[0]->size; i++)
            v = v * 2 + (a[0]->data[i] == '1');
        return fn_int(v);
    }
    case FN_BIT_OF: {
        long i, k;
        CobFld *f = fn_alnum(a[0]->data, a[0]->size * 8);
        for (i = 0; i < a[0]->size; i++)
            for (k = 0; k < 8; k++)
                f->data[i * 8 + k] = (unsigned char)(
                    '0' + ((a[0]->data[i] >> (7 - k)) & 1));
        return f;
    }
    case FN_BIT_TO_CHAR: {
        long nb = a[0]->size / 8, i, k;
        CobFld *f = fn_alnum(a[0]->data, nb);
        for (i = 0; i < nb; i++) {
            int c = 0;
            for (k = 0; k < 8; k++)
                c = c * 2 + (a[0]->data[i * 8 + k] == '1');
            f->data[i] = (unsigned char)c;
        }
        return f;
    }
    case FN_EXCEPTION_FILE: case FN_EXCEPTION_LOCATION:
    case FN_EXCEPTION_STATEMENT: case FN_EXCEPTION_STATUS: {
        unsigned char sp[31];
        memset(sp, ' ', sizeof sp);
        return fn_alnum(sp, id == FN_EXCEPTION_FILE ? 2 : 31);
    }
    case FN_LOCALE_DATE: {
        return fn_fmt_dt("YYYY-MM-DD", fn_int_of_date(cob_toi(a[0])),
                         0, 0, 0, 0);
    }
    case FN_LOCALE_TIME:
        return fn_fmt_dt("hh:mm:ss", 0, cob_toi(a[0]), 0, 0, 0);
    case FN_DISPLAY_OF: {
        long i, m = a[0]->size / 2;
        CobFld *f = fn_alnum(a[0]->data, m);
        for (i = 0; i < m; i++)
            f->data[i] = a[0]->data[2 * i] ? '?' :
                         a[0]->data[2 * i + 1];
        return f;
    }
    case FN_NATIONAL_OF: case FN_CHAR_NATIONAL: {
        long i, m = id == FN_CHAR_NATIONAL ? 1 : a[0]->size;
        CobFld *f = fn_alnum(a[0]->data, m * 2);
        for (i = 0; i < m; i++) {
            f->data[2 * i] = 0;
            f->data[2 * i + 1] = id == FN_CHAR_NATIONAL ?
                (unsigned char)(cob_toi(a[0]) - 1) : a[0]->data[i];
        }
        {
            FnSlot *r = &fn_ring[(fn_k - 1) & 31];
            r->a.type = CT_NAT;
        }
        return f;
    }
    case FN_USUBSTR: {
        long pos = cob_toi(a[1]), len = n > 2 ? cob_toi(a[2]) :
                   a[0]->size / 2 - pos + 1;
        CobFld *f;
        if (pos < 1) pos = 1;
        if (len < 0) len = 0;
        if (pos > a[0]->size / 2) len = 0;
        else if (pos - 1 + len > a[0]->size / 2)
            len = a[0]->size / 2 - pos + 1;
        f = fn_alnum(a[0]->data + (pos - 1) * 2, len * 2);
        fn_ring[(fn_k - 1) & 31].a.type = CT_NAT;
        return f;
    }
    case FN_UPOS: {
        long i, m = a[0]->size / 2, k = a[1]->size / 2;
        for (i = 0; i + k <= m; i++)
            if (!memcmp(a[0]->data + 2 * i, a[1]->data,
                        (size_t)(2 * k))) return fn_int(i + 1);
        return fn_int(0);
    }
    case FN_UUID4: {
        static const char hx[] = "0123456789abcdef";
        char b[40];
        int i, p = 0;
        for (i = 0; i < 32; i++) {
            int v = rand() & 15;
            if (i == 12) v = 4;
            if (i == 16) v = 8 | (v & 3);
            if (i == 8 || i == 12 || i == 16 || i == 20) b[p++] = '-';
            b[p++] = hx[v];
        }
        return fn_alnum((unsigned char *)b, p);
    }
    default:
        break;
    }
    cob_fatal("intrinsic function not implemented");
    return 0;
}
