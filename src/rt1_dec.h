/* rt1_dec.h -- decimal arithmetic engine (exact, no 64-bit types)
   A CobDec holds sign, scale and up to CD_MAX decimal digits, least
   significant digit first: value = +-sum(d[i] * 10**(i - scale)). */


typedef struct {
    int neg;
    int n;                      /* number of digits, no leading 0s */
    int scale;
    unsigned char d[CD_MAX];
} CobDec;

static void cob_fatal(const char *m);
static int cd_ovf = 0;          /* set when a result lost digits */

static void cd_zero(CobDec *r)
{
    r->neg = 0;
    r->n = 0;
    r->scale = 0;
}

static void cd_norm(CobDec *r)
{
    while (r->n > 0 && r->d[r->n - 1] == 0) r->n--;
    if (r->n == 0) r->neg = 0;
}

/* make the number fit: drop low fraction digits if it is too long */
static void cd_fit(CobDec *r)
{
    int k;
    if (r->n <= CD_MAX) return;
    cd_ovf = 1;
    k = r->n - CD_MAX;
    if (k > r->scale) k = r->scale;
    if (k < 0) k = 0;
    memmove(r->d, r->d + k, (size_t)(r->n - k));
    r->n -= k;
    r->scale -= k;
    if (r->n > CD_MAX) r->n = CD_MAX;
}

static void cd_from_long(CobDec *r, long v)
{
    unsigned long u;
    cd_zero(r);
    if (v < 0) {
        r->neg = 1;
        u = (unsigned long)(-(v + 1)) + 1UL;
    } else {
        u = (unsigned long)v;
    }
    while (u) {
        r->d[r->n++] = (unsigned char)(u % 10UL);
        u /= 10UL;
    }
    if (r->n == 0) r->neg = 0;
}

/* from a string of digits with optional sign and decimal point */
static void cd_from_str(CobDec *r, const char *s, long len)
{
    long i;
    int seen = 0, fr = 0, neg = 0, k = 0;
    unsigned char t[CD_MAX * 2];
    cd_zero(r);
    for (i = 0; i < len; i++) {
        int c = (unsigned char)s[i];
        if (c == '-') neg = 1;
        else if (c == '.') seen = 1;
        else if (c >= '0' && c <= '9') {
            if (k < CD_MAX * 2) t[k++] = (unsigned char)(c - '0');
            if (seen) fr++;
        }
    }
    /* t is most significant first */
    for (i = 0; i < k; i++) {
        if (k - 1 - i < CD_MAX) r->d[k - 1 - i] = t[i];
    }
    r->n = k > CD_MAX ? CD_MAX : k;
    r->scale = fr;
    r->neg = neg;
    cd_norm(r);
}

static void cd_shift(CobDec *r, int k)      /* multiply by 10**k */
{
    int i;
    if (k <= 0 || r->n == 0) {
        if (k > 0) r->scale += k;
        return;
    }
    if (r->n + k > CD_MAX) {
        cd_ovf = 1;
        k = CD_MAX - r->n;
        if (k < 0) k = 0;
    }
    for (i = r->n - 1; i >= 0; i--) r->d[i + k] = r->d[i];
    for (i = 0; i < k; i++) r->d[i] = 0;
    r->n += k;
}

/* give both operands the same scale */
static void cd_align(CobDec *a, CobDec *b)
{
    int k;
    if (a->scale < b->scale) {
        k = b->scale - a->scale;
        cd_shift(a, k);
        a->scale = b->scale;
    } else if (b->scale < a->scale) {
        k = a->scale - b->scale;
        cd_shift(b, k);
        b->scale = a->scale;
    }
}

/* compare magnitudes; same scale */
static int cd_cmpabs(const CobDec *a, const CobDec *b)
{
    int i, n = a->n > b->n ? a->n : b->n;
    for (i = n - 1; i >= 0; i--) {
        int x = i < a->n ? a->d[i] : 0, y = i < b->n ? b->d[i] : 0;
        if (x != y) return x < y ? -1 : 1;
    }
    return 0;
}

static int cd_cmp(const CobDec *a0, const CobDec *b0)
{
    CobDec a = *a0, b = *b0;
    int c;
    if (a.n == 0 && b.n == 0) return 0;
    if (a.neg != b.neg) return a.neg ? -1 : 1;
    cd_align(&a, &b);
    c = cd_cmpabs(&a, &b);
    return a.neg ? -c : c;
}

static void cd_addabs(CobDec *r, const CobDec *a, const CobDec *b)
{
    int i, c = 0, n = a->n > b->n ? a->n : b->n, s;
    CobDec t;
    t.n = 0;
    for (i = 0; i < n || c; i++) {
        s = (i < a->n ? a->d[i] : 0) + (i < b->n ? b->d[i] : 0) + c;
        c = s >= 10;
        if (i < CD_MAX) t.d[i] = (unsigned char)(s % 10);
        else cd_ovf = 1;
        t.n = i + 1 < CD_MAX ? i + 1 : CD_MAX;
    }
    t.scale = a->scale;
    t.neg = 0;
    *r = t;
}

