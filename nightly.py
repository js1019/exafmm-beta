import itertools
import subprocess
kernelList = ['laplace_cartesian','laplace_spherical','helmholtz_spherical','biotsavart_spherical','laplace_cartesian_mpi','laplace_spherical_mpi','helmholtz_spherical_mpi','biotsavart_spherical_mpi','ewald_mpi']
nList = ['2','10','100','1000','10000','100000']
tList = ['.5','.4','.3']
TList = ['1','2','4']
cList = ['1','8','64','512']
dList = ['c','l','s','o','p']
for kernel in kernelList:
  if 'mpi' in kernel:
    argList = ['g','j','m','o','x']
    npList = ['1','2','4','8','16']
  else:
    argList = ['j','m','o','x']
    npList = ['1']
  if 'ewald' in kernel:
    iList = ['3']
  else:
    iList = ['0','1','2','3']
  for np in npList:
    exe = ' '.join([''.join(['./examples/',kernel]),'-aDv','-p','./examples/','-r','10'])
    if 'mpi' in kernel:
      exe = ' '.join(['mpirun','-np',np,exe])
    for n in nList:
      for t in tList:
        for T in TList:
          for c in cList:
            for d in dList:
              for i in iList:
                for numArgs in range(0, len(argList)+1):
                  for args in itertools.combinations(argList, numArgs):
                    args = list(args)
                    if len(args): args.insert(0,'-')
                    args = ' '.join([exe,''.join(args),'-n',n,'-t',t,'-T',T,'-c',c,'-d',d,'-i',i])
                    print args
                    try:
                      subprocess.check_call(args, shell=True)
                    except subprocess.CalledProcessError:
                      print 'Regression failed @',args
                      raise SystemExit
