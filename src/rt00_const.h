/* rt00_const.h -- constants shared by the translator and the
   run-time system */

#define CD_MAX 80
#define CD_DIVS 34              /* fraction digits of a division */

#define CT_GROUP 0
#define CT_ALNUM 1
#define CT_NUMDISP 2
#define CT_BIN 3                /* big-endian binary */
#define CT_BIN5 4               /* native binary */
#define CT_PACKED 5
#define CT_FLT 6                /* COMP-1 */
#define CT_DBL 7                /* COMP-2 */
#define CT_NUMEDIT 8
#define CT_ALNEDIT 9
#define CT_PTR 10
#define CT_OBJ 11
#define CT_NAT 12               /* national, 2 bytes per character */
#define CT_BOOL 13
#define CF_SIGNED 1
#define CF_SEP 2                /* sign is a separate character */
#define CF_LEAD 4               /* sign leading (else trailing) */
#define CF_JUST 8               /* JUSTIFIED RIGHT */
#define CF_BLANK 16             /* BLANK WHEN ZERO */
#define CF_ALPHA 32             /* PIC A only */
#define CF_LIT 64               /* literal */
#define CF_IDX 128              /* INDEX item */
#define CF_DPC 256              /* DECIMAL-POINT IS COMMA */
#define CF_NOTRUNC 512          /* binary not truncated to the PIC */
#define CF_PLAIN 1024           /* function result: shown plainly */
#define FG_ZERO 1
#define FG_SPACE 2
#define FG_HIGH 3
#define FG_LOW 4
#define FG_QUOTE 5

#define CR_NONE 0               /* truncate */
#define CR_AWAY 1               /* nearest, half away from zero */
#define CR_EVEN 2               /* nearest, half to even */
#define CR_TRUNC 3
#define CR_UP 4                 /* away from zero */
#define CR_NEARTOZERO 5
#define CR_PROHIB 6
#define CR_HALFUP 7             /* toward +infinity on a half */
#define CR_HALFDOWN 8           /* toward -infinity on a half */
