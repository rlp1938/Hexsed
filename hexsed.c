
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

static char *helpmsg="\nNAME hexsed - a stream editor for hex values.\n"
  "\tSYNOPSIS\n"
  "\thexsed [-n] /hex values to find/d filename\n"
  "\thexsed [-n] /hex values to find/hex values to replace/s filename\n"
  "\thexsed -[a|i] parameter\n"
  "\thexsed -s string\n"
  "\n\tOptions:\n"
  "\t-h outputs this help message.\n"
  "\t-a char - outputs the hex value of char.\n"
  "\t-e \\char. Outputs the 2 digit hex representation of the escape"
  "\n\tsequence input. Single char only.\n"
  "\t-i decimal digits - ouputs the hex value of the digits.\n"
  "\t   Range 0-255. Outside that range is an error.\n"
  "\t-o octal digits - outputs the hex value of the digits.\n"
  "\t   Range 0-377. Outside that range is an error.\n"
  "\t-s string. Outputs the 2 byte hex representation of every byte\n"
  "\t   in the input string. Multibyte strings are accepted also.\n"
  "\t-n Causes the count of applied expression to be output.\n"
  ;

typedef struct sedex {
	int op;
	int flen;
	int rlen;
	int edcount;
	char *tofind;
	char *toreplace;
} sedex;

static char *eslookup(const char *tofind);
static void dohelp(int forced);
static sedex validate_expr(const char *expr);
static char *str2hex(const char *str);
static int validatehexstr(const char *hexstr);
static char *hex2asc(const char *hexstr);
static int hexchar2int(const char *hexpair);

int main(int argc, char **argv)
{
	int opt;
	char ch = 0;
	unsigned int ci;
	int quiet = 1;	// default no report

	while((opt = getopt(argc, argv, ":ha:e:i:o:ns:")) != -1) {
		switch(opt){
		char *line;
		case 'h':
			dohelp(0);
		break;
		case 'a':	// ascii char
			ch = optarg[0];
			ci = ch;
			fprintf(stdout, "%X\n", ci);
			exit(EXIT_SUCCESS);
		break;
		case 'e':	// escape sequence, \n etc
			fprintf(stdout, "%s\n", eslookup(optarg));
			exit(EXIT_SUCCESS);
		break;
		case 'i':	// int
			ci = strtoul(optarg, NULL, 10);
			fprintf(stdout, "%X\n", ci);
			exit(EXIT_SUCCESS);
		break;
		case 'o':	// octal
			ci = strtoul(optarg, NULL, 8);
			fprintf(stdout, "%X\n", ci);
			exit(EXIT_SUCCESS);
		break;
		// Where is case 'u' ?
		case 'n':
			quiet = 0;
		break;
		case 's':	// string
			line = str2hex(optarg);
			fprintf(stdout, "%s\n", line);
			free(line);
			exit(EXIT_SUCCESS);
		break;
		case ':':
			fprintf(stderr, "Option %c requires an argument\n",optopt);
			dohelp(1);
		break;
		case '?':
			fprintf(stderr, "Illegal option: %c\n",optopt);
			dohelp(1);
		break;
		} //switch()
	}//while()
	// now process the non-option arguments

/*	test for hexchar2int()
	int i;
	char *fmt1 = "%d %x %d %d\t";
	char *fmt2 = "%d %x %d %d\n";

	for (i = 0; i < 256; i++) {
		char buf[5];
		char *fmt = (i % 4 == 3)? fmt2:fmt1;
		sprintf(buf, "%.2x", i);
		int res = hexchar2int(buf);
		printf(fmt, i, i, res, res-i);
		if (i<10) fputs("\t", stdout);
	}
	fputs("\n", stdout);

	exit(0);
*/

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

	// 4. Check that it's meaningful, ie file/dir exists.
	if (fileexists(argv[optind]) == -1) {
		fprintf(stderr, "No such file: %s\n", argv[optind]);
		dohelp(1);
	}
	// now do the edits
	int fcount = 0;
	fdata mydat = readfile(argv[optind], 0, 0);
	char *cp = mydat.from;
	while (cp < mydat.to) {
		char *found = memmem(cp, mydat.to - cp, mysx.tofind, mysx.flen);
		int doit;
		// number of edits may be limited by count.
		doit = (found && (fcount < mysx.edcount));
		if (!doit) {
			fwrite(cp, 1, mydat.to - cp, stdout);
			cp = mydat.to;
		} else {
			fcount++;
			fwrite(cp, 1, found - cp, stdout);
			cp = found + mysx.flen;
			if (mysx.op == 's') {
				// write out the substitute bytes
				fwrite(mysx.toreplace, 1, mysx.rlen, stdout);
			}
		}
	} // while()
	if (!quiet) {
		char *what = (mysx.op == 'd') ? "deletions" : "substitutions";
		fprintf(stdout, "Did %i %s.\n", fcount, what);
	}

	free(mydat.from);
	if (mysx.toreplace) free(mysx.toreplace);
	free(mysx.tofind);
	return 0;
}//main()

