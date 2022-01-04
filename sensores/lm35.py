import csv
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import AutoMinorLocator
import sys

patron_meas_csv = []
disp_meas_csv = []
meas_grp = []
if len(sys.argv) != 2:
    print('Faltan argumentos')
    exit
with open(sys.argv[1]) as csvfile:
    reader = csv.reader(csvfile, quoting = csv.QUOTE_NONNUMERIC)
    group_len = 0
    group = []
    for row in reader:
        if len(row) >= 2:
            patron_meas_csv.append(row[0])
            disp_meas_csv.append(row[1])
            #group.append(row)
            #group_len += 1
            #if group_len == 10:
            #    meas_grp.append(group)
            #    group_len = 0
            #    group = []
#err_bar_y = []
#err_bar_xerr = []
#for g in meas_grp:
#    #print(g)
#    g_mean = np.mean(g[1])
#    g_std = np.std(g[1])
#    print('For {:.1f}, m = {:.1f}, d = {:.1f}'.format(g[0][0], g_mean, g_std))
#    err_bar_y.append(g_mean)
#    err_bar_xerr = g_std
disp_meas = np.sort(np.array(disp_meas_csv))
patron_meas = np.sort(np.array(patron_meas_csv))
y_min = min(patron_meas) if min(patron_meas) < min(disp_meas) else min(disp_meas)
y_max = max(patron_meas) if max(patron_meas) > max(disp_meas) else max(disp_meas)
x = range(1, len(patron_meas) + 1)
## Graficar
err_bar_x = range(1, len(meas_grp) + 1)
fig, ax = plt.subplots()
#ax.errorbar(err_bar_x, err_bar_y, yerr = err_bar_xerr, fmt = 'o')
ax.plot(disp_meas, patron_meas, '.b');
ax.plot([0, max(patron_meas)], [0, max(patron_meas)] , '--', color = '#999999');
#ax.scatter(x, disp_meas,  marker='o',c = 'blue', linewidth = 1.6, label='Dispositivo');
ax.set_xlabel('Patrón')
ax.set_ylabel('Dispositivo')
ax.set_title('Calibración LM35')
#ax.tick_params(which = 'minor',  bottom = False, left = False)
ax.grid(which = 'major', color = '#404040', linewidth = 0.7)
ax.grid(which = 'minor', color = '#C0C0C0', linewidth = 0.3)
ax.minorticks_on()
ax.yaxis.set_minor_locator(AutoMinorLocator(5))
plt.show()
