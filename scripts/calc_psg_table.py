#!/usr/bin/env python

from array import array

drop= 10**-0.1
table= array ( 'd', [0.0]*16 )
prev= table[0]= 0.25
for i in xrange(1,15):
    prev= table[i]= prev*drop
table[15]= 0
print "static const double _volume_table[16]="
print "  {"
print reduce(lambda x,y:str(x)+', '+str(y),table)
print "  };"
