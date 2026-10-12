/* c70_main.c -- driver and command line */

#define VERSION "1.0"
#define MAXSRC 64

#ifdef _WIN32
#include <windows.h>
#define EXE_SFX ".exe"
#define NULDEV " >NUL 2>&1"
#define LIBM ""
#define DIRSEP '\\'
#else
#define EXE_SFX ""
#define NULDEV " >/dev/null 2>&1"
#define LIBM " -rdynamic -lm -ldl"
#define DIRSEP '/'
#endif

static char *outname(const char *in)
{
    char *o = xdup(in), *dot, *sl;
#ifdef COBOL_CMS
    dot = strchr(o, ' ');
    if (dot) *dot = 0;
    return cat2(o, " c");
#else
    sl = strrchr(o, '/');
    dot = strrchr(o, '.');
    if (dot && (!sl || dot > sl)) *dot = 0;
    return cat2(o, ".c");
#endif
}

static char *self_dir(const char *argv0)
{
    char *d;
    char *sl;
#ifdef _WIN32
    char buf[520];
    DWORD n = GetModuleFileNameA(0, buf, 512);
    if (n > 0 && n < 512) argv0 = buf;
#endif
    d = xdup(argv0);
    sl = strrchr(d, DIRSEP);
#ifdef _WIN32
    if (!sl) sl = strrchr(d, '/');
#endif
    if (!sl) return xdup(".");
    *sl = 0;
    return d;
}

static int works(const char *cmd)
{
    return system(cat2(cmd, NULDEV)) == 0;
}

static char *pick_cc(const char *want, const char *dir, int *kind)
{
    static const char *names[] = { "gcc", "cc", "clang", "tcc", 0 };
    char *b;
    int i;
    *kind = 0;
    if (!want) want = getenv("COBOL_CC");
    b = cat4("\"", dir, "/tcc/tcc" EXE_SFX, "\"");
    if (want && !strcmp(want, "bundled")) {
        *kind = 1;
        return b;
    }
    if (want) {
        *kind = !strcmp(want, "cl") ? 2 : 0;
        return xdup(want);
    }
    for (i = 0; names[i]; i++)
        if (works(cat2(names[i], " --version"))) return xdup(names[i]);
#ifdef _WIN32
    if (works("cl /? ")) { *kind = 2; return xdup("cl"); }
#endif
    if (file_ok(cat2(dir, "/tcc/tcc" EXE_SFX))) {
        *kind = 1;
        return b;
    }
    return 0;
}

static int build_exe(char **files, int nf, const char *exe,
                     const char *want, const char *argv0, int mode)
{
    char *dir = self_dir(argv0);
    char *cc;
    char *cmd;
    char *inc;
    char *list = xdup("");
    int kind, rc, i;
    cc = pick_cc(want, dir, &kind);
    if (!cc) {
        fprintf(stderr, "cobol: no C compiler found; install gcc, "
                "clang or tcc, or use -cc name\n");
        return 8;
    }
    inc = cat5("-I\"", dir, "\" -I\"", dir, "/src\" -I\"");
    inc = cat5(inc, dir, "/../share/cobol\" -I/usr/share/cobol ",
               "-I/usr/local/share/cobol", "");
    for (i = 0; i < nf; i++)
        list = cat4(list, " \"", files[i], "\"");
    if (kind == 2) {
        cmd = cat5(cc, " /nologo /I\"", dir, "\"",
                   mode == 2 ? " /c" : cat3(" /Fe\"", exe, "\""));
    } else {
        cmd = cat3(cc, " ", inc);
        if (mode == 2) cmd = cat2(cmd, " -c");
        else if (mode == 3)
            cmd = cat4(cmd, " -shared -fPIC -o \"", exe, "\"");
        else cmd = cat4(cmd, " -o \"", exe, "\"");
    }
    cmd = cat3(cmd, list, kind != 2 && mode != 2 ? LIBM : "");
#ifdef _WIN32
    cmd = cat3("\"", cmd, "\"");
#endif
    rc = system(cmd);
    if (rc != 0) {
        fprintf(stderr, "cobol: C compiler failed: %s\n", cmd);
        return 8;
    }
    return 0;
}

