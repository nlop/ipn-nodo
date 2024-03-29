## Instalación del kit de desarrollo
Instalar el kit de desarrollo ESP-IDF según [estas instrucciones](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#installation-step-by-step). **Utilizar la rama estable (stable) al momento de descargar el repositorio**
Dentro de la carpeta *examples/getting-started* del repositorio de *esp-idf* se pueden encontrar otros ejemplos para
probar la tarjeta.

## Instalación del firmware
Clonar el código de este repositorio y ejecutar el siguiente comando **dentro** de la carpeta del repositorio:
```bash
idf.py menuconfig
```
Aparecerá el siguiente menu:
![Menú de configuración](menuconfig_root.png)
Esta es la raíz del menú de configuración, aquí se pueden manipular diferentes parámetros de
configuración de la tarjeta, habilitar algunas funciones, etc. Dentro de este menú existen un submenu para 
ajustar parámetros operativos del nodo tales como la URL del servidor al que se conecta, la frecuencia de 
actualizaciones al servidor, habilitar o deshabilitar algunos sensores, etc. Se puede acceder a este menú mediante
la entrada *IPN-NODO Menu*
![Menú de IPN-Nodo](menu_root.png)
En la imagen se resaltan los **parámetros obligatorios** que hay que modificar antes de programar la tarjeta 
por primera vez.
## Establecer la dirección del servidor web
Se tiene que establecer una dirección IP o de dominio para acceder a los servicios web a los que
manda actualizaciones el dispositivo, esto se declara en el menú *Protocolos Web* en el campo
indicado en la siguiente imagen.
![Menú de opciones web](menu_webserv.png)
Además de establecer la dirección del servidor, se tiene que establecer el número de los puertos
para el servicio HTTP y WebSocket. Esto se hace entrando a los submenus de *HTTP* y *WebSocket*,
respectivamente, como se muestra en las imágenes. 
![Menú de opciones HTTP](menu_http.png)
![Menú de opciones WebSockets](menu_ws.png)
## Habilitar o deshabilitar el uso de sensores ópticos
En el menu de *Sensores* se puede activar el uso del sensor TSL2561 o el sensor BH1750, por default, cualquiera 
de los dos se encuentra. desactivado. Cabe mencionar que solo una de las dos
opciones debe de estar seleccionada.
![Menu de sensores](menu_sensores.png)

Nota: En caso de utilizar cualquier sensor, se debe de clonar previamente el
repositorio de *esp-idf-lib* ([link](https://github.com/UncleRus/esp-idf-lib)), además de agregar la siguiente 
linea al archivo *main/CMakeLists.txt*
```
set(EXTRA_COMPONENT_DIRS <dirección del repositorio>)
```
