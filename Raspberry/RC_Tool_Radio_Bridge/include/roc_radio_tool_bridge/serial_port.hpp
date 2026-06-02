#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace roc_radio_tool_bridge
{

class SerialPort
{
public:
  SerialPort();
  ~SerialPort();

  SerialPort(const SerialPort&) = delete;
  SerialPort& operator=(const SerialPort&) = delete;

  bool openPort(const std::string& device, int baudrate);
  void closePort();
  bool isOpen() const;

  bool writeAll(const uint8_t* data, std::size_t size);
  bool readFrame(std::vector<uint8_t>& encoded_without_delimiters, unsigned int timeout_ms);

private:
  static bool configureTermios(int fd, int baudrate);
  static int baudrateToConstant(int baudrate);

  int fd_;
};

}  // namespace roc_radio_tool_bridge
