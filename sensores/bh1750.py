import csv
import numpy as np
import matplotlib.pyplot as plt
from matplotlib import ticker
from mpl_toolkits.axes_grid1.inset_locator import inset_axes, InsetPosition, mark_inset

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
                'Desviación', capsize = 5, markeredgewidth = 1)
ax.plot(xSamples, yMeasPat, 's', color = 'red', label = 'Patron (media)',
        markersize = 5);
ax.plot(xSamplesAll, yMeasDev, '^', color = 'orange', label = 'Dispositivo',
        markersize = 5);

#ax.scatter(x, disp_meas,  marker='o',c = 'blue', linewidth = 1.6, label='Dispositivo');
ax.set_xlabel('Muestras', fontsize = 'large')
ax.set_ylabel('Lux', fontsize = 'large')
ax.set_title('Calibración BH1750', fontsize = 'x-large')
#ax.tick_params(which = 'minor',  bottom = False, left = False)
ax.grid(which = 'major', color = '#404040', linewidth = 0.7)
ax.grid(which = 'minor', color = '#C0C0C0', linewidth = 0.3)
ax.minorticks_on()
plt.xticks(xSamples)
#ax.yaxis.set_major_formatter(ticker.StrMethodFormatter('{x:.2f}°C'))
#ax.xaxis.set_major_locator(ticker.MaxNLocator(len(xSamples), integer = True))
ax.yaxis.set_minor_locator(ticker.AutoMinorLocator(5))
# Inset
groupI = 0
ax2 = plt.axes([0,0,1,1])
ip = InsetPosition(ax, [.15,.2,0.1,0.3])
ax2.set_axes_locator(ip)
mark_inset(ax, ax2, loc1=2, loc2=4, fc="none", ec='0.5')
ax2.plot(xSamplesAll[groupI * 10 :groupI * 10 + 10], yMeasDev[groupI * 10: groupI*10 + 10], ls = 'none', marker = '^', color = 'orange')
ax2.errorbar(groupI + 1, yDevMean[groupI], yerr = yDevStd[groupI], marker = 'o', color = 'blue', capsize = 5, markeredgewidth = 1)
ax2.plot(groupI + 1, yMeasPat[groupI], 's', color = 'red');
ax2.grid(which = 'major', color = '#404040', linewidth = 0.7)
ax2.grid(which = 'minor', color = '#C0C0C0', linewidth = 0.3)
ax2.minorticks_on()
ax2.set_xticks([])

# Inset
groupI = 1
ax3 = plt.axes([0,0,1,1])
ip = InsetPosition(ax, [.45,.5,0.1,0.3])
ax3.set_axes_locator(ip)
mark_inset(ax, ax3, loc1=2, loc2=4, fc="none", ec='0.5')
ax3.plot(xSamplesAll[groupI * 10 :groupI * 10 + 10], yMeasDev[groupI * 10: groupI*10 + 10], ls = 'none', marker = '^', color = 'orange')
ax3.errorbar(groupI + 1, yDevMean[groupI], yerr = yDevStd[groupI], marker = 'o', color = 'blue', capsize = 5, markeredgewidth = 1)
ax3.plot(groupI + 1, yMeasPat[groupI], 's', color = 'red');
ax3.grid(which = 'major', color = '#404040', linewidth = 0.7)
ax3.grid(which = 'minor', color = '#C0C0C0', linewidth = 0.3)
ax3.minorticks_on()
ax3.set_xticks([])

# Inset
groupI = 2
ax4 = plt.axes([0,0,1,1])
ip = InsetPosition(ax, [.8,.65,0.1,0.3])
ax4.set_axes_locator(ip)
mark_inset(ax, ax4, loc1=2, loc2=4, fc="none", ec='0.5')
ax4.plot(xSamplesAll[groupI * 10 :groupI * 10 + 10], yMeasDev[groupI * 10: groupI*10 + 10], ls = 'none', marker = '^', color = 'orange')
ax4.errorbar(groupI + 1, yDevMean[groupI], yerr = yDevStd[groupI], marker = 'o', color = 'blue', capsize = 5, markeredgewidth = 1)
ax4.plot(groupI + 1, yMeasPat[groupI], 's', color = 'red');
ax4.grid(which = 'major', color = '#404040', linewidth = 0.7)
ax4.grid(which = 'minor', color = '#C0C0C0', linewidth = 0.3)
ax4.minorticks_on()
ax4.set_xticks([])

ax.legend()
#plt.show()
fig.savefig('bh1750.png', dpi = 160.0)
