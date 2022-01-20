import csv
import numpy as np
import matplotlib.pyplot as plt
from matplotlib import ticker
import sys

def measSort(measTup):
    return measTup[0]

rawMeas = []
yMeasDev = []
yMeasPat = []
yDevStd = []
xSamplesAll = []
yDevMean = []
if len(sys.argv) != 2:
    print('Faltan argumentos')
    exit
plt.rcParams.update({
    "text.usetex" : True,
    "font.family" : "serif",
    "figure.figsize" : (6.8, 5)
    })
with open(sys.argv[1]) as csvfile:
    reader = csv.reader(csvfile, quoting = csv.QUOTE_NONNUMERIC)
    group_len = 0
    group = []
    for row in reader:
        if len(row) == 2:
            rawMeas.append(row)
rawMeas.sort(key = measSort)
for i in range(0, int(len(rawMeas)/ 10)):
    measGroup = rawMeas[i * 10 : i * 10 + 10]
    meanPat = np.mean([pat for pat,dev in measGroup])
    measDev = [dev for pat,dev in measGroup]
    yMeasPat.append(meanPat)
    yMeasDev.extend(measDev)
    xSamplesAll.extend(10 * [i + 1])
    yDevStd.append(np.std(measDev))
    yDevMean.append(np.mean(measDev))
xSamples = range(1, int(len(rawMeas) / 10) + 1)
# Graficar
fig, ax = plt.subplots()
#ax.errorbar(err_bar_x, err_bar_y, yerr = err_bar_xerr, fmt = 'o')
ax.errorbar(xSamples, yDevMean, yerr = yDevStd, fmt = 'o', label =
                'Desviación', capsize = 8, markeredgewidth = 1)
ax.plot(xSamples, yMeasPat, 's', color = 'red', label = 'Patron (media)',
        markersize = 5);
ax.plot(xSamplesAll, yMeasDev, '^', color = 'orange', label = 'Dispositivo',
        markersize = 5);

#ax.scatter(x, disp_meas,  marker='o',c = 'blue', linewidth = 1.6, label='Dispositivo');
ax.set_xlabel('Muestras', fontsize = 'large')
ax.set_ylabel('Humedad del suelo', fontsize = 'large')
ax.set_title('Calibración HW395', fontsize = 'x-large')
#ax.tick_params(which = 'minor',  bottom = False, left = False)
ax.grid(which = 'major', color = '#404040', linewidth = 0.7)
ax.grid(which = 'minor', color = '#C0C0C0', linewidth = 0.3)
ax.minorticks_on()
plt.xticks(xSamples)
ax.yaxis.set_major_formatter(ticker.StrMethodFormatter('{x:.0f}\%'))
#ax.xaxis.set_major_locator(ticker.MaxNLocator(len(xSamples), integer = True))
ax.yaxis.set_minor_locator(ticker.AutoMinorLocator(5))
ax.legend()
#plt.show()
#fig.savefig('hw390.png', dpi = 300.0)
fig.savefig('hw390.png', dpi = 210.0)
