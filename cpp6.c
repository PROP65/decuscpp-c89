/*  cpp6.c
 *  Support Routines */

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include"cppdef.h"
#include"cpp.h"

int  get     (void);
void unget   (void);
void save    (register int c);
void cwarn   (char* format, char* sarg);
void cerror  (char* format, char* sarg);
void cierror (char* format, int narg);
void cfatal  (char* format, char* sarg);
void dumpadef(char* why, register DEFBUF* dp);

extern void      ungetstring(char* text);
extern DEFBUF*   lookid     (int c);
extern char*     getmem     (int size);
extern char*     savestring (char* text);
extern int       cget       (void);
extern FILEINFO* getfile    (int buffer_size, char* name);
extern void      expand     (register DEFBUF* tokenp);

/* skipnl()	skips over input text to the end of the line.
 * skipws()	skips over "whitespace" (spaces or tabs), but
 *		not skip over the end of the line.  It skips over
 *		TOK_SEP, however (though that shouldn't happen).
 * scanid()	reads the next token (C identifier) into token[].
 *		The caller has already read the first character of
 *		the identifier.  Unlike macroid(), the token is
 *		never expanded.
 * macroid()	reads the next token (C identifier) into token[].
 *		If it is a #defined macro, it is expanded, and
 *		macroid() returns TRUE, otherwise, FALSE.
 * catenate()	Does the dirty work of token concatenation, TRUE if it did.
 * scanstring()	Reads a string from the input stream, calling
 *		a user-supplied function for each character.
 *		This function may be output() to write the
 *		string to the output file, or save() to save
 *		the string in the work buffer.
 * scannumber()	Reads a C numeric constant from the input stream,
 *		calling the user-supplied function for each
 *		character.  (output() or save() as noted above.)
 * save()	Save one character in the work[] buffer.
 * savestring()	Saves a string in malloc() memory.
 * getfile()	Initialize a new FILEINFO structure, called when
 *		#include opens a new file, or a macro is to be
 *		expanded.
 * getmem()	Get a specified number of bytes from malloc memory.
 * output()	Write one character to stdout (calling putchar) --
 *		implemented as a function so its address may be
 *		passed to scanstring() and scannumber().
 * lookid()	Scans the next token (identifier) from the input
 *		stream.  Looks for it in the #defined symbol table.
 *		Returns a pointer to the definition, if found, or NULL
 *		if not present.  The identifier is stored in token[].
 * defnedel()	Define enter/delete subroutine.  Updates the
 *		symbol table.
 * get()	Read the next byte from the current input stream,
 *		handling end of (macro/file) input and embedded
 *		comments appropriately.  Note that the global
 *		instring is -- essentially -- a parameter to get().
 * cget()	Like get(), but skip over TOK_SEP.
 * unget()	Push last gotten character back on the input stream.
 * cerror(), cwarn(), cfatal(), cierror(), ciwarn()
 *		These routines format an print messages to the user.
 *		cerror & cwarn take a format and a single string argument.
 *		cierror & ciwarn take a format and a single int (char) argument.
 *		cfatal takes a format and a single string argument. */

/* This table must be rewritten for a non-Ascii machine.
 *
 * Note that several "non-visible" characters have special meaning:
 * Hex 1D DEF_MAGIC -- a flag to prevent #define recursion.
 * Hex 1E TOK_SEP   -- a delimiter for token concatenation
 * Hex 1F COM_SEP   -- a zero-width whitespace for comment concatenation */
#if TOK_SEP != 0x1E || COM_SEP != 0x1F || DEF_MAGIC != 0x1D
#	error "<< error type table isn't correct >>"
#endif

#if OK_DOLLAR
#define	DOL	LET
#else
#define	DOL	000
#endif

