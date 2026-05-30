#include "hil_communication/serial.h"

#include <cmath>
#include <iostream>
#include <utility>

using namespace boost::asio;

Serial::Serial()
  : _io(new io_service), _port(new serial_port(*_io))
{
}

Serial::~Serial()
{
  close();
  if (_io != nullptr)
  {
    _io->stop();
  }
}

Serial::Serial(Serial&& other) noexcept
  : _io(std::move(other._io)),
    _port(std::move(other._port)),
    _portName(std::move(other._portName))
{
}

Serial& Serial::operator=(Serial&& other) noexcept
{
  if (&other == this)
    return *this;

  close();

  _io = std::move(other._io);
  _port = std::move(other._port);
  _portName = std::move(other._portName);

  return *this;
}

const std::string& Serial::getPortName() const
{
  return _portName;
}

bool Serial::setSerialPortName(const std::string& name)
{
  if (isOpen())
    return false;

  _portName = name;
  return true;
}

bool Serial::isOpen() const
{
  return _port != nullptr && _port->is_open();
}

bool Serial::open()
{
  if (_portName.empty())
    return false;

  if (_io == nullptr)
    _io.reset(new io_service);

  if (_port == nullptr)
    _port.reset(new serial_port(*_io));

  if (isOpen())
    return true;

  try
  {
    _port->open(_portName);
    return true;
  }
  catch (const boost::system::system_error& e)
  {
    logException("open", e.what());
    return false;
  }
}

bool Serial::open(const std::string& portName,
                  unsigned int baudrate,
                  unsigned int byteSize,
                  Serial::parity_t::type parity,
                  Serial::stop_bits_t::type stopBits,
                  Serial::flow_control_t::type flowControl)
{
  if (isOpen())
    close();

  _portName = portName;

  if (!open())
    return false;

  if (!setBaudrate(baudrate) ||
      !setByteSize(byteSize) ||
      !setParity(parity) ||
      !setNumStopBits(stopBits) ||
      !setFlowControl(flowControl))
  {
    close();
    return false;
  }

  return true;
}

void Serial::close()
{
  if (_port == nullptr)
    return;

  boost::system::error_code ec;
  if (_port->is_open())
  {
    _port->cancel(ec);
    _port->close(ec);
  }
}

unsigned int Serial::getBaudrate() const
{
  if (_port == nullptr)
    return 0;

  try
  {
    serial_port_base::baud_rate rate;
    _port->get_option(rate);
    return rate.value();
  }
  catch (const boost::system::system_error& e)
  {
    logException("getBaudrate", e.what());
    return 0;
  }
}

bool Serial::setBaudrate(unsigned int rate)
{
  if (_port == nullptr)
    return false;

  try
  {
    _port->set_option(serial_port_base::baud_rate(rate));
    return true;
  }
  catch (const boost::system::system_error& e)
  {
    logException("setBaudrate", e.what());
    return false;
  }
}

Serial::stop_bits_t::type Serial::getNumStopBits() const
{
  if (_port == nullptr)
    return Serial::stop_bits_t::one;

  try
  {
    serial_port_base::stop_bits bits;
    _port->get_option(bits);
    return bits.value();
  }
  catch (const boost::system::system_error& e)
  {
    logException("getNumStopBits", e.what());
    return Serial::stop_bits_t::one;
  }
}

bool Serial::setNumStopBits(Serial::stop_bits_t::type bits)
{
  if (_port == nullptr)
    return false;

  try
  {
    _port->set_option(serial_port_base::stop_bits(bits));
    return true;
  }
  catch (const boost::system::system_error& e)
  {
    logException("setNumStopBits", e.what());
    return false;
  }
}

bool Serial::setNumStopBits(double bits)
{
  if (std::fabs(bits - 1.0) < 1e-12)
    return setNumStopBits(stop_bits_t::one);
  if (std::fabs(bits - 1.5) < 1e-12)
    return setNumStopBits(stop_bits_t::onepointfive);
  if (std::fabs(bits - 2.0) < 1e-12)
    return setNumStopBits(stop_bits_t::two);
  return false;
}

bool Serial::setOneStopBit()
{
  return setNumStopBits(stop_bits_t::one);
}

bool Serial::setOneAndHalfStopBits()
{
  return setNumStopBits(stop_bits_t::onepointfive);
}

bool Serial::setTwoStopBits()
{
  return setNumStopBits(stop_bits_t::two);
}

Serial::parity_t::type Serial::getParity() const
{
  if (_port == nullptr)
    return Serial::parity_t::none;

  try
  {
    Serial::parity_t parity;
    _port->get_option(parity);
    return parity.value();
  }
  catch (const boost::system::system_error& e)
  {
    logException("getParity", e.what());
    return Serial::parity_t::none;
  }
}

bool Serial::isParityEnabled() const
{
  return getParity() != Serial::parity_t::none;
}

bool Serial::setParity(Serial::parity_t::type parity)
{
  if (_port == nullptr)
    return false;

  try
  {
    _port->set_option(Serial::parity_t(parity));
    return true;
  }
  catch (const boost::system::system_error& e)
  {
    logException("setParity", e.what());
    return false;
  }
}

bool Serial::disableParity()
{
  return setParity(Serial::parity_t::none);
}

bool Serial::enableOddParity()
{
  return setParity(Serial::parity_t::odd);
}

