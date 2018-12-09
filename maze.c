/*
  Name:     maze.c
  Purpose:  Maze generation library.
  Author:   M. J. Fromberger <http://www.dartmouth.edu/~sting/>

  Copyright (C) 1998-2006 M. J. Fromberger, All Rights Reserved.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
 */

#include "maze.h"

#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

#include "gd.h"

#define LINE_WIDTH 80 /* characters */

/* Population count for adjacent cell values */
static unsigned int adj_pop[] = {
  0, 1, 1, 2, 1, 2, 2, 3,
  1, 2, 2, 3, 2, 3, 3, 4
};

/* {{{ s_can_move(*mp, r, c, dir)

   Return true if it is possible to move in the given direction from
   the cell whose row and column are specified.  You can move in a
   direction from a cell if there is no wall in the way.

   Note that there are magic unbreakable walls around the entire
   maze.
 */

static int s_can_move(maze_t *mp, rowcol_t r, rowcol_t c, 
		      unsigned int dir)
{
  assert(0 <= dir && dir < 4);
  switch(dir) {
  case DIR_U:
    return (r > 0 && CELLV(mp, r - 1, c).b_wall == 0);
  case DIR_R:
    return (c < mp->n_cols && CELLV(mp, r, c).r_wall == 0);
  case DIR_D:
    return (r < mp->n_rows - 1 && CELLV(mp, r, c).b_wall == 0);
  default:
    return (c > 0 && CELLV(mp, r, c - 1).r_wall == 0);
  }
}

/* }}} */

/* {{{ s_findset(mp, pos)

   Find which path set the given position occupies.  Performs path
   compression as needed.
 */

rowcol_t s_findset(maze_t *mp, rowcol_t pos)
{
  if(mp->sets[pos] != pos) {
    rowcol_t real = s_findset(mp, mp->sets[pos]);

    mp->sets[pos] = real;
  }

  return mp->sets[pos];
}

/* }}} */

/* {{{ s_union(mp, pos1, pos2)

   Put positions pos1 and pos2 into the same path set.
 */

void    s_union(maze_t *mp, rowcol_t pos1, rowcol_t pos2)
{
  rowcol_t  set1 = s_findset(mp, pos1);
  rowcol_t  set2 = s_findset(mp, pos2);

  mp->sets[set1] = set2;
}

/* }}} */

/* {{{ s_adj(*mp, pos) 

   Return a bit vector flagging which directions you can go from the
   given maze position to reach a different path set than the position
   is currently in.
 */

rowcol_t s_adj(maze_t *mp, rowcol_t pos) 
{
  rowcol_t  set = s_findset(mp, pos);
  rowcol_t  out = 0, r, c;

  r = pos / mp->n_cols;
  c = pos % mp->n_cols;

  if(r > 0 && s_findset(mp, OFFSET(mp, r - 1, c)) != set)
    out |= (1 << DIR_U);
  if(r < mp->n_rows - 1 && s_findset(mp, OFFSET(mp, r + 1, c)) != set)
    out |= (1 << DIR_D);
  if(c > 0 && s_findset(mp, OFFSET(mp, r, c - 1)) != set)
    out |= (1 << DIR_L);
  if(c < mp->n_cols - 1 && s_findset(mp, OFFSET(mp, r, c + 1)) != set)
    out |= (1 << DIR_R);

  return out;
}

/* }}} */

/* {{{ maze_init(*mp, nr, nc)

   Create a new maze structure with nr rows and nc columns.  The
   resulting maze is initialized to "all walls".
 */

int    maze_init(maze_t *mp, rowcol_t nr, rowcol_t nc)
{
  unsigned int n_cells = nr * nc;

  assert(mp != NULL);
  assert(n_cells > 0);
  
  if((mp->cells = malloc(n_cells * sizeof(*(mp->cells)))) == NULL)
    return 0; /* out of memory */

  mp->n_rows = nr;
  mp->n_cols = nc;
  mp->sets = NULL;
  mp->exit_1 = EXIT(0, DIR_L);
  mp->exit_2 = EXIT(nr - 1, DIR_R);
  maze_reset(mp);

  return 1;
}

/* }}} */

/* {{{ maze_load(*mp, *ifp)

   Load a maze from the given file stream, in the pickled format
   generated by maze_store().  Returns false in case of error; a true
   result means the load was successful.
*/

