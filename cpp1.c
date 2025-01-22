/*  cpp1.c
 *  Main program */
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<assert.h>
#include"cppdef.h"
#include"cpp.h"

/* COMMON GLOBALS */
int       line;             /* current line number */
int       wrongline;        /* (bool) output line is out of sync, forces #line outputs to correct */
char      token[IDMAX + 1]; /* last identifier scanned */
int       errors;           /* error counter */
FILEINFO* infile = NULL;    /* head of linked list of input files, infile->fd == NULL if input is macro */
#if DEBUG
int       debug;            /* (bool) debugging? */
#endif

int recursion;          /* increments every time a macro is expanded */
int rec_recover = TRUE; /* (bool) runaway prevention (unimplemented?) */

/* get() flags */
int instring = FALSE; /* (bool) true if scaning string, changes get() behavior */
int inmacro  = FALSE; /* (bool) true if macro is being defined */
/* NOTE: (char[2]){'\\','\n'} DOES NOT delimit tokens, this is not standard compliant */

/* work[] and workp are used to store one piece of text in a temporay
 * buffer.  To initialize storage, set workp = work.  To store one
 * character, call save(c);  (This will fatally exit if there isn't
 * room.)  To terminate the string, call save(EOS).  Note that
 * the work buffer is used by several subroutines -- be sure your
 * data won't be overwritten.  The extra byte in the allocation is
 * needed for string formal replacement. */
char  work[NWORK + 1];
char* workp;

/* runtime / command line flags */
int keepcomments = FALSE; /* (bool) false will prune comments in output, may be modified by at runtime */
int cflag = FALSE;        /* (bool) -C option (keep comments) static after dooptions */
int eflag = FALSE;        /* (bool) -E option (always return 0 from main()) */
int nflag = 0;            /* (bool) -N option (no predefines) */

/* ifstack[] holds information about nested #if's.  It is always
 * accessed via *ifptr.  The information is as follows:
 *   WAS_COMPILING state of compiling flag at outer level.
 *   ELSE_SEEN set TRUE when #else seen to prevent 2nd #else.
 *   TRUE_SEEN set TRUE when #if or #elif succeeds
 * ifstack[0] holds the compiling flag.  It is TRUE if compilation
 * is currently enabled.  Note that this must be initialized TRUE. */
char  ifstack[BLK_NEST] = { TRUE }; /* #if information */
char* ifptr = ifstack;              /* -> current ifstack[] */

/* includes */
char*  incdir[NINCLUDE]; /* include directories */
char** incend = incdir;  /* free space in incdir[] */

/* preset defines, disable with -N option */
char* preset[] = {
#if	DEBUG
	"decus_cpp",
#endif
	NULL /* NULL terminated list */
};

/* The value of these predefined symbols must be recomputed whenever
 * they are evaluated.  The order must not be changed. */
char* magic[] = { /* Note: order is important */
	"__LINE__",
	"__FILE__",
	NULL /* NULL terminated list */
};

void cppmain(void);
void output (int c); /* Output one character */
void sharp  (void);

extern int   get        (void);
extern void  unget      (void);
extern char* savestring (char* text);
extern int   dooptions  (int argc, char** argv);
extern int   macroid    (register int c);
extern int   catenate   (void);
extern int   scanstring (register int delim, void (*outfun)(int));
extern void  scannumber (register int c, register void (*outfun)(int));
extern void  skipnl     (void);
extern void  cierror    (char* format, int narg);
extern int   control    (int counter);
extern void  cerror     (char* format, char* sarg);
extern void  initdefines(void);
extern void  addfile    (FILE* fp, char* filename);
extern void  setincdirs (void);
extern void  dumpdef    (char* why);

