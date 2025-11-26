#!/usr/bin/python3

import random
def mquicksort (mlist):
	if len(mlist) == 0: 
		return mlist
	elif len(mlist) == 1:
		return mlist
	else:
		pivot =  random.choice(mlist[1:])
		smallerlist, equallist, greaterlist = [],[],[]
		for x in mlist:
			if x < pivot:
				smallerlist.append(x)
			elif x == pivot:
				equallist.append(x)
			else:
				greaterlist.append(x)
		return mquicksort(smallerlist) + equallist + mquicksort(greaterlist)



print(mquicksort([3,4,5,34,6,100,0,-8,0]))
