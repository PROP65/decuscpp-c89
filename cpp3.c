/*  cpp3.c
 *  File open and command line options */

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<time.h>
#include"cppdef.h"
#include"cpp.h"

void addfile(FILE* fp, char* filename);

extern DEFBUF*   defendel  (char* name, int delete);
extern char*     savestring(char* text);
extern char*     getmem    (int size);
extern FILEINFO* getfile   (int bufsize, char* name);
extern void      cerror    (char* format, char* sarg);
extern void      cfatal    (char* format, char* sarg);
extern void      cwarn     (char* format, char* sarg);

/* Open a file, add it to the linked list of open files.
 * This is called only from openfile() above. */
int openfile(char* filename){
	register FILE* fp;

	if((fp = fopen(filename, "r")) == NULL){
#if DEBUG
		perror(filename);
#endif
		return FALSE;
	}

#if DEBUG
	if(debug)
		fprintf(stderr, "Reading from \"%s\"\n", filename);
#endif

	addfile(fp, filename);
	return TRUE;
}

/* Initialize tables for this open file.  This is called from openfile()
 * above (for #include files), and from the entry to cpp to open the main
 * input file.  It calls a common routine, getfile() to build the FILEINFO
 * structure which is used to read characters.  (getfile() is also called
 * to setup a macro replacement.) */
void addfile(FILE* fp, char* file_name){
	register FILEINFO* file;

	file          = getfile(NBUFF, file_name);
	file->fp      = fp;
	*file->buffer = EOS;  /* Initialize for first read */
	line          = 1;
	wrongline     = TRUE; /* Force out initial #line */
}

/* Append system-specific directories to the include directory list.
 * Called only when cpp is started. */
void setincdirs(void){
#ifdef CPP_INCLUDE
	*incend++ = CPP_INCLUDE;
#	define	IS_INCLUDE	1
#else
#	define	IS_INCLUDE	0
#endif

	*incend++ = "/usr/include"; /* unix */
#define	MAXINCLUDE	(NINCLUDE - 1 - IS_INCLUDE)
}

/* dooptions is called to process command line arguments (-Detc).
 * It is called only at cpp startup. */
int dooptions(int argc, char** argv){
	register char*   ap;
	register DEFBUF* dp;
	register int     c;
	char*  arg;
	SIZES* sizp; /* For -S */
	int    i, j;
	int    size;    /* For -S */
	int    isdatum; /* FALSE for -S* */
	int    endtest; /* For -S */

	for(i = j = 1; i < argc; i++){
		arg = ap = argv[i];
		if(*ap++ != '-' || *ap == EOS){
			argv[j++] = argv[i];
			continue;
		}

		c = *ap++; /* Option byte */

		if(islower(c)) /* Normalize case */
			c = toupper(c);

		switch(c){ /* Command character */
			case 'C': /* Keep comments */
				cflag = TRUE;
				keepcomments = TRUE;
				break;
			case 'D': /* Define symbol */
				/* If the option is just "-Dfoo", make it -Dfoo=1 */

				while(*ap != EOS && *ap != '=')
					ap++;

				if(*ap == EOS) ap   = "1";
						 else *ap++ = EOS;

				/* Now, save the word and its definition. */
				dp        = defendel(argv[i] + 2, FALSE);
				dp->repl  = savestring(ap);
				dp->nargs = DEF_NOARGS;
				break;
			case 'E': /* Ignore non-fatal errors */
				eflag = TRUE;
				break;
			case 'I': /* Include directory */
				if(incend >= &incdir[MAXINCLUDE])
					cfatal("Too many include directories", NULLST);

				*incend++ = ap;
				break;
			case 'N': /* No predefineds, repeat to undefine */
				nflag++;
				break;
			case 'S':
				sizp = size_table;
				isdatum = (*ap != '*');
				if(isdatum) /* If it's just -S, stop */
					endtest = T_FPTR;
				else{
					/* But if it's -S*, step over '*', stop at end marker */
					ap++;
					endtest = 0;
				}

				while(sizp->bits != endtest && *ap != EOS){
					if(!isdigit(*ap)){ /* Skip to next digit */
						ap++;
						continue;
					}

					size = 0; /* Compile the value */
					while(isdigit(*ap)){
						size *= 10;
						size += (*ap++ - '0');
					}

					if(isdatum) sizp->size  = size; /* Datum size   */
						   else sizp->psize = size; /* Pointer size */

					sizp++;
				}

				if(sizp->bits != endtest)
					cwarn("-S, too few values specified in %s", argv[i]);
				else if(*ap != EOS)
					cwarn("-S, too many values, \"%s\" unused", ap);
				break;
			case 'U': /* Undefine symbol */
				if(defendel(ap, TRUE) == NULL)
					cwarn("\"%s\" wasn't defined", ap);
				break;
	#if DEBUG
			case 'X': /* Debug */
				debug = (isdigit(*ap))
					? atoi(ap)
					: 1;

				fprintf(stderr, "Debug set to %d\n", debug);
				break;
	#endif
			default: /* What is this one? */
				cwarn("Unknown option \"%s\"", arg);
				fprintf(stderr,
					"The following options are valid:\n"
					"    -C             Write source file comments to output\n"
					"    -Dsymbol=value Define a symbol with the given (optional) value\n"
					"    -Idirectory    Add a directory to the #include search list\n"
					"    -N             Don't predefine target-specific names\n"
					"    -Stext         Specify sizes for #if sizeof\n"
					"    -Usymbol       Undefine symbol\n"
				);
	#if DEBUG
			fprintf(stderr, "    -Xvalue        Set internal debug flag\n");
	#endif
				break;
		}
	}

	if(j > 3)
		cerror("Too many file arguments.  Usage: cpp [input [output]]", NULLST);

	return j;
}

/*
 * Initialize the built-in #define's.  There are two flavors:
 * 	#define decus	1		(static definitions)
 *	#define	__FILE__ ??		(dynamic, evaluated by magic)
 * Called only on cpp startup.
 *
 * Note: the built-in static definitions are supressed by the -N option.
 * __LINE__, __FILE__, and __DATE__ are always present.
 */
void initdefines(void){
	register char**  pp;
	register char*   tp;
	register DEFBUF* dp;
	int  i;
	long tvec;

	/* Predefine the built-in symbols.  Allow the
	 * implementor to pre-define a symbol as "" to
	 * eliminate it. */
	if(nflag == 0){
		for(pp = preset; *pp != NULL; pp++){
			if(*pp[0] != EOS){
				dp        = defendel(*pp, FALSE);
				dp->repl  = savestring("1");
				dp->nargs = DEF_NOARGS;
			}
		}
	}

	/* The magic pre-defines (__FILE__ and __LINE__ are
	 * initialized with negative argument counts.  expand()
	 * notices this and calls the appropriate routine.
	 * DEF_NOARGS is one greater than the first "magic" definition. */
	if(nflag < 2){
		for(pp = magic, i = DEF_NOARGS; *pp != NULL; pp++){
			dp        = defendel(*pp, FALSE);
			dp->nargs = --i;
		}

#if OK_DATE
		/* Define __DATE__ as today's date. */
		dp        = defendel("__DATE__", FALSE);
		dp->repl  = tp = getmem(27);
		dp->nargs = DEF_NOARGS;

		time(&tvec);
		*tp++ = '"';
		strcpy(tp, ctime(&tvec));
		tp[24] = '"'; /* Overwrite newline */
#endif
	}
}
