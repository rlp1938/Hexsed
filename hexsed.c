
/*      hexsed.c - stream editor uses hex strings
 *
 *	Copyright 2015 Bob Parker rlp1938@gmail.com
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
  "\n\tOptions:\n"
  "\t-h outputs this help message.\n"
  "\t-a char - outputs the hex value of char.\n"
  "\t-e \\char. Outputs  the  2  digit  hex representation of the escape"
  "\n\tsequence input. Single char only.\n"
  "\t-i decimal digits - ouputs the hex value of the digits.\n"
  "\t   Range 0-255. Outside that range is an error.\n"
  "\t-o octal digits - outputs the hex value of the digits.\n"
  "\t   Range 0-377. Outside that range is an error.\n"
  "\t-n Causes the count of applied expression to be output.\n"
  ;

typedef struct sedex {
	int op;
	int flen;
	int rlen;
	unsigned char *tofind;
	unsigned char *toreplace;
} sedex;

static char *eslookup(const char *tofind);
static void dohelp(int forced);
static sedex validate_expr(const char *expr);
static uint8_t hex2char(const char *hh, int *ecode);

int main(int argc, char **argv)
{
	int opt;
	char ch = 0;
	unsigned int ci;
	int quiet = 1;	// default no report

	while((opt = getopt(argc, argv, ":ha:e:i:o:n")) != -1) {
		switch(opt){
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
		if (!found) {
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
	char buf[NAME_MAX];
	size_t len = strlen(expr);
	if (len > NAME_MAX - 1) {
		fprintf(stderr, "Input too long:\n%s\n", expr);
		exit(EXIT_FAILURE);
	}
	strcpy(buf, expr);

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
	}
	if (badform) {
		fprintf(stderr, "Badly formed expression:\n%s\n", expr);
		exit(EXIT_FAILURE);
	}

	// format the hex lists
	sedex mysx;
	mysx.op = buf[len - 1];
	mysx.flen = mysx.rlen = 0;
	// calculate lengths
	char *cp = buf;
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
	mysx.flen /= 2;	// 1 byte out for each ascii hex pair.
	mysx.rlen /= 2;	// 1 byte out for each ascii hex pair.

	// malloc list space.
	mysx.tofind = malloc(sizeof(char) * mysx.flen);
	// I'm not really sure if I should bother about this for
	// small memory allocations.
	if (!mysx.tofind) {
		fprintf(stderr, "Could not allocate %i chars.\n", mysx.flen);
		exit(EXIT_FAILURE);
	}

	if (mysx.rlen) {
		mysx.toreplace = malloc(sizeof(char) * mysx.rlen);
		// I'm not really sure if I should bother about this for
		// small memory allocations.
		if (!mysx.toreplace) {
			fprintf(stderr, "Could not allocate %i chars.\n",
						mysx.rlen);
			exit(EXIT_FAILURE);
		}
	} else {
		mysx.toreplace = NULL;
	}

	// get the byte values for the string hex values
	cp = buf + 1;	// char after '/'
	for (i = 0; i < mysx.flen; i++) {
		int idx = i + i;
		char hbuf[3];
		strncpy(hbuf, cp + idx, 2);
		hbuf[2] = 0;
		int ecode;
		int ret = hex2char(hbuf, &ecode);
		if (ecode != 0) {
			fprintf(stderr, "Malformed hex input: %s\n", hbuf);
		} else {
			mysx.tofind[i] = ret;
		}
	} // for(i...)
	if (mysx.rlen) {
		cp = strchr(cp, '/');	// between find and replace
		cp++;	// next char
		for (i = 0; i < mysx.rlen; i++) {
			int idx = i + i;
			char hbuf[3];
			strncpy(hbuf, cp + idx, 2);
			hbuf[2] = 0;
			int ecode;
			int ret = hex2char(hbuf, &ecode);
			if (ecode != 0) {
				fprintf(stderr, "Malformed hex input: %s\n", hbuf);
			} else {
				mysx.toreplace[i] = ret;
			}
		} // for(i...)
	}
	return mysx;
} // validate_expr()

uint8_t hex2char(const char* hh, int *ecode)
{
	char *hexlist[256] = {
"00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "0A", "0B",
"0C", "0D", "0E", "0F", "10", "11", "12", "13", "14", "15", "16", "17",
"18", "19", "1A", "1B", "1C", "1D", "1E", "1F", "20", "21", "22", "23",
"24", "25", "26", "27", "28", "29", "2A", "2B", "2C", "2D", "2E", "2F",
"30", "31", "32", "33", "34", "35", "36", "37", "38", "39", "3A", "3B",
"3C", "3D", "3E", "3F", "40", "41", "42", "43", "44", "45", "46", "47",
"48", "49", "4A", "4B", "4C", "4D", "4E", "4F", "50", "51", "52", "53",
"54", "55", "56", "57", "58", "59", "5A", "5B", "5C", "5D", "5E", "5F",
"60", "61", "62", "63", "64", "65", "66", "67", "68", "69", "6A", "6B",
"6C", "6D", "6E", "6F", "70", "71", "72", "73", "74", "75", "76", "77",
"78", "79", "7A", "7B", "7C", "7D", "7E", "7F", "80", "81", "82", "83",
"84", "85", "86", "87", "88", "89", "8A", "8B", "8C", "8D", "8E", "8F",
"90", "91", "92", "93", "94", "95", "96", "97", "98", "99", "9A", "9B",
"9C", "9D", "9E", "9F", "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7",
"A8", "A9", "AA", "AB", "AC", "AD", "AE", "AF", "B0", "B1", "B2", "B3",
"B4", "B5", "B6", "B7", "B8", "B9", "BA", "BB", "BC", "BD", "BE", "BF",
"C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8", "C9", "CA", "CB",
"CC", "CD", "CE", "CF", "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7",
"D8", "D9", "DA", "DB", "DC", "DD", "DE", "DF", "E0", "E1", "E2", "E3",
"E4", "E5", "E6", "E7", "E8", "E9", "EA", "EB", "EC", "ED", "EE", "EF",
"F0", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "FA", "FB",
"FC", "FD", "FE", "FF"
					};
	int i;
	uint8_t ret;
	*ecode = 0;
	for (i = 0; i < 256; i++) {
		if (strcasecmp(hh, hexlist[i]) == 0) {
			ret = i;	// done
			return ret;
		}
	}
	*ecode = -1;	// failed
	return 0;	// legal value but ecode over rides it.
}
