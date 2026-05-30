#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <iomanip>

#include "hil_communication/frame.h"
#include "hil_communication/serial.h"
#include "hil_communication/hil_serial.h"

void printFrameHex(Frame& frame)
{
  frame.build();

  uint8_t* buffer = frame.buffer();
  int size = frame.buffer_size();

  std::cout << "Frame HEX: ";

  for (int i = 0; i < size; ++i) {
    std::cout
      << "0x"
      << std::hex
      << std::setw(2)
      << std::setfill('0')
      << static_cast<int>(buffer[i])
      << " ";
  }

  std::cout << std::dec << std::setfill(' ') << std::endl;
}

int main()
{
  std::unique_ptr<HilSerial> hilSerial;

  const std::string port_name = "/dev/ttyAMA0";
  const unsigned int baud = 9600;

  Serial* serial = new Serial();

  if (serial->open(port_name, baud)) {
    hilSerial.reset(new HilSerial(serial));
    std::cerr << "Serial abierto: " << port_name << std::endl;
  } else {
    delete serial;
    std::cerr << "Serial fallo: " << port_name << std::endl;
    return 1;
  }

  while (true) {
    Frame request;

    request.addFloat(1.0f);  // device_id
    request.addFloat(1.0f);  // command

    printFrameHex(request);

    if (!hilSerial->sendFrame(request)) {
      std::cerr << "No se pudo enviar el frame" << std::endl;
      return 1;
    }

    std::cout << "Frame enviado" << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }

  return 0;
}