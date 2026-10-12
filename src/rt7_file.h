/* rt7_file.h -- file handling: sequential, line sequential,
   relative and indexed organizations */

#define FK_MAX 256

typedef struct CobFile {
    const char *name;           /* ASSIGN name or SELECT name */
    int org;                    /* 0 seq 1 line 2 relative 3 indexed */
    int access;                 /* 0 seq 1 random 2 dynamic */
    int optional, rectype;
    long minrec, maxrec;
    unsigned char *rec;         /* record area */
    long recsz;                 /* size of the record area */
    CobFld *status, *depend, *relkey, *dynname, *key;
    CobFld *akey[8];
    int adup[8];
    int nak, dupkey;
    /* run-time state */
    FILE *fp;
    int mode, isopen, st, absent, lastop;
    long rn, lastpos, nrec, rs;
    int neednl;
    long extra;
    long koff[9], klen[9];
    unsigned char **dat;
    long *seq;
    long *ord[9];
    long n, cap, nseq, nlive;
    int curk, sp_valid, have;
    long sp_pos;
    unsigned char ckey[FK_MAX];
    long cseq, wseq;
    unsigned char lkey[FK_MAX];
    int haslkey;
    char path[300];
} CobFile;

#define CF_IN 0
#define CF_OUT 1
#define CF_IO 2
#define CF_EXT 3

static CobFile *cob_files[64];
static int cob_nfiles = 0;

static void cob_fstat(CobFile *f, int st)
{
    f->st = st;
    if (f->status && f->status->size >= 2) {
        f->status->data[0] = (unsigned char)('0' + st / 10);
        f->status->data[1] = (unsigned char)('0' + st % 10);
    }
}

static long cob_frs(CobFile *f)
{
    if (f->maxrec > 0) return f->maxrec;
    return f->recsz;
}

static void cob_fpath(CobFile *f)
{
    char nm[300];
    const char *e;
    long i, n = 0;
    nm[0] = 0;
    if (f->dynname) {
        n = f->dynname->size;
        if (n > 290) n = 290;
        memcpy(nm, f->dynname->data, (size_t)n);
        while (n > 0 && (nm[n - 1] == ' ' || nm[n - 1] == 0)) n--;
        nm[n] = 0;
    } else {
        strncpy(nm, f->name, 290);
        nm[290] = 0;
    }
    f->path[0] = 0;
    if (!strchr(nm, '/') && !strchr(nm, '\\') && nm[0]) {
        char v[320];
        sprintf(v, "DD_%s", nm);
        e = getenv(v);
        if (!e) { sprintf(v, "dd_%s", nm); e = getenv(v); }
        if (!e) e = getenv(nm);
        if (e && *e) { strncpy(nm, e, 290); nm[290] = 0; }
    }
    for (i = 0; nm[i] && i < 299; i++) f->path[i] = nm[i];
    f->path[i] = 0;
}

/* ---- indexed file index handling ---- */

static int cob_ixcmp(CobFile *f, int k, long a, long b)
{
    int c = memcmp(f->dat[a] + f->koff[k], f->dat[b] + f->koff[k],
                   (size_t)f->klen[k]);
    if (c) return c;
    return f->seq[a] < f->seq[b] ? -1 : f->seq[a] > f->seq[b] ? 1 : 0;
}

/* compare key bytes with entry e, ignoring the sequence number */
static int cob_ixkc(CobFile *f, int k, const unsigned char *kb, long e)
{
    return memcmp(kb, f->dat[e] + f->koff[k], (size_t)f->klen[k]);
}