char type[256] = { /* Character type codes */
   END,    000,    000,    000,    000,   000,    000,    000,    /* 00          */
   000,    SPA,    000,    000,    000,   000,    000,    000,    /* 08          */
   000,    000,    000,    000,    000,   000,    000,    000,    /* 10          */
   000,    000,    000,    000,    000,   LET,    000,    SPA,    /* 18          */
   SPA,    OP_NOT, QUO,    000,    DOL,   OP_MOD, OP_AND, QUO,    /* 20  !"#$%&' */
   OP_LPA, OP_RPA, OP_MUL, OP_ADD, 000,   OP_SUB, DOT,    OP_DIV, /* 28 ()*+,-./ */
   DIG,    DIG,    DIG,    DIG,    DIG,   DIG,    DIG,    DIG,    /* 30 01234567 */
   DIG,    DIG,    OP_COL, 000,    OP_LT, OP_EQ,  OP_GT,  OP_QUE, /* 38 89:;<=>? */
   000,    LET,    LET,    LET,    LET,   LET,    LET,    LET,    /* 40 @ABCDEFG */
   LET,    LET,    LET,    LET,    LET,   LET,    LET,    LET,    /* 48 HIJKLMNO */
   LET,    LET,    LET,    LET,    LET,   LET,    LET,    LET,    /* 50 PQRSTUVW */
   LET,    LET,    LET,    000,    BSH,   000,    OP_XOR, LET,    /* 58 XYZ[\]^_ */
   000,    LET,    LET,    LET,    LET,   LET,    LET,    LET,    /* 60 `abcdefg */
   LET,    LET,    LET,    LET,    LET,   LET,    LET,    LET,    /* 68 hijklmno */
   LET,    LET,    LET,    LET,    LET,   LET,    LET,    LET,    /* 70 pqrstuvw */
   LET,    LET,    LET,    000,    OP_OR, 000,    OP_NOT, 000,    /* 78 xyz{|}~  */
   000,    000,    000,    000,    000,   000,    000,    000,    /*   80 .. FF  */
};

/* Skip to the end of the current input line. */
void skipnl(void){
	register int c;

	/* Skip to newline */
	   do c = get();
	while(c != '\n' && c != EOF_CHAR);
}

/* Skip over whitespace */
int skipws(void){
	register int c;

	/* Skip whitespace */
	   do c = get();
#if COMMENT_INVISIBLE
	while(type[c] == SPA || c == COM_SEP);
#else
	while(type[c] == SPA);
#endif
	return c;
}

/* Get the next token (an id) into the token buffer.
 * Note: this code is duplicated in lookid().
 * Change one, change both. */
void scanid(register int c /* First char of id */){
	register char* bp;

	if(c == DEF_MAGIC) /* Eat the magic token */
		c = get();     /* undefiner.          */

	bp = token;

	do{
		/* token dim is IDMAX+1 */
		if(bp < &token[IDMAX])
			*bp++ = c;

		c = get();
	}while(type[c] == LET || type[c] == DIG);

	unget();
	*bp = EOS;
}

/* If c is a letter, scan the id.  if it's #defined, expand it and scan
 * the next character and try again.
 *
 * Else, return the character.  If type[c] is a LET, the token is in token. */
int macroid(register int c){
	register DEFBUF* dp;

	if(infile != NULL && infile->fp != NULL)
		recursion = 0;

	while(type[c] == LET && (dp = lookid(c)) != NULL){
		expand(dp);
		c = get();
	}

	return c;
}

/* A token was just read (via macroid).
 * If the next character is TOK_SEP, concatenate the next token
 * return TRUE -- which should recall macroid after refreshing
 * macroid's argument.  If it is not TOK_SEP, unget() the character
 * and return FALSE. */