static char *defext(char *n)
{
    char *base = strrchr(n, '/');
    char *back = strrchr(n, '\\');
    static const char *ext[] = { ".cob", ".cbl", ".cobol", ".COB",
                                 ".CBL", 0 };
    int i;
    if (back && (!base || back > base)) base = back;
    base = base ? base + 1 : n;
    if (strchr(base, '.') || strchr(base, ' ')) return n;
    for (i = 0; ext[i]; i++) {
        char *t = cat2(n, ext[i]);
        if (file_ok(t)) return t;
    }
    return n;
}

static char *exename(const char *in)
{
    char *o = xdup(in), *dot, *sl;
    sl = strrchr(o, '/');
    dot = strrchr(o, '.');
    if (dot && (!sl || dot > sl)) *dot = 0;
    return cat2(o, EXE_SFX);
}

static void usage(void)
{
    fprintf(stderr, "cobol %s -- COBOL to C89 translator\n", VERSION);
    fprintf(stderr,
        "usage: cobol [options] src.cob ...\n");
    fprintf(stderr, "  -o f      output file (- for stdout)\n");
    fprintf(stderr, "  -c        build the program (needs a C\n");
    fprintf(stderr, "            compiler); -o names the exe\n");
    fprintf(stderr, "  -C        compile to objects, no link\n");
    fprintf(stderr, "  -M        module: no main program\n");
    fprintf(stderr, "  -free     free-format source\n");
    fprintf(stderr, "  -fixed    fixed-format source (default)\n");
    fprintf(stderr, "  -I dir    copybook directory\n");
    fprintf(stderr, "  -Dn[=v]   define n for >>IF\n");
    fprintf(stderr, "  -debug    compile debugging lines (D)\n");
    fprintf(stderr, "  -check    check subscripts at run time\n");
    fprintf(stderr, "  -binsize 1-2-4-8|2-4-8   binary field sizes\n");
    fprintf(stderr, "  -cc c     C compiler, or 'bundled'\n");
    exit(4);
}

static int is_cobol(const char *n)
{
    const char *d = strrchr(n, '.');
    const char *sl = strrchr(n, '/');
    if (!d || (sl && d < sl)) return 1;
    d++;
    return !strcmp(d, "cob") || !strcmp(d, "cbl") ||
           !strcmp(d, "cobol") || !strcmp(d, "COB") ||
           !strcmp(d, "CBL") || !strcmp(d, "cpy");
}

static void put_wrapped(FILE *o, const char *text)
{
    /* the generated code is written as is: long lines are not
       wrapped, the C compiler does not mind; CMS output wrapping is
       done by emit_wrapped when -cms is given */
    fputs(text, o);
}

static int translate(char **pf, int npf, char *out, int tostdout)
{
    int i;
    FILE *o;
    for (i = 0; i < npf; i++) {
        TV t;
        long k;
        memset(&t, 0, sizeof t);
        srcname = pf[i];
        load_source(pf[i], &t);
        for (k = 0; k < t.n; k++) tv_add(&toks, t.a[k]);
        if (i + 1 < npf) toks.n--;
            /* drop the EOF of all but last */
    }
    tp = 0;
    while (!at_eof()) parse_program(0);
    o = tostdout ? stdout : fopen(out, "w");
    if (!o) die("cannot open output file");
    fprintf(o, "/* generated by cobol %s from %s */\n", VERSION,
        pf[0]);
    fprintf(o, "#include \"cobrt.h\"\n\n");
    put_wrapped(o, sb_str(&out_lit));
    fprintf(o, "\n");
    put_wrapped(o, sb_str(&out_code));
    if (!opt_nomain) {
        int first = 0;
        fprintf(o, "int main(int argc, char **argv)\n{\n");
        fprintf(o, "    cob_init(argc, argv);\n");
        for (i = 0; i < nallprogs; i++)
            fprintf(o, "    cob_reg(\"%s\", PGM%d);\n",
                    allprogs[i]->name, allprogs[i]->id_);
        for (i = 0; i < nallprogs; i++)
            if (!allprogs[i]->outer && !allprogs[i]->isfunc) {
                first = i;
                break;
            }
        fprintf(o, "    PGM%d(0, 0, 0);\n", allprogs[first]->id_);
        fprintf(o,
                "    cob_stop((int)cob_getrc());\n    return 0;\n}\n");
    } else {
        fprintf(o, "void cobmod_register(void (*reg)(const char *, "
                   "CobFn))\n{\n");
        for (i = 0; i < nallprogs; i++)
            fprintf(o, "    reg(\"%s\", PGM%d);\n",
                    allprogs[i]->name, allprogs[i]->id_);
        fprintf(o, "}\n");
    }
    if (o != stdout) fclose(o);
    return !opt_nomain;
}

