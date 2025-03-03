/* Internal Definitions for CPP
 * In general, definitions in this file should not be changed. */

#define	TRUE  1
#define	FALSE 0

#ifndef EOS         /* This is predefined in Decus C */
#	define EOS '\0' /* End of string                 */
#endif

#define	EOF_CHAR   0             /* Returned by get() on eof     */
#define NULLST     ((char*)NULL) /* Pointer to nowhere (linted)  */
#define	DEF_NOARGS (-1)          /* #define foo vs #define foo() */

/* The following may need to change if the host system doesn't use ASCII. */
#define	DEF_MAGIC 0x1D /* Magic for #defines         */
#define	TOK_SEP   0x1E /* Token concatenation delim. */
#define COM_SEP   0x1F /* Magic comment separator    */

/* Note -- in Ascii, the following will map macro formals onto DEL + the
 * C1 control character region (decimal 128 .. (128 + PAR_MAC)) which will
 * be ok as long as PAR_MAC is less than 33).  Note that the last PAR_MAC
 * value is reserved for string substitution. */
#define	MAC_PARM 0x7F /* Macro formals start here */

#if PAR_MAC >= 33
#	error "assertion fails -- PAR_MAC isn't less than 33"
#endif

#define	LASTPARM (PAR_MAC - 1)

/* Character type codes. */
#define	INV         0      /* Invalid, must be zero */
#define	OP_EOE      INV    /* End of expression     */
#define	DIG         1      /* Digit                 */
#define	LET         2      /* Identifier start      */
#define	FIRST_BINOP OP_ADD
#define	OP_ADD      3
#define	OP_SUB      4
#define	OP_MUL      5
#define	OP_DIV      6
#define	OP_MOD      7
#define	OP_ASL      8
#define	OP_ASR      9
#define	OP_AND      10     /* &, not &&             */
#define	OP_OR       11     /* |, not ||             */
#define	OP_XOR      12
#define	OP_EQ       13
#define	OP_NE       14
#define	OP_LT       15
#define	OP_LE       16
#define	OP_GE       17
#define	OP_GT       18
#define	OP_ANA      19     /* &&                    */
#define	OP_ORO      20     /* ||                    */
#define	OP_QUE      21     /* ?                     */
#define	OP_COL      22     /* :                     */
#define	OP_CMA      23     /* , (relevant?)         */
#define	LAST_BINOP  OP_CMA /* Last binary operand   */

/* The following are unary. */
#define	FIRST_UNOP OP_PLU       /* First Unary operand      */
#define	OP_PLU     24           /* +                        */
#define	OP_NEG     25           /* -                        */
#define	OP_COM     26           /* ~                        */
#define	OP_NOT     27           /* !                        */
#define	LAST_UNOP  OP_NOT
#define	OP_LPA     28           /* (                        */
#define	OP_RPA     29           /* )                        */
#define	OP_END     30           /* End of expression marker */
#define	OP_MAX     (OP_END + 1)	/* Number of operators      */
#define	OP_FAIL    (OP_END + 1)	/* For error returns        */

/* The following are for lexical scanning only. */
#define	QUO 65 /* Both flavors of quotation */
#define	DOT 66 /* . might start a number    */
#define	SPA 67 /* Space and tab             */
#define	BSH 68 /* Just a backslash          */
#define	END 69 /* EOF                       */

/* These bits are set in ifstack[] */
#define	WAS_COMPILING 1 /* TRUE if compile set at entry */
#define	ELSE_SEEN     2 /* TRUE when #else processed    */
#define	TRUE_SEEN     4 /* TRUE when #if TRUE processed */

/* Define bits for the basic types and their adjectives */
#define	T_CHAR     1
#define	T_INT      2
#define	T_FLOAT    4
#define	T_DOUBLE   8
#define	T_SHORT    16
#define	T_LONG     32
#define	T_SIGNED   64
#define	T_UNSIGNED 128
#define	T_PTR      256 /* Pointer              */
#define	T_FPTR     512 /* Pointer to functions */

/* The DEFBUF structure stores information about #defined macros.
 * Note that the defbuf->repl information is always in malloc storage. */
typedef struct defbuf{
	struct defbuf* link;    /* Next define in chain */
	char*          repl;    /* -> replacement       */
	int            hash;    /* Symbol table hash    */
	int            nargs;   /* For define(args)     */
	char           name[1]; /* #define name         */
} DEFBUF;

/* The FILEINFO structure stores information about open files
 * and macros being expanded. */
typedef struct fileinfo{
	char*            bptr;      /* Buffer pointer       */
	int              line;      /* for include or macro */
	FILE*            fp;        /* File if non-null     */
	struct fileinfo* parent;    /* Link to includer     */
	char*            filename;  /* File/macro name      */
	char*            progname;  /* From #line statement */
	unsigned int     unrecur;   /* For macro recursion  */
	char             buffer[1]; /* current input line   */
} FILEINFO;

/* The SIZES structure is used to store the values for #if sizeof */
typedef struct sizes{
    short bits;  /* If this bit is set,          */
    short size;  /* this is the datum size value */
    short psize; /* this is the pointer size     */
} SIZES;

/* nomacarg is a built-in #define on Decus C. */
#ifdef nomacarg
#	define cput output /* cput concatenates tokens */
#else
#	if COMMENT_INVISIBLE
#		define cput(c) { if (c != TOK_SEP && c != COM_SEP) putchar(c); }
#	else
#		define cput(c) { if (c != TOK_SEP) putchar(c); }
#	endif
#endif

#ifndef	nomacarg
#	define streq(s1, s2) (strcmp(s1, s2) == 0)
#endif

/* exit codes */
#define	IO_NORMAL 0
#define	IO_ERROR  1

/* Externs */
extern int    line;              /* Current line number          */
extern int    wrongline;         /* Force #line to cc pass 1     */
extern char   type[];            /* Character classifier         */
extern char   token[IDMAX + 1];  /* Current input token          */
extern int    instring;          /* TRUE if scanning string      */
extern int    inmacro;           /* TRUE if scanning #define     */
extern int    errors;            /* Error counter                */
extern int    recursion;         /* Macro depth counter          */
extern char   ifstack[BLK_NEST]; /* #if information              */
extern char*  ifptr;             /* -> current ifstack item      */
extern char*  incdir[NINCLUDE];  /* -i directories               */
extern char** incend;            /* -> active end of incdir      */
extern int    cflag;             /* -C option (keep comments)    */
extern int    eflag;             /* -E option (ignore errors)    */
extern int    nflag;             /* -N option (no pre-defines)   */
extern int    rec_recover;       /* unwind recursive macros      */
extern char*  preset[];          /* Standard predefined symbols  */
extern char*  magic[];           /* Magic predefined symbols     */
extern char   work[NWORK + 1];   /* #define scratch              */
extern char*  workp;             /* Free space in work           */
extern int    keepcomments;      /* Don't remove comments if set */
extern SIZES  size_table[];      /* For #if sizeof sizes         */
#if	DEBUG
extern int    debug;             /* Debug level                  */
#endif
extern FILEINFO* infile;         /* Current input file           */
#define	compiling ifstack[0]