int main(int argc, char** argv){
	register int i;

	initdefines();
	i = dooptions(argc, argv);

	switch(i){
		case 3:
			/* Get output file, "-" means use stdout. */
			if(!streq(argv[2], "-")){
				if(freopen(argv[2], "w", stdout) == NULL){
					perror(argv[2]);
					cerror("Can't open output file \"%s\"", argv[2]);
					exit(IO_ERROR);
				}
			}
			/* fallthrough */
		case 2: /* One file -> stdin */
			/* Open input file, "-" means use stdin. */
			if(!streq(argv[1], "-")){
				if(freopen(argv[1], "r", stdin) == NULL){
					perror(argv[1]);
					cerror("Can't open input file \"%s\"", argv[1]);
					exit(IO_ERROR);
				}
				strcpy(work, argv[1]); /* Remember input filename */
				break;
			}
		case 0: /* No args? */
		case 1: /* No files, stdin -> stdout */
			work[0] = EOS; /* Unix can't find stdin name */
			break;
		default:
			exit(IO_ERROR);
	}
	setincdirs();
	addfile(stdin, work);
#if DEBUG
	if(debug > 0)
		dumpdef("preset #define symbols");
#endif
	cppmain(); /* Process main file */
	if((i = (ifptr - &ifstack[0])) != 0){
#if OLD_PREPROCESSOR
		ciwarn("Inside #ifdef block at end of input, depth = %d", i);
#else
		cierror("Inside #ifdef block at end of input, depth = %d", i);
#endif
	}

	fclose(stdout);
	if(errors > 0){
		fprintf(stderr, "%d error(s) in preprocessor\n", errors);

		if(!eflag)
			exit(IO_ERROR);
	}
	exit(IO_NORMAL);
}

/* Main process for cpp -- copies tokens from the current input
 * stream (main file, include file, or a macro) to the output
 * file. */
void cppmain(void){
	register int c;
	register int counter;

	/* Explicitly output a #line at the start of cpp output so
	 * that lint (etc.) knows the name of the original source
	 * file.  If we don't do this explicitly, we may get
	 * the name of the first #include file instead.
	 * We also seem to need a blank line following that first #line. */
	sharp();
	putchar('\n');

	for(;;){
		counter = 0;

		for(;;){
			/* Skip leading blanks */
			while(type[(c = get())] == SPA);

			switch(c){
				case '\n':
					counter++;
					break;
				case '#':
					keepcomments = FALSE;
					counter = control(counter);
					keepcomments = (cflag && compiling);
					break;
				case EOF_CHAR:
					goto escape_loop_1;
				default:
					if(compiling) /* #ifdef true? */
						goto escape_loop_1;

					skipnl();
					counter++;
					break;
			}
		}
escape_loop_1:

		if(c == EOF_CHAR) break;

		/* If the loop didn't terminate because of end of file, we
		 * know there is a token to compile.  First, clean up after
		 * absorbing newlines.  counter has the number we skipped. */
		if((wrongline && infile->fp != NULL) || counter > 4)
			sharp();
		else{
			/* If just a few, stuff them out ourselves */
			while(--counter >= 0)
				putchar('\n');
		}

		/* Process each token on this line. */
		unget();
		for(;;){
			do{
				for(counter = 0; type[(c = get())] == SPA;){
#if COMMENT_INVISIBLE
					if(c != COM_SEP)
#endif
					counter++;
				}
				if(c == EOF_CHAR || c == '\n')
					goto end_line;
				else if(counter > 0) /* replace \x20{2,} with \x20 */
					putchar(' ');

				c = macroid(c);
			}while(type[c] == LET && catenate());

			if(c == EOF_CHAR || c == '\n') /* From macro exp error */
				goto end_line;

			switch(type[c]){
				case LET:
					fputs(token, stdout);
					break;
				case DIG: case DOT:
					scannumber(c, output);
					break;
				case QUO:
					scanstring(c, output);
					break;
				default:
					cput(c);
					break;
			}
		}
end_line:
		if(c == '\n'){
			putchar('\n');

			if(infile->fp == NULL) /* Expanding a macro */
				wrongline = TRUE;
		}
	}
}

/* Output one character to stdout -- output() is passed as an
 * argument to scanstring() */
void output(int c){
#if COMMENT_INVISIBLE
	if(c != TOK_SEP && c != COM_SEP)
#else
	if(c != TOK_SEP)
#endif
		putchar(c);
}

static char* sharpfilename = NULL;

/* Output a line number line. */
void sharp(void){
	register char* name;

	if(keepcomments)
		putchar('\n');

	printf("#%s %d", LINE_PREFIX, line);

	assert(infile != NULL);
	if(infile->fp != NULL){
		name = (infile->progname != NULL)
			? infile->progname
			: infile->filename;

		if(sharpfilename == NULL || (sharpfilename != NULL && !streq(name, sharpfilename))){
			if(sharpfilename != NULL)
				free(sharpfilename);

			sharpfilename = savestring(name);
			printf(" \"%s\"", name);
		}
	}

	putchar('\n');
	wrongline = FALSE;
}
