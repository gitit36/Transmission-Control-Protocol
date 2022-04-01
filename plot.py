import numpy as np
import matplotlib.pyplot as plt
import sys
from argparse import ArgumentParser

def scale(a):
    return a/1000000.0

parser = ArgumentParser(description="plot")

parser.add_argument('--dir', '-d',
                    help="Directory to store outputs",
                    required=True)

parser.add_argument('--name', '-n',
                    help="name of the experiment",
                    required=True)

parser.add_argument('--trace', '-tr',
                    help="name of the trace",
                    required=True)

args = parser.parse_args()

fig = plt.figure(figsize=(21,3), facecolor='w')
ax = plt.gca()


# plotting the trace file
f1 = open (args.trace,"r")
BW = []
nextTime = 1000
cnt = 0
for line in f1:
    if int(line.strip()) > nextTime:
        BW.append(cnt*1492*8)
        cnt = 0
        nextTime+=1000
    else:
        cnt+=1
f1.close()

ax.fill_between(range(len(BW)), 0, list(map(scale,BW)),color='#D3D3D3')

# plotting throughput
throughputDL = []
timeDL = []

traceDL = open (args.dir+"/"+str(args.name), 'r')
traceDL.readline()

tmp = traceDL.readline().strip().split(",")
bytes = int(tmp[1])
startTime = float(tmp[0])
stime=float(startTime)

for time in traceDL:
    if (float(time.strip().split(",")[0]) - float(startTime)) <= 1.0:
        bytes += int(time.strip().split(",")[1])
    else:
        throughputDL.append(bytes*8/1000000.0)
        timeDL.append(float(startTime)-stime)
        bytes = int(time.strip().split(",")[1])
        startTime += 1.0

print (timeDL)
print (throughputDL)

plt.plot(timeDL, throughputDL, lw=2, color='r')

plt.ylabel("Throughput (Mbps)")
plt.xlabel("Time (s)")
# plt.xlim([0,300])
plt.grid(True, which="both")
plt.savefig(args.dir+'/throughput.pdf',dpi=1000,bbox_inches='tight')
