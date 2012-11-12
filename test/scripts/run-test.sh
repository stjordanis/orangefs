#!/bin/bash
script runtest.txt
export RUN_MPI_TEST
export RUN_VFS_TEST
export SVNBRANCH
export LD_LIBRARY_PATH=/opt/db4/lib
mkdir ~/test2
cd ~/test2
echo svn export --force -q http://www.orangefs.org/svn/orangefs/${SVNBRANCH}/test/run-nightly-setup
svn export --force -q http://www.orangefs.org/svn/orangefs/${SVNBRANCH}/test/run-nightly-setup
./run-nightly-setup ${SVNBRANCH}
cd ~/test2/pvfs2/test
ls
./run-nightly ${SVNBRANCH}
exit
exit