int    maze_load(maze_t *mp, FILE *ifp)
{
  rowcol_t rows, cols, exit_1, exit_2;
  rowcol_t r, c;
  int      result, ch;

  result = fscanf(ifp, "%u %u %u %u\n",
		  &rows, &cols, &exit_1, &exit_2);
  if(result == EOF || result < 4) {
    fprintf(stderr, "maze_load:  missing dimension line\n");
    return 0;
  }

  if(!maze_init(mp, rows, cols))
    return 0;

  mp->exit_1 = exit_1;
  mp->exit_2 = exit_2;

  ch = fgetc(ifp);
  for(r = 0; r < rows; ++r) {
    while(isspace(ch))
      ch = fgetc(ifp);

    for(c = 0; c < cols; ++c) {
      maze_node  cell = { 0, 0, DIR_U, 0 };
      rowcol_t   v;
      
      if(ch == EOF) {
	fprintf(stderr, "maze_load:  premature end of input at %u x %u\n", 
		r, c);
	return 0;
      }
      cell.visit = isupper(ch) ? 1 : 0;
      v = cell.visit ? (ch - 'A') : (ch - 'a');

      cell.r_wall = v & 1;
      cell.b_wall = (v >> 1) & 1;
      cell.marker = (v >> 2) & 3;

      CELLV(mp, r, c) = cell;

      do 
	ch = fgetc(ifp);
      while(isspace(ch));
    }
  }

  return 1;
}

/* }}} */

/* {{{ maze_store(*mp, *ofp)

   Write a compact representation of the maze as text to the given
   output file stream.
 */

void   maze_store(maze_t *mp, FILE *ofp)
{
  rowcol_t  r, c, pos = 0;

  fprintf(ofp, "%u %u %u %u\n",
	  mp->n_rows, mp->n_cols, mp->exit_1, mp->exit_2);
  
  for(r = 0; r < mp->n_rows; ++r) {
    for(c = 0; c < mp->n_cols; ++c) {
      maze_node cell = CELLV(mp, r, c);
      rowcol_t  v = 0;
      
      v |= cell.marker << 2;
      v |= cell.b_wall << 1;
      v |= cell.r_wall;

      if(cell.visit)
	fputc('A' + v, ofp);
      else
	fputc('a' + v, ofp);

      pos = (pos + 1) % LINE_WIDTH;
      if(!pos)
	fputc('\n', ofp);
    }
  }
  if(pos)
    fputc('\n', ofp);
}

/* }}} */

/* {{{ maze_clear(*mp) 

   Release the memory occupied by a maze structure.  It is safe to
   call this multiple times on the same structure.
 */

void   maze_clear(maze_t *mp)
{
  assert(mp != NULL);

  if(mp->cells != NULL)
    free(mp->cells);

  mp->cells = NULL;
  mp->n_rows = 0;
  mp->n_cols = 0;
}

/* }}} */

/* {{{ maze_reset(*mp)

   Reset every cell of the maze to have two walls (right and bottom),
   its marker oriented UP, and its visited bit off.
 */

void   maze_reset(maze_t *mp)
{
  rowcol_t   r, c;
  maze_node  def;

  assert(mp != NULL);

  def.r_wall = 1;
  def.b_wall = 1;
  def.marker = DIR_U;
  def.visit  = 0;

  for(r = 0; r < mp->n_rows; ++r) 
    for(c = 0; c < mp->n_cols; ++c) 
      CELLV(mp, r, c) = def;
}

/* }}} */

/* {{{ maze_unmark(*mp)

   Clear all visitation marks and reset markers to up.
 */

void   maze_unmark(maze_t *mp)
{
  rowcol_t  pos, n_cells;

  assert(mp != NULL);

  n_cells = mp->n_rows * mp->n_cols;

  for(pos = 0; pos < n_cells; ++pos) {
    mp->cells[pos].visit = 0;
    mp->cells[pos].marker = DIR_U;
  }
}

/* }}} */

/* {{{ maze_generate(*mp, start_row, start_col, random)

   Generate a random maze, starting at th
 */

