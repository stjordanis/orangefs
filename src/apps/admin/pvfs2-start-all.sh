#!/bin/bash
# (C) 2013 Clemson University and The University of Chicago.
#  See COPYING in top-level directory.

# Purpose: Start all PVFS2 Servers based on contents of config file.

cd `dirname $0`/..

help()
{
    SP=40
    printf "%s\n" "Usage: pvfs2-start-all.sh -c <config_file_path> [OPTION]"
    printf "%-${SP}s%s\n" "  -c, --conf <config_file_path>" "PVFS configuration file path"

    printf "%-${SP}s%s\n" "  -e, --exclusions <exclusions_string>" "String of space separated expressions: "
    printf "%-${SP}s%s\n" "" "    Addresses on the 'Alias' lines of the config file will be "
    printf "%-${SP}s%s\n" "" "    excluded from the server list if any part of the "
    printf "%-${SP}s%s\n" "" "    address matches any of the expressions."

    printf "%-${SP}s%s\n" "  -h, --help" "Show help information"
    printf "%-${SP}s%s\n" "  -m, --mnt <mnt_path>" "PVFS mount path"
    printf "%-${SP}s%s\n" "  -o, --options <options_string>" "String of options passed to ssh"
    printf "%-${SP}s%s\n" "" "    ex: -o \"-t headnode ssh\""
    printf "%-${SP}s%s\n" "  -s, --server_options <options_string>" "String of options to pass to pvfs2-server"
}

PVFS2_SERVER=`pwd`/sbin/pvfs2-server

# optionally set by options
PVFS_CONF_FILE=                         #Ex: '/opt/orangefs/orangefs.conf'
EXCLUSIONS=                             #Ex: 'ib0 myri0'
MNT=                                    #Ex: '/mnt/orangefs'
SSH_OPTIONS=                            #Ex: '-t another_host ssh'
PVFS2_SERVER_OPTIONS=                   #Ex: '-f' or '-r'

# Execute getopt
ARGS=`getopt -o "c:e:hm:o:s:" -l "conf:,exclusions:,help,mnt:,options:,server_options:" -n "$0" -- "$@"`

# Check if getopt returned an error b/c of bad arguments
if [ $? -ne 0 ]; then
    exit 1
fi

# Handle whitespace
eval set -- "$ARGS"

MNT_SET=0

# Check for Required Options
CONF_SET=0

# Iterate over options
while [ $# -ne 0 ]; do
    case "$1" in
        -c|--conf)
        if [ -n "$2" ]; then
            PVFS_CONF_FILE=$2
            CONF_SET=1
        fi
        shift 2;;

        -e|--exclusions)
        EXCLUSIONS=$2
        shift 2;;

        -h|--help)
        help
        exit 0
        shift 2;;

        -m|--mnt)
        if [ -n "$2" ]; then
            MNT=$2
            MNT_SET=1
        fi
        shift 2;;

        -o|--options)
        if [ -n "$2" ]; then
            SSH_OPTIONS=$2
        fi
        shift 2;;

        -s|--server_options)
        if [ -n "$2" ]; then
            PVFS2_SERVER_OPTIONS=$2
        fi
        shift 2;;

        --)
        shift
    esac
done

if [ $CONF_SET -eq 0 ]; then
    help
    exit 1
fi

# Store Server List in SERVERS variable
SERVERS=`cat $PVFS_CONF_FILE | grep "Alias " | tr ' ,' '\n' | grep ":" | cut -d ':' -f2 | sed 's/\///g'`

# Exclude hostnames from config file that match a certain expression
# Ex: Can be used to ignore Infiniband connections designated ib0...
if [ ${#EXCLUSIONS} -gt 0 ]; then
    for EXCLUSION in $EXCLUSIONS; do
        SERVERS=`echo $SERVERS | tr ' ' '\n' | grep -v "$EXCLUSION"`
    done
fi

SPACING=$[`echo "$SERVERS" | wc -L`+4]

# Start Servers
for SERVER in $SERVERS; do
    OUTPUT=`ssh $SSH_OPTIONS $SERVER $PVFS2_SERVER $PVFS2_SERVER_OPTIONS $PVFS_CONF_FILE 2>&1`
    printf "%-${SPACING}s%s\n" "$SERVER:" "$OUTPUT"
done

# Give servers time to start
sleep 3

# Ping the servers
if [ $MNT_SET -eq 1 ]; then
    printf "\n%s\n" "Pinging the PVFS2 File System..."
    `pwd`/bin/pvfs2-ping -m $MNT
fi
