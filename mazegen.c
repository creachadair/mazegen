/*
    Name:    mazegen.c
    Purpose: Maze generation driver program.
    Author:  M. J. Fromberger <http://www.dartmouth.edu/~sting/>

    Copyright (C) 1998, 2004 M. J. Fromberger, All Rights Reserved

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
 */

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h> /* for getopt() */

#include "maze.h"

typedef struct {
  unsigned int x;
  unsigned int y;
} dims_t;

/* parse_dims(*str, *out)

   Parse a string of dimensions in the form A x B, with whitespace
   allowed.  Returns true if the parse was successful, otherwise
   false indicating a syntax error.
 */

static int parse_dims(const char *str, dims_t *out) {
  char *divider = strchr(str, 'x');
  unsigned long v;

  if (divider == NULL) return 0;

  if ((v = strtoul(str, NULL, 10)) == ULONG_MAX || (v == 0 && errno == EINVAL))
    return 0;

  out->x = (unsigned int)v;

  if ((v = strtoul(divider + 1, NULL, 10)) == ULONG_MAX ||
      (v == 0 && errno == EINVAL))
    return 0;

  out->y = (unsigned int)v;

  return 1;
}

/* parse_dim_pair(*str, *out)

   Parse a string of two positions separated by a hyphen.
 */

static int parse_dim_pair(const char *str, dims_t *out1, dims_t *out2) {
  char *divider = strchr(str, '-');

  if (divider == NULL) return 0;

  if (!parse_dims(str, out1)) return 0;

  return parse_dims(divider + 1, out2);
}

/* parse_exit_pos(*str, *out)

   Parse an exit position specification, giving a direction and a
   position.  Directions include:

   u, d, l, r    -- up, down, left, right
   t, b          -- top, bottom
   <, >, ^, v    -- left, right, top, bottom

   A position is an unsigned integer value.
 */

static int parse_exit_pos(const char *str, rowcol_t *out) {
  rowcol_t dir;
  unsigned long v;

  switch (str[0]) {
    case 't':
    case 'T':
    case 'u':
    case 'U':
    case '^':
      dir = DIR_U;
      break;
    case 'l':
    case 'L':
    case '<':
      dir = DIR_L;
      break;
    case 'r':
    case 'R':
    case '>':
      dir = DIR_R;
      break;
    case 'b':
    case 'B':
    case 'd':
    case 'D':
    case 'v':
      dir = DIR_D;
      break;
    default:
      return 0;
  }

  if ((v = strtoul(str + 1, NULL, 10)) == ULONG_MAX ||
      (v == 0 && errno == EINVAL))
    return 0;

  *out = EXIT(v - 1, dir);
  return 1;
}

/* randomizer()

   Return a pseudo-random double precision value in the half-open
   interval [0, 1).
 */

static double randomizer(void) {
  static const double w = (double)INT_MAX + 1.0;
  double v = random();

  return v / w;
}

/* set_seed(seed)

   Seed the random number generator.  Right now, this is just a
   wrapper.
 */

static void set_seed(unsigned long seed) { srandom(seed); }

static const char *g_usage = "Usage: mazegen [options] [output-file]\n";

extern char *optarg;
extern int optind;

/* Output format selectors */
#define FORMAT_TEXT 0
#define FORMAT_PNG 1
#define FORMAT_EPS 2
#define FORMAT_COMP 3

/* Solution selectors */
#define SOLN_NONE 0
#define SOLN_DEFAULT 1
#define SOLN_CHOSEN 2

