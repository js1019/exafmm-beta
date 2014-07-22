#ifndef build_tree_h
#define build_tree_h
#include <algorithm>
#include "logger.h"
#include "thread.h"
#include "types.h"
#include "utils.h"
#ifndef _OPENMP
int omp_get_num_threads() {return 1;}
int omp_get_thread_num() {return 0;}
#else
#include <omp.h>
#endif

class BuildTree {
private:
  int maxlevel;

private:
  //! Transform Xmin & Xmax to X (center) & R (radius)
  Box bounds2box(Bounds bounds) {
    vec3 Xmin = bounds.Xmin;                                    // Set local Xmin
    vec3 Xmax = bounds.Xmax;                                    // Set local Xmax
    Box box;                                                    // Bounding box
    for (int d=0; d<3; d++) box.X[d] = (Xmax[d] + Xmin[d]) / 2; // Calculate center of domain
    box.R = 0;                                                  // Initialize localRadius
    for (int d=0; d<3; d++) {                                   // Loop over dimensions
      box.R = std::max(box.X[d] - Xmin[d], box.R);              //  Calculate min distance from center
      box.R = std::max(Xmax[d] - box.X[d], box.R);              //  Calculate max distance from center
    }                                                           // End loop over dimensions
    box.R *= 1.00001;                                           // Add some leeway to radius
    bounds.Xmin = box.X - box.R;
    bounds.Xmax = box.X + box.R;
    return box;                                                 // Return box.X and box.R
  }

  //! Calculate the Morton key
  inline void getKey(Bodies &bodies, uint64_t * key, Bounds bounds, int level) {
    Box box = bounds2box(bounds);
    float d = 2 * box.R / (1 << level);                         // Cell size at current level
#pragma omp parallel for
    for (int b=0; b<int(bodies.size()); b++) {                  // Loop over bodies
      B_iter B=bodies.begin()+b;                                //  Body iterator
      int ix = (B->X[0] - bounds.Xmin[0]) / d;                  //  Index in x dimension
      int iy = (B->X[1] - bounds.Xmin[1]) / d;                  //  Index in y dimension
      int iz = (B->X[2] - bounds.Xmin[2]) / d;                  //  Index in z dimension
      int id = 0;                                               //  Initialize Morton key
      for( int l=0; l!=level; ++l ) {                           //  Loop over levels
	id += (ix & 1) << (3 * l);                              //   Interleave x bit
	id += (iy & 1) << (3 * l + 1);                          //   Interleave y bit
	id += (iz & 1) << (3 * l + 2);                          //   Interleave z bit
	ix >>= 1;                                               //   Shift x index
	iy >>= 1;                                               //   Shift y index
	iz >>= 1;                                               //   Shift z index
      }                                                         //  End loop over levels
      key[b] = id;                                              //  Store Morton key in array
      B->ICELL = id;                                            //  Store Morton key in body struct
    }                                                           // End loop over bodies
  }

  void radixSort(uint64_t * key, uint * value, uint64_t * buffer, int * permutation, int size) {
    const int bitStride = 8;
    const int stride = 1 << bitStride;
    const int mask = stride - 1;
    int numThreads;
    int (*bucketPerThread)[stride];
    uint64_t maxKey = 0;
    uint64_t * maxKeyPerThread;
#pragma omp parallel
    {
      numThreads = omp_get_num_threads();
#pragma omp single
      {
	bucketPerThread = new int [numThreads][stride]();
	maxKeyPerThread = new uint64_t [numThreads];
	for (int i=0; i<numThreads; i++)
	  maxKeyPerThread[i] = 0;
      }
#pragma omp for
      for (int i=0; i<size; i++)
	if (key[i] > maxKeyPerThread[omp_get_thread_num()])
	  maxKeyPerThread[omp_get_thread_num()] = key[i];
#pragma omp single
      for (int i=0; i<numThreads; i++)
	if (maxKeyPerThread[i] > maxKey) maxKey = maxKeyPerThread[i];
      while (maxKey > 0) {
	int bucket[stride] = {0};
#pragma omp single
	for (int t=0; t<numThreads; t++)
	  for (int i=0; i<stride; i++)
	    bucketPerThread[t][i] = 0;
#pragma omp for
	for (int i=0; i<size; i++)
	  bucketPerThread[omp_get_thread_num()][key[i] & mask]++;
#pragma omp single
	{
	  for (int t=0; t<numThreads; t++)
	    for (int i=0; i<stride; i++)
	      bucket[i] += bucketPerThread[t][i];
	  for (int i=1; i<stride; i++)
	    bucket[i] += bucket[i-1];
	  for (int i=size-1; i>=0; i--)
	    permutation[i] = --bucket[key[i] & mask];
	}
#pragma omp for
	for (int i=0; i<size; i++)
	  buffer[permutation[i]] = value[i];
#pragma omp for
	for (int i=0; i<size; i++)
	  value[i] = buffer[i];
#pragma omp for
	for (int i=0; i<size; i++)
	  buffer[permutation[i]] = key[i];
#pragma omp for
	for (int i=0; i<size; i++)
	  key[i] = buffer[i] >> bitStride;
#pragma omp single
	maxKey >>= bitStride;
      }
    }
    delete[] bucketPerThread;
    delete[] maxKeyPerThread;
  }

