#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

#include "hil_communication/frame.h"
#include "hil_communication/serial.h"
#include "hil_communication/hil_serial.h"

int main()
{
  std::unique_ptr<HilSerial> hilSerial;
 	const std::string port_name = "/dev/ttyAMA0";
  const unsigned int baud = 9600;
  bool _isRunning = false;
   std::chrono::milliseconds time(10);
   

	Serial* serial = new Serial();

  if (serial->open(port_name, baud))
  {
    hilSerial.reset(new HilSerial(serial));
    std::cerr << "Serial Abierto\n";
    _isRunning = true;
  }else{
    delete serial;
    std::cerr << "Serial Fallo\n";
    _isRunning = false;
  }

  Frame request;
  request.addFloat(1.0);
  request.addFloat(1.0);

  while(true)
  {
    if (!hilSerial->sendFrame(request)) {
      std::cerr << "No se pudo enviar el frame\n";
      return 1;
    }

    std::cout << "Frame enviado\n";
    std::this_thread::sleep_for(time);
  }
  return 0;
}