int main(int argc, char *argv[]) {
  int opt, format = FORMAT_TEXT, solution = SOLN_NONE;
  int set_exit_1 = 0, set_exit_2 = 0;
  dims_t cells = {10, 10};  /* default maze dimensions, RRxCC */
  dims_t area = {612, 612}; /* default output area, HHxVV     */
  dims_t src, dst;
  unsigned long rnd_seed = (unsigned long)time(NULL);
  FILE *ofp = stdout, *ifp = NULL;
  maze_t the_maze;
  rowcol_t in, out;

  while ((opt = getopt(argc, argv, "d:z:r:m:e:x:L:cgpsth")) != EOF) {
    switch (opt) {
      case 'd':
        if (parse_dims(optarg, &cells) == 0) {
          fprintf(stderr,
                  "Error:  Incorrect format for maze dimensions\n"
                  "  -- use RRxCC format\n\n");
          return 1;
        }
        break;
      case 'z':
        if (parse_dims(optarg, &area) == 0) {
          fprintf(stderr,
                  "Error:  Incorrect format for output area\n"
                  "  -- use HHxVV format\n\n");
          return 1;
        }
        break;
      case 'r':
        if ((rnd_seed = strtoul(optarg, NULL, 0)) == ULONG_MAX ||
            (rnd_seed == 0 && errno == EINVAL)) {
          fprintf(stderr,
                  "Error:  Incorrect format for random seed\n"
                  "  -- value must be an unsigned long integer\n\n");
          return 1;
        }
        break;
      case 'm':
        if (parse_dim_pair(optarg, &src, &dst) == 0) {
          fprintf(stderr,
                  "Error:  Incorrect format for path endpoints\n"
                  "  -- use RRxCC-RRxCC format\n\n");
          return 1;
        }
        solution = SOLN_CHOSEN;
        src.x -= 1;
        src.y -= 1;
        dst.x -= 1;
        dst.y -= 1;
        break;
      case 'e':
        if (parse_exit_pos(optarg, &in) == 0) {
          fprintf(stderr,
                  "Error:  Incorrect format for entrance position\n"
                  "  -- use 'dPOS' format\n\n");
          return 1;
        }
        set_exit_1 = 1;
        break;
      case 'x':
        if (parse_exit_pos(optarg, &out) == 0) {
          fprintf(stderr,
                  "Error:  Incorrect format for exit position\n"
                  "  -- use 'dPOS' format\n\n");
          return 1;
        }
        set_exit_2 = 1;
        break;
      case 'L':
        if (strcmp(optarg, "-") == 0)
          ifp = stdin;
        else if ((ifp = fopen(optarg, "rt")) == NULL) {
          fprintf(stderr,
                  "Error:  Unable to open input file '%s'\n"
                  "  -- %s\n\n",
                  optarg, strerror(errno));
          return 1;
        }
        break;
      case 'g':
        format = FORMAT_PNG;
        break;
      case 'p':
        format = FORMAT_EPS;
        break;
      case 't':
        format = FORMAT_TEXT;
        break;
      case 's':
        solution = SOLN_DEFAULT;
        break;
      case 'c':
        format = FORMAT_COMP;
        break;
      case 'h':
        fputs(g_usage, stderr);
        fprintf(
            stderr,
            "\nCommand line options include:\n"
            "  -d RxC     : specify maze dimensions (rows x columns)\n"
            "  -z HxV     : specify output area (horizontal x vertical)\n"
            "  -r seed    : specify random seed (default: current time)\n"
            "  -m RxC-RxC : mark a path from RxC to RxC (1-based)\n"
            "  -e dPos    : specify maze entrance position\n"
            "  -x dPos    : specify maze exit position\n"
            "  -L file    : load stored maze from file (- for stdin)\n"
            "  -c         : write output in compact pickled format\n"
            "  -g         : write output in PNG format\n"
            "  -p         : write output in EPS format\n"
            "  -t         : write output in text format (default)\n"
            "  -s         : include a solution (entrance to exit)\n"
            "  -h         : display this help message\n\n"

            "Output is written to standard output, unless an alternative\n"
            "output file name is given.  For PNG output, area is interpreted\n"
            "as pixels.  For EPS output, area is interpreted as points.  For\n"
            "text output, area is ignored.\n\n"

            "Entrance and exit positions are given by specifying an edge and\n"
            "a position on that edge.  Edges are T, L, B, R.  Positions are\n"
            "indexed from one to the length of the edge in question.\n\n");
        return 0;
      default:
        fputs(g_usage, stderr);
        fputs("  [use `mazegen -h' for help with options]\n", stderr);
        return 1;
    }
  }

  set_seed(rnd_seed);

  if (cells.x == 0 || cells.y == 0) {
    fprintf(stderr,
            "Error:  A maze must have at least one row "
            "and one column\n\n");
    return 1;
  }
  if (format != FORMAT_TEXT && (area.x == 0 || area.y == 0)) {
    fprintf(stderr, "Error:  Output area requires nonzero dimensions\n\n");
    return 1;
  }

  if (optind < argc) {
    if ((ofp = fopen(argv[optind], "wb")) == NULL) {
      fprintf(stderr,
              "Error:  Unable to open output file '%s'\n"
              "  -- %s\n\n",
              argv[optind], strerror(errno));
      return 1;
    }
  }

  if (ifp != NULL) {
    if (!maze_load(&the_maze, ifp)) {
      fprintf(stderr, "Error:  Unable to load maze from input stream\n\n");
      return 1;
    }
  } else if (maze_init(&the_maze, cells.x, cells.y)) {
    if (set_exit_1) the_maze.exit_1 = in;
    if (set_exit_2) the_maze.exit_2 = out;

    if (!maze_generate(&the_maze, randomizer)) {
      fprintf(stderr,
              "Error:  Insufficient memory to generate %u x %u maze\n\n",
              the_maze.n_rows, the_maze.n_cols);
      maze_clear(&the_maze);
      return 1;
    }
  } else {
    fprintf(stderr, "Error:  Insufficient memory to create %u x %u maze\n\n",
            cells.x, cells.y);
    return 1;
  }

  if (solution == SOLN_DEFAULT) {
    rowcol_t pos, dir;

    pos = EPOS(the_maze.exit_1);
    dir = EDIR(the_maze.exit_1);
    switch (dir) {
      case DIR_U:
        src.x = 0;
        src.y = pos;
        break;
      case DIR_D:
        src.x = the_maze.n_rows - 1;
        src.y = pos;
        break;
      case DIR_L:
        src.x = pos;
        src.y = 0;
        break;
      case DIR_R:
        src.x = pos;
        src.y = the_maze.n_cols - 1;
        break;
    }

    pos = EPOS(the_maze.exit_2);
    dir = EDIR(the_maze.exit_2);
    switch (dir) {
      case DIR_U:
        dst.x = 0;
        dst.y = pos;
        break;
      case DIR_D:
        dst.x = the_maze.n_rows - 1;
        dst.y = pos;
        break;
      case DIR_L:
        dst.x = pos;
        dst.y = 0;
        break;
      case DIR_R:
        dst.x = pos;
        dst.y = the_maze.n_cols - 1;
        break;
    }
  }

  if (solution != SOLN_NONE) {
    if (src.x >= the_maze.n_rows || src.y >= the_maze.n_cols) {
      fprintf(stderr,
              "Error:  Source position %ux%u out of range\n"
              "  -- maze dimensions are %ux%u\n\n",
              src.x, src.y, the_maze.n_rows, the_maze.n_cols);
      return 1;
    }
    if (dst.x >= the_maze.n_rows || dst.y >= the_maze.n_cols) {
      fprintf(stderr,
              "Error:  Target position %ux%u out of range\n"
              "  -- maze dimensions are %ux%u\n\n",
              dst.x, dst.y, the_maze.n_rows, the_maze.n_cols);
      return 1;
    }
    maze_find_path(&the_maze, src.x, src.y, dst.x, dst.y);
  }

  fprintf(stderr,
          "Maze parameters:\n"
          "  Dimensions:  %ux%u\n"
          " Output area:  %ux%u\n"
          "      Format:  %s\n"
          " Random seed:  %ld\n"
          "      Target:  %s\n",
          the_maze.n_rows, the_maze.n_cols, area.x, area.y,
          ((format == FORMAT_TEXT)
               ? "Text"
               : ((format == FORMAT_PNG)
                      ? "PNG"
                      : (format == FORMAT_COMP) ? "Compact" : "PostScript")),
          rnd_seed, (ofp == stdout) ? "<standard output>" : argv[optind]);

  if (solution == SOLN_NONE) {
    fputs("    Solution:  NONE\n", stderr);
  } else {
    fprintf(stderr, "    Solution:  (%u x %u) to (%u x %u)\n", src.x + 1,
            src.y + 1, dst.x + 1, dst.y + 1);
  }

  switch (format) {
    case FORMAT_TEXT:
      maze_write_text(&the_maze, ofp, area.x, area.y);
      break;

    case FORMAT_PNG:
      maze_write_png(&the_maze, ofp, area.x, area.y);
      break;

    case FORMAT_EPS:
      maze_write_eps(&the_maze, ofp, area.x, area.y);
      break;

    case FORMAT_COMP:
      maze_store(&the_maze, ofp);
      break;

    default:
      assert(0 &&
             "Unknown format code in switch(format) "
             "of main(...)");
      break;
  }

  fclose(ofp);
  maze_clear(&the_maze);

  return 0;
}

/* Here there be dragons */