  void permute(Bodies & bodies, uint * index) {
    const int n = bodies.size();
    Bodies buffer = bodies;
#pragma omp parallel for
    for (int b=0; b<n; b++)
      bodies[b] = buffer[index[b]];
  }

  void bodies2leafs(Bodies & bodies, Cells & cells, Bounds bounds, int level) {
    int I = -1;
    C_iter C;
    cells.reserve(1 << (3 * level));
    Box box = bounds2box(bounds);
    float d = 2 * box.R / (1 << level);
    for (B_iter B=bodies.begin(); B!=bodies.end(); B++) {
      int IC = B->ICELL;
      int ix = (B->X[0] - bounds.Xmin[0]) / d;
      int iy = (B->X[1] - bounds.Xmin[1]) / d;
      int iz = (B->X[2] - bounds.Xmin[2]) / d;
      if( IC != I ) {
	Cell cell;
	cell.NCHILD = 0;
	cell.NBODY  = 0;
	cell.ICHILD = 0;
	cell.BODY   = B;
	cell.ICELL  = IC;
	cell.X[0]   = d * (ix + .5) + bounds.Xmin[0];
	cell.X[1]   = d * (iy + .5) + bounds.Xmin[1];
	cell.X[2]   = d * (iz + .5) + bounds.Xmin[2];
	cell.R      = d * .5;
	cells.push_back(cell);
	C = cells.end()-1;
	I = IC;
      }
      C->NBODY++;
    }
  }

  void leafs2cells(Cells & cells, Bounds bounds, int level) {
    int begin = 0, end = cells.size();
    Box box = bounds2box(bounds);
    float d = 2 * box.R / (1 << level);
    for (int l=1; l<=level; l++) {
      int div = (1 << (3 * l));
      d *= 2;
      int I = -1;
      int p = end - 1;
      for (int c=begin; c!=end; c++) {
	B_iter B = cells[c].BODY;
	int IC = B->ICELL / div;
	int ix = (B->X[0] - bounds.Xmin[0]) / d;
	int iy = (B->X[1] - bounds.Xmin[1]) / d;
	int iz = (B->X[2] - bounds.Xmin[2]) / d;
	if (IC != I) {
	  Cell cell;
	  cell.NCHILD = 0;
	  cell.NBODY  = 0;
	  cell.ICHILD = c;
	  cell.BODY   = cells[c].BODY;
	  cell.ICELL  = IC;
	  cell.X[0]   = d * (ix + .5) + bounds.Xmin[0];
	  cell.X[1]   = d * (iy + .5) + bounds.Xmin[1];
	  cell.X[2]   = d * (iz + .5) + bounds.Xmin[2];
	  cell.R      = d * .5;
	  cells.push_back(cell);
	  p++;
	  I = IC;
	}
	cells[p].NCHILD++;
	cells[p].NBODY += cells[c].NBODY;
	cells[c].IPARENT = p;
      }
      begin = end;
      end = cells.size();
    }
    cells.back().IPARENT = 0;
  }

  void reverseOrder(Cells & cells, int * permutation) {
    const int numCells = cells.size();
    int ic = numCells - 1;
    for (int c=0; c<numCells; c++,ic--) {
      permutation[c] = ic;
    }
    for (C_iter C=cells.begin(); C!=cells.end(); C++) {
      C->ICHILD = permutation[C->ICHILD] - C->NCHILD + 1;
      C->IPARENT = permutation[C->IPARENT];
    }
    std::reverse(cells.begin(), cells.end());
  }

public:
  BuildTree(int, int) : maxlevel(0) {}