/* |a| - |b| with |a| >= |b| */
static void cd_subabs(CobDec *r, const CobDec *a, const CobDec *b)
{
    int i, br = 0, s;
    CobDec t;
    t.n = a->n;
    for (i = 0; i < a->n; i++) {
        s = a->d[i] - (i < b->n ? b->d[i] : 0) - br;
        br = s < 0;
        if (br) s += 10;
        t.d[i] = (unsigned char)s;
    }
    t.scale = a->scale;
    t.neg = 0;
    *r = t;
}

static void cd_add(CobDec *r, const CobDec *a0, const CobDec *b0)
{
    CobDec a = *a0, b = *b0, t;
    int neg;
    cd_align(&a, &b);
    if (a.neg == b.neg) {
        cd_addabs(&t, &a, &b);
        t.neg = a.neg;
    } else if (cd_cmpabs(&a, &b) >= 0) {
        cd_subabs(&t, &a, &b);
        t.neg = a.neg;
    } else {
        cd_subabs(&t, &b, &a);
        t.neg = b.neg;
    }
    neg = t.neg;
    cd_norm(&t);
    if (t.n == 0) neg = 0;
    t.neg = neg;
    *r = t;
}

static void cd_sub(CobDec *r, const CobDec *a, const CobDec *b)
{
    CobDec nb = *b;
    if (nb.n) nb.neg = !nb.neg;
    cd_add(r, a, &nb);
}

static void cd_neg(CobDec *r)
{
    if (r->n) r->neg = !r->neg;
}

static void cd_mul(CobDec *r, const CobDec *a, const CobDec *b)
{
    unsigned long acc[CD_MAX * 2 + 2];
    CobDec t;
    int i, j, n;
    unsigned long c;
    if (a->n == 0 || b->n == 0) {
        t.n = 0;
        t.neg = 0;
        t.scale = a->scale + b->scale;
        *r = t;
        return;
    }
    n = a->n + b->n;
    for (i = 0; i < n + 1; i++) acc[i] = 0;
    for (i = 0; i < a->n; i++)
        for (j = 0; j < b->n; j++)
            acc[i + j] += (unsigned long)(a->d[i] * b->d[j]);
    c = 0;
    for (i = 0; i < n; i++) {
        acc[i] += c;
        c = acc[i] / 10UL;
        acc[i] %= 10UL;
    }
    t.neg = a->neg != b->neg;
    t.scale = a->scale + b->scale;
    t.n = n;
    if (n > CD_MAX) {
        int k = n - CD_MAX;
        cd_ovf = 1;
        if (k > t.scale) k = t.scale;
        for (i = 0; i < n - k && i < CD_MAX; i++)
            t.d[i] = (unsigned char)acc[i + k];
        t.scale -= k;
        t.n = n - k > CD_MAX ? CD_MAX : n - k;
    } else {
        for (i = 0; i < n; i++) t.d[i] = (unsigned char)acc[i];
    }
    cd_norm(&t);
    *r = t;
}

/* a / b with s fraction digits, truncated; 0 if b is zero (ok = 0) */
static int cd_div(CobDec *r, const CobDec *a, const CobDec *b, int s)
{
    CobDec q, rem, den, t;
    unsigned char num[CD_MAX * 2 + 8];
    int nn, i, k, sh;
    if (b->n == 0) {
        cd_zero(r);
        return 0;
    }
    if (a->n == 0) {
        cd_zero(r);
        r->scale = s;
        return 1;
    }
    /* integers: A * 10**(s + b.scale - a.scale) / B */
    sh = s + b->scale - a->scale;
    nn = 0;
    if (sh >= 0) {
        for (i = 0; i < sh && nn < CD_MAX * 2; i++) num[nn++] = 0;
        for (i = 0; i < a->n && nn < CD_MAX * 2 + 4; i++)
            num[nn++] = a->d[i];
    } else {
        for (i = -sh; i < a->n; i++) num[nn++] = a->d[i];
    }
    den = *b;
    den.scale = 0;
    den.neg = 0;
    cd_zero(&q);
    cd_zero(&rem);
    q.n = nn;
    for (i = nn - 1; i >= 0; i--) {
        cd_shift(&rem, 1);
        rem.scale = 0;
        if (rem.n == 0) rem.n = 1;
        rem.d[0] = num[i];
        cd_norm(&rem);
        k = 0;
        while (cd_cmpabs(&rem, &den) >= 0) {
            cd_subabs(&t, &rem, &den);
            rem = t;
            cd_norm(&rem);
            k++;
        }
        q.d[i] = (unsigned char)k;
    }
    q.neg = a->neg != b->neg;
    q.scale = s;
    if (q.n > CD_MAX) {
        cd_ovf = 1;
        q.n = CD_MAX;
    }
    cd_norm(&q);
    *r = q;
    return 1;
}

