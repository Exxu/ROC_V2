# hil_communication_standalone

Version standalone de `hil_communication` para pruebas iniciales en Raspberry/Ubuntu sin ROS/catkin.

Esta carpeta compila la libreria C++ original y un ejecutable de prueba llamado `hil_serial_test`.

## Dependencias

En Raspberry Pi / Ubuntu:

```bash
sudo apt update
sudo apt install build-essential cmake libboost-system-dev libboost-thread-dev
```

## Compilar

Desde esta carpeta:

```bash
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

El ejecutable queda en:

```bash
build/hil_serial_test
```

## Uso

Enviar un frame y terminar:

```bash
./hil_serial_test /dev/ttyUSB0 9600 tx 1 0.0
```

Enviar un frame y esperar una respuesta:

```bash
./hil_serial_test /dev/ttyUSB0 9600 request 1 0.0
```

Escuchar frames indefinidamente:

```bash
./hil_serial_test /dev/ttyUSB0 9600 rx
```

## Formato del payload de prueba

El `main.cpp` envia un payload simple:

```text
int32_t command
float value
```

Por ejemplo:

```bash
./hil_serial_test /dev/ttyUSB0 9600 tx 2 1.5
```

envia:

```text
command = 2
value   = 1.5
```

## Nota sobre bloqueo

`HilSerial::readFrame()` es bloqueante. En modo `rx` y `request`, el programa puede quedarse esperando hasta recibir un frame completo.
Para una aplicacion final, conviene usar lectura en un hilo separado o agregar timeout a la clase `Serial`.

## Nota sobre permisos del puerto serial

Si aparece permiso denegado con `/dev/ttyUSB0`, agrega tu usuario al grupo `dialout`:

```bash
sudo usermod -a -G dialout $USER
```

Luego cierra sesion y vuelve a entrar, o reinicia.