int catenate(void){
	register int   c;
	register char* token1;

#if !OK_CONCAT
	return FALSE;
#endif
	if(get() != TOK_SEP){
		unget();
		return FALSE;
	}

    token1 = savestring(token);
    c = macroid(get());
    switch(type[c]){
		case LET:
			if(strlen(token1) + strlen(token) >= NWORK)
				cfatal("work buffer overflow doing %s #", token1);

			sprintf(work, "%s%s", token1, token);
			break;
		case DIG:
			strcpy(work, token1);
			workp = work + strlen(work);

			   do save(c);
			while((c = get()) != TOK_SEP);

			/* The trailing TOK_SEP is no longer needed. */
			save(EOS);
			break;
		default: /* An error */
#if ! COMMENT_INVISIBLE
			cierror(
				isprint(c)
					? "Strange character '%c' after #"
					: "Strange character (%d.) after #",
				c);
#endif
			strcpy(work, token1);
			unget();
			break;
    }

	/* work has the concatenated token and token1 has
	 * the first token (no longer needed).  Unget the
	 * new (concatenated) token after freeing token1.
	 * Finally, setup to read the new token. */
    free(token1);
    ungetstring(work);
    return TRUE;
}

/* Scan off a string.  Warning if terminated by newline or EOF.
 * output_func() outputs the character -- to a buffer if in a macro.
 * TRUE if ok, FALSE if error. */
int scanstring(register int delim_char, void (*output_func)(int)){
	register int c;

	instring = TRUE; /* Don't strip comments */
	(*output_func)(delim_char);

	while((c = get()) != delim_char && c != '\n' && c != EOF_CHAR){
		if(c != DEF_MAGIC)
			(*output_func)(c);

		if(c == '\\')
			(*output_func)(get());
	}

	instring = FALSE;

	if(c == delim_char){
		(*output_func)(c);
		return (TRUE);
	}

    cerror("Unterminated string", NULLST);
    unget();
    return (FALSE);
}

/* Process a number.  We know that c is from 0 to 9 or dot.
 * Algorithm from Dave Conroy's Decus C. */
void scannumber(register int c, register void (*output_func)(int)){
	register int radix = 10; /* 8, 10, or 16         */
	int expseen = FALSE;     /* 'e' seen in floater  */
	int signseen = TRUE;     /* '+' or '-' seen      */
	int octal89 = FALSE;     /* For bad octal test   */
	int dotflag;             /* TRUE if '.' was seen */

	dotflag = (c == '.');
	if(dotflag != FALSE){
		(*output_func)('.');
		c = get();
		/* If next char is not digit, rescan char */
		if(type[c] != DIG){
			unget();
			return;
		}
	}else if(c == '0'){ /* octal / hex */
		(*output_func)('0');
		radix = 8; /* default to octal */
		c = get();
		if(c == 'x' || c == 'X'){
			radix = 16;
			(*output_func)(c);
			c = get();
		}
	}

	for(;;){
		/* Note that this algorithm accepts "012e4" and "03.4"
		 * as legitimate floating-point numbers. */
		if(radix != 16 && (c == 'e' || c == 'E')){
			if(expseen) /* exit loop, bad number */
				break;

			expseen = TRUE;
			/* signseen = FALSE; We can read '+' now */
			radix = 10;
		}else if(radix != 16 && c == '.'){
			if(dotflag) /* exit loop, double dots */
				break;
			dotflag = TRUE;
			radix = 10;
		}else if(c == '+' || c == '-'){ /* 1.0e+10 */
			if(signseen)
				break;
		}else {
			switch(c){
				case '8': case '9':
					octal89 = TRUE;
				case '0': case '1': case '2': case '3':
				case '4': case '5': case '6': case '7':
					break; /* Always ok */

				case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
				case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
					if(radix == 16)
						break;
				default: /* end of number */
					goto loop_exit;
			}
		}

		(*output_func)(c);
		signseen = TRUE;
		c = get();
	}
	/* When we break out of the scan loop, c contains the first
	 * character (maybe) not in the number.  If the number is an
	 * integer, allow a trailing 'L' for long and/or a trailing 'U'
	 * for unsigned.  If not those, push the trailing character back
	 * on the input stream.  Floating point numbers accept a trailing
	 * 'L' for "long double". */
loop_exit:
	/* float? */
	if(dotflag || expseen){
		/* long suffix */
		if(c == 'l' || c == 'L'){
			(*output_func)(c);
			get(); /* Ungotten later */
		}
	}else{
		/* We know that dotflag and expseen are both zero, now:
		 * dotflag signals "saw 'L'", and
		 * expseen signals "saw 'U'". */
		for(;;){
			switch(c){
				/* long suffix */
				case 'l': case 'L':
					if(dotflag)
						goto nomore;

					dotflag = TRUE;
					break;
				/* unsigned suffix */
				case 'u': case 'U':
					if(expseen)
						goto nomore;

					expseen = TRUE;
					break;
				default: goto nomore;
			}
			(*output_func)(c); /* Got 'L' or 'U'.    */
			c = get();         /* Look at next, too. */
		}
	}
nomore:
	unget();
	if(octal89 && radix == 8)
		cwarn("Illegal digit in octal number", NULLST);
}