bool Serial::enableEvenParity()
{
  return setParity(Serial::parity_t::even);
}

Serial::flow_control_t::type Serial::getFlowControl() const
{
  if (_port == nullptr)
    return Serial::flow_control_t::none;

  try
  {
    Serial::flow_control_t ctrl;
    _port->get_option(ctrl);
    return ctrl.value();
  }
  catch (const boost::system::system_error& e)
  {
    logException("getFlowControl", e.what());
    return Serial::flow_control_t::none;
  }
}

bool Serial::isFlowControlEnabled() const
{
  return getFlowControl() != Serial::flow_control_t::none;
}

bool Serial::setFlowControl(Serial::flow_control_t::type ctrl_type)
{
  if (_port == nullptr)
    return false;

  try
  {
    _port->set_option(Serial::flow_control_t(ctrl_type));
    return true;
  }
  catch (const boost::system::system_error& e)
  {
    logException("setFlowControl", e.what());
    return false;
  }
}

bool Serial::disableFlowControl()
{
  return setFlowControl(serial_port_base::flow_control::none);
}

bool Serial::enableSoftwareFlowControl()
{
  return setFlowControl(serial_port_base::flow_control::software);
}

bool Serial::enableHardwareFlowControl()
{
  return setFlowControl(serial_port_base::flow_control::hardware);
}

unsigned int Serial::getByteSize() const
{
  if (_port == nullptr)
    return 0;

  try
  {
    serial_port_base::character_size size;
    _port->get_option(size);
    return size.value();
  }
  catch (const boost::system::system_error& e)
  {
    logException("getByteSize", e.what());
    return 0;
  }
}

bool Serial::setByteSize(unsigned int size)
{
  if (_port == nullptr || size < 5 || size > 8)
    return false;

  try
  {
    _port->set_option(serial_port_base::character_size(size));
    return true;
  }
  catch (const boost::system::system_error& e)
  {
    logException("setByteSize", e.what());
    return false;
  }
}

std::size_t Serial::readByte(uint8_t* byte)
{
  if (byte == nullptr)
    return 0;

  mutable_buffer buf = boost::asio::buffer(byte, 1);
  return readBytes(&buf);
}

std::size_t Serial::readBytes(uint8_t* buffer, std::size_t nbytes)
{
  if (buffer == nullptr || nbytes == 0)
    return 0;

  mutable_buffer buf = boost::asio::buffer(buffer, nbytes);
  return readBytes(&buf);
}

std::size_t Serial::readBytes(std::vector<uint8_t>* buffer, std::size_t nbytes)
{
  if (buffer == nullptr || nbytes == 0)
    return 0;

  buffer->assign(nbytes, 0);
  mutable_buffer buf = boost::asio::buffer(buffer->data(), buffer->size());
  return readBytes(&buf);
}

std::size_t Serial::readBytes(std::list<uint8_t>* buffer, std::size_t nbytes)
{
  if (buffer == nullptr || nbytes == 0)
    return 0;

  std::vector<uint8_t> tmp(nbytes, 0);
  const std::size_t nread = readBytes(tmp.data(), tmp.size());

  buffer->clear();
  buffer->insert(buffer->end(), tmp.begin(), tmp.begin() + static_cast<std::ptrdiff_t>(nread));

  return nread;
}

std::size_t Serial::readBytes(boost::asio::mutable_buffer* buf)
{
  if (buf == nullptr || !isOpen() || boost::asio::buffer_size(*buf) == 0)
    return 0;

  try
  {
    return boost::asio::read(*_port, *buf);
  }
  catch (const boost::system::system_error& e)
  {
    logException("readBytes", e.what());
    return 0;
  }
}

std::size_t Serial::sendByte(uint8_t byte)
{
  return sendBytes(&byte, 1);
}

std::size_t Serial::sendBytes(const uint8_t* bytes, std::size_t nbytes)
{
  if (bytes == nullptr || nbytes == 0)
    return 0;

  return sendBytes(boost::asio::buffer(bytes, nbytes));
}

std::size_t Serial::sendBytes(uint8_t* bytes, std::size_t nbytes)
{
  return sendBytes(static_cast<const uint8_t*>(bytes), nbytes);
}

std::size_t Serial::sendBytes(const std::vector<uint8_t>& bytes)
{
  if (bytes.empty())
    return 0;

  return sendBytes(bytes.data(), bytes.size());
}

std::size_t Serial::sendBytes(const std::list<uint8_t>& bytes)
{
  if (bytes.empty())
    return 0;

  const std::vector<uint8_t> tmp(bytes.begin(), bytes.end());
  return sendBytes(tmp.data(), tmp.size());
}

std::size_t Serial::sendBytes(boost::asio::const_buffer buf)
{
  if (!isOpen() || boost::asio::buffer_size(buf) == 0)
    return 0;

  try
  {
    return boost::asio::write(*_port, buf);
  }
  catch (const boost::system::system_error& e)
  {
    logException("sendBytes", e.what());
    return 0;
  }
}

void Serial::logError(const std::string& msg) const
{
  std::cerr << "[SyncHIL][Serial] " << msg << std::endl;
}

void Serial::logException(const std::string& method, const std::string& what) const
{
  logError("Exception in " + method + ": " + what);
}

void Serial::logNullPortError(const std::string& method) const
{
  logError("Null serial port in " + method);
}
