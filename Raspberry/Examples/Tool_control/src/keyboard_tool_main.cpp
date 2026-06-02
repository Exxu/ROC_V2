#include "hil_communication/hil_serial.h"
#include "hil_communication/serial.h"
#include "hil_communication/frame.h"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

namespace
{
enum CommandCode
{
  CMD_STOP = 0,
  CMD_UP = 1,
  CMD_DOWN = 2,
  CMD_RESET_FAULT = 3,
  CMD_STATUS_REQUEST = 4
};

enum ResponseType
{
  RESPONSE_ACK = 1,
  RESPONSE_STATUS = 2
};

const char* commandName(int command)
{
  switch (command)
  {
    case CMD_STOP: return "STOP";
    case CMD_UP: return "UP";
    case CMD_DOWN: return "DOWN";
    case CMD_RESET_FAULT: return "RESET_FAULT";
    case CMD_STATUS_REQUEST: return "STATUS_REQUEST";
    default: return "UNKNOWN_COMMAND";
  }
}

const char* resultName(int result)
{
  switch (result)
  {
    case 0: return "OK / accepted";
    case 1: return "REJECTED_FAULT";
    case 2: return "REJECTED_AT_TOP";
    case 3: return "REJECTED_AT_BOTTOM";
    case 4: return "INVALID_COMMAND";
    case 5: return "ALREADY_MOVING";
    case 6: return "REJECTED_SENSOR_CONFLICT";
    default: return "UNKNOWN_RESULT";
  }
}

const char* statusName(int status)
{
  switch (status)
  {
    case 0: return "UNKNOWN";
    case 1: return "AT_BOTTOM";
    case 2: return "AT_TOP";
    case 3: return "MOVING_UP";
    case 4: return "MOVING_DOWN";
    case 5: return "STOPPED_BETWEEN_LIMITS";
    case 6: return "FAULT";
    default: return "UNKNOWN_STATUS";
  }
}

const char* faultName(int fault)
{
  switch (fault)
  {
    case 0: return "NONE";
    case 1: return "TIMEOUT_UP";
    case 2: return "TIMEOUT_DOWN";
    case 3: return "OVERCURRENT_UP";
    case 4: return "OVERCURRENT_DOWN";
    case 5: return "RELAY_CONFLICT";
    case 6: return "SENSOR_CONFLICT";
    default: return "UNKNOWN_FAULT";
  }
}

bool parseInt(const std::string& text, int* value)
{
  if (value == nullptr)
    return false;

  try
  {
    std::size_t used = 0;
    const int parsed = std::stoi(text, &used, 10);
    if (used != text.size())
      return false;

    *value = parsed;
    return true;
  }
  catch (...)
  {
    return false;
  }
}

bool sendCommand(HilSerial& hil, int device_id, int command)
{
  Frame request;
  request.addFloat(static_cast<float>(device_id));
  request.addFloat(static_cast<float>(command));

  std::cout << "TX -> device=" << device_id
            << " command=" << command
            << " (" << commandName(command) << ")" << std::endl;

  return hil.sendFrame(request);
}

bool printResponse(Frame& response)
{
  if (!response.unbuild())
  {
    std::cout << "RX <- frame invalido: checksum o formato incorrecto" << std::endl;
    return false;
  }

  if (response.size() < 2 * static_cast<int>(sizeof(float)))
  {
    std::cout << "RX <- payload demasiado corto" << std::endl;
    return false;
  }

  const int device = static_cast<int>(response.getFloat());
  const int response_type = static_cast<int>(response.getFloat());

  if (response_type == RESPONSE_ACK)
  {
    if (response.size() < 4 * static_cast<int>(sizeof(float)))
    {
      std::cout << "RX <- ACK incompleto" << std::endl;
      return false;
    }

    const int command = static_cast<int>(response.getFloat());
    const int result = static_cast<int>(response.getFloat());

    std::cout << "RX <- ACK" << std::endl;
    std::cout << "  device_id: " << device << std::endl;
    std::cout << "  command:   " << command << " (" << commandName(command) << ")" << std::endl;
    std::cout << "  result:    " << result << " (" << resultName(result) << ")" << std::endl;
    return true;
  }

  if (response_type == RESPONSE_STATUS)
  {
    if (response.size() < 7 * static_cast<int>(sizeof(float)))
    {
      std::cout << "RX <- STATUS incompleto" << std::endl;
      return false;
    }

    const int status = static_cast<int>(response.getFloat());
    const int fault = static_cast<int>(response.getFloat());
    const int hl = static_cast<int>(response.getFloat());
    const int ll = static_cast<int>(response.getFloat());
    const float current = response.getFloat();

    std::cout << "RX <- STATUS" << std::endl;
    std::cout << "  device_id:  " << device << std::endl;
    std::cout << "  status:     " << status << " (" << statusName(status) << ")" << std::endl;
    std::cout << "  fault:      " << fault << " (" << faultName(fault) << ")" << std::endl;
    std::cout << "  HL_active:  " << hl << std::endl;
    std::cout << "  LL_active:  " << ll << std::endl;
    std::cout << "  current_A:  " << std::fixed << std::setprecision(3) << current << std::endl;
    return true;
  }

  std::cout << "RX <- tipo de respuesta desconocido: " << response_type << std::endl;
  return false;
}

bool transactWithRetries(HilSerial& hil,
                         int device_id,
                         int command,
                         unsigned int timeout_ms,
                         int max_attempts)
{
  for (int attempt = 1; attempt <= max_attempts; ++attempt)
  {
    std::cout << "\nIntento " << attempt << "/" << max_attempts << std::endl;

    if (!sendCommand(hil, device_id, command))
    {
      std::cout << "ERROR: no se pudo enviar el frame HIL." << std::endl;
      return false;
    }

    std::cout << "Esperando respuesta por " << timeout_ms << " ms..." << std::endl;

    Frame response;
    if (!hil.readFrameTimed(&response, timeout_ms))
    {
      std::cout << "Timeout: no llego respuesta valida del Arduino." << std::endl;
      continue;
    }

    if (printResponse(response))
      return true;

    std::cout << "Respuesta invalida. Se intentara nuevamente." << std::endl;
  }

  std::cout << "\nERROR: no se recibio respuesta valida despues de "
            << max_attempts << " intentos. Regresando al menu." << std::endl;
  return false;
}

void printMenu(int device_id, unsigned int timeout_ms, int max_attempts)
{
  std::cout << "\n========================================" << std::endl;
  std::cout << "Tool keyboard controller" << std::endl;
  std::cout << "device_id=" << device_id
            << " timeout_ms=" << timeout_ms
            << " max_attempts=" << max_attempts << std::endl;
  std::cout << "----------------------------------------" << std::endl;
  std::cout << "0 = STOP" << std::endl;
  std::cout << "1 = UP" << std::endl;
  std::cout << "2 = DOWN" << std::endl;
  std::cout << "3 = RESET FAULT" << std::endl;
  std::cout << "4 = STATUS REQUEST" << std::endl;
  std::cout << "q = salir" << std::endl;
  std::cout << "========================================" << std::endl;
}
}  // namespace