void save(register int c){
	if(workp >= &work[NWORK]){
		work[NWORK-1] = '\0';
		cfatal("Work buffer overflow:  %s", work);
		return;
	}

	*workp++ = c;
}

/* Store a string into free memory. */
char* savestring(char* text){
	register char* result;

	result = getmem(strlen(text) + 1);
	strcpy(result, text);
	return result;
}

/* Common FILEINFO buffer initialization for a new file or macro. */
/* Line or define buffer size */
/* File or macro name string  */
FILEINFO* getfile(int buffer_size, char* name){
	register FILEINFO* file;
	register int       size;

	size            = strlen(name); /* File/macro name */
	file            = (FILEINFO*)getmem(sizeof(FILEINFO) + buffer_size + size);
	file->parent    = infile;
	file->fp        = NULL;
	file->filename  = savestring(name);
	file->progname  = NULL;
	file->unrecur   = 0;   /* No macro fixup */
	file->bptr      = file->buffer;
	file->buffer[0] = EOS; /* Force first read */
	file->line      = 0;

	if(infile != NULL)       /* If #include file */
		infile->line = line; /* Save current line */

	infile = file; /* New current file */
	line = 1;      /* Note first line  */
	return file;
}

/* Get a block of free memory */
char* getmem(int size){
	register void* result = malloc((unsigned)size);

	if(result == NULL)
		cfatal("Out of memory", NULLST);

	return result;
}

/* C P P   S y m b o l   T a b l e s */

/* SBSIZE defines the number of hash-table slots for the symbol table.
 * It must be a power of 2. */
#ifndef	SBSIZE
#	define SBSIZE 64
#endif

#define SBMASK (SBSIZE - 1)

#if(SBSIZE ^ SBMASK) != ((SBSIZE * 2) - 1)
#	error "<< error, SBSIZE must be a power of 2 >>"
#endif

static DEFBUF* symtab[SBSIZE]; /* Symbol table queue headers */

/* Look for the next token in the symbol table.  Returns token in "token".
 * If found, returns the table pointer;  Else returns NULL. */
/* First character of token */
DEFBUF* lookid(int c){
	register int     nhash;
	register DEFBUF* dp;
	register char*   np;
	int temp = 0;
	int isrecurse; /* For #define foo foo */

	np = token;
	nhash = 0;
	if((isrecurse = (c == DEF_MAGIC))) /* If recursive macro   */
		c = get();                     /* hack, skip DEF_MAGIC */

	do{
		if(np < &token[IDMAX]){ /* token dim is IDMAX+1 */
			*np++ = c;
			nhash += c;
		}
		c = get();
	}while(type[c] == LET || type[c] == DIG);

	unget(); /* Rescan terminator */
	*np = EOS;

	if(isrecurse)    /* Recursive definition */
		return NULL; /* undefined just now   */

	nhash += (np - token);
	dp     = symtab[nhash & SBMASK]; /* Starting bucket */

	while(dp != (DEFBUF *) NULL){ /* Search symbol table */
		if(dp->hash == nhash /* Fast precheck */
		 && (temp = strcmp(dp->name, token)) >= 0)
			break;

		dp = dp->link; /* NEXT */
	}

	return (temp == 0)
		? dp
		: NULL;
}

