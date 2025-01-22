/*  cpp2.c
 *  Process #control lines */

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include"cppdef.h"
#include"cpp.h"

/* Generate (by hand-inspection) a set of unique values for each control
 * operator.  Note that this is not guaranteed to work for non-Ascii
 * machines.  CPP won't compile if there are hash conflicts. */
#define	L_assert  ('a' + ('s' << 1))
#define	L_define  ('d' + ('f' << 1))
#define	L_elif    ('e' + ('i' << 1))
#define	L_else    ('e' + ('s' << 1))
#define	L_endif   ('e' + ('d' << 1))
#define	L_if      ('i' + (EOS << 1))
#define	L_ifdef	  ('i' + ('d' << 1))
#define	L_ifndef  ('i' + ('n' << 1))
#define	L_include ('i' + ('c' << 1))
#define	L_line    ('l' + ('n' << 1))
#define	L_nogood  (EOS + (EOS << 1)) /* To catch #i */
#define	L_pragma  ('p' + ('a' << 1))
#define L_undef   ('u' + ('d' << 1))
#if DEBUG
#	define	L_debug   ('d' + ('b' << 1)) /* #debug   */
#	define	L_nodebug ('n' + ('d' << 1)) /* #nodebug */
#endif

void doif     (int hash);
void doinclude(void);

extern char*   savestring  (char* text);
extern void    cwarn       (char* format, char* sarg);
extern void    cerror      (char* format, char* sarg);
extern void    cfatal      (char* format, char* sarg);
extern int     get         (void);
extern void    unget       (void);
extern DEFBUF* lookid      (int c);
extern int     macroid     (register int c);
extern int     eval        (void);
extern void    skipnl      (void);
extern int     skipws      (void);
extern int     openfile    (char* file_name);
extern void    dodefine    (void);
extern void    doinclude   (void);
extern void    save        (register int c);
extern int     openinclude (char* file_name, int search_local);
extern void    scanid      (register int c);
extern void    doinclude   (void);
extern void    doundef     (void);
extern int     hasdirectory(char* source, char* result);
extern void    dumpdef     (char* why);

/* Process #control lines.  Simple commands are processed inline,
 * while complex commands have their own subroutines.
 *
 * The counter is used to force out a newline before #line, and
 * #pragma commands.  This prevents these commands from ending up at
 * the end of the previous line if cpp is invoked with the -C option. */