void dohelp(int forced)
{
  fputs(helpmsg, stderr);
  exit(forced);
}

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
	if(!(buf[len - 1] == 'd' || buf[len - 1] == 's')) {
		badform = 1;
	}
	if (buf[len - 1] == 'd' && count == 3) {
		badform = 1;
	}
	if (buf[len - 1] == 's' && count == 2) {
		badform = 1;
	};
	if (badform) {
		fprintf(stderr, "Badly formed expression:\n%s\n", expr);
		exit(EXIT_FAILURE);
	}

	// format the hex lists
	mysx.op = buf[len - 1];
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
	if (mysx.op == 's') {
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

	if (mysx.op == 's') {
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
{	/* get the int value for hexpair 00, 01, .... ,FE, FF */
	char test[5] = {0};
	char *list =
	"0X000X010X020X030X040X050X060X070X080X090X0A0X0B0X0C0X0D0X0E0X0F"
	"0X100X110X120X130X140X150X160X170X180X190X1A0X1B0X1C0X1D0X1E0X1F"
	"0X200X210X220X230X240X250X260X270X280X290X2A0X2B0X2C0X2D0X2E0X2F"
	"0X300X310X320X330X340X350X360X370X380X390X3A0X3B0X3C0X3D0X3E0X3F"
	"0X400X410X420X430X440X450X460X470X480X490X4A0X4B0X4C0X4D0X4E0X4F"
	"0X500X510X520X530X540X550X560X570X580X590X5A0X5B0X5C0X5D0X5E0X5F"
	"0X600X610X620X630X640X650X660X670X680X690X6A0X6B0X6C0X6D0X6E0X6F"
	"0X700X710X720X730X740X750X760X770X780X790X7A0X7B0X7C0X7D0X7E0X7F"
	"0X800X810X820X830X840X850X860X870X880X890X8A0X8B0X8C0X8D0X8E0X8F"
	"0X900X910X920X930X940X950X960X970X980X990X9A0X9B0X9C0X9D0X9E0X9F"
	"0XA00XA10XA20XA30XA40XA50XA60XA70XA80XA90XAA0XAB0XAC0XAD0XAE0XAF"
	"0XB00XB10XB20XB30XB40XB50XB60XB70XB80XB90XBA0XBB0XBC0XBD0XBE0XBF"
	"0XC00XC10XC20XC30XC40XC50XC60XC70XC80XC90XCA0XCB0XCC0XCD0XCE0XCF"
	"0XD00XD10XD20XD30XD40XD50XD60XD70XD80XD90XDA0XDB0XDC0XDD0XDE0XDF"
	"0XE00XE10XE20XE30XE40XE50XE60XE70XE80XE90XEA0XEB0XEC0XED0XEE0XEF"
	"0XF00XF10XF20XF30XF40XF50XF60XF70XF80XF90XFA0XFB0XFC0XFD0XFE0XFF"
	;
	strcpy(test, "0X");
	test[2] = toupper(hexpair[0]);
	test[3] = toupper(hexpair[1]);
	char *cp = strstr(list, test);
	if (!cp) {
		fprintf(stderr, "Non-hex input: %s\n", hexpair);
		exit(EXIT_FAILURE);
	}
	int res = (cp - list)/4;
	return res;
}