int   maze_generate(maze_t *mp, rand_f random)
{
  rowcol_t  n_cells = mp->n_rows * mp->n_cols;
  rowcol_t *queue;
  rowcol_t  pos, count = 0;

  maze_reset(mp);

  if((mp->sets = malloc(n_cells * sizeof(*(mp->sets)))) == NULL)
    return 0; /* out of memory */
  
  if((queue = malloc(n_cells * sizeof(*queue))) == NULL) {
    free(mp->sets);
    mp->sets = NULL;
    return 0; /* out of memory */
  }

  /* Initially, all cells belong to their own set, and the queue is in
     scan order. */
  for(pos = 0; pos < n_cells; ++pos) {
    queue[pos] = pos;
    mp->sets[pos] = pos;
  }

  while(count < n_cells) {
    /* As long as there are cells which have neighbours not in their
       own set, scan the queue and connect unrelated regions.
       
       Each cell is examined to see if it has any adjacent cells which
       are in a different path set.  If not, randomly choose a
       neighbor in a different path set, and kick down the wall
       between them.
    */

    /* Reshuffle the queue */
    for(pos = n_cells - 1; pos > 0; --pos) {
      double   v = random();
      rowcol_t exch = (rowcol_t)(v * (pos + 1)), t;
      
      t = queue[pos];
      queue[pos] = queue[exch];
      queue[exch] = t;
    }

    /* Scan the queue */
    for(pos = 0; pos < n_cells; ++pos) {
      rowcol_t  cur = queue[pos];
      rowcol_t  adj, apop, wall, r, c;
      rowcol_t  skip = 0;
      
      adj =  s_adj(mp, cur);
      apop = adj_pop[adj];
      
      if(apop == 0) {
	count += 1;
	continue;
      }
      
      if(apop > 1)
	skip = (rowcol_t)(random() * apop);
      
      for(wall = 0; wall < 4; ++wall) {
	if((adj >> wall) & 1) {
	  if(skip == 0)
	    break;
	  
	  --skip;
	}
      }
      
      /* Now, wall is the direction of the wall to kick down */
      r = cur / mp->n_cols;
      c = cur % mp->n_cols;
      
      switch(wall) {
      case DIR_U: --r; CELLV(mp, r, c).b_wall = 0; break;
      case DIR_R: CELLV(mp, r, c).r_wall = 0; ++c; break;
      case DIR_D: CELLV(mp, r, c).b_wall = 0; ++r; break;
      case DIR_L: --c; CELLV(mp, r, c).r_wall = 0; break;
      default:
	assert(0 && "Unreachable case in switch(wall) of "
	       "maze_generate(...)");
	break;
      }
      
      /* Join this cell to the path set of the one we just connected to */
      s_union(mp, OFFSET(mp, r, c), cur);
    }
  }

  /* When finished, clean up temporary memory */
  free(mp->sets);
  mp->sets = NULL;
  free(queue);
  return 1;
}

/* }}} */

/* {{{ maze_find_path(*mp, start_row, start_col, end_row, end_col)

   Find and mark a path from the specified starting position of the
   maze to the specified ending position, if possible.  This assumes
   the maze is a connected graph.

   The algorithm is simple right-handed depth-first search, using the
   markers and visited bits of the maze cells to keep track of where
   the search has been.  It is possible this will not terminate if the
   goal is not reachable from the start.
 */

void   maze_find_path(maze_t *mp, rowcol_t start_row, rowcol_t start_col,
		      rowcol_t end_row, rowcol_t end_col)
{
  rowcol_t   c_row, c_col;

  maze_unmark(mp);

  c_row = start_row;
  c_col = start_col;

  for(;;) {
    unsigned int  c_dir, num_walls;

    if(c_row == end_row && c_col == end_col)
      break; /* done, found the goal */

    /* Find a direction to go which is a clear path */
    c_dir = CELLV(mp, c_row, c_col).marker;

    for(num_walls = 0; num_walls < 4; ++num_walls) {
      c_dir = (c_dir + 1) % 4;

      /* Mark which way we're about to move */
      if(s_can_move(mp, c_row, c_col, c_dir)) {
	CELLV(mp, c_row, c_col).marker = c_dir; 
	break;
      }
    }

    /* Safety check, shouldn't trigger unless you're trying to find a
       path in an incomplete maze, which is an error. */
    assert(num_walls < 4 || 
	   "Unescapable start position in maze_find_path(...)");
    
    /* Move in that new direction, and set the new cell's marker to
       point the way back in case we need to backtrack. */
    switch(c_dir) {
    case DIR_U:
      c_row--; CELLV(mp, c_row, c_col).marker = DIR_D;
      break;
    case DIR_R:
      c_col++; CELLV(mp, c_row, c_col).marker = DIR_L;
      break;
    case DIR_D:
      c_row++; CELLV(mp, c_row, c_col).marker = DIR_U;
      break;
    case DIR_L:
      c_col--; CELLV(mp, c_row, c_col).marker = DIR_R;
      break;
    default:
      assert(0 && "Unreachable case in switch(c_dir) "
	     "of maze_find_path(...)");
      break;
    }
  } /* end for(ever) */

  /* At this point, we have found the path, and the markers point the
     route back to the starting cell.  This loop traverses the path
     back to the start and sets all the visited flags of the cells
     that are on-route.
   */
  c_row = start_row;
  c_col = start_col;
  do {
    CELLV(mp, c_row, c_col).visit = 1;
    
    switch(CELLV(mp, c_row, c_col).marker) {
    case DIR_U: c_row--; break;
    case DIR_R: c_col++; break;
    case DIR_D: c_row++; break;
    case DIR_L: c_col--; break;
    default:
      assert(0 && "Unreachable case in switch(marker) "
	     "of maze_find_path(...)");
    }
  } while(c_row != end_row || c_col != end_col);

  CELLV(mp, c_row, c_col).visit = 1;
} 

