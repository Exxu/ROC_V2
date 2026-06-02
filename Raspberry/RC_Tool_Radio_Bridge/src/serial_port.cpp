#include "roc_radio_tool_bridge/serial_port.hpp"
#include "roc_radio_tool_bridge/hil_frame.hpp"

#include <cerrno>
#include <cstring>
#include <iostream>

#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

namespace roc_radio_tool_bridge
{

SerialPort::SerialPort()
  : fd_(-1)
{
}

SerialPort::~SerialPort()
{
  closePort();
}

bool SerialPort::openPort(const std::string& device, int baudrate)
{
  closePort();

  fd_ = ::open(device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd_ < 0)
  {
    std::cerr << "ERROR: cannot open serial port " << device << ": " << std::strerror(errno) << std::endl;
    return false;
  }

  if (!configureTermios(fd_, baudrate))
  {
    std::cerr << "ERROR: cannot configure serial port " << device << " at " << baudrate << " bps" << std::endl;
    closePort();
    return false;
  }

  ::tcflush(fd_, TCIOFLUSH);
  return true;
}

void SerialPort::closePort()
{
  if (fd_ >= 0)
  {
    ::close(fd_);
    fd_ = -1;
  }
}

bool SerialPort::isOpen() const
{
  return fd_ >= 0;
}

bool SerialPort::writeAll(const uint8_t* data, std::size_t size)
{
  if (fd_ < 0 || data == nullptr)
    return false;

  std::size_t written = 0;
  while (written < size)
  {
    const ssize_t n = ::write(fd_, data + written, size - written);
    if (n < 0)
    {
      if (errno == EINTR)
        continue;

      if (errno == EAGAIN || errno == EWOULDBLOCK)
      {
        pollfd pfd{};
        pfd.fd = fd_;
        pfd.events = POLLOUT;
        if (::poll(&pfd, 1, 1000) <= 0)
          return false;
        continue;
      }

      std::cerr << "ERROR: serial write failed: " << std::strerror(errno) << std::endl;
      return false;
    }

    written += static_cast<std::size_t>(n);
  }

  ::tcdrain(fd_);
  return true;
}

bool SerialPort::readFrame(std::vector<uint8_t>& encoded_without_delimiters, unsigned int timeout_ms)
{
  encoded_without_delimiters.clear();

  if (fd_ < 0)
    return false;

  bool in_frame = false;
  uint8_t byte = 0;
  const int timeout_i = static_cast<int>(timeout_ms);

  while (true)
  {
    pollfd pfd{};
    pfd.fd = fd_;
    pfd.events = POLLIN;

    const int ret = ::poll(&pfd, 1, timeout_i);
    if (ret == 0)
      return false;

    if (ret < 0)
    {
      if (errno == EINTR)
        continue;
      return false;
    }

    const ssize_t n = ::read(fd_, &byte, 1);
    if (n <= 0)
      continue;

    if (!in_frame)
    {
      if (byte == HilFrame::kDelimiter)
      {
        encoded_without_delimiters.clear();
        in_frame = true;
      }
      continue;
    }

    if (byte == HilFrame::kDelimiter)
    {
      if (encoded_without_delimiters.empty())
      {
        // Adjacent delimiters. Keep the second one as the start of the next frame.
        in_frame = true;
        continue;
      }

      return true;
    }

    encoded_without_delimiters.push_back(byte);

    if (encoded_without_delimiters.size() > 256)
    {
      encoded_without_delimiters.clear();
      in_frame = false;
    }
  }
}

bool SerialPort::configureTermios(int fd, int baudrate)
{
  termios tty{};
  if (::tcgetattr(fd, &tty) != 0)
    return false;

  ::cfmakeraw(&tty);

  const int speed = baudrateToConstant(baudrate);
  if (speed < 0)
    return false;

  ::cfsetispeed(&tty, static_cast<speed_t>(speed));
  ::cfsetospeed(&tty, static_cast<speed_t>(speed));

  tty.c_cflag |= static_cast<tcflag_t>(CLOCAL | CREAD);
  tty.c_cflag &= static_cast<tcflag_t>(~PARENB);
  tty.c_cflag &= static_cast<tcflag_t>(~CSTOPB);
  tty.c_cflag &= static_cast<tcflag_t>(~CSIZE);
  tty.c_cflag |= CS8;
  tty.c_cflag &= static_cast<tcflag_t>(~CRTSCTS);

  tty.c_cc[VMIN] = 0;
  tty.c_cc[VTIME] = 0;

  return ::tcsetattr(fd, TCSANOW, &tty) == 0;
}

int SerialPort::baudrateToConstant(int baudrate)
{
  switch (baudrate)
  {
    case 9600: return B9600;
    case 19200: return B19200;
    case 38400: return B38400;
    case 57600: return B57600;
    case 115200: return B115200;
#ifdef B230400
    case 230400: return B230400;
#endif
#ifdef B460800
    case 460800: return B460800;
#endif
#ifdef B921600
    case 921600: return B921600;
#endif
    default: return -1;
  }
}

}  // namespace roc_radio_tool_bridge