int control(int counter /* Pending newline counter */){
	register int   c;
	register char* tp;
	register int   hash;
	char* ep;

	c = skipws();
	if(c == '\n' || c == EOF_CHAR)
		return (counter + 1);

	if(!isdigit(c))
		scanid(c);
	else{
		unget();               /* Hack -- allow #123 as a */
		strcpy(token, "line"); /* synonym for #line 123   */
	}

	hash = (token[1] == EOS)
		? L_nogood
		: (token[0] + (token[2] << 1));

	switch(hash){
		case L_assert:  tp = "assert";  break;
		case L_define:  tp = "define";  break;
		case L_elif:    tp = "elif";    break;
		case L_else:    tp = "else";    break;
		case L_endif:   tp = "endif";   break;
		case L_if:      tp = "if";      break;
		case L_ifdef:   tp = "ifdef";   break;
		case L_ifndef:  tp = "ifndef";  break;
		case L_include: tp = "include"; break;
		case L_line:    tp = "line";    break;
		case L_pragma:  tp = "pragma";  break;
		case L_undef:   tp = "undef";   break;
#if DEBUG
		case L_debug:   tp = "debug";   break;
		case L_nodebug: tp = "nodebug"; break;
#endif
		default:      hash = L_nogood;
		case L_nogood:  tp = "";        break;
	}

	if(!streq(tp, token))
		hash = L_nogood;

	/* hash is set to a unique value corresponding to the
	 * control keyword (or L_nogood if we think it's nonsense). */
	if(infile->fp == NULL)
		cwarn("Control line \"%s\" within macro expansion", token);

	if(!compiling){
		switch(hash){
			case L_if:     /* These can't turn    */
			case L_ifdef:  /* compilation on, but */
			case L_ifndef: /* we must nest #if's  */
			if(++ifptr >= &ifstack[BLK_NEST])
				goto if_nest_err;

			*ifptr = 0;

			case L_line:
			/* Are pragma's always processed? */
			case L_pragma:
			case L_include:
			case L_define:
			case L_undef:
			case L_assert:
dump_line:
				skipnl();
			return (counter + 1);
		}
	}

	/* Make sure that #line and #pragma are output on a fresh line. */
	if(counter > 0 && (hash == L_line || hash == L_pragma)){
		putchar('\n');
		counter--;
	}

	switch(hash){
		case L_line:
			/* Parse the line to update the line number and "progname"
			 * field and line number for the next input line.
			 * Set wrongline to force it out later. */
			c = skipws();
			workp = work; /* Save name in work */
			while(c != '\n' && c != EOF_CHAR){
				save(c);
				c = get();
			}
			unget();
			save(EOS);
			/* Split #line argument into <line-number> and <name>
			 * We subtract 1 as we want the number of the next line. */
			line = atoi(work) - 1; /* Reset line number */

			/* Skip over digits */
			for(tp = work; isdigit(*tp) || type[(int)(*tp)] == SPA; tp++);

			if(*tp != EOS){ /* Got a filename, so: */

				if(*tp == '"' && (ep = strrchr(tp + 1, '"')) != NULL){
					tp++;      /* Skip over left quote */
					*ep = EOS; /* And ignore right one */
				}

				if(infile->progname != NULL)
					free(infile->progname);

				infile->progname = savestring(tp);
			}
			wrongline = TRUE;
			break;
		case L_include:
			doinclude();
			break;
		case L_define:
			dodefine();
			break;
		case L_undef:
			doundef();
			break;
		case L_else:
			if(ifptr == &ifstack[0])
				goto nest_err;
			else if((*ifptr & ELSE_SEEN) != 0)
				goto else_seen_err;

			*ifptr |= ELSE_SEEN;
			if((*ifptr & WAS_COMPILING) != 0)
				compiling = !(compiling || (*ifptr & TRUE_SEEN) != 0);
			break;
		case L_elif:
			if(ifptr == &ifstack[0])
				goto nest_err;
			else if((*ifptr & ELSE_SEEN) != 0){
else_seen_err:
				cerror("#%s may not follow #else", token);
				goto dump_line;
			}
			if((*ifptr & (WAS_COMPILING | TRUE_SEEN)) != WAS_COMPILING){
				compiling = FALSE;
				goto dump_line;
			}
			doif(L_if);
			break;
		case L_if:
		case L_ifdef:
		case L_ifndef:
			if(++ifptr >= &ifstack[BLK_NEST])
if_nest_err:
				cfatal("Too many nested #%s statements", token);
			*ifptr = WAS_COMPILING;
			doif(hash);
			break;
		case L_endif:
			if(ifptr == &ifstack[0]){
nest_err:
				cerror("#%s must be in an #if", token);
				goto dump_line;
			}
			if(!compiling && (*ifptr & WAS_COMPILING) != 0)
				wrongline = TRUE;
			compiling = ((*ifptr & WAS_COMPILING) != 0);
			--ifptr;
			break;
		case L_assert:
			if(eval() == 0)
			cerror("Preprocessor assertion failure", NULLST);
			break;
		case L_pragma:
			/* #pragma is provided to pass "options" to later
			 * passes of the compiler.  cpp doesn't have any yet. */
			printf("#pragma ");
			while((c = get()) != '\n' && c != EOF_CHAR)
				cput(c);
			unget();
			break;
#if DEBUG
		case L_debug:
			if(debug == 0)
				dumpdef("debug set on");
			debug++;
			break;
		case L_nodebug:
			debug--;
			break;
#endif
		default:
			/* Undefined #control keyword.
			 * Note: the correct behavior may be to warn and
			 * pass the line to a subsequent compiler pass.
			 * This would allow #asm or similar extensions. */
			cerror("Illegal # command \"%s\"", token);
			break;
	}

	if(hash != L_include){
#if OLD_PREPROCESSOR
		/* Ignore the rest of the #control line so you can write
		 * #if foo
		 * #endif foo */
		goto dump_line; /* Take common exit */
#else
		if(skipws() != '\n'){
			cwarn("Unexpected text in #control line ignored", NULLST);
			skipnl();
		}
#endif
	}

	return (counter + 1);
}

