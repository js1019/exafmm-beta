#!/bin/bash
#SBATCH -J test
#SBATCH -o test_%j.txt
#SBATCH -e errtest_%j.err
##SBATCH -p development
#SBATCH -p skx-dev
##SBATCH --ntasks=32
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=48
##SBATCH -c 4
#SBATCH --export=ALL
#SBATCH --time=2:00:00
##SBATCH -A TG-CCR110036
##SBATCH -A TG-EAR160023
#SBATCH -A TG-EAR170019
#SBATCH --mail-user=shijia1019@gmail.com
#SBATCH --mail-type=all

export OMP_NUM_THREADS=2
export MV2_ENABLE_AFFINITY=0

cd /work/04149/tg834482/stampede2/solver/intel18/exafmm-alpha/wrappers
ibrun ./petiga_laplace_mpi

