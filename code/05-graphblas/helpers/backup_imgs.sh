#!/bin/bash
#This script just backs up existing images to a separate folder so they don't get removed

#Print all the files to an array
img_files=`ls *.png 2>/dev/null`
#Then check to see if the array is empty; if not move PNG files to a new folder
if [ "$img_files" -a ${#img_files[@]} ]; then
	mkdir -p figs
	mv *.png figs/.
fi
