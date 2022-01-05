import csv
import sys
from itertools import groupby
import numpy as np
import matplotlib.pyplot as plt
from matplotlib import ticker
def sortRSSI(rssiTup):
    return rssiTup[0]
colors = ['','r','b','g']
labels = ['', 'Entorno cerrado', 'Entorno abierto']
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
ax.grid(which = 'major', color = '#404040', linewidth = 0.7)
ax.grid(which = 'minor', color = '#C0C0C0', linewidth = 0.3)
ax.minorticks_on()
ax.set_xlabel('Distancia (m)')
ax.set_ylabel('RSSI')
ax.set_title('Caracterización señal WiFi')
ax.yaxis.set_minor_locator(ticker.AutoMinorLocator(5))
plt.xticks(xDist)
ax.legend()
#plt.show()
fig.savefig('rssi_wifi.png', dpi = 300)