/* Enter this name in the lookup table (delete = FALSE)
 * or delete this name (delete = TRUE).
 * Returns a pointer to the define block (delete = FALSE)
 * Returns NULL if the symbol wasn't defined (delete = TRUE). */
DEFBUF* defendel(char* name, int remove){
	register DEFBUF*  dp;
	register DEFBUF** prevp;
	register char*    np;
	int nhash;
	int temp;
	int size;

	for(nhash = 0, np = name; *np != EOS;)
		nhash += *np++;

	size   = (np - name);
	nhash += size;
	prevp  = &symtab[nhash & SBMASK];

	while((dp = *prevp) != (DEFBUF *) NULL){
		if(dp->hash == nhash && (temp = strcmp(dp->name, name)) >= 0){
			if(temp > 0)
				dp = NULL;
			else {
				*prevp = dp->link;   /* Found, unlink and    */
				if(dp->repl != NULL) /* free the replacement */
					free(dp->repl);  /* if any               */

				free((void*)dp);
			}
			break;
		}
		prevp = &dp->link;
	}

	if(remove)
		return NULL;

	dp        = (DEFBUF *) getmem(sizeof (DEFBUF) + size);
	dp->link  = *prevp;
	*prevp    = dp;
	dp->hash  = nhash;
	dp->repl  = NULL;
	dp->nargs = 0;
	strcpy(dp->name, name);
	return dp;
}

#if DEBUG
void dumpdef(char* why){
	register DEFBUF*  dp;
	register DEFBUF** syp;

	printf("CPP symbol table dump %s\n", why);

	for(syp = symtab; syp < &symtab[SBSIZE]; syp++){
		if((dp = *syp) != NULL){
			printf("symtab[%d]\n", (int)(syp - symtab));

			   do dumpadef((char *) NULL, dp);
			while((dp = dp->link) != NULL);
		}
	}
}

void dumpadef(char* why, register DEFBUF* dp){
	register char* cp;
	register int   c;

	printf(" \"%s\" [%d]", dp->name, dp->nargs);
	if(why != NULL)
		printf(" (%s)", why);

	if(dp->repl == NULL){
		printf(", no replacement.\n");
		return;
	}

    printf(" => ");
    for(cp = dp->repl; (c = *cp++ & 0xFF) != EOS;){
		if(c >= MAC_PARM && c <= (MAC_PARM + PAR_MAC))
			printf("<%d>", c - MAC_PARM);
		else if(isprint(c) || c == '\n' || c == '\t')
			putchar(c);
		else if(c < ' ')
			printf("<^%c>", c + '@');
		else
			printf("<\\0%o>", c);
    }

	putchar('\n');
}
#endif

/* G E T */

/* Return the next character from a macro or the current file.
 * Handle end of file from #include files. */
