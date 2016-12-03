
/*      hexsed.c - stream editor that uses hex strings
 *
 *	Copyright 2016 Bob Parker rlp1938@gmail.com
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *	MA 02110-1301, USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdint.h>
#include "fileops.h"
#include "gopt.h"

typedef struct sedex {
	int op;
	int flen;
	int rlen;
	int edcount;
	char *tofind;
	char *toreplace;
} sedex;

static char *eslookup(const char *tofind);
static sedex validate_expr(const char *expr);
static char *str2hex(const char *str);
static int validatehexstr(const char *hexstr);
static char *hex2asc(const char *hexstr);
static int hexchar2int(const char *hexpair);
static void editfile(const char *fn, sedex mysx, int quiet);

int main(int argc, char **argv)
{
	options_t opts = process_options(argc, argv);
	// 3 opts vars are processed here, the other is dealt with in gopt.
	int quiet = opts.quiet;
	if (opts.line) {
		char *cp = str2hex(opts.line);
		fprintf(stdout, "%s\n", cp);
		free(cp);
		free(opts.line);
		exit(EXIT_SUCCESS);
	}
	if (opts.esc) {
		fprintf(stdout, "%s\n", eslookup(opts.esc));
		free(opts.esc);
		exit(EXIT_SUCCESS);
	}

	// now process the non-option arguments

	// 1.Check that argv[optind] exists.
	if (!(argv[optind])) {
		fprintf(stderr, "No expression provided\n");
		dohelp(1);
	}

	// 2. Check that it's meaningful, a valid expression.
	sedex mysx = validate_expr(argv[optind]);	// no return if error

	optind++;
	// 3.Check that argv[optind] exists.
	if (!(argv[optind])) {
		fprintf(stderr, "No file name provided\n");
		dohelp(1);
	}

	// 4. Check that it's meaningful, ie file exists.
	if (fileexists(argv[optind]) == -1) {
		fprintf(stderr, "No such file: %s\n", argv[optind]);
		dohelp(1);
	}
	// now do the edits
	char *edfile = argv[optind];
	editfile(edfile, mysx, quiet);
	return 0;
}//main()

char *eslookup(const char *tofind)
{
	static char result[3];
	const char *escseq = "abfnrtv\\'\"?";
	const char *esvalue = "07080C0A0D090B5C27223F";
	if (strlen(tofind) != 2) {
		fprintf(stderr, "Badly formed parameter: %s\n", tofind);
		exit(EXIT_FAILURE);
	}
	char *cp = strchr(escseq, tofind[1]);
	if (!cp) {
		fprintf(stderr, "Unknow escape sequence: %s\n", tofind);
		exit(EXIT_FAILURE);
	}
	int idx = (cp - escseq) * 2;
	strncpy(result, esvalue + idx, 2);
	result[2] = 0;
	return result;
} //

sedex validate_expr(const char *expr)
{
	/*
	 * Fill in the sedex struct with proper values from expr or abort
	 * if anything in expr is malformed.
	*/
	char *buf;
	char *cp;
	buf = strdup(expr);
	sedex mysx = {0};
	/* Adding the ability to specify a count of patterns to be edited.*/
	if (buf[0] == '=') {
		mysx.edcount = strtol(&buf[1], NULL, 10);
		cp = &buf[1];
		while (isdigit(*cp)) cp++;
		char *tmp = strdup(cp);
		strcpy(buf, tmp);
		free(tmp);
	} else {
		mysx.edcount = INT_MAX;
	}
	size_t len = strlen(buf);
	// test that expr has properly formed separators and command.
	int badform = 0;
	int count = 0;
	int i;
	for (i = 0; i < (int)len; i++) {
		if (buf[i] == '/') {
			count++;
		}
	}
	if (buf[0] != '/' || buf[len - 2] != '/') {
		badform = 1;
	}
	if (!(count == 2 || count == 3)) {
		badform = 1;
	}
	char op = buf[len-1];
	cp = strchr("dsai", op);
	if (!cp) badform = 1;
	if (op == 'd') {
		if (count != 2) badform = 1;
	} else {
		if (count != 3) badform = 1;
	}
	if (badform) {
		fprintf(stderr, "Badly formed expression:\n%s\n", expr);
		exit(EXIT_FAILURE);
	}

	// format the hex lists
	mysx.op = op;
	mysx.flen = mysx.rlen = 0;
	// calculate lengths
	cp = buf;
	cp++;	// get past initial '/'
	while ((*cp != '/')) {
		mysx.flen++;
		cp++;
	}

	if (mysx.op == 's') {
		cp++;	//  get past initial '/'
		while ((*cp != '/')) {
			mysx.rlen++;
			cp++;
		}
	}
	// check that we don't have 0 length strings
	if (mysx.flen == 0) {
		fprintf(stderr, "Zero length search string input %s\n", expr);
		exit(EXIT_FAILURE);
	}
	if (mysx.op == 's') {
		if (mysx.rlen == 0) {
		fprintf(stderr, "Zero length replacement string input %s\n"
					, expr);
		exit(EXIT_FAILURE);
		}
	}
	// check that user has not obviously fubarred the hex input
	if (mysx.flen %2 != 0 || mysx.rlen %2 != 0) {
		fprintf(stderr, "Each hex value must be input as a pair,"
		" eg 00..0F etc\n, %s\n", expr);
		exit(EXIT_FAILURE);
	}

	// set the find and replace strings and check for non-hex chars.
	char *tofind, *toreplace;
	cp = buf;
	cp++;	// past initial '/'
	char *ep = strchr(cp, '/');	// content of buf is valid.
	*ep = 0;
	tofind = strdup(cp);
	if (mysx.op != 'd') {
		cp = ep + 1;
		ep = strchr(cp, '/');
		*ep = 0;
		toreplace = strdup(cp);
	}
	free(buf);
	if (validatehexstr(tofind) == -1) {
		fprintf(stderr, "invalid hex chars tofind: \n %s", tofind);
		free(tofind);
		exit(EXIT_FAILURE);
	} else {
		char *res = hex2asc(tofind);
		mysx.tofind = strdup(res);
		free(res);
		free(tofind);
	}

	if (mysx.op != 'd') {
		if (validatehexstr(toreplace) == -1) {
			fprintf(stderr, "invalid hex chars toreplace: \n %s",
					toreplace);
			free(toreplace);
			exit(EXIT_FAILURE);
		} else {
			char *res = hex2asc(toreplace);
			mysx.toreplace = strdup(res);
			free(res);
			free(toreplace);
		}
	}
	// mysx.[f|r]len is double what it now is, re-assign them.
	mysx.flen = strlen(mysx.tofind);
	if (mysx.toreplace) mysx.rlen = strlen(mysx.toreplace);
	return mysx;
} // validate_expr()

