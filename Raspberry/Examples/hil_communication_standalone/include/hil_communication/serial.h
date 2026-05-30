#ifndef SYNC_HIL_SERIAL_H
#define SYNC_HIL_SERIAL_H

#include <boost/asio.hpp>
#include <boost/asio/serial_port.hpp>

#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <vector>

class Serial
{
public:
  using stop_bits_t = boost::asio::serial_port_base::stop_bits;
  using parity_t = boost::asio::serial_port_base::parity;
  using flow_control_t = boost::asio::serial_port_base::flow_control;

  Serial();
  virtual ~Serial();

  Serial(const Serial& other) = delete;
  Serial& operator=(const Serial& other) = delete;

  Serial(Serial&& other) noexcept;
  Serial& operator=(Serial&& other) noexcept;

  const std::string& getPortName() const;
  bool setSerialPortName(const std::string& name);

  bool isOpen() const;
  bool open();
  bool open(const std::string& portName,
            unsigned int baudrate = 115200,
            unsigned int byteSize = 8,
            parity_t::type parity = parity_t::none,
            stop_bits_t::type stopBits = stop_bits_t::one,
            flow_control_t::type flowControl = flow_control_t::none);
  void close();

  unsigned int getBaudrate() const;
  bool setBaudrate(unsigned int rate);

  stop_bits_t::type getNumStopBits() const;
  bool setNumStopBits(stop_bits_t::type bits);
  bool setNumStopBits(double bits);
  bool setOneStopBit();
  bool setOneAndHalfStopBits();
  bool setTwoStopBits();

  parity_t::type getParity() const;
  bool isParityEnabled() const;
  bool setParity(parity_t::type parity);
  bool disableParity();
  bool enableOddParity();
  bool enableEvenParity();

  flow_control_t::type getFlowControl() const;
  bool isFlowControlEnabled() const;
  bool setFlowControl(flow_control_t::type ctrl_type);
  bool disableFlowControl();
  bool enableSoftwareFlowControl();
  bool enableHardwareFlowControl();

  unsigned int getByteSize() const;
  bool setByteSize(unsigned int size);

  // Synchronous/blocking reads. These return the number of bytes read.
  // readByte() blocks until one byte is received or an error/close occurs.
  std::size_t readByte(uint8_t* byte);
  std::size_t readBytes(uint8_t* buffer, std::size_t nbytes);
  std::size_t readBytes(std::vector<uint8_t>* buffer, std::size_t nbytes);
  std::size_t readBytes(std::list<uint8_t>* buffer, std::size_t nbytes);

  // Synchronous/blocking writes. These return the number of bytes written.
  std::size_t sendByte(uint8_t byte);
  std::size_t sendBytes(const uint8_t* bytes, std::size_t nbytes);
  std::size_t sendBytes(uint8_t* bytes, std::size_t nbytes);
  std::size_t sendBytes(const std::vector<uint8_t>& bytes);
  std::size_t sendBytes(const std::list<uint8_t>& bytes);

protected:
  std::unique_ptr<boost::asio::io_service> _io;
  std::unique_ptr<boost::asio::serial_port> _port;

  void logError(const std::string& msg) const;
  void logException(const std::string& method, const std::string& what) const;
  void logNullPortError(const std::string& method) const;

  std::size_t readBytes(boost::asio::mutable_buffer* buf);
  std::size_t sendBytes(boost::asio::const_buffer buf);

private:
  std::string _portName;
};

#endif  // SYNC_HIL_SERIAL_H
