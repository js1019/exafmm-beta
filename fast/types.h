/*
Copyright (C) 2011 by Rio Yokota, Simon Layton, Lorena Barba

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
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#ifndef types_h
#define types_h

#ifdef __INTEL_COMPILER
#pragma warning(disable:193 383 444 981 1572 2259)
#endif

#include <algorithm>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <queue>
#include <string>
#include <utility>
#include <vector>
#include "vec.h"
#include <xmmintrin.h>

#if PAPI
#include <papi.h>
#endif

#if QUARK
#include <quark.h>
#endif

#if MTHREADS
#include <mttb/task_group.h>
int omp_get_thread_num() {
  return 0;
}
#define OMP_NUM_THREADS 1
#else
#include <omp.h>
#define OMP_NUM_THREADS 12
#endif

typedef float              real;                                //!< Real number type on CPU
typedef float              gpureal;                             //!< Real number type on GPU
typedef std::complex<real> complex;                             //!< Complex number type
typedef vec<3,real>        vect;                                //!< 3-D vector type

#ifndef KERNEL
int MPIRANK    = 0;                                             //!< MPI comm rank
int MPISIZE    = 1;                                             //!< MPI comm size
int DEVICE     = 0;                                             //!< GPU device ID
int IMAGES     = 0;                                             //!< Number of periodic image sublevels
real THETA     = .5;                                            //!< Multipole acceptance criteria
vect Xperiodic = 0;                                             //!< Coordinate offset of periodic image
#if PAPI
int PAPIEVENT  = PAPI_NULL;                                     //!< PAPI event handle
#endif
#else
extern int MPIRANK;                                             //!< MPI comm rank
extern int MPISIZE;                                             //!< MPI comm size
extern int DEVICE;                                              //!< GPU device ID
extern int IMAGES;                                              //!< Number of periodic image sublevels
extern real THETA;                                              //!< Multipole acceptance criteria
extern vect Xperiodic;                                          //!< Coordinate offset of periodic image
#if PAPI
extern int PAPIEVENT;                                           //!< PAPI event handle
#endif
#endif

const int  P        = 3;                                        //!< Order of expansions
const int  NCRIT    = 10;                                       //!< Number of bodies per cell
const real EPS      = 1e-6;                                     //!< Single precision epsilon
const real EPS2     = 0;                                        //!< Softening parameter (squared)

#if COMkernel
const int MTERM = P*(P+1)*(P+2)/6-3;                            //!< Number of Cartesian mutlipole terms
#else
const int MTERM = P*(P+1)*(P+2)/6;                              //!< Number of Cartesian mutlipole terms
#endif
const int LTERM = (P+1)*(P+2)*(P+3)/6;                          //!< Number of Cartesian local terms
const int NTERM = P*(P+1)/2;                                    //!< Number of Spherical multipole/local terms

#if Cartesian
typedef vec<MTERM,real>    Mset;                                //!< Multipole coefficient type for Cartesian
typedef vec<LTERM,real>    Lset;                                //!< Local coefficient type for Cartesian
#elif Spherical
typedef vec<NTERM,complex> Mset;                                //!< Multipole coefficient type for spherical
typedef vec<NTERM,complex> Lset;                                //!< Local coefficient type for spherical
#endif

//! Structure for pthread based trace
struct Trace {
  pthread_t thread;
  double    begin;
  double    end;
  int       color;
};
typedef std::map<pthread_t,double>             ThreadTrace;     //!< Map of pthread id to traced value
typedef std::map<pthread_t,int>                ThreadMap;       //!< Map of pthread id to thread id
typedef std::queue<Trace>                      Traces;          //!< Queue of traces
typedef std::map<std::string,double>           Timer;           //!< Map of timer event name to timed value
typedef std::map<std::string,double>::iterator TI_iter;         //!< Iterator for timer event name map

enum Equation {                                                 //!< Equation type enumeration
  Laplace,                                                      //!< Laplace equation
  Yukawa,                                                       //!< Yukawa equation
  Helmholtz,                                                    //!< Helmholtz equation
  Stokes,                                                       //!< Stokes equation
  VanDerWaals                                                   //!< Van der Walls equation
};

//! Structure of source bodies (stuff to send)
struct Body {
  int  IBODY;                                                   //!< Initial body numbering for sorting back
  int  IPROC;                                                   //!< Initial process numbering for partitioning back
  int  ICELL;                                                   //!< Cell index
  vect X;                                                       //!< Position
  real SRC;                                                     //!< Scalar source values
  vec<4,real> TRG;                                              //!< Scalar+vector target values
};
typedef std::vector<Body>            Bodies;                    //!< Vector of bodies
typedef std::vector<Body>::iterator  B_iter;                    //!< Iterator for body vector

//! Linked list of leafs (only used in fast/topdown.h)
struct Leaf {
  int I;                                                        //!< Unique index for every leaf
  vect X;                                                       //!< Coordinate of leaf
  Leaf *NEXT;                                                   //!< Pointer to next leaf
};
typedef std::vector<Leaf>           Leafs;                      //!< Vector of leafs
typedef std::vector<Leaf>::iterator L_iter;                     //!< Iterator for leaf vector

//! Structure of nodes (only used in fast/topdown.h)
struct Node {
  bool NOCHILD;                                                 //!< Flag for twig nodes
  int  LEVEL;                                                   //!< Level in the tree structure
  int  NLEAF;                                                   //!< Number of descendant leafs
  int  CHILD[8];                                                //!< Index of child node
  vect X;                                                       //!< Coordinate at center
  Leaf *LEAF;                                                   //!< Pointer to first leaf
};
typedef std::vector<Node>           Nodes;                      //!< Vector of nodes
typedef std::vector<Node>::iterator N_iter;                     //!< Iterator for node vector

//! Structure of cells
struct Cell {
  int      ICELL;                                               //!< Cell index
  int      NCHILD;                                              //!< Number of child cells
  int      NCLEAF;                                              //!< Number of child leafs
  int      NDLEAF;                                              //!< Number of descendant leafs
  int      PARENT;                                              //!< Iterator offset of parent cell
  int      CHILD;                                               //!< Iterator offset of child cells
  B_iter   LEAF;                                                //!< Iterator of first leaf
  vect     X;                                                   //!< Cell center
  real     R;                                                   //!< Cell radius
  real     RMAX;                                                //!< Max cell radius
  real     RCRIT;                                               //!< Critical cell radius
  Mset     M;                                                   //!< Multipole coefficients
  Lset     L;                                                   //!< Local coefficients
};
typedef std::vector<Cell>           Cells;                      //!< Vector of cells
typedef std::vector<Cell>::iterator C_iter;                     //!< Iterator for cell vector
typedef std::queue<C_iter>          CellQueue;                  //!< Queue of cell iterators
typedef std::pair<C_iter,C_iter>    Pair;                       //!< Pair of interacting cells
typedef std::deque<Pair>            PairQueue;                  //!< Queue of interacting cell pairs

//! Structure for Ewald summation
struct Ewald {
  vect K;                                                       //!< 3-D wave number vector
  real REAL;                                                    //!< real part of wave
  real IMAG;                                                    //!< imaginary part of wave
};
typedef std::vector<Ewald>           Ewalds;                    //!< Vector of Ewald summation types
typedef std::vector<Ewald>::iterator E_iter;                    //!< Iterator for Ewald summation types

#endif