int main(int argc, char **argv)
{
    char *out = 0, *ccname = 0;
    char *pf[MAXSRC], *oth[MAXSRC], *cf[MAXSRC + 2];
    int npf = 0, noth = 0, ncf = 0;
    int build = 0, objonly = 0, i, rc;
    progname = "cobol";
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "-c")) build = 1;
        else if (!strcmp(argv[i], "-x")) build = 1;
        else if (!strcmp(argv[i], "-C")) objonly = 1;
        else if (!strcmp(argv[i], "-M")) opt_nomain = 1;
        else if (!strcmp(argv[i], "-free")) opt_free = 1;
        else if (!strcmp(argv[i], "-fixed")) opt_free = 0;
        else if (!strcmp(argv[i], "-debug")) opt_debug = 1;
        else if (!strcmp(argv[i], "-check")) opt_check = 1;
        else if (!strcmp(argv[i], "-I") && i + 1 < argc) {
            if (nincdirs < 32) incdirs[nincdirs++] = argv[++i];
        } else if (!strncmp(argv[i], "-I", 2) && argv[i][2]) {
            if (nincdirs < 32) incdirs[nincdirs++] = argv[i] + 2;
        } else if (!strcmp(argv[i], "-binsize") && i + 1 < argc) {
            i++;
            opt_binsize = !strcmp(argv[i], "1-2-4-8") ? 12 : 24;
        } else if (!strcmp(argv[i], "-cc") && i + 1 < argc)
            ccname = argv[++i];
        else if (argv[i][0] == '-' && argv[i][1] == 'D'
            && argv[i][2]) {
            char *e = xdup(argv[i] + 2), *q = strchr(e, '=');
            if (q) { *q = 0; def_set(upcase(e), q + 1); }
            else def_set(upcase(e), "1");
        } else if (!strcmp(argv[i], "--version")) {
            printf("cobol %s\n", VERSION);
            return 0;
        } else if (argv[i][0] == '-' && argv[i][1]) usage();
        else if (is_cobol(argv[i])) {
            if (npf >= MAXSRC - 1) usage();
            pf[npf++] = defext(argv[i]);
        } else {
            if (noth >= MAXSRC - 1) usage();
            oth[noth++] = argv[i];
        }
    }
    if (!npf) usage();
    {
        char *oc = (build || objonly || !out) ? outname(pf[0]) : out;
        translate(pf, npf, oc,
                  !build && !objonly && out && !strcmp(out, "-"));
        cf[ncf++] = oc;
    }
    if (!build && !objonly) return 0;
    for (i = 0; i < noth; i++) cf[ncf++] = oth[i];
    if (opt_nomain && !objonly) {
        char *so = out;
        if (!so) {
            so = exename(pf[0]);
            so[strlen(so) - strlen(EXE_SFX)] = 0;
            so = cat2(so, ".so");
        }
        return build_exe(cf, ncf, so, ccname, argv[0], 3);
    }
    rc = build_exe(cf, ncf, out ? out : exename(pf[0]), ccname,
                   argv[0], objonly ? 2 : 1);
    return rc;
}