char *str2hex(const char *str)
{	/* For each byte in str, return the 2 byte hex code.
	 * Handle embedded escape sequences.
	 * */
	size_t len = strlen(str);
	char *buf = calloc(2 * len + 1, 1);
	if (!buf) {
		perror("Could not get memory in str2hex()");
		exit(EXIT_FAILURE);
	}
	size_t i, idx;	// need 2 indexes to handle escape sequences.
	for (idx=0, i=0; i < len; i++, idx++) {
		char res[3] = {0};
		if (str[i] == '\\') {
			strncpy(res, &str[i], 2);
			char *cp = eslookup(res);
			strncpy(res, cp, 2);
			i++;	// get past the escaped char
		} else {
			sprintf(res, "%X", str[i]);
		}
		strncpy(&buf[2*idx], res, 2);
	}
	return buf;
}

int validatehexstr(const char *hexstr)
{
	char *validhex = "0123456789abcdefABCDEF";
	size_t len = strlen(hexstr);
	size_t i;
	for (i = 0; i < len; i++) {
		char *cp = strchr(validhex, hexstr[i]);
		if (!cp) return -1;
	}
	return 0;
}

char *hex2asc(const char *hexstr)
{	/* take a string of hex ascii represented digits and return the
	 * normal ascii representation.
	*/
	size_t len = strlen(hexstr);
	char *res = calloc(len / 2 + 1, 1);	// 1 byte result for each 2 in.
	char wrk[3] = {0};
	size_t i, idx;
	for (i=0, idx=0; i < len; idx++, i += 2) {
		strncpy(wrk, &hexstr[i], 2);
		char c = hexchar2int(wrk);
		res[idx] = c;
	}
	return res;
}

int hexchar2int(const char *hexpair)
{
	char wrk[3] = {0};
	int c;
	char *list = "0123456789ABCDEF";
	int i;
	for (i = 0; i < 2; i++) {
		wrk[i] = toupper(hexpair[i]);
		char *test = strchr(list, wrk[i]);
		if (!test) {
			fputs("\n", stdout);
			fflush(stdout);
			fprintf(stderr, "Not legal hex: %s\n", wrk);
			exit(EXIT_FAILURE);
		}
	}
	c = strtol(wrk, NULL, 16);
	return c;
} // hexchar2int()

void editfile(const char *fn, sedex mysx, int quiet)
{
	int fcount = 0;
	fdata mydat = readfile(fn, 0, 0);
	char *cp = mydat.from;
	while (cp < mydat.to) {
		char *found = memmem(cp, mydat.to - cp, mysx.tofind, mysx.flen);
		int doit;
		// number of edits may be limited by count.
		doit = (found && (fcount < mysx.edcount));
		if (!doit) {
			// write out the rest of the file.
			fwrite(cp, 1, mydat.to - cp, stdout);
			cp = mydat.to;
		} else {
			fcount++;
			// write the file content up to the find string
			fwrite(cp, 1, found - cp, stdout);
			switch (mysx.op)
			{
				case 'a':	// append to find string
					fwrite(mysx.tofind, 1, mysx.rlen, stdout);
					fwrite(mysx.toreplace, 1, mysx.rlen, stdout);
					break;
				case 'i':	// insert before find string
					fwrite(mysx.toreplace, 1, mysx.rlen, stdout);
					fwrite(mysx.tofind, 1, mysx.rlen, stdout);
					break;
				case 'd':	// delete find string
					// do nothing
					break;
				case 's':	// substitute find string.
					fwrite(mysx.toreplace, 1, mysx.rlen, stdout);
					break;
			} // switch()
			cp = found + mysx.flen;
		}
	} // while()
	if (!quiet) {
		char *what = (mysx.op == 'd') ? "deletions" : "substitutions";
		fprintf(stdout, "Did %i %s.\n", fcount, what);
	}

	free(mydat.from);
	if (mysx.toreplace) free(mysx.toreplace);
	free(mysx.tofind);

} // editfile()