/* }}} */

/* {{{ maze_write_png(*mp, *ofp, h_res, v_res)

   Write the specified maze as a PNG file to the given output stream.
   The resulting image file is h_res pixels wide and v_res pixels
   tall.  Solutions are plotted if present.
 */

void   maze_write_png(maze_t *mp, FILE *ofp,
		      unsigned int h_res, unsigned int v_res)
{
  gdImagePtr   img;
  unsigned int h_wid, v_wid;
  int          clr_black, clr_white, clr_path;
  rowcol_t     r, c, h_base, v_base;
  rowcol_t     p1, p2, dir1, dir2;

  p1 = EPOS(mp->exit_1); dir1 = EDIR(mp->exit_1);
  if(dir1 == DIR_R)
    CELLV(mp, p1, mp->n_cols - 1).r_wall = 0;
  else if(dir1 == DIR_D)
    CELLV(mp, mp->n_rows - 1, p1).b_wall = 0;

  p2 = EPOS(mp->exit_2); dir2 = EDIR(mp->exit_2);
  if(dir2 == DIR_R)
    CELLV(mp, p2, mp->n_cols - 1).r_wall = 0;
  else if(dir2 == DIR_D)
    CELLV(mp, mp->n_rows - 1, p2).b_wall = 0;

  img = gdImageCreate(h_res + 1, v_res + 1);

  h_wid = h_res / mp->n_cols;
  v_wid = v_res / mp->n_rows;

  /* Allocate colours for background, maze lines, and solutions */
  clr_black = gdImageColorAllocate(img, 0, 0, 0);
  clr_white = gdImageColorAllocate(img, 255, 255, 255);
  clr_path  = gdImageColorAllocate(img, 102, 102, 255);
  
  /* Clear the image to the background colour */
  gdImageFilledRectangle(img, 0, 0, h_res, v_res, clr_white);

  /* Draw the top and left exterior walls */
  for(c = 0; c < mp->n_cols; ++c) {
    if((dir1 == DIR_U && p1 == c) ||
       (dir2 == DIR_U && p2 == c))
      continue;
    else
      gdImageLine(img, c * h_wid, 0, 
		  c * h_wid + h_wid, 0, clr_black);
  }
  
  for(r = 0; r < mp->n_rows; ++r) {
    if((dir1 == DIR_L && p1 == r) ||
       (dir2 == DIR_L && p2 == r))
      continue;
    else {
      gdImageLine(img, 0, r * v_wid, 
		  0, r * v_wid + v_wid, clr_black);
    }
  }
  
  for(r = 0; r < mp->n_rows; ++r) {
    v_base = r * v_wid;
    
    for(c = 0; c < mp->n_cols; ++c) {
      h_base = c * h_wid;

      if(CELLV(mp, r, c).r_wall)
	gdImageLine(img, h_base + h_wid, v_base, 
		    h_base + h_wid, v_base + v_wid, 
		    clr_black);
      
      if(CELLV(mp, r, c).b_wall)
	gdImageLine(img, h_base, v_base + v_wid, 
		    h_base + h_wid, v_base + v_wid, 
		    clr_black);
      
      /* Mark path components, if present */
      if(CELLV(mp, r, c).visit) {
	rowcol_t   left = 0, top = 0, width = 0, height = 0;
	
	switch(CELLV(mp, r, c).marker) {
	case DIR_U: case DIR_D:
	  width = h_wid - 4;
	  height = 2 * v_wid - 4;
	  break;
	case DIR_L: case DIR_R:
	  width = 2 * h_wid - 4;
	  height = v_wid - 4;
	  break;
	}

	switch(CELLV(mp, r, c).marker) {
	case DIR_R: case DIR_D:
	  left = h_base + 2; top = v_base + 2;
	  break;
	case DIR_L:
	  left = h_base - h_wid + 2;
	  top  = v_base + 2;
	  break;
	case DIR_U:
	  left = h_base + 2;
	  top  = v_base - v_wid + 2;
	  break;
	}

	gdImageFilledRectangle(img, left, top, 
			       left + width, top + height, 
			       clr_path);
      }

    } /* end column loop */
  } /* end row loop */

  gdImagePng(img, ofp);
  gdImageDestroy(img);
}

