pMeas_volt = [2.4620,1.4346,1.2740,1.2380,1.0900];
pMeas_perc = [0,38,46,66,100];

pLog = log(pMeas_volt);
pPoly = polyfit(pLog, pMeas_perc, 1);
pX = min(pMeas_volt) - 0.2 : 0.001 : max(pMeas_volt) + 0.2;
pY = pPoly(1)*log(pX) + p(2);
grid on;
grid minor on;
title('Interpolación con logaritmo');
xlabel('Voltaje sensor (V)');
ylabel('Porcentaje contenido de agua(%)');
set(gca, "fontsize", 14)
hold on;
plot(pX,pY, 'b', linewidth = 5);
plot(pMeas_volt,pMeas_perc,'^', markersize = 2, markerfacecolor = 'r');
hold off;