#ifndef _REGEXP_H_
#define _REGEXP_H_ 1

/*
 * Definitions etc. for regexp(3) routines.
 *
 * Caveat:  this is V8 regexp(3) [actually, a reimplementation thereof],
 * not the System V one.
 */
// #define void int
#define CHARBITS 0377
// #define strchr index
#define NSUBEXP  10
typedef struct regexp {
	char *startp[NSUBEXP];
	char *endp[NSUBEXP];
	char regstart;		/* Internal use only. */
	char reganch;		/* Internal use only. */
	char *regmust;		/* Internal use only. */
	int regmlen;		/* Internal use only. */
	char program[1];	/* Unwarranted chumminess with compiler. */
} regexp;

extern regexp *regcomp();
extern int regexec();
extern void regsub(regexp *prog, char *source, char *dest);
extern void regerror();

#endif