/* rounding modes */

/* set the scale to s, rounding the discarded digits by mode */
static void cd_setscale(CobDec *r, int s, int mode)
{
    int k, i, up = 0, rest = 0, half = 0;
    if (r->scale <= s) {
        if (r->n) cd_shift(r, s - r->scale);
        r->scale = s;
        return;
    }
    k = r->scale - s;
    if (r->n == 0) {
        r->scale = s;
        return;
    }
    if (k > r->n + 1) k = r->n + 1;
    {
        int d = k - 1 < r->n ? r->d[k - 1] : 0;
        for (i = 0; i < k - 1 && i < r->n; i++)
            if (r->d[i]) rest = 1;
        half = d == 5 && !rest;
        switch (mode) {
        case CR_AWAY: up = d >= 5; break;
        case CR_EVEN: {
            int low = k < r->n ? r->d[k] : 0;
            up = d > 5 || (d == 5 && (rest || (low & 1)));
            break;
        }
        case CR_UP: up = d > 0 || rest; break;
        case CR_NEARTOZERO: up = d > 5 || (d == 5 && rest); break;
        case CR_HALFUP: up = d > 5 || (d == 5 && (rest || !r->neg));
            break;
        case CR_HALFDOWN: up = d > 5 || (d == 5 && (rest || r->neg));
            break;
        default: up = 0; break;
        }
        (void)half;
    }
    if (k >= r->n) r->n = 0;
    else {
        memmove(r->d, r->d + k, (size_t)(r->n - k));
        r->n -= k;
    }
    r->scale = s;
    if (up) {
        CobDec one, t;
        cd_zero(&one);
        one.d[0] = 1;
        one.n = 1;
        one.scale = s;
        one.neg = r->neg;
        if (r->n == 0) r->scale = s;
        cd_add(&t, r, &one);
        *r = t;
    }
    cd_norm(r);
}

static void cd_trunc(CobDec *r, int s)
{
    cd_setscale(r, s, CR_NONE);
}

static double cd_to_double(const CobDec *a)
{
    double v = 0.0, p = 1.0;
    int i;
    for (i = 0; i < a->n; i++) {
        v += a->d[i] * p;
        p *= 10.0;
    }
    for (i = 0; i < a->scale; i++) v /= 10.0;
    return a->neg ? -v : v;
}

static void cd_from_double(CobDec *r, double v)
{
    char b[64];
    if (v != v || v > 1e60 || v < -1e60) {
        cd_zero(r);
        cd_ovf = 1;
        return;
    }
    sprintf(b, "%.30f", v);
    cd_from_str(r, b, (long)strlen(b));
}

/* integer value in a long, saturated; sets *ok = 0 when it does not
   fit */
static long cd_to_long(const CobDec *a0, int *ok)
{
    CobDec a = *a0;
    unsigned long u = 0;
    int i;
    cd_trunc(&a, 0);
    *ok = 1;
    for (i = a.n - 1; i >= 0; i--) {
        if (u > 214748364UL) {
            *ok = 0;
            return a.neg ? -2147483647L : 2147483647L;
        }
        u = u * 10UL + a.d[i];
    }
    if (u > 2147483647UL) *ok = 0;
    return a.neg ? -(long)u : (long)u;
}

/* integer power; negative or large exponents are done in double */
static void cd_pow(CobDec *r, const CobDec *a, const CobDec *e)
{
    int ok;
    long n = cd_to_long(e, &ok);
    CobDec t, one, ie;
    ie = *e;
    cd_trunc(&ie, 0);
    if (cd_cmp(&ie, e) != 0) ok = 0;
    if (ok && n >= 0 && n <= 200) {
        cd_from_long(&one, 1);
        t = one;
        while (n-- > 0) {
            cd_mul(&t, &t, a);
            if (t.n > 60 && t.scale > 20) cd_trunc(&t, 20);
        }
        *r = t;
        return;
    }
    if (a->n == 0) {
        cd_zero(r);
        return;
    }
    cd_from_double(r, pow(cd_to_double(a), cd_to_double(e)));
}

/* a mod b (result has the sign of b); b <> 0 */
static void cd_mod(CobDec *r, const CobDec *a, const CobDec *b)
{
    CobDec q, p, t;
    if (b->n == 0) {
        cd_zero(r);
        return;
    }
    cd_div(&q, a, b, 0);
    /* floor for negative quotient */
    cd_mul(&p, &q, b);
    cd_sub(&t, a, &p);
    if (t.n && (t.neg != b->neg)) cd_add(&t, &t, b);
    *r = t;
}
