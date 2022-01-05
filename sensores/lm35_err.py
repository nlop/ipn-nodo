import csv
import numpy as np
import matplotlib.pyplot as plt
from matplotlib import ticker
import sys
from itertools import groupby

def tempSort(tempDup):
    return tempDup[0]

measRaw = []
if len(sys.argv) != 2:
    print('Faltan argumentos')
    exit
yPatMean = []
yDevMean = []
yDevStd = []

fig, ax = plt.subplots()
with open(sys.argv[1]) as csvfile:
    reader = csv.reader(csvfile, quoting = csv.QUOTE_NONNUMERIC)
    for row in reader:
        if len(row) == 2:
            measRaw.append(row)
measRaw.sort(key = tempSort)
for i in range(int(len(measRaw)/10)):
    measGroup = measRaw[i*10 : i*10 + 10]
    measDev = []
    measPat = []
    for pat,dev in measGroup:
        measDev.append(dev)
        measPat.append(pat)
    yPatMean.append(np.mean(measPat))
    yDevMean.append(np.mean(measDev))
    yDevStd.append(np.std(measDev))
xSamples = range(1, len(yPatMean) + 1)
ax.errorbar(xSamples, yDevMean, yerr = yDevStd, fmt = 'o', label =
                'Dispositivo', capsize = 5, markeredgewidth = 1)
ax.plot(xSamples, yPatMean,'o', color =
            'orange', label = 'Patron (media)')
ax.yaxis.set_major_formatter(ticker.StrMethodFormatter('{x:.2f}°C'))
ax.legend()
plt.show()
