/*  cpp4.c
 *  Macro Definitions */

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include"cppdef.h"
#include"cpp.h"

/* parm[], parmp, and parlist[] are used to store #define() argument
 * lists.  nargs contains the actual number of parameters stored. */
static char  parm[NPARMWORK + 1]; /* define param work buffer   */
static char* parmp;               /* Free space in parm         */
static char* parlist[LASTPARM];   /* -> start of each parameter */
static int   nargs;               /* Parameters for this macro  */

void stparmscan(int delim);
void checkparm (register int c, DEFBUF* dp);
void expstuff  (DEFBUF* tokenp);
void charput   (register int c);
void textput   (char* text);
void dumpparm  (char* why);

extern void      save       (register int c);
extern DEFBUF*   defendel   (char* name, int delete);
extern char*     savestring (char* text);
extern DEFBUF*   lookid     (int c);
extern void      cwarn      (char* format, char* sarg);
extern void      ciwarn     (char* format, int narg);
extern void      cerror     (char* format, char* sarg);
extern void      cfatal     (char* format, char* sarg);
extern FILEINFO* getfile    (int bufsize, char* name);
extern int       skipws     (void);
extern int       get        (void);
extern void      unget      (void);
extern int       cget       (void);
extern int       scanstring (register int delim, void (*outfun)(int));
extern void      scannumber (register int c, register void (*outfun)(int));
extern void      ungetstring(char* text);
extern int       expcollect (void);
extern void      scanid     (register int c);
extern void      dumpadef   (char* why, register DEFBUF* dp);

/* Called from control when a #define is scanned.  This module
 * parses formal parameters and the replacement string.  When
 * the formal parameter name is encountered in the replacement
 * string, it is replaced by a character in the range 128 to
 * 128+NPARAM (this allows up to 32 parameters within the
 * Dec Multinational range).  If cpp is ported to an EBCDIC
 * machine, you will have to make other arrangements.
 *
 * There is some special case code to distinguish
 *	#define foo	bar
 * from	#define foo()	bar
 *
 * Also, we make sure that
 *	#define	foo	foo
 * expands to "foo" but doesn't put cpp into an infinite loop.
 *
 * A warning message is printed if you redefine a symbol to a
 * different text.  I.e,
 *	#define	foo	123
 *	#define foo	123
 * is ok, but
 *	#define foo	123
 *	#define	foo	+123
 * is not.
 *
 * The following subroutines are called from define():
 * checkparm	called when a token is scanned.  It checks through the
 *		array of formal parameters.  If a match is found, the
 *		token is replaced by a control byte which will be used
 *		to locate the parameter when the macro is expanded.
 * textput	puts a string in the macro work area (parm[]), updating
 *		parmp to point to the first free byte in parm[].
 *		textput() tests for work buffer overflow.
 * charput	puts a single character in the macro work area (parm[])
 *		in a manner analogous to textput(). */
