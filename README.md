# About the Maze Generator

The source code in this distribution provides a library and a command-line
driver for the generation of two dimensional mazes at random.  The library
files are `maze.h` and `maze.c`, the source for the driver program is
`mazegen.c`.

To build the driver, use:

    make all

You will need a GNU compatible make for this to work properly.

```
Usage:
  mazegen [options] [output-file]

  -d RxC     : specify maze dimensions (rows x columns)
  -z HxV     : specify output area (horizontal x vertical)
  -r seed    : specify random seed (default: current time)
  -m RxC-RxC : mark a path from RxC to RxC (1-based)
  -e dPos    : specify maze entrance position
  -x dPos    : specify maze exit position
  -g         : write output in PNG format
  -p         : write output in EPS format
  -t         : write output in text format (default)
  -s         : include a solution (entrance to exit)
  -h         : display this help message
```

Output is written to standard output, unless an alternative output file name is
given.  For PNG output, area is interpreted as pixels.  For EPS output, area is
interpreted as points.  For text output, area is ignored.

Entrance and exit positions are given by specifying an edge and a position on
that edge.  Edges are T, L, B, R.  Positions are indexed from one to the length
of the edge in question.

## Algorithm

The maze generation algorithm begins with a blank 2-D grid, in which each cell
of the grid is conceived as having four walls.  The walls around the outside
edges are indestructible, but all the interior walls can be "knocked down" to
connect two adjacent cells to each other.  Each cell belongs to a "group"
consisting of all the cells that are reachable by walking along the grid
without knocking down any walls.  Initially, each cell is an "island", a group
consisting of only itself.

Every cell of the grid is visited in random order.  As each cell is examined,
the algorithm looks to see if there are any adjacent cells which are in a
different path group from the current cell.  If so, one of these cells is
chosen at random, the wall separating them is knocked down, and the two cells
are "joined" into one group.  (If you are curious, this uses a simple variation
of the "union-find" data structure).

This process is repeated until no cell in the grid has any neighbors which are
in a different path group from the cell itself.  It is not too difficult to
show that this algorithm guarantees a unique path between any two points in the
grid.  This algorithm generates mazes which are "more interesting" than the
algorithm I used for Version 1 of this library, which was a randomized
depth-first search -- with the latter algorithm, there were long path segments
which were convoluted, but with no difficult branch choices.

## Copyright and License Terms

This software and its accompanying documentation, are copyright (C) 1998, 2004
M. J. Fromberger, All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Acknowledgements

Many thanks to Tom Boutell, whose free GD library makes it simple to generate
PNG output.  Thanks also to Leigh K. Williams, who found a bunch of subtle bugs
in the original version of the `mazegen` driver.

Last updated: 18-Apr-2006.