  Cells buildTree(Bodies & bodies, Bounds bounds) {
    const int numBodies = bodies.size();
    const int N = numBodies; // TODO: Change to numBodies
    const int level = 5;
    maxlevel = level;

    float *X = (float*)malloc(3*N*sizeof(float));
    uint* codes = (uint*)malloc(3*N*sizeof(uint)); // TODO: Use new
    unsigned long int* mcodes = (unsigned long int*)malloc(N*sizeof(unsigned long int));
    uint long* scodes = (uint long*)malloc(N*sizeof(uint long)); // TODO: Use standard types
    uint* pointIds = (uint*)malloc(N*sizeof(uint));
    uint* index = (uint*)malloc(N*sizeof(uint));
    uint long* bins = (uint long*)malloc(N*sizeof(uint long*));
    int* levels = (int*)malloc(N*sizeof(int));
    int b = 0;
    for (B_iter B=bodies.begin(); B!=bodies.end(); B++, b++) {
      X[3*b+0] = B->X[0];
      X[3*b+1] = B->X[1];
      X[3*b+2] = B->X[2];
      index[b] = b;
    }
    int maxlev = 6;
    int maxheight = 6;
    int nbins = (1 << maxlev);
    compute_quantization_codes_T(codes, X, N, nbins);
    morton_encoding_T(mcodes, codes, N);
    uint64_t * key = new uint64_t [numBodies];
    uint64_t * buffer = new uint64_t [numBodies];
    uint * index2 = new uint [numBodies];
    int * permutation = new int [numBodies];
    Cells cells;
    for (int b=0; b<int(bodies.size()); b++) {
      index2[b] = b;
    }

    logger::startTimer("Morton key");
    getKey(bodies, key, bounds, level);
    logger::stopTimer("Morton key");

    logger::startTimer("Radix sort");
#if 0
    radixSort(key, index2, buffer, permutation, numBodies);
#else
    for (int b=0; b<int(bodies.size()); b++) {
      mcodes[b] = key[b];
    }
    bin_sort_radix6(mcodes, scodes, index2, index, bins, levels, N, 3*(maxlev-2), 0, 0, 3*(maxlev-maxheight));
#endif    
    logger::stopTimer("Radix sort");

    logger::startTimer("Permutation");
#if 0
    permute(bodies, index2);
#else
    float *input = (float*)malloc(12*N*sizeof(float));
    float *output = (float*)malloc(12*N*sizeof(float));
    b = 0;
    for (B_iter B=bodies.begin(); B!=bodies.end(); B++, b++) {
      input[12*b+0] = B->X[0];
      input[12*b+1] = B->X[1];
      input[12*b+2] = B->X[2];
      input[12*b+3] = B->SRC;
      input[12*b+4] = B->IBODY;
      input[12*b+5] = B->IRANK;
      input[12*b+6] = B->ICELL;
      input[12*b+7] = B->WEIGHT;
      input[12*b+8] = B->TRG[0];
      input[12*b+9] = B->TRG[1];
      input[12*b+10] = B->TRG[2];
      input[12*b+11] = B->TRG[3];
    }
    rearrange_dataTL(output, input, index2, N); // TODO: Use Body type directly
    b = 0;
    for (B_iter B=bodies.begin(); B!=bodies.end(); B++, b++) {
      B->X[0]   = output[12*b+0];
      B->X[1]   = output[12*b+1];
      B->X[2]   = output[12*b+2];
      B->SRC    = output[12*b+3];
      B->IBODY  = output[12*b+4];
      B->IRANK  = output[12*b+5];
      B->ICELL  = output[12*b+6];
      B->WEIGHT = output[12*b+7];
      B->TRG[0] = output[12*b+8];
      B->TRG[1] = output[12*b+9];
      B->TRG[2] = output[12*b+10];
      B->TRG[3] = output[12*b+11];
    }
#endif
    logger::stopTimer("Permutation");

    logger::startTimer("Bodies to leafs");
    bodies2leafs(bodies, cells, bounds, level);
    logger::stopTimer("Bodies to leafs");

    logger::startTimer("Leafs to cells");
    leafs2cells(cells, bounds, level);
    logger::stopTimer("Leafs to cells");

    logger::startTimer("Reverse order");
    reverseOrder(cells, permutation);
    logger::stopTimer("Reverse order");

    delete[] key;
    delete[] buffer;
    delete[] index2;
    delete[] permutation;
    return cells;
  }

  //! Print tree structure statistics
  void printTreeData(Cells & cells) {
    if (logger::verbose && !cells.empty()) {                    // If verbose flag is true
      logger::printTitle("Tree stats");                         //  Print title
      std::cout  << std::setw(logger::stringLength) << std::left//  Set format
		 << "Bodies"     << " : " << cells.front().NBODY << std::endl// Print number of bodies
		 << std::setw(logger::stringLength) << std::left//  Set format
		 << "Cells"      << " : " << cells.size() << std::endl// Print number of cells
		 << std::setw(logger::stringLength) << std::left//  Set format
		 << "Tree depth" << " : " << maxlevel << std::endl;//  Print number of levels
    }                                                           // End if for verbose flag
  }
};
#endif