void dodefine(void){
	register int     c;
	register DEFBUF* dp; /* -> new definition */
	int   isredefine; /* TRUE if redefined  */
	char* old;        /* Remember redefined */

	if(type[(c = skipws())] != LET)
		goto bad_define;

	isredefine = FALSE;

	/* check if name is known */
	if((dp = lookid(c)) == NULL)
		dp = defendel(token, FALSE);
	else{
		isredefine = TRUE;
		old        = dp->repl;
		dp->repl   = NULL;
	}

	parlist[0] = parmp = parm;

	if((c = get()) == '('){ /* check for arguments */
		nargs = 0;
		do{
			if(nargs >= LASTPARM)
				cfatal("Too many arguments for macro", NULLST);
			else if((c = skipws()) == ')')
				break;
			else if(type[c] != LET) /* Bad formal syntax */
				goto bad_define;

			scanid(c);
			parlist[nargs++] = parmp; /* Save it */
			textput(token); /* Save text in parm[] */
		}while((c = skipws()) == ','); /* Get another argument */

		if(c != ')') /* Must end at ) */
			goto bad_define;

		c = ' ';
	}else
		nargs = DEF_NOARGS; /* No () parameters */

	if(type[c] == SPA) /* At whitespace? */
		c = skipws();  /* Not any more.  */

	workp   = work; /* Replacement put here */
	inmacro = TRUE; /* Keep \<newline> now */
	while(c != EOF_CHAR && c != '\n'){ /* Compile macro body */
#if OK_CONCAT
#if COMMENT_INVISIBLE
		if(c == COM_SEP){
			save(TOK_SEP);
			c = get();
#else
		if(c == '#'){ /* Token concatenation? */
			while(workp > work && type[(int)workp[-1]] == SPA)
				--workp; /* Erase leading spaces */

			save(TOK_SEP);
			c = skipws();
#endif
			if(type[c] == LET);
			else if(type[c] == DIG){
				while(type[c] == DIG){
					save(c);
					c = get();
				}
				save(TOK_SEP); /* Delimit 2nd token */
			}
#if !COMMENT_INVISIBLE
			else ciwarn("Strange character after # (%d.)", c);
#endif
			continue;
		}
#endif
		switch(type[c]){
			case LET:
				checkparm(c, dp);
				break;
			case DIG:case DOT:
				scannumber(c, save);
				break;
			case QUO:
#if STRING_FORMAL
				stparmscan(c, dp); /* Do string magic */
#else
				stparmscan(c);
#endif
				break;
			case BSH:
				save('\\');
				if((c = get()) == '\n')
					wrongline = TRUE;

				save(c);
				break;
			case SPA:
				/* Note: the "end of comment" marker is passed on
				 * to allow comments to separate tokens. */
				if(workp[-1] == ' ') /* Absorb multiple spaces */
					break;
				else if(c == '\t')
					c = ' ';

				/* Fall through to store character */
				default:
				save(c);
				break;
		}

		c = get();
	}
	inmacro = FALSE;
	unget(); /* For control check */

	if(workp > work && workp[-1] == ' ') /* Drop trailing blank */
		workp--;

	*workp = EOS;
	dp->repl = savestring(work);
	dp->nargs = nargs;
#if DEBUG
	if(debug)
		dumpadef("macro definition", dp);
#endif
	if(isredefine){ /* Error if redefined */
		if((old != NULL && dp->repl != NULL && !streq(old, dp->repl))
		|| (old == NULL && dp->repl != NULL)
		|| (old != NULL && dp->repl == NULL)){
#ifdef STRICT_UNDEF
			cerror("Redefining defined variable \"%s\"", dp->name);
#else
			cwarn("Redefining defined variable \"%s\"", dp->name);
#endif
		}
		if(old != NULL)
			free(old);
	}
	return;

bad_define:
	cerror("#define syntax error", NULLST);
	inmacro = FALSE;
}

/* Replace this param if it's defined.  Note that the macro name is a
 * possible replacement token.  We stuff DEF_MAGIC in front of the token
 * which is treated as a LETTER by the token scanner and eaten by
 * the output routine.  This prevents the macro expander from
 * looping if someone writes "#define foo foo". */
void checkparm(register int c, DEFBUF* dp){
	register int   i;
	register char* cp;

	scanid(c);
	for(i = 0; i < nargs; i++){
		if(streq(parlist[i], token)){
			save(i + MAC_PARM); /* Save a magic cookie */
			return;
		}
	}
	if(streq(dp->name, token))
		save(DEF_MAGIC); /* Save magic marker */

	for(cp = token; *cp != EOS;)
		save(*cp++);
}

#if STRING_FORMAL
/* Scan the string (starting with the given delimiter).
 * The token is replaced if it is the only text in this string or
 * character constant.  The algorithm follows checkparm() above.
 * Note that scanstring() has approved of the string. */
void stparmscan(int delim, register DEFBUF* dp){
	register int c;

	/* Warning -- this code hasn't been tested for a while.
	 * It exists only to preserve compatibility with earlier
	 * implementations of cpp.  It is not part of the Draft
	 * ANSI Standard C language. */
	save(delim);
	instring = TRUE;
	while((c = get()) != delim
		 && c != '\n'
		 && c != EOF_CHAR){
		if(type[c] == LET) /* Maybe formal parm */
			checkparm(c, dp);
		else{
			save(c);
			if(c == '\\')
				save(get());
		}
	}

	instring = FALSE;
	if(c != delim)
		cerror("Unterminated string in macro body", NULLST);

	save(c);
}
#else

/* Normal string parameter scan. */
void stparmscan(int delim){
	register char* wp;
	register int   i;

	wp = workp;
	if(!scanstring(delim, save))
		return;

	workp[-1] = EOS; /* Erase trailing quote */
	wp++;

	for(i = 0; i < nargs; i++){
		if(streq(parlist[i], wp)){
			*wp++ = (char)(MAC_PARM + PAR_MAC); /* Stuff a magic marker */
			*wp++ = (i + MAC_PARM);             /* Make a formal marker */
			*wp   = wp[-3];                     /* Add on closing quote */
			workp = wp + 1;                     /* Reset string end     */
			return;
		}
	}
	workp[-1] = wp[-1]; /* Nope, reset end quote. */
}
#endif

/* Remove the symbol from the defined list.
 * Called from the #control processor. */
void doundef(void){
	register int c;

	if(type[(c = skipws())] != LET){
		cerror("Illegal #undef argument", NULLST);
		return;
	}

    scanid(c);
    if(defendel(token, TRUE) == NULL){
#ifdef STRICT_UNDEF
		cwarn("Symbol \"%s\" not defined in #undef", token);
#endif
    }
}

/* Put the string in the parm[] buffer. */
void textput(char* text){
	register int size;

	size = strlen(text) + 1;
	if((parmp + size) >= &parm[NPARMWORK]){
		cfatal("Macro work area overflow", NULLST);
		return;
	}

    strcpy(parmp, text);
    parmp += size;
}

/* Put the byte in the parm[] buffer. */
void charput(register int c){
	if(parmp >= &parm[NPARMWORK]){
		cfatal("Macro work area overflow", NULLST);
		return;
	}

    *parmp++ = c;
}

/* M a c r o   E x p a n s i o n */
static DEFBUF* macro; /* Catches start of infinite macro */

/* Expand a macro.  Called from the cpp mainline routine (via subroutine
 * macroid()) when a token is found in the symbol table.  It calls
 * expcollect() to parse actual parameters, checking for the correct number.
 * It then creates a "file" containing a single line containing the
 * macro with actual parameters inserted appropriately.  This is
 * "pushed back" onto the input stream.  (When the get() routine runs
 * off the end of the macro line, it will dismiss the macro itself.) */
void expand(register DEFBUF* tokenp){
	register int       c;
	register FILEINFO* file;

#if DEBUG
	if(debug)
		dumpadef("expand entry", tokenp);
#endif

	/* If no macro is pending, save the name of this macro
	 * for an eventual error message. */
	if(recursion++ == 0)
		macro = tokenp;
	else if(recursion == RECURSION_LIMIT){
		cerror("Recursive macro definition of \"%s\"", tokenp->name);
		fprintf(stderr, "(Defined by \"%s\")\n", macro->name);
		if(rec_recover){
			   do get();
			while(infile != NULL && infile->fp == NULL);

			unget();
			recursion = 0;
			return;
		}
	}

	/* Here's a macro to expand. */
	nargs = 0;
	parmp = parm;
	switch(tokenp->nargs){
		case (-2): /* __LINE__ */
			sprintf(work, "%d", line);
			ungetstring(work);
			break;
		case (-3): /* __FILE__ */
			for(file = infile; file != NULL; file = file->parent){
				if(file->fp != NULL){
					sprintf(work, "\"%s\"",
						(file->progname != NULL)
							? file->progname
							: file->filename
					);
					ungetstring(work);
					break;
				}
			}
			break;
		default:
			/* Normal macro. */
			if(tokenp->nargs < 0)
				cfatal("Bug: Illegal __ macro \"%s\"", tokenp->name);

			/* Look for '(', skipping spaces / newline*/
			while((c = skipws()) == '\n')
				wrongline = TRUE;

			if(c != '('){
				/*  output
				 *    #define foo ...
				 *  When given
				 *    #define foo() ...
				 *                ^-- (Useless Parentheses)  */
				unget();
				cwarn("Macro \"%s\" needs arguments", tokenp->name);
				fputs(tokenp->name, stdout);
				return;
			}

			if(expcollect()){
				if(tokenp->nargs != nargs){
					cwarn("Wrong number of macro arguments for \"%s\"",
					tokenp->name);
				}
#if DEBUG
				if(debug)
					dumpparm("expand");
#endif
			}
		case DEF_NOARGS:
			expstuff(tokenp);
	}
}

/* Collect the actual parameters for this macro.  TRUE if ok. */
int expcollect(void){
	register int c;
	register int paren;

	for(;;){
		paren = 0;
		while((c = skipws()) == '\n')
			wrongline = TRUE;

		if(c == ')'){
			/* Note that there is a guard byte in parm[]
			 * so we don't have to check for overflow here. */
			*parmp = EOS;
			break;
		}else if(nargs >= LASTPARM)
			cfatal("Too many arguments in macro expansion", NULLST);

		parlist[nargs++] = parmp; /* At start of new arg */

		for(;; c = cget()){

			if(c == EOF_CHAR){
				cerror("end of file within macro argument", NULLST);
				return FALSE;
			}else if(c == '\\'){ /* Quote next character */
				charput(c); /* Save the \ for later */
				charput(cget()); /* Save the next char. */
				continue; /* And go get another */
			}else if(type[c] == QUO){ /* Start of string? */
				scanstring(c, charput); /* Scan it off */
				continue; /* Go get next char */
			}else if(c == '(') /* Worry about balance */
				paren++; /* To know about commas */
			else if(c == ')'){ /* Other side too */
				if(paren == 0){ /* At the end? */
					unget(); /* Look at it later */
					break; /* Exit arg getter. */
				}
				paren--; /* More to come. */
			}else if(c == ',' && paren == 0) /* Comma delimits args */
				break;
			else if(c == '\n') /* Newline inside arg? */
				wrongline = TRUE; /* We'll need a #line */

			charput(c);
		}
		charput(EOS);
#if DEBUG
		if(debug)
			printf("parm[%d] = \"%s\"\n", nargs, parlist[nargs - 1]);
#endif
	} /* Collect all args. */
	return TRUE;
}

/* Stuff the macro body, replacing formal parameters by actual parameters. */
void expstuff(DEFBUF* tokenp){
	register int   c;
	register char* inp;  /* -> repl string */
	register char* defp; /* -> macro output buff */
	int       size;
	char*     defend;
	int       string_magic;
	FILEINFO* file;

	file   = getfile(NBUFF, tokenp->name);
	inp    = tokenp->repl; /* -> macro replacement */
	defp   = file->buffer; /* -> output buffer */
	defend = defp + (NBUFF - 1);

	if(inp != NULL){
		while((c = (*inp++ & 0xFF)) != EOS){
			if(c >= MAC_PARM && c <= (MAC_PARM + PAR_MAC)){
				string_magic = (c == (MAC_PARM + PAR_MAC));
				if(string_magic)
			 		c = (*inp++ & 0xFF);

				/* Replace formal parameter by actual parameter string. */
				if((c -= MAC_PARM) < nargs){
					size = strlen(parlist[c]);
					if((defp + size) >= defend)
						goto nospace;

					/* Erase the extra set of quotes. */
					if(string_magic && defp[-1] == parlist[c][0]){
						strcpy(defp-1, parlist[c]);
						defp += (size - 2);
					}else{
						strcpy(defp, parlist[c]);
						defp += size;
					}
				}
			}else if(defp >= defend){
nospace:
				cfatal("Out of space in macro \"%s\" arg expansion", tokenp->name);
			}else
				*defp++ = c;
		}
	}

	*defp = EOS;
#if DEBUG
	if(debug > 1)
		printf("macroline: \"%s\"\n", file->buffer);
#endif
}

#if DEBUG

/* Dump parameter list. */
void dumpparm(char* why){
	register int i;

	printf("dump of %d parameters (%d bytes total) %s\n", nargs, (int)(parmp - parm), why);

	for(i = 0; i < nargs; i++){
		printf("parm[%d] (%d) = \"%s\"\n", i + 1, (int)strlen(parlist[i]), parlist[i]);
	}
}
#endif
