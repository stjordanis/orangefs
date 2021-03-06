#!/bin/bash
#
# A PVFS2 test script using only sysint test programs
#
# This script focuses on making sure that basic mkdir,
# create, lookup, and symlink programs are working properly.
#
# During operation, this test stresses the lookup operation
# and symlink resolution.
#

# check if a sysint test path was specified
if test "x$1" = "x"; then
    SYSINT_TEST_DIR=.
else
    SYSINT_TEST_DIR=$1
fi

# check if we can find the mkdir program
if ! test -x $SYSINT_TEST_DIR/mkdir; then
    echo "Cannot find sysint test program: $SYSINT_TEST_DIR/mkdir"
    exit 1
fi

# check if we can find the create program
if ! test -x $SYSINT_TEST_DIR/create; then
    echo "Cannot find sysint test program: $SYSINT_TEST_DIR/create"
    exit 1
fi

# check if we can find the symlink program
if ! test -x $SYSINT_TEST_DIR/symlink; then
    echo "Cannot find sysint test program: $SYSINT_TEST_DIR/symlink"
    exit 1
fi

# check if we can find the lookup program
if ! test -x $SYSINT_TEST_DIR/lookup; then
    echo "Cannot find sysint test program: $SYSINT_TEST_DIR/lookup"
    exit 1
fi

# absolute links now require the file system root,
# so read this from the tab file
TAB_FILE=$PVFS2TAB_FILE
if test "x$TAB_FILE" = "x";  then
    TAB_FILE=/etc/pvfs2tab
fi
if ! test -f $TAB_FILE; then
    echo "Cannot open tab file: $TAB_FILE"
    exit 1
fi

LINK_ROOT=`head -n 1 $TAB_FILE | cut -d' ' -f 2`
if test "x$LINK_ROOT" = "x"; then
    echo "Could not read file system root from tab file"
    exit 1
fi

echo "Using root $LINK_ROOT..."

# create the test directory tree
$SYSINT_TEST_DIR/mkdir /foo
$SYSINT_TEST_DIR/mkdir /foo/bar
$SYSINT_TEST_DIR/mkdir /foo/bar/baz

# symlink /foo/bar/barlink1 -> ../bar
$SYSINT_TEST_DIR/symlink /foo/bar/barlink1 ../bar

# symlink /foo/bar/barlink2 -> ../bar/barlink1
$SYSINT_TEST_DIR/symlink /foo/bar/barlink2 ../bar/barlink1

# symlink /foo/bar/barlink3 -> ../../foo/bar/barlink2
$SYSINT_TEST_DIR/symlink /foo/bar/barlink3 ../../foo/bar/barlink2

# symlink /foo/bar/barlink4 -> ../bar/barlink3
$SYSINT_TEST_DIR/symlink /foo/bar/barlink4 ../bar/barlink3

# symlink /foo/bar/barlink5 -> /foo/bar/barlink4
$SYSINT_TEST_DIR/symlink /foo/bar/barlink5 $LINK_ROOT/foo/bar/barlink4

# symlink /foo/bar/barlink6 -> ../../foo/bar/barlink5
$SYSINT_TEST_DIR/symlink /foo/bar/barlink6 ../../foo/bar/barlink5

# symlink /foo/bar/barlink7 -> ../../foo/bar/barlink6
$SYSINT_TEST_DIR/symlink /foo/bar/barlink7 ../../foo/bar/barlink6

# symlink /foo/bar/barlink8 -> /foo/bar/barlink7
$SYSINT_TEST_DIR/symlink /foo/bar/barlink8 $LINK_ROOT/foo/bar/barlink7


# lookup all links to make sure they can be resolved if followed
$SYSINT_TEST_DIR/lookup /foo/bar/barlink1
$SYSINT_TEST_DIR/lookup /foo/bar/barlink2
$SYSINT_TEST_DIR/lookup /foo/bar/barlink3
$SYSINT_TEST_DIR/lookup /foo/bar/barlink4
$SYSINT_TEST_DIR/lookup /foo/bar/barlink5
$SYSINT_TEST_DIR/lookup /foo/bar/barlink6
$SYSINT_TEST_DIR/lookup /foo/bar/barlink7
$SYSINT_TEST_DIR/lookup /foo/bar/barlink8
$SYSINT_TEST_DIR/lookup /foo/bar/barlink1 1
$SYSINT_TEST_DIR/lookup /foo/bar/barlink2 1
$SYSINT_TEST_DIR/lookup /foo/bar/barlink3 1
$SYSINT_TEST_DIR/lookup /foo/bar/barlink4 1
$SYSINT_TEST_DIR/lookup /foo/bar/barlink5 1
$SYSINT_TEST_DIR/lookup /foo/bar/barlink6 1
$SYSINT_TEST_DIR/lookup /foo/bar/barlink7 1
$SYSINT_TEST_DIR/lookup /foo/bar/barlink8 1


# cleanup all objects created
#
if test "x$2" = "x"; then
    $SYSINT_TEST_DIR/remove /foo/bar/barlink8
    $SYSINT_TEST_DIR/remove /foo/bar/barlink7
    $SYSINT_TEST_DIR/remove /foo/bar/barlink6
    $SYSINT_TEST_DIR/remove /foo/bar/barlink5
    $SYSINT_TEST_DIR/remove /foo/bar/barlink4
    $SYSINT_TEST_DIR/remove /foo/bar/barlink3
    $SYSINT_TEST_DIR/remove /foo/bar/barlink2
    $SYSINT_TEST_DIR/remove /foo/bar/barlink1

    $SYSINT_TEST_DIR/remove /foo/bar/baz
    $SYSINT_TEST_DIR/remove /foo/bar
    $SYSINT_TEST_DIR/remove /foo

fi