/* Process an #if, #ifdef, or #ifndef.  The latter two are straightforward,
 * while #if needs a subroutine of its own to evaluate the expression.
 *
 * doif() is called only if compiling is TRUE.  If false, compilation
 * is always supressed, so we don't need to evaluate anything.  This
 * supresses unnecessary warnings. */
void doif(int hash){
	register int c;
	register int found;

	if((c = skipws()) == '\n' || c == EOF_CHAR){
		unget();
		goto badif;
	}

	if(hash == L_if){
		unget();
		found = (eval() != 0);
		hash = L_ifdef; /* #if is now like #ifdef */
	}else{
		if(type[c] != LET) /* Next non-blank isn't letter */
			goto badif;    /* ... is an error             */

		found = (lookid(c) != NULL); /* Look for it in symbol table */
	}

	if(found == (hash == L_ifdef)){
		compiling = TRUE;
		*ifptr |= TRUE_SEEN;
	}else
		compiling = FALSE;

	return; /* good if */

badif:
	cerror("#if, #ifdef, or #ifndef without an argument", NULLST);
#if !OLD_PREPROCESSOR
	skipnl(); /* Prevent an extra */
	unget();  /* Error message    */
#endif
	return; /* bad if */
}

/* Process the #include control line.
 * There are three variations:
 *	#include "file"		search somewhere relative to the
 *				current source file, if not found,
 *				treat as #include <file>.
 *	#include <file>		Search in an implementation-dependent
 *				list of places.
 *	#include token		Expand the token, it must be one of
 *				"file" or <file>, process as such. */
void doinclude(void){
	register int c;
	register int delim;

	delim = macroid(skipws());

	if(delim != '<' && delim != '"')
		goto incerr;

	if(delim == '<')
		delim = '>';

	workp = work;
	instring = TRUE; /* Accept all characters */
#ifdef CONTROL_COMMENTS_NOT_ALLOWED

	while((c = get()) != '\n' && c != EOF_CHAR)
		save(c);

	unget(); /* Force nl after include */

	/* NOT STANDARD */
	while(--workp >= work && *workp == ' '); /* Trim blanks from filename */

	if(*workp != delim)
		goto incerr;
#else
	while((c = get()) != delim && c != EOF_CHAR)
		save(c);
#endif
	*workp = EOS;
	instring = FALSE;

	if(openinclude(work, (delim == '"')))
		return;

	/* No sense continuing if #include file isn't there. */
	cfatal("Cannot open include file \"%s\"", work);

incerr:
	cerror("#include syntax error", NULLST);
	return;
}

/* Actually open an include file.  This routine is only called from
 * doinclude() above, but was written as a separate subroutine for
 * programmer convenience.  It searches the list of directories
 * and actually opens the file, linking it into the list of
 * active files.  Returns TRUE if the file was opened, FALSE
 * if openinclude() fails.  No error message is printed. */
int openinclude(char* file_name, int search_local){
	register char** incptr;
	char tmpname[NWORK]; /* Filename work area */

	if(search_local){
		/* Look in local directory first */

		/* Try to open filename relative to the directory of the current
		 * source file (as opposed to the current directory). (ARF, SCK). */
		if(file_name[0] != '/' && hasdirectory(infile->filename, tmpname))
			strcat(tmpname, file_name);
		else
			strcpy(tmpname, file_name);

		if(openfile(tmpname))
			return TRUE;
	}

	/* Look in any directories specified by -I command line
	 * arguments, then in the builtin search list. */
	for(incptr = incdir; incptr < incend; incptr++){
		if(strlen(*incptr) + strlen(file_name) >= (NWORK - 1))
		cfatal("Filename work buffer overflow", NULLST);
		else {

		if(file_name[0] == '/')
			strcpy(tmpname, file_name);
		else {
			sprintf(tmpname, "%s/%s", *incptr, file_name);
		}

		if(openfile(tmpname))
			return TRUE;
		}
	}
	return FALSE;
}

/* If a device or directory is found in the source filename string, the
 * node/device/directory part of the string is copied to result and
 * hasdirectory returns TRUE.  Else, nothing is copied and it returns FALSE. */
int hasdirectory(char* source, char* result){
	register char* tp;

	if((tp = strrchr(source, '/')) == NULL)
		return FALSE;

    strncpy(result, source, tp - source + 1);
    result[tp - source + 1] = EOS;
    return TRUE;
}
