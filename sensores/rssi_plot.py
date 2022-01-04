import csv
import sys
from itertools import groupby
import numpy as np
import matplotlib.pyplot as plt

def sortRSSI(rssiTup):
    return rssiTup[0]
colors = ['','r','b','g']
labels = ['', 'Abierto', 'Cerrado']
measRaw = []
if len(sys.argv) < 2:
    print('Faltan argumentos')
    exit
fig, ax = plt.subplots()
for i in range(1, len(sys.argv)):
    xDist = []
    yRSSI = []
    with open(sys.argv[i]) as csvfile:
        reader = csv.reader(csvfile, quoting = csv.QUOTE_NONNUMERIC)
        for row in reader:
            if len(row) == 2:
                measRaw.append(row)
    measRaw.sort(key = sortRSSI)
    for key,group in groupby(measRaw, key = sortRSSI):
        rssiArr = [rssi for d,rssi in list(group)]
        rssiMean = np.mean(rssiArr)
        xDist.append(key)
        yRSSI.append(rssiMean)
    ax.plot(xDist, yRSSI, 's-{color}'.format(color = colors[i]), label =
            labels[i])
ax.legend()
plt.show()
