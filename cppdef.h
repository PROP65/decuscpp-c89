/* System Dependent, Definitions for CPP */

/* This redundant definition of TRUE and FALSE works around
 * a limitation of Decus C. */
#define TRUE  1
#define FALSE 0

/* defaults */
#ifndef MSG_PREFIX
#define MSG_PREFIX "cpp: "
#endif

/*  line markers,
 *  "" outputs "# 123" line markers */
#ifndef LINE_PREFIX
#define LINE_PREFIX ""
#endif

/* RECURSION_LIMIT may be set to -1 to disable the macro recursion test. */
#ifndef	RECURSION_LIMIT
#define	RECURSION_LIMIT	1000
#endif

/* BITS_CHAR may be defined to set the number of bits per character.
 * it is needed only for multi-byte character constants. */
#ifndef	BITS_CHAR
#define	BITS_CHAR 8
#endif

/*  might be able to be replaced with
 *      (!(union{short u16; char u8;}){ 1 }.u8)
 *  original comment states BIG_ENDIAN code is untested */
#ifndef	BIG_ENDIAN
#define	BIG_ENDIAN FALSE
#endif

/* if TRUE comments will not delimit tokens, as opposed to the C89 standard (see 2.1.1.2 phase 3)*/
#ifndef	COMMENT_INVISIBLE
#define	COMMENT_INVISIBLE FALSE
#endif

/* STRING_FORMAL may be defined to allow recognition of macro parameters
 * anywhere in replacement strings.  This was removed from the Draft Ansi
 * Standard and a limited recognition capability added. */
#ifndef	STRING_FORMAL
#define	STRING_FORMAL FALSE
#endif

/* OK_DOLLAR enables use of $ as a valid "letter" in identifiers.
 * This is a permitted extension to the Ansi Standard and is required
 * for e.g., VMS, RSX-11M, etc. It should be set FALSE if cpp is
 * used to preprocess assembler source on Unix systems. OLD_PREPROCESSOR
 * sets OK_DOLLAR FALSE for that reason. */
#ifndef	OK_DOLLAR
#define	OK_DOLLAR TRUE
#endif

/* OK_CONCAT enables (one possible implementation of) token concatenation.
 * If cpp is used to preprocess Unix assembler source, this should be
 * set FALSE as the concatenation character, #, is used by the assembler. */
#ifndef	OK_CONCAT
#define	OK_CONCAT TRUE
#endif

/* OK_DATE may be enabled to predefine today's date as a string
 * at the start of each compilation. This is apparently not permitted
 * by the Draft Ansi Standard. */
#ifndef	OK_DATE
#define	OK_DATE TRUE
#endif

#ifdef NDEBUG
#	define DEBUG FALSE
#else
#	define DEBUG TRUE
#endif

#ifndef IDMAX
#	define IDMAX  31          /* max length of identifier          */
#endif
#define PAR_MAC   (31 + 1)    /* #define function argument limit   */
#define NBUFF     4096        /* input buffer size                 */
#define NWORK     4096        /* working buffer size               */
#define NEXP      128         /* max nesting of #if expressions    */
#define NINCLUDE  7           /* max number of include directories */
#define NPARMWORK (NWORK * 2) /* parameter working buffer size     */
#define BLK_NEST  32          /* max nesting of #if                */