int get(void){
	register int       c;
	register FILEINFO* file;
	register int       popped = 0; /* Recursion fixup */

get_from_file:
	if((file = infile) == NULL)
		return EOF_CHAR;
newline:
#if 0
	fprintf(stderr, "get(%s), recursion %d, line %d, bptr = %d, buffer \"%s\"\n",
		file->filename, recursion, line,
		file->bptr - file->buffer, file->buffer);
#endif
	/* Read a character from the current input line or macro.
	 * At EOS, either finish the current macro (freeing temp.
	 * storage) or read another line from the current input file.
	 * At EOF, exit the current file (#include) or, at EOF from
	 * the cpp input file, return EOF_CHAR to finish processing. */
	if((c = *file->bptr++ & 0xFF) == EOS){
		/* Nothing in current line or macro.  Get next line (if
		 * input from a file), or do end of file/macro processing.
		 * In the latter case, jump back to restart from the top. */
		if(file->fp == NULL){ /* NULL if macro */
			popped++;
			recursion -= file->unrecur;

			if(recursion < 0)
				recursion = 0;

			infile = file->parent; /* Unwind file chain */
		}else{ /* Else get from a file */
			if((file->bptr = fgets(file->buffer, NBUFF, file->fp)) != NULL){
#if DEBUG
				if(debug > 1) /* Dump it to stdout */
					printf("\n#line %d (%s), %s", line, file->filename, file->buffer);
#endif
				goto newline; /* process the line */
			}else{
				fclose(file->fp);
				if((infile = file->parent) != NULL){
					/* There is an "ungotten" newline in the current
					 * infile buffer (set there by doinclude() in
					 * cpp1.c).  Thus, we know that the mainline code
					 * is skipping over blank lines and will do a
					 * #line at its convenience. */
					wrongline = TRUE;
				}
			}
		}
		/* Free up space used by the (finished) file or macro and
		 * restart input from the parent file/macro, if any. */
		free(file->filename);

		if(file->progname != NULL) /* if a #line was seen, free it, too. */
			free(file->progname);

		free((char*)file);

		if(infile == NULL)   /* If at end of file */
			return EOF_CHAR; /* Return end of file */

		line = infile->line; /* Reset line number */
		goto get_from_file;  /* Get from the top. */
	}

	/* Common processing for the new character. */
	if(c == DEF_MAGIC && file->fp != NULL) /* Don't allow delete from a file */
		goto newline;

	if(file->parent != NULL){ /* Macro or #include */
		if(popped != 0)
		file->parent->unrecur += popped;
		else {
		recursion -= file->parent->unrecur;
		if(recursion < 0)
			recursion = 0;
		file->parent->unrecur = 0;
		}
	}

	if(c == '\n')
		line++;

	if(instring)
		return c;

	else if(c == '/'){    /* Comment?            */
		instring = TRUE;  /* So get() won't loop */
		if(get() != '*'){
			instring = FALSE; /* Nope, no comment    */
			unget();          /* Push the char. back */
			return '/';
		}
		if(keepcomments){ /* If writing comments       */
			putchar('/'); /* Write out the initializer */
			putchar('*');
		}
		for(;;){ /* Eat a comment */
			c = get();
test:
			if(keepcomments && c != EOF_CHAR)
				cput(c);

			switch(c){
				case EOF_CHAR:
					cerror("EOF in comment", NULLST);
					return EOF_CHAR;

				case '/':
					if((c = get()) != '*') /* Don't let comments nest */
						goto test;
#ifdef STRICT_COMMENTS
					cwarn("Nested comments", NULLST);
#endif
				case '*':
					if((c = get()) != '/') /* If comment doesn't end, look at next */
						goto test;

					instring = FALSE; /* End of comment, */

					if(keepcomments) /* Put out the comment */
						cput(c);     /* terminator, too     */
					/* A comment is syntactically "whitespace" --
					 * however, there are certain strange sequences
					 * such as
					 *		#define foo(x)	(something)
					 *			foo|* comment *|(123)
					 *       these are '/' ^           ^
					 * where just returning space (or COM_SEP) will cause
					 * problems.  This can be "fixed" by overwriting the
					 * '/' in the input line buffer with ' ' (or COM_SEP)
					 * but that may mess up an error message.
					 * So, we peek ahead -- if the next character is
					 * "whitespace" we just get another character, if not,
					 * we modify the buffer.  All in the name of purity. */
					if(*file->bptr == '\n' || type[*file->bptr & 0xFF] == SPA)
						goto newline;
#if COMMENT_INVISIBLE
					/* Return magic (old-fashioned) syntactic space. */
					return (file->bptr[-1] = COM_SEP);
#else
					return (file->bptr[-1] = ' ');
#endif

				case '\n':
					if(!keepcomments)
						wrongline = TRUE;
				default:
					break;
			}
		}
	}else if(!inmacro && c == '\\'){
		if(get() == '\n'){
			wrongline = TRUE;
			goto newline;
		}

		unget();
		return '\\';
	}else if(c == '\f' || c == '\v')
		c = ' ';

	return c;
}

