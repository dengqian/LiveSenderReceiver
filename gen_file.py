#!/usr/bin/python

import sys

outfile = open("1G.dat", "w")
count = 0
i = 0
while count < 1024*1024*1000:
	outfile.write(str(i)+'\n')
	count += len(str(i)+'\n')
	i += 1
outfile.close()
