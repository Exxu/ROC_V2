#include "hil_communication/hil_serial.h"
#include "hil_communication/serial.h"
#include "hil_communication/frame.h"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

namespace
{
// Firmware simple:
//   1 = UP
//   2 = DOWN
//   3 = STATUS
//
// ACK response for UP/DOWN/invalid command:
//   float[0] = device_id
//   float[1] = RESPONSE_ACK
//   float[2] = command
//   float[3] = result_code
//
// STATUS response for command 3:
//   float[0] = device_id
//   float[1] = RESPONSE_STATUS
//   float[2] = HL_active
//   float[3] = LL_active

enum CommandCode
{
  CMD_UP = 1,
  CMD_DOWN = 2,
  CMD_STATUS = 3
};

enum ResponseType
{
  RESPONSE_ACK = 1,
  RESPONSE_STATUS = 2
};

enum CommandResult
{
  RESULT_OK = 0,
  RESULT_INVALID_COMMAND = 1,
  RESULT_REJECTED_AT_TOP = 2,
  RESULT_REJECTED_AT_BOTTOM = 3,
  RESULT_RELAY_CONFLICT = 4
};

const unsigned int RETRY_DELAY_MS = 100U;

const char* commandName(int command)
{
  switch (command)
  {
    case CMD_UP: return "UP";
    case CMD_DOWN: return "DOWN";
    case CMD_STATUS: return "STATUS";
    default: return "UNKNOWN_COMMAND";
  }
}

const char* resultName(int result)
{
  switch (result)
  {
    case RESULT_OK: return "OK / accepted";
    case RESULT_INVALID_COMMAND: return "INVALID_COMMAND";
    case RESULT_REJECTED_AT_TOP: return "REJECTED_AT_TOP";
    case RESULT_REJECTED_AT_BOTTOM: return "REJECTED_AT_BOTTOM";
    case RESULT_RELAY_CONFLICT: return "RELAY_CONFLICT";
    default: return "UNKNOWN_RESULT";
  }
}

const char* positionName(int hl, int ll)
{
  if (hl == 1 && ll == 0)
    return "AT_TOP";

  if (hl == 0 && ll == 1)
    return "AT_BOTTOM";

  if (hl == 0 && ll == 0)
    return "BETWEEN_LIMITS_OR_UNKNOWN";

  if (hl == 1 && ll == 1)
    return "INVALID_SENSOR_STATE";

  return "UNKNOWN_SENSOR_VALUES";
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
    if (response.size() < 4 * static_cast<int>(sizeof(float)))
    {
      std::cout << "RX <- STATUS incompleto" << std::endl;
      return false;
    }

    const int hl = static_cast<int>(response.getFloat());
    const int ll = static_cast<int>(response.getFloat());

    std::cout << "RX <- STATUS" << std::endl;
    std::cout << "  device_id: " << device << std::endl;
    std::cout << "  HL_active: " << hl << std::endl;
    std::cout << "  LL_active: " << ll << std::endl;
    std::cout << "  position:  " << positionName(hl, ll) << std::endl;
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

      if (attempt < max_attempts)
        std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));

      continue;
    }

    if (printResponse(response))
      return true;

    std::cout << "Respuesta invalida. Se intentara nuevamente." << std::endl;

    if (attempt < max_attempts)
      std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
  }

  std::cout << "\nERROR: no se recibio respuesta valida despues de "
            << max_attempts << " intentos. Regresando al menu." << std::endl;
  return false;
}

void printMenu(int device_id, unsigned int timeout_ms, int max_attempts)
{
  std::cout << "\n========================================" << std::endl;
  std::cout << "Simple tool keyboard controller" << std::endl;
  std::cout << "device_id=" << device_id
            << " timeout_ms=" << timeout_ms
            << " max_attempts=" << max_attempts << std::endl;
  std::cout << "----------------------------------------" << std::endl;
  std::cout << "1 = UP" << std::endl;
  std::cout << "2 = DOWN" << std::endl;
  std::cout << "3 = STATUS sensors" << std::endl;
  std::cout << "q = salir" << std::endl;
  std::cout << "========================================" << std::endl;
}
}  // namespace

int main(int argc, char** argv)
{
  std::cout.setf(std::ios::unitbuf);
  std::cerr.setf(std::ios::unitbuf);

  const char* build_id = "keyboard_tool_controller_simple_status V5 - HIL readFrameTimed - no ROS";
  std::cout << build_id << std::endl;
  std::cout << "argc=" << argc << std::endl;

  if (argc < 4 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")
  {
    std::cerr << "Uso:\n"
              << "  " << argv[0] << " <serial_port> <baudrate> <device_id> [timeout_ms] [max_attempts]\n\n"
              << "Ejemplo:\n"
              << "  " << argv[0] << " /dev/ttyAMA0 9600 1 500 3\n\n"
              << "Comandos del firmware simple:\n"
              << "  1 = UP\n"
              << "  2 = DOWN\n"
              << "  3 = STATUS sensors\n\n"
              << "Nota: si ejecuta sin parametros, este programa NO abre serial y termina.\n";
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
    if (!parseInt(line, &command) || command < 1 || command > 3)
    {
      std::cout << "Comando invalido. Use 1, 2, 3 o q." << std::endl;
      continue;
    }

    transactWithRetries(hil, device_id, command, timeout_ms, max_attempts);
  }

  std::cout << "Saliendo." << std::endl;
  return 0;
}