/* Backup the pointer to reread the last character.  Fatal error
 * (code bug) if we backup too far.  unget() may be called,
 * without problems, at end of file.  Only one character may
 * be ungotten.  If you need to unget more, call ungetstring(). */
void unget(void){
	register FILEINFO* file;

	if((file = infile) == NULL)
		return; /* Unget after EOF */

	if(--file->bptr < file->buffer)
		cfatal("Too much pushback", NULLST);

	if(*file->bptr == '\n') /* Ungetting a newline? */
		--line; /* Unget the line number, too */
}

/* Push a string back on the input stream.  This is done by treating
 * the text as if it were a macro. */
void ungetstring(char* text){
	register FILEINFO* file;

	file = getfile(strlen(text) + 1, "");
	strcpy(file->buffer, text);
}

/* Get one character, absorb "funny space" after comments or
 * token concatenation */
int cget(void){
	register int c;

	   do c = get();
#if COMMENT_INVISIBLE
	while(c == TOK_SEP || c == COM_SEP);
#else
	while(c == TOK_SEP);
#endif
	return c;
}

/* Error messages and other hacks.  The first byte of severity
 * is 'S' for string arguments and 'I' for int arguments.  This
 * is needed for portability with machines that have int's that
 * are shorter than  char *'s. */

/* union to prevent int -> void* -> int cast warnings */
union str_n_int{ char* s; int i; };

/* Print filenames, macro names, and line numbers for error messages. */
static void domsg(char* severity, char* format, union str_n_int arg){
	register char*     tp;
	register FILEINFO* file;

	fprintf(stderr, "%sline %d, %s: ", MSG_PREFIX, line, &severity[1]);

	if(*severity == 'S') fprintf(stderr, format, arg.s);
	                else fprintf(stderr, format, arg.i);

	putc('\n', stderr);

	if((file = infile) == NULL)
		return; /* At end of file */

	if(file->fp != NULL){
		tp = file->buffer;             /* Print current file */
		fprintf(stderr, "%s", tp);     /* name, making sure  */
		if(tp[strlen(tp) - 1] != '\n') /* there's a newline  */
			putc('\n', stderr);
	}

	while((file = file->parent) != NULL){ /* Print #includes, too */
		if(file->fp == NULL)
			fprintf(stderr, "from macro %s\n", file->filename);
		else {
			tp = file->buffer;
			fprintf(stderr, "from file %s, line %d:\n%s",
				(file->progname != NULL)
					? file->progname
					: file->filename,
				file->line, tp);
			if(tp[strlen(tp) - 1] != '\n')
				putc('\n', stderr);
		}
	}
}

/* Print a normal error message, string argument. */
void cerror(char* format, char* str_arg){
	union str_n_int u;
	u.s = str_arg;
	domsg("SError", format, u);
	errors++;
}

/* Print a normal error message, numeric argument. */
void cierror(char* format, int num_arg){
	union str_n_int u;
	u.i = num_arg;
	domsg("IError", format, u);
	errors++;
}

/* A real disaster */
void cfatal(char* format, char* str_arg){
	union str_n_int u;
	u.s = str_arg;
	domsg("SFatal error", format, u);
	exit(IO_ERROR);
}

/* A non-fatal error, string argument. */
void cwarn(char* format,char* str_arg){
	union str_n_int u;
	u.s = str_arg;
	domsg("SWarning", format, u);
}

/* A non-fatal error, numeric argument. */
void ciwarn(char* format, int num_arg){
	union str_n_int u;
	u.i = num_arg;
	domsg("IWarning", format, u);
}