int main(int argc, char** argv)
{
  if (argc < 4)
  {
    std::cerr << "Uso:\n"
              << "  " << argv[0] << " <serial_port> <baudrate> <device_id> [timeout_ms] [max_attempts]\n\n"
              << "Ejemplo:\n"
              << "  " << argv[0] << " /dev/ttyAMA0 9600 1 500 3\n";
    return 1;
  }

  const std::string port = argv[1];
  const unsigned int baudrate = static_cast<unsigned int>(std::stoul(argv[2]));
  const int device_id = std::stoi(argv[3]);
  const unsigned int timeout_ms = argc >= 5 ? static_cast<unsigned int>(std::stoul(argv[4])) : 500U;
  const int max_attempts = argc >= 6 ? std::stoi(argv[5]) : 3;

  std::unique_ptr<Serial> serial(new Serial());
  if (!serial->open(port, baudrate))
  {
    std::cerr << "ERROR: no se pudo abrir " << port << " a " << baudrate << " baudios." << std::endl;
    return 1;
  }

  HilSerial hil(serial.release());

  std::cout << "Puerto abierto: " << port << " @ " << baudrate << " baudios" << std::endl;

  std::string line;
  while (true)
  {
    printMenu(device_id, timeout_ms, max_attempts);
    std::cout << "> ";

    if (!std::getline(std::cin, line))
      break;

    if (line == "q" || line == "Q" || line == "exit")
      break;

    int command = -1;
    if (!parseInt(line, &command) || command < 0 || command > 4)
    {
      std::cout << "Comando invalido. Use 0, 1, 2, 3, 4 o q." << std::endl;
      continue;
    }

    transactWithRetries(hil, device_id, command, timeout_ms, max_attempts);
  }

  std::cout << "Saliendo." << std::endl;
  return 0;
}
