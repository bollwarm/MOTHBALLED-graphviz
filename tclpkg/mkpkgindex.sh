#!/bin/sh

# $1 = .la file in build tree (doesn't need to be installed)
# $2 = Name of extension
# $3 = Version of extension

lib=`grep library_names $1 | sed -e "s/.*=.//" -e "s/ .*//"`
echo "package ifneeded $2 $3 \"" >pkgIndex.tcl
case "$1" in
  *tk* )
    echo "	package require Tk 8.3" >>pkgIndex.tcl
    ;;
esac
echo "	load [file join \$dir $lib] $2\"" >>pkgIndex.tcl