/* }}} */

/* {{{ maze_write_eps(*mp, *ofp, h_res, v_res)

   Write an Encapsulated PostScript (EPS) version of the given maze to
   the specified output stream.  A two-point padding is placed around
   the bounding box of the resulting figure.
 */

void   maze_write_eps(maze_t *mp, FILE *ofp, 
		      unsigned int h_res, unsigned int v_res)
{
  static const double  line_width = 1.0; /* Weight of walls */
  static const double  line_grey  = 0.0; /* Colour of walls */
  static const double  soln_grey  = 0.7; /* Colour of solution markers */
  static const double  soln_gap   = 0.2; /* % marker gap from walls */
  
  double  d_hres = (double)h_res, d_vres = (double)v_res;
  double  h_wid = d_hres / mp->n_cols;
  double  v_wid = d_vres / mp->n_rows;
  
  rowcol_t  r, c;
  rowcol_t  p1, p2, dir1, dir2;

  p1 = EPOS(mp->exit_1); dir1 = EDIR(mp->exit_1);
  if (dir1 == DIR_R)
    CELLV(mp, p1, mp->n_cols - 1).r_wall = 0;
  else if(dir1 == DIR_D)
    CELLV(mp, mp->n_rows - 1, p1).b_wall = 0;

  p2 = EPOS(mp->exit_2); dir2 = EDIR(mp->exit_2);
  if(dir2 == DIR_R)
    CELLV(mp, p2, mp->n_cols - 1).r_wall = 0;
  else if(dir2 == DIR_D)
    CELLV(mp, mp->n_rows - 1, p2).b_wall = 0;

  /* Emit minimal EPS header */
  fprintf(ofp, 
	  "%%!PS-Adobe-3.0 EPSF-3.0\n"
	  "%%%%BoundingBox: %d %d %u %u\n"
	  "%%%%DocumentData: Clean7Bit\n\n",
	  -2, -2, h_res + 2, v_res + 2);

  /* Emit common definitions */
  fprintf(ofp, 
	  "/np  {newpath} bind def\n"
	  "/slw {setlinewidth} bind def\n"
	  "/sg  {setgray} bind def\n"
	  "/mt  {moveto} bind def\n"
	  "/rmt {rmoveto} bind def\n"
	  "/lt  {lineto} bind def\n"
	  "/rlt {rlineto} bind def\n"
	  "/stk {stroke} bind def\n"
	  "/sgrey %.1f def\n"
	  "/lgrey %.1f def\n"
	  "/lwid  %.1f def\n"
	  "/dr {lwid slw lgrey sg stk} def\n\n",
	  soln_grey, line_grey, line_width);

  /* Draw top and left walls */
  fprintf(ofp, 
	  "%% Exterior walls\n"
	  "np\n%u %u mt\n", 0, v_res);
  
  for(c = 0; c < mp->n_cols; ++c) {
    if((dir1 == DIR_U && p1 == c) ||
       (dir2 == DIR_U && p2 == c))
      fprintf(ofp, "%.1f 0 rmt ", h_wid);
    else
      fprintf(ofp, "%.1f 0 rlt ", h_wid);
  }
  fprintf(ofp, 
	  "dr\n"
	  "np\n%u %u mt\n", 0, v_res);

  for(r = 0; r < mp->n_rows; ++r) {
    if((dir1 == DIR_L && p1 == r) ||
       (dir2 == DIR_L && p2 == r))
      fprintf(ofp, "0 %.1f neg rmt ", v_wid);
    else
      fprintf(ofp, "0 %.1f neg rlt ", v_wid);
  }
  fputs("dr\n\n", ofp);
  
  for(r = 0; r < mp->n_rows; ++r) {
    double v_base = r * v_wid;

    for(c = 0; c < mp->n_cols; ++c) {
      double h_base = c * h_wid;
      maze_node n = CELLV(mp, r, c);
      
      if(n.r_wall || n.b_wall) {
	fprintf(ofp, "np ");
	
	if(n.r_wall)
	  fprintf(ofp, "%.1f %.1f mt 0 %.1f neg rlt ",
		  h_base + h_wid, 
		  v_res - v_base,
		  v_wid);
	
	if(n.b_wall)
	  fprintf(ofp, "%.1f %.1f mt %.1f 0 rlt ",
		  h_base, 
		  v_res - v_base - v_wid,
		  h_wid);
	
	fprintf(ofp, "dr\n");
      }

      if(n.visit) {
	double hp = 0., vp = 0., h_dis = 0., v_dis = 0.;

	switch(n.marker) {
	case DIR_U: case DIR_D:
	  h_dis = (1.0 - 2 * soln_gap) * h_wid;
	  v_dis = (2.0 - 2 * soln_gap) * v_wid;
	  break;
	case DIR_L: case DIR_R:
	  h_dis = (2.0 - 2 * soln_gap) * h_wid;
	  v_dis = (1.0 - 2 * soln_gap) * v_wid;
	  break;
	}
	
	switch(n.marker) {
	case DIR_U: 
	  hp = h_base + soln_gap * h_wid;
	  vp = v_base - (1.0 - soln_gap) * v_wid;
	  break;
	case DIR_D: case DIR_R:
	  hp = h_base + soln_gap * h_wid;
	  vp = v_base + soln_gap * v_wid;
	  break;
	case DIR_L:
	  hp = h_base - (1.0 - soln_gap) * h_wid;
	  vp = v_base + soln_gap * v_wid;
	  break;
	}
	
	fprintf(ofp, "np %.1f %.1f mt %.1f 0 rlt 0 %.1f neg rlt "
		"%.1f neg 0 rlt 0 %.1f rlt ",
		hp, v_res - vp, h_dis, v_dis, h_dis, v_dis);
	fprintf(ofp, "sgrey sg fill\n");
      }

    } /* end column loop */
  } /* end row loop */
}

