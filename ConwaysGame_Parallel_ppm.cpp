// team name: Old Team
// acse-kx20, acse-rt120, acse-rz1120, acse-zz420
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <time.h>
#include <vector>
#include <chrono>
#include <omp.h>

using namespace std;

// this is a parallel implementation of Conway's game of life with a periodic grid

// use 2-d vector to store the grid, variable grid means last generation grid,
// variable new_grid means next generation grid
vector<vector<bool>> grid, new_grid;
// the rows and cols of grid
int imax, jmax;
// the number of generations
int max_steps = 100;

int num_neighbours(int ii, int jj)
{
	// this function counts the number of neighbours around cell(ii, jj)

	// the coordinates of neighbour cell
	int ix, jx;
	// count of neighbours
	int cnt = 0;

	// loop over cell(ii, jj)'s neighbour
	// note we do not use parallel for here, because the number of iterations and
	// the workload in each iteration are both small,
	// that is, the time actually spent on computing is much less
	// than the time consumed by creating, call, destroying threads.
	for (int i = -1; i <= 1; i++)
		for (int j = -1; j <= 1; j++)
			// count does not include cell(ii, jj) itself
			if (i != 0 || j != 0)
			{
				// using periodic grid
				ix = (i + ii + imax) % imax;
				jx = (j + jj + jmax) % jmax;
				if (grid[ix][jx])
					cnt++;
			}
	return cnt;
}

void grid_to_file(int it)
{
	// this function outputs a grid as a ppm image file

	// set file name of image, and file stream object to open and write image
	stringstream fname;
	fstream f1;
	fname << "OldTeam_" << it << "_" << imax << "*" << jmax << ".ppm";
	f1.open(fname.str().c_str(), ios_base::out);

	// the first line of ppm is the type name P1
	// the second line of ppm is cols and rows
	// see more in https://en.wikipedia.org/wiki/Netpbm#File_formats
	f1 << "P1" << endl
	   << jmax << " " << imax << endl;

	// write each line of image (0 or 1 separated by whitespace)
	// note we cannot use parallel for here, because writing data to disk in parallel
	// is almost impossible and can easily lead to chaos.
	for (int i = 0; i < imax; i++)
	{
		for (int j = 0; j < jmax; j++)
			f1 << grid[i][j] << " ";
		f1 << endl;
	}
	f1.close();
}

void do_iteration(void)
{
	// this function uses data in grid to generate new grid i.e. do one iteration
	// note we only read data from grid and only write data to new_grid in this function

	// loop over each cell to decide it is alive or dead
	// here we use parallel for, the grid is divided approximately equally by row
	// to each thread for iteration.
	// we do not use collapse(2) here because we cannot see improvement of performance with it.
#pragma omp parallel for
	for (int i = 0; i < imax; i++)
		for (int j = 0; j < jmax; j++)
		{
			// get the current condition of the cell and its neighbour's count
			new_grid[i][j] = grid[i][j];
			int num_n = num_neighbours(i, j);
			// any live cell with less than two or more than three live neighbours dies
			if (grid[i][j])
			{
				if (num_n != 2 && num_n != 3)
					new_grid[i][j] = false;
			}
			// any dead cell with three live neighbours becomes a live cell
			else if (num_n == 3)
				new_grid[i][j] = true;
		}
}

int main(int argc, char *argv[])
{
	// start the timer
	auto time_start = chrono::steady_clock::now();
	srand(time(NULL));
	// set the size of grid
	imax = 100;
	jmax = 100;
	grid.resize(imax, vector<bool>(jmax));
	new_grid.resize(imax, vector<bool>(jmax));
	// set an initial random collection of points
	// here we use parallel for, the grid is divided approximately equally by row
	// to each thread for iteration.
#pragma omp parallel for
	for (int i = 0; i < imax; i++)
		for (int j = 0; j < jmax; j++)
			grid[i][j] = (rand() % 2);

	// we cannot write one file in parallel, but we can let do_iteration() compute
	// next generation while grid_to_file(n - 1) print last generation because both functions
	// just read data from grid but don't change data in grid.
	// enable nested parallel because in the do_iteration() we use parallel for
	omp_set_nested(1);
	// n represents the number of iterations i.e. n = 0 means initial condition,
	// n = 1 means 1 iteration from initial condion...
	for (int n = 1; n <= max_steps; n++)
	{
#pragma omp parallel sections
		{
#pragma omp section
			grid_to_file(n - 1);
#pragma omp section
			do_iteration();
		}
		// note we move "grid.swap(new_grid)" from do_iteration() to here, because
		// there is implicit barrier after sections, assigning new_grid to grid
		// will not lead to conflict only if both of the above functions have been done.
		grid.swap(new_grid);
	}
	// output the result of last iteration
	grid_to_file(max_steps);

	// end the timer and output the duration in unit second
	auto time_end = chrono::steady_clock::now();
	cout << "Grid size: " << imax << " * " << jmax << ", number of iterations: " << max_steps << endl;
	cout << "Program uses time: " << chrono::duration_cast<chrono::milliseconds>(time_end - time_start).count() / 1000.0 << " seconds" << endl;

	return 0;
}
