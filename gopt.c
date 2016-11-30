
/*      gopt.c
 *
 *  Copyright Robert L Parker 2016 rlp1938@gmail.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
*/

#include "fileops.h"
#include "stringops.h"
#include "gopt.h"


options_t process_options(int argc, char **argv)
{
	synopsis =
  "\tSYNOPSIS\n"
  "\thexsed [-n] [=count]/find/d filename\n\n"
  "\thexsed [-n] [=count]/find/replace/s filename\n\n"
  "\tWhere both find and replace must be strings of hex digits\n"
  "\texpressed in ASCII. The edited result is sent to stdout.\n"
  "\tThe optional count if specified will cause editing to quit once\n"
  "\tthe number of edits performed reaches the specified count.\n\n"
  "\thexsed -[a|e|i|o] char|esc sequence. Delivers the 2 digit hex\n"
  "\tASCII string that represents the input char.\n\n"
  "\thexsed -s string Delivers the 2 didgit hex ASCII string for each\n"
  "\tbyte in string.\n\n"
  "\tDESCRIPTION\n"
  "\thexsed writes the edited content of the input file to stdout. If\n"
  "\tthe hex sequence to find is not found then the output is a copy\n"
  "\tof the input. Optionally it will write a count of the deletions\n"
  "\tor changes.\n\n"
  ;
	helptext =
  "\tOPTIONS\n"
  "\t-h, --help\n\n"
  "\tOutputs the is help message and then quits.\n"
  "\t-a, --ascii\n"
  "\tChar. Outputs the 2 digit hex representation of the char input.\n\n"
  "\t-e, --escape\n"
  "\t\\char. Outputs the 2 digit hex representation of the escape\n"
  "\tsequence input. Single digits only.\n\n"
  "\t-i, --integer\n"
  "\tDecimal digits. Outputs the 2 digit hex representation of the\n"
  "\tdigits input. Legal range is 0 - 255, outside range is an error.\n"
  "\n\t-o, --octal\n"
  "\tOctal digits. Outputs the 2 digit hex representation of the\n"
  "\tdig‐ its input. Legal range is 0 - 377, outside range is an\n"
  "\terror.\n\n"
  "\t-s, --string\n"
  "\tNull terminated array of bytes. Outputs the 2 digit hex "
  "representation\n\t of each byte in the string.\n\n"
  "\t-n, --edit-count\n"
  "\tCauses the count of applied edits to be output.\n"
  ;

	optstring = ":ha:e:i:o:s:n";

	/* declare and set defaults for local variables. */

	/* set up defaults for opt vars. */
	options_t opts;
	opts.ci = 0;
	opts.line = (char *)NULL;
	opts.quiet = 1;
	opts.esc = (char *)NULL;

	int c;

	while(1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
		{"help",		0,	0,	'h' },
		{"ascii",		1,	0,	'a' },
		{"escape",		1,	0,	'e' },
		{"integer",		1,	0,	'i' },
		{"octal",		1,	0,	'o' },
		{"string",		1,	0,	's' },
		{"edit-count",	0,	0,	'n' },
		{0,	0,	0,	0 }
			};


		c = getopt_long(argc, argv, optstring,
                        long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
		case 0:
			switch (option_index) {
			} // switch()
		break;
		case 'h':
		dohelp(0);
		break;
		case 'a':
			opts.ci = optarg[0];
			fprintf(stdout, "%X\n", opts.ci);
			exit(EXIT_SUCCESS);
		break;
		case 'e':
			opts.esc = dostrdup(optarg);
		break;
		case 'i':
			opts.ci = strtoul(optarg, NULL, 10);
			fprintf(stdout, "%X\n", opts.ci);
			exit(EXIT_SUCCESS);
		break;
		case 'o':
			opts.ci = strtoul(optarg, NULL, 8);
			fprintf(stdout, "%X\n", opts.ci);
			exit(EXIT_SUCCESS);
		break;
		case 's':
			opts.line = dostrdup(optarg);
		break;
		case 'n':
			opts.quiet = 0;
		break;
		case ':':
			fprintf(stderr, "Option %s requires an argument\n",
					argv[this_option_optind]);
			dohelp(1);
		break;
		case '?':
			fprintf(stderr, "Unknown option: %s\n",
					 argv[this_option_optind]);
			dohelp(1);
		break;
		} // switch()
	} // while()
	return opts;
} // process_options()

void dohelp(int forced)
{
  if(strlen(synopsis)) fputs(synopsis, stderr);
  fputs(helptext, stderr);
  exit(forced);
} // dohelp()