/* }}} */

/* {{{ maze_write_text(*mp, *ofp, h_res, v_res)

   Write a maze in a plain-text format.  The h_res and v_res
   parameters are ignored (they are accepted so that the write
   functions will have a uniform interface).
 */

void   maze_write_text(maze_t *mp, FILE *ofp, 
		       unsigned int h_res, unsigned int v_res)
{
  rowcol_t  r, c, p1, dir1, p2, dir2;
  
  /* Down-facing and right-facing exits can be modelled by editing the
     maze to kick out the walls in the appropriate cells. */
  p1 = EPOS(mp->exit_1); dir1 = EDIR(mp->exit_1);
  if(dir1 == DIR_D)
    CELLV(mp, mp->n_rows - 1, p1).b_wall = 0;
  else if(dir1 == DIR_R)
    CELLV(mp, p1, mp->n_cols - 1).r_wall = 0;

  p2 = EPOS(mp->exit_2); dir2 = EDIR(mp->exit_2);
  if(dir2 == DIR_D)
    CELLV(mp, mp->n_rows - 1, p2).b_wall = 0;
  else if(dir2 == DIR_R)
    CELLV(mp, p2, mp->n_cols - 1).r_wall = 0;
  
  /* Draw the top border, respecting possible exits */
  for(r = 0; r < mp->n_cols; ++r) {
    if((dir1 == DIR_U && p1 == r) ||
       (dir2 == DIR_U && p2 == r)) {
      fprintf(ofp, "+   "); 
    }
    else
      fprintf(ofp, "+---");
  }
  fputc('+', ofp);
  fputc('\n', ofp);

  /* Draw all the cells.  The left border is drawn as we go, because
     of the line-oriented nature of stream output. */
  for(r = 0; r < mp->n_rows; ++r) {
    if((dir1 == DIR_L && p1 == r) ||
       (dir2 == DIR_L && p2 == r))
      fputc(' ', ofp);
    else
      fputc('|', ofp);
    
    for(c = 0; c < mp->n_cols; ++c) {
      if(CELLV(mp, r, c).visit) 
	fprintf(ofp, " @ ");
      else
	fprintf(ofp, "   ");

      fputc(CELLV(mp, r, c).r_wall ? '|' : ' ', ofp);
    }
    fputc('\n', ofp);
    fputc('+', ofp);

    for(c = 0; c < mp->n_cols; ++c) 
      fputs(CELLV(mp, r, c).b_wall ? "---+" : "   +", ofp);
    
    fputc('\n', ofp);
  }
}

/* }}} */

/* Here there be dragons */