/* first position in ord[k] whose key is >= kb (or > kb when gt) */
static long cob_ixfind(CobFile *f, int k, const unsigned char *kb,
                       int gt)
{
    long lo = 0, hi = f->nlive;
    while (lo < hi) {
        long mid = (lo + hi) / 2;
        int c = cob_ixkc(f, k, kb, f->ord[k][mid]);
        if (c > 0 || (gt && c == 0)) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

static void cob_ixsetup(CobFile *f)
{
    int k;
    f->koff[0] = f->key ? (long)(f->key->data - f->rec) : 0;
    f->klen[0] = f->key ? f->key->size : 1;
    for (k = 0; k < f->nak; k++) {
        f->koff[k + 1] = (long)(f->akey[k]->data - f->rec);
        f->klen[k + 1] = f->akey[k]->size;
    }
}

static void cob_ixgrow(CobFile *f)
{
    int k;
    if (f->n < f->cap) return;
    f->cap = f->cap ? f->cap * 2 : 64;
    f->dat = (unsigned char **)realloc(f->dat,
                         (size_t)f->cap * sizeof(unsigned char *));
    f->seq = (long *)realloc(f->seq, (size_t)f->cap * sizeof(long));
    for (k = 0; k <= f->nak; k++)
        f->ord[k] = (long *)realloc(f->ord[k],
                                    (size_t)f->cap * sizeof(long));
    if (!f->dat || !f->seq) cob_fatal("out of memory");
}

static void cob_ixins(CobFile *f, int k, long e)
{
    long lo = 0, hi = f->nlive;
    while (lo < hi) {
        long mid = (lo + hi) / 2;
        if (cob_ixcmp(f, k, f->ord[k][mid], e) < 0) lo = mid + 1;
        else hi = mid;
    }
    memmove(f->ord[k] + lo + 1, f->ord[k] + lo,
            (size_t)(f->nlive - lo) * sizeof(long));
    f->ord[k][lo] = e;
}

static void cob_ixdel(CobFile *f, int k, long e)
{
    long i;
    for (i = 0; i < f->nlive; i++)
        if (f->ord[k][i] == e) {
            memmove(f->ord[k] + i, f->ord[k] + i + 1,
                    (size_t)(f->nlive - i - 1) * sizeof(long));
            return;
        }
}

/* add a record to the in-memory store; returns entry or -1 */
static long cob_ixadd(CobFile *f, const unsigned char *d)
{
    long e;
    int k;
    cob_ixgrow(f);
    e = f->n;
    f->dat[e] = (unsigned char *)malloc((size_t)f->rs + 1);
    if (!f->dat[e]) cob_fatal("out of memory");
    memcpy(f->dat[e], d, (size_t)f->rs);
    f->seq[e] = ++f->nseq;
    for (k = 0; k <= f->nak; k++) cob_ixins(f, k, e);
    f->n++;
    f->nlive++;
    return e;
}

static void cob_ixremove(CobFile *f, long e)
{
    int k;
    for (k = 0; k <= f->nak; k++) cob_ixdel(f, k, e);
    f->nlive--;
    free(f->dat[e]);
    f->dat[e] = 0;
}

static void cob_ixfree(CobFile *f)
{
    long i;
    int k;
    for (i = 0; i < f->n; i++) if (f->dat[i]) free(f->dat[i]);
    free(f->dat); free(f->seq);
    for (k = 0; k < 9; k++) { free(f->ord[k]); f->ord[k] = 0; }
    f->dat = 0; f->seq = 0;
    f->n = f->cap = f->nlive = f->nseq = 0;
}

/* locate an entry with the given primary key; -1 if none */
static long cob_ixlook(CobFile *f, int k, const unsigned char *kb)
{
    long p = cob_ixfind(f, k, kb, 0);
    if (p < f->nlive && cob_ixkc(f, k, kb, f->ord[k][p]) == 0)
        return f->ord[k][p];
    return -1;
}

static void cob_ixflush(CobFile *f)
{
    long i;
    FILE *o = fopen(f->path, "wb");
    if (!o) return;
    for (i = 0; i < f->nlive; i++)
        fwrite(f->dat[f->ord[0][i]], 1, (size_t)f->rs, o);
    fclose(o);
}

/* ---- open and close ---- */

static int cob_open(CobFile *f, int mode)
{
    const char *m;
    FILE *t;
    int i;
    if (f->isopen) { cob_fstat(f, 41); return 41; }
    for (i = 0; i < cob_nfiles; i++) if (cob_files[i] == f) break;
    if (i == cob_nfiles && cob_nfiles < 64)
        cob_files[cob_nfiles++] = f;
    cob_fpath(f);
    f->rs = cob_frs(f);
    f->mode = mode;
    f->absent = 0;
    f->rn = 0;
    f->have = 0;
    f->sp_valid = 0;
    f->curk = 0;
    f->neednl = 0;
    f->extra = 0;
    f->haslkey = 0;
    f->lastop = 0;
    if (f->org == 3) cob_ixsetup(f);
    t = fopen(f->path, "rb");
    if (t) fclose(t);
    if (!t && mode != CF_OUT) {
        if (mode == CF_IN && f->optional) {
            f->absent = 1; f->isopen = 1;
            cob_fstat(f, 5);
            return 5;
        }
        if ((mode == CF_IO || mode == CF_EXT) && f->optional) {
            FILE *c = fopen(f->path, "wb");
            if (!c) { cob_fstat(f, 37); return 37; }
            fclose(c);
            if (f->org != 3) mode = f->mode = CF_IO;
        } else {
            cob_fstat(f, 35);
            return 35;
        }
    }
    if (f->org == 3) {
        f->fp = 0;
        if (mode != CF_OUT && !f->absent) {
            FILE *r = fopen(f->path, "rb");
            unsigned char *b = (unsigned char *)malloc(
                                       (size_t)f->rs + 1);
            if (!r || !b) { cob_fstat(f, 37); return 37; }
            while (fread(b, 1, (size_t)f->rs, r) == (size_t)f->rs)
                cob_ixadd(f, b);
            free(b);
            fclose(r);
        } else if (mode == CF_OUT) {
            FILE *c = fopen(f->path, "wb");
            if (!c) { cob_fstat(f, 37); return 37; }
            fclose(c);
        }
        f->isopen = 1;
        cob_fstat(f, 0);
        return 0;
    }
    if (f->absent) { f->fp = 0; f->isopen = 1; return 5; }
    switch (mode) {
    case CF_IN: m = "rb"; break;
    case CF_OUT: m = f->org == 2 ? "w+b" : "wb"; break;
    case CF_IO: m = "r+b"; break;
    default: m = f->org == 2 ? "r+b" : "ab"; break;
    }
    f->fp = fopen(f->path, m);
    if (!f->fp) {
        int st = 37;
        cob_fstat(f, st);
        return st;
    }
    if (mode == CF_EXT && f->org == 2) fseek(f->fp, 0L, SEEK_END);
    f->isopen = 1;
    cob_fstat(f, 0);
    return 0;
}

static int cob_close(CobFile *f)
{
    if (!f->isopen) { cob_fstat(f, 42); return 42; }
    if (f->org == 3) {
        if (f->mode != CF_IN && !f->absent) cob_ixflush(f);
        cob_ixfree(f);
    } else if (f->fp) {
        if (f->org == 1 && f->neednl && f->mode != CF_IN) {
            long x;
            fputc('\n', f->fp);
            for (x = 0; x < f->extra; x++) fputc('\n', f->fp);
        }
        fclose(f->fp);
    }
    f->fp = 0;
    f->isopen = 0;
    cob_fstat(f, 0);
    return 0;
}

static void cob_close_all(void)
{
    int i;
    for (i = 0; i < cob_nfiles; i++)
        if (cob_files[i]->isopen) cob_close(cob_files[i]);
}

/* ---- record helpers ---- */

static void cob_setlen(CobFile *f, long n)
{
    if (f->depend) cob_seti(f->depend, n);
}

static void cob_relset(CobFile *f, long n)
{
    f->rn = n;
    if (f->relkey) cob_seti(f->relkey, n);
}

/* ---- READ ---- */

/* how: 0 next, 1 by key, 2 previous; kn = key number */
static int cob_read(CobFile *f, int how, int kn)
{
    long rs = f->rs;
    int c;
    if (!f->isopen) { cob_fstat(f, 42); return 42; }
    if (f->mode != CF_IN && f->mode != CF_IO) {
        cob_fstat(f, 47); return 47;
    }
    f->lastop = 1;
    if (f->absent) { cob_fstat(f, how == 1 ? 23 : 10); return f->st; }
    if (f->org == 3) {
        long p, e = -1;
        int k = how == 1 ? kn : f->curk;
        if (how == 1) {
            unsigned char kb[FK_MAX];
            memcpy(kb, f->rec + f->koff[k], (size_t)f->klen[k]);
            p = cob_ixfind(f, k, kb, 0);
            if (p >= f->nlive ||
                cob_ixkc(f, k, kb, f->ord[k][p]) != 0) {
                cob_fstat(f, 23); return 23;
            }
            e = f->ord[k][p];
            f->curk = k;
        } else if (how == 0) {
            if (f->sp_valid) {
                p = f->sp_pos;
                f->sp_valid = 0;
            } else if (!f->have) {
                p = 0;
            } else {
                unsigned char kb[FK_MAX];
                long lo = 0, hi = f->nlive;
                memcpy(kb, f->ckey, (size_t)f->klen[k]);
                while (lo < hi) {
                    long mid = (lo + hi) / 2;
                    long en = f->ord[k][mid];
                    int cc = cob_ixkc(f, k, kb, en);
                    if (cc > 0 || (cc == 0 && f->cseq >= f->seq[en]))
                        lo = mid + 1;
                    else hi = mid;
                }
                p = lo;
            }
            if (p >= f->nlive) { cob_fstat(f, 10); return 10; }
            e = f->ord[k][p];
        } else {
            long lo = 0, hi = f->nlive;
            if (f->sp_valid) { lo = f->sp_pos; f->sp_valid = 0; }
            else if (!f->have) lo = f->nlive;
            else {
                unsigned char kb[FK_MAX];
                memcpy(kb, f->ckey, (size_t)f->klen[k]);
                while (lo < hi) {
                    long mid = (lo + hi) / 2;
                    long en = f->ord[k][mid];
                    int cc = cob_ixkc(f, k, kb, en);
                    if (cc > 0 || (cc == 0 && f->cseq > f->seq[en]))
                        lo = mid + 1;
                    else hi = mid;
                }
            }
            p = lo - 1;
            if (p < 0) { cob_fstat(f, 10); return 10; }
            e = f->ord[k][p];
        }
        memcpy(f->rec, f->dat[e], (size_t)rs);
        if (rs < f->recsz) memset(f->rec + rs, ' ',
                                  (size_t)(f->recsz - rs));
        f->have = 1;
        memcpy(f->ckey, f->dat[e] + f->koff[f->curk],
               (size_t)f->klen[f->curk]);
        f->cseq = f->seq[e];
        cob_fstat(f, 0);
        return 0;
    }
    if (f->org == 2) {
        long n;
        unsigned char flag;
        if (how == 1) {
            n = f->relkey ? cob_toi(f->relkey) : f->rn;
            if (n < 1) { cob_fstat(f, 23); return 23; }
            fseek(f->fp, (n - 1) * (rs + 1), SEEK_SET);
            if (fread(&flag, 1, 1, f->fp) != 1 || flag != 1) {
                cob_fstat(f, 23); return 23;
            }
        } else {
            n = f->sp_valid ? f->sp_pos : f->rn + (how == 2 ? -1 : 1);
            f->sp_valid = 0;
            for (;;) {
                if (n < 1) { cob_fstat(f, 10); return 10; }
                fseek(f->fp, (n - 1) * (rs + 1), SEEK_SET);
                if (fread(&flag, 1, 1, f->fp) != 1) {
                    cob_fstat(f, 10); return 10;
                }
                if (flag == 1) break;
                n += how == 2 ? -1 : 1;
            }
            cob_relset(f, n);
        }
        f->rn = n;
        if (fread(f->rec, 1, (size_t)rs, f->fp) != (size_t)rs) {
            cob_fstat(f, 30); return 30;
        }
        if (rs < f->recsz) memset(f->rec + rs, ' ',
                                  (size_t)(f->recsz - rs));
        f->have = 1;
        cob_fstat(f, 0);
        return 0;
    }
    /* sequential and line sequential */
    f->lastpos = ftell(f->fp);
    if (f->org == 1) {
        long n = 0;
        int got = 0;
        while ((c = fgetc(f->fp)) != EOF) {
            got = 1;
            if (c == '\n') break;
            if (c == '\r') continue;
            if (n < rs) f->rec[n++] = (unsigned char)c;
        }
        if (!got) { cob_fstat(f, 10); return 10; }
        if (n < f->recsz) memset(f->rec + n, ' ',
                                 (size_t)(f->recsz - n));
        cob_setlen(f, n);
        f->have = 1;
        cob_fstat(f, 0);
        return 0;
    } else {
        long n = rs;
        if (f->rectype) {
            unsigned char h[4];
            if (fread(h, 1, 4, f->fp) != 4) {
                cob_fstat(f, 10); return 10;
            }
            n = ((long)h[0] << 24) | ((long)h[1] << 16) |
                ((long)h[2] << 8) | h[3];
            if (n > rs) { cob_fstat(f, 34); return 34; }
        }
        {
            long g = (long)fread(f->rec, 1, (size_t)n, f->fp);
            if (g == 0) { cob_fstat(f, 10); return 10; }
            if (g < n) { cob_fstat(f, 4); n = g; }
            if (n < f->recsz) memset(f->rec + n, ' ',
                                     (size_t)(f->recsz - n));
            cob_setlen(f, n);
            f->have = 1;
            if (g < n) return 4;
        }
        cob_fstat(f, 0);
        return 0;
    }
}

/* ---- START ---- */

/* op: 0 =, 1 >, 2 >=, 3 <, 4 <= */
static int cob_start(CobFile *f, int op, int kn)
{
    if (!f->isopen) { cob_fstat(f, 42); return 42; }
    if (f->mode != CF_IN && f->mode != CF_IO) {
        cob_fstat(f, 47); return 47;
    }
    if (f->absent) { cob_fstat(f, 23); return 23; }
    if (f->org == 2) {
        long n = f->relkey ? cob_toi(f->relkey) : 1;
        long rs = f->rs;
        unsigned char flag;
        int dir = (op == 3 || op == 4) ? -1 : 1;
        if (op == 1) n++;
        else if (op == 3) n--;
        for (;;) {
            if (n < 1) { cob_fstat(f, 23); return 23; }
            fseek(f->fp, (n - 1) * (rs + 1), SEEK_SET);
            if (fread(&flag, 1, 1, f->fp) != 1) {
                cob_fstat(f, 23); return 23;
            }
            if (flag == 1) break;
            if (op == 0) { cob_fstat(f, 23); return 23; }
            n += dir;
        }
        f->sp_valid = 1;
        f->sp_pos = n;
        cob_fstat(f, 0);
        return 0;
    }
    if (f->org != 3) { cob_fstat(f, 47); return 47; }
    {
        unsigned char kb[FK_MAX];
        long p;
        int k = kn;
        memcpy(kb, f->rec + f->koff[k], (size_t)f->klen[k]);
        switch (op) {
        case 0:
            p = cob_ixfind(f, k, kb, 0);
            if (p >= f->nlive ||
                cob_ixkc(f, k, kb, f->ord[k][p]) != 0) p = -1;
            break;
        case 1: p = cob_ixfind(f, k, kb, 1); break;
        case 2: p = cob_ixfind(f, k, kb, 0); break;
        case 3: p = cob_ixfind(f, k, kb, 0) - 1; break;
        default: p = cob_ixfind(f, k, kb, 1) - 1; break;
        }
        if (p < 0 || p >= f->nlive) { cob_fstat(f, 23); return 23; }
        f->curk = k;
        f->sp_valid = 1;
        f->sp_pos = p;
        f->have = 0;
        cob_fstat(f, 0);
        return 0;
    }
}

/* ---- WRITE ---- */

/* len: size of the record named; adv: 0 none 1 BEFORE 2 AFTER;
   lines: line count, -1 for PAGE */
static int cob_write(CobFile *f, long len, int adv, long lines)
{
    long rs = f->rs;
    if (!f->isopen) { cob_fstat(f, 42); return 42; }
    if (f->mode == CF_IN || (f->mode == CF_IO && f->org <= 1)) {
        cob_fstat(f, f->mode == CF_IN ? 48 : 48); return 48;
    }
    f->lastop = 2;
    if (f->org == 3) {
        unsigned char kb[FK_MAX];
        long p;
        int k;
        memcpy(kb, f->rec + f->koff[0], (size_t)f->klen[0]);
        p = cob_ixfind(f, 0, kb, 0);
        if (f->access == 0 && f->mode == CF_OUT && f->nlive > 0 &&
            cob_ixkc(f, 0, kb, f->ord[0][f->nlive - 1]) < 0) {
            cob_fstat(f, 21); return 21;
        }
        if (p < f->nlive && cob_ixkc(f, 0, kb, f->ord[0][p]) == 0) {
            cob_fstat(f, 22); return 22;
        }
        for (k = 0; k < f->nak; k++) {
            long q;
            if (f->adup[k]) continue;
            memcpy(kb, f->rec + f->koff[k + 1],
                   (size_t)f->klen[k + 1]);
            q = cob_ixfind(f, k + 1, kb, 0);
            if (q < f->nlive &&
                cob_ixkc(f, k + 1, kb, f->ord[k + 1][q]) == 0) {
                cob_fstat(f, 22); return 22;
            }
        }
        cob_ixadd(f, f->rec);
        cob_fstat(f, 0);
        for (k = 0; k < f->nak; k++)
            if (f->adup[k]) {
                long q;
                memcpy(kb, f->rec + f->koff[k + 1],
                       (size_t)f->klen[k + 1]);
                q = cob_ixfind(f, k + 1, kb, 0);
                if (q + 1 < f->nlive &&
                    cob_ixkc(f, k + 1, kb,
                             f->ord[k + 1][q + 1]) == 0)
                    cob_fstat(f, 2);
            }
        return f->st;
    }
    if (f->org == 2) {
        long n;
        unsigned char flag = 1;
        if (f->access == 0) n = f->rn + 1;
        else n = f->relkey ? cob_toi(f->relkey) : f->rn + 1;
        if (n < 1) { cob_fstat(f, 24); return 24; }
        fseek(f->fp, (n - 1) * (rs + 1), SEEK_SET);
        {
            unsigned char old;
            if (fread(&old, 1, 1, f->fp) == 1 && old == 1) {
                cob_fstat(f, 22); return 22;
            }
        }
        fseek(f->fp, (n - 1) * (rs + 1), SEEK_SET);
        fwrite(&flag, 1, 1, f->fp);
        fwrite(f->rec, 1, (size_t)rs, f->fp);
        fflush(f->fp);
        cob_relset(f, n);
        cob_fstat(f, 0);
        return 0;
    }
    if (f->org == 1) {
        long n = len > 0 && len < f->recsz ? len : f->recsz;
        if (rs < n) n = rs;
        if (f->depend) {
            long d = cob_toi(f->depend);
            if (d >= 0 && d < n) n = d;
        }
        while (n > 0 && f->rec[n - 1] == ' ') n--;
        if (f->neednl) {
            long x;
            fputc('\n', f->fp);
            for (x = 0; x < f->extra; x++) fputc('\n', f->fp);
            f->extra = 0;
        }
        if (adv == 2) {
            long x;
            if (lines < 0) fputc('\f', f->fp);
            else for (x = 1; x < lines; x++) fputc('\n', f->fp);
        }
        fwrite(f->rec, 1, (size_t)n, f->fp);
        f->neednl = 1;
        if (adv == 1) {
            if (lines < 0) { fputc('\n', f->fp); fputc('\f', f->fp);
                f->neednl = 0; }
            else f->extra = lines > 1 ? lines - 1 : 0;
        }
        cob_fstat(f, 0);
        return 0;
    }
    {
        long n = rs;
        if (f->rectype) {
            unsigned char h[4];
            n = len > 0 ? len : rs;
            if (f->depend) n = cob_toi(f->depend);
            if (n > rs) n = rs;
            if (n < 0) n = 0;
            h[0] = (unsigned char)(n >> 24);
            h[1] = (unsigned char)(n >> 16);
            h[2] = (unsigned char)(n >> 8);
            h[3] = (unsigned char)n;
            fwrite(h, 1, 4, f->fp);
        }
        f->lastpos = ftell(f->fp);
        fwrite(f->rec, 1, (size_t)n, f->fp);
        cob_fstat(f, 0);
        return 0;
    }
}

/* ---- REWRITE and DELETE ---- */

static int cob_rewrite(CobFile *f)
{
    long rs = f->rs;
    if (!f->isopen) { cob_fstat(f, 42); return 42; }
    if (f->mode != CF_IO) { cob_fstat(f, 49); return 49; }
    f->lastop = 3;
    if (f->org == 3) {
        unsigned char kb[FK_MAX];
        long e, p;
        int k;
        if (f->access == 0 && !f->have) {
            cob_fstat(f, 43); return 43;
        }
        memcpy(kb, f->rec + f->koff[0], (size_t)f->klen[0]);
        e = cob_ixlook(f, 0, kb);
        if (e < 0) { cob_fstat(f, f->access == 0 ? 21 : 23);
            return f->st; }
        for (k = 0; k < f->nak; k++) {
            if (f->adup[k]) continue;
            memcpy(kb, f->rec + f->koff[k + 1],
                   (size_t)f->klen[k + 1]);
            p = cob_ixfind(f, k + 1, kb, 0);
            if (p < f->nlive &&
                cob_ixkc(f, k + 1, kb, f->ord[k + 1][p]) == 0 &&
                f->ord[k + 1][p] != e) {
                cob_fstat(f, 22); return 22;
            }
        }
        for (k = 1; k <= f->nak; k++) cob_ixdel(f, k, e);
        f->nlive--;
        memcpy(f->dat[e], f->rec, (size_t)rs);
        f->nlive++;
        {
            long sv = f->nlive;
            f->nlive = sv - 1;
            for (k = 1; k <= f->nak; k++) cob_ixins(f, k, e);
            f->nlive = sv;
        }
        cob_fstat(f, 0);
        return 0;
    }
    if (f->org == 2) {
        long n = f->access == 0 ? f->rn :
                 (f->relkey ? cob_toi(f->relkey) : f->rn);
        unsigned char flag;
        if (f->access == 0 && !f->have) {
            cob_fstat(f, 43); return 43;
        }
        if (n < 1) { cob_fstat(f, 23); return 23; }
        fseek(f->fp, (n - 1) * (rs + 1), SEEK_SET);
        if (fread(&flag, 1, 1, f->fp) != 1 || flag != 1) {
            cob_fstat(f, 23); return 23;
        }
        fseek(f->fp, (n - 1) * (rs + 1) + 1, SEEK_SET);
        fwrite(f->rec, 1, (size_t)rs, f->fp);
        fflush(f->fp);
        cob_fstat(f, 0);
        return 0;
    }
    if (f->org == 1 || f->rectype) { cob_fstat(f, 49); return 49; }
    if (!f->have) { cob_fstat(f, 43); return 43; }
    {
        long here = ftell(f->fp);
        fseek(f->fp, f->lastpos, SEEK_SET);
        fwrite(f->rec, 1, (size_t)rs, f->fp);
        fflush(f->fp);
        fseek(f->fp, here, SEEK_SET);
    }
    cob_fstat(f, 0);
    return 0;
}

static int cob_delete(CobFile *f)
{
    if (!f->isopen) { cob_fstat(f, 42); return 42; }
    if (f->mode != CF_IO) { cob_fstat(f, 49); return 49; }
    f->lastop = 4;
    if (f->org == 3) {
        unsigned char kb[FK_MAX];
        long e;
        if (f->access == 0) {
            if (!f->have) { cob_fstat(f, 43); return 43; }
            memcpy(kb, f->ckey, (size_t)f->klen[f->curk]);
            {
                long p = cob_ixfind(f, f->curk, kb, 0);
                e = -1;
                while (p < f->nlive &&
                       cob_ixkc(f, f->curk, kb,
                                f->ord[f->curk][p]) == 0) {
                    if (f->seq[f->ord[f->curk][p]] == f->cseq) {
                        e = f->ord[f->curk][p]; break;
                    }
                    p++;
                }
            }
        } else {
            memcpy(kb, f->rec + f->koff[0], (size_t)f->klen[0]);
            e = cob_ixlook(f, 0, kb);
        }
        if (e < 0) { cob_fstat(f, 23); return 23; }
        cob_ixremove(f, e);
        cob_fstat(f, 0);
        return 0;
    }
    if (f->org == 2) {
        long n = f->access == 0 ? f->rn :
                 (f->relkey ? cob_toi(f->relkey) : f->rn);
        unsigned char flag;
        if (f->access == 0 && !f->have) {
            cob_fstat(f, 43); return 43;
        }
        if (n < 1) { cob_fstat(f, 23); return 23; }
        fseek(f->fp, (n - 1) * (f->rs + 1), SEEK_SET);
        if (fread(&flag, 1, 1, f->fp) != 1 || flag != 1) {
            cob_fstat(f, 23); return 23;
        }
        flag = 0;
        fseek(f->fp, (n - 1) * (f->rs + 1), SEEK_SET);
        fwrite(&flag, 1, 1, f->fp);
        fflush(f->fp);
        cob_fstat(f, 0);
        return 0;
    }
    cob_fstat(f, 49);
    return 49;
}

/* ---- error disposition ---- */

/* an exception that no phrase handled and no declarative took */
static void cob_fnh(CobFile *f, int st)
{
    char b[400];
    if (f->status) return;
    sprintf(b, "file %s (status = %02d)", f->name, st);
    cob_fatal(b);
}


/* ---- SORT and MERGE ---- */

typedef struct CobSort {
    CobFile *sd;
    unsigned char **r;
    long n, cap, pos;
    int nk;
    CobFld *kf[16];
    int kd[16];
} CobSort;

static CobSort cob_srt;

static void cob_sort_begin(CobFile *sd)
{
    memset(&cob_srt, 0, sizeof cob_srt);
    cob_srt.sd = sd;
    sd->rs = sd->recsz;
}

static void cob_sort_key(CobFld *k, int desc)
{
    if (cob_srt.nk < 16) {
        cob_srt.kf[cob_srt.nk] = k;
        cob_srt.kd[cob_srt.nk++] = desc;
    }
}

static void cob_release(CobFile *sd)
{
    CobSort *s = &cob_srt;
    if (s->n >= s->cap) {
        s->cap = s->cap ? s->cap * 2 : 256;
        s->r = (unsigned char **)realloc(s->r,
                         (size_t)s->cap * sizeof(unsigned char *));
        if (!s->r) cob_fatal("out of memory");
    }
    s->r[s->n] = (unsigned char *)malloc((size_t)sd->recsz + 1);
    if (!s->r[s->n]) cob_fatal("out of memory");
    memcpy(s->r[s->n++], sd->rec, (size_t)sd->recsz);
}

static int cob_sortcmp(const unsigned char *a, const unsigned char *b)
{
    CobSort *s = &cob_srt;
    int i;
    for (i = 0; i < s->nk; i++) {
        long off = (long)(s->kf[i]->data - s->sd->rec);
        CobFld x, y;
        int c;
        x = *s->kf[i];
        y = *s->kf[i];
        x.data = (unsigned char *)a + off;
        y.data = (unsigned char *)b + off;
        c = cob_cmp(&x, &y);
        if (c) return s->kd[i] ? -c : c;
    }
    return 0;
}

static void cob_msort(unsigned char **a, unsigned char **t, long n)
{
    long m, i, j, k;
    if (n < 2) return;
    m = n / 2;
    cob_msort(a, t, m);
    cob_msort(a + m, t, n - m);
    i = 0; j = m; k = 0;
    while (i < m && j < n)
        t[k++] = cob_sortcmp(a[j], a[i]) < 0 ? a[j++] : a[i++];
    while (i < m) t[k++] = a[i++];
    while (j < n) t[k++] = a[j++];
    memcpy(a, t, (size_t)n * sizeof(unsigned char *));
}

static void cob_sort_run(void)
{
    CobSort *s = &cob_srt;
    unsigned char **t;
    if (s->n > 1) {
        t = (unsigned char **)malloc((size_t)s->n *
                                     sizeof(unsigned char *));
        if (!t) cob_fatal("out of memory");
        cob_msort(s->r, t, s->n);
        free(t);
    }
    s->pos = 0;
}

static int cob_return(CobFile *sd)
{
    CobSort *s = &cob_srt;
    if (s->pos >= s->n) { cob_fstat(sd, 10); return 10; }
    memcpy(sd->rec, s->r[s->pos++], (size_t)sd->recsz);
    cob_fstat(sd, 0);
    return 0;
}

static void cob_sort_end(void)
{
    CobSort *s = &cob_srt;
    long i;
    for (i = 0; i < s->n; i++) free(s->r[i]);
    free(s->r);
    memset(s, 0, sizeof *s);
}

static int cob_sort_using(CobFile *sd, CobFile *f)
{
    int st = cob_open(f, CF_IN);
    long n;
    if (st >= 10) return st;
    for (;;) {
        st = cob_read(f, 0, 0);
        if (st >= 10) break;
        n = f->recsz < sd->recsz ? f->recsz : sd->recsz;
        memcpy(sd->rec, f->rec, (size_t)n);
        if (n < sd->recsz) memset(sd->rec + n, ' ',
                                  (size_t)(sd->recsz - n));
        cob_release(sd);
    }
    cob_close(f);
    return st == 10 ? 0 : st;
}

static int cob_sort_giving(CobFile *sd, CobFile *f)
{
    CobSort *s = &cob_srt;
    int st = cob_open(f, CF_OUT);
    long n, i;
    if (st >= 10) return st;
    for (i = s->pos; i < s->n; i++) {
        n = f->recsz < sd->recsz ? f->recsz : sd->recsz;
        memcpy(f->rec, s->r[i], (size_t)n);
        if (n < f->recsz) memset(f->rec + n, ' ',
                                 (size_t)(f->recsz - n));
        st = cob_write(f, n, 0, 0);
        if (st >= 10) break;
    }
    cob_close(f);
    return st >= 10 ? st : 0;
}
