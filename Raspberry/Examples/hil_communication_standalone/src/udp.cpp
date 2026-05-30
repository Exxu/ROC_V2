#include "hil_communication/udp.h"

#include <algorithm>
#include <iostream>
#include <utility>

Udp::Udp()
  : _io(new boost::asio::io_service()),
    _socket(new udp_t::socket(*_io)),
    _hasRemoteEndpoint(false),
    _hasLastSenderEndpoint(false)
{
}

Udp::~Udp()
{
  close();
}

Udp::Udp(Udp&& other) noexcept
  : _io(std::move(other._io)),
    _socket(std::move(other._socket)),
    _remoteEndpoint(std::move(other._remoteEndpoint)),
    _lastSenderEndpoint(std::move(other._lastSenderEndpoint)),
    _hasRemoteEndpoint(other._hasRemoteEndpoint),
    _hasLastSenderEndpoint(other._hasLastSenderEndpoint)
{
  other._hasRemoteEndpoint = false;
  other._hasLastSenderEndpoint = false;
}

Udp& Udp::operator=(Udp&& other) noexcept
{
  if (&other == this)
    return *this;

  close();

  _io = std::move(other._io);
  _socket = std::move(other._socket);
  _remoteEndpoint = std::move(other._remoteEndpoint);
  _lastSenderEndpoint = std::move(other._lastSenderEndpoint);
  _hasRemoteEndpoint = other._hasRemoteEndpoint;
  _hasLastSenderEndpoint = other._hasLastSenderEndpoint;

  other._hasRemoteEndpoint = false;
  other._hasLastSenderEndpoint = false;

  return *this;
}

bool Udp::isOpen() const
{
  return _socket && _socket->is_open();
}

bool Udp::open(uint16_t localPort)
{
  return open("0.0.0.0", localPort);
}

bool Udp::open(const std::string& localAddress, uint16_t localPort)
{
  try
  {
    if (!_io)
      _io.reset(new boost::asio::io_service());

    if (!_socket)
      _socket.reset(new udp_t::socket(*_io));

    if (_socket->is_open())
      _socket->close();

    boost::system::error_code ec;
    const boost::asio::ip::address address = boost::asio::ip::address::from_string(localAddress, ec);
    if (ec)
    {
      logException("open", ec.message());
      return false;
    }

    udp_t::endpoint localEndpoint(address, localPort);
    _socket->open(localEndpoint.protocol());
    _socket->set_option(boost::asio::socket_base::reuse_address(true));
    _socket->bind(localEndpoint);

    _hasLastSenderEndpoint = false;
    return true;
  }
  catch (const std::exception& e)
  {
    logException("open", e.what());
    return false;
  }
}

bool Udp::open(const std::string& localAddress,
               uint16_t localPort,
               const std::string& remoteHost,
               uint16_t remotePort)
{
  if (!open(localAddress, localPort))
    return false;

  return setRemote(remoteHost, remotePort);
}

void Udp::close()
{
  if (!_socket)
    return;

  boost::system::error_code ignored;
  _socket->close(ignored);
}

bool Udp::resolveRemote(const std::string& remoteHost, uint16_t remotePort, udp_t::endpoint* endpoint)
{
  if (endpoint == nullptr || !_io)
    return false;

  try
  {
    udp_t::resolver resolver(*_io);
    udp_t::resolver::query query(udp_t::v4(), remoteHost, std::to_string(remotePort));
    udp_t::resolver::iterator it = resolver.resolve(query);

    if (it == udp_t::resolver::iterator())
      return false;

    *endpoint = *it;
    return true;
  }
  catch (const std::exception& e)
  {
    logException("resolveRemote", e.what());
    return false;
  }
}

bool Udp::setRemote(const std::string& remoteHost, uint16_t remotePort)
{
  udp_t::endpoint endpoint;
  if (!resolveRemote(remoteHost, remotePort, &endpoint))
    return false;

  _remoteEndpoint = endpoint;
  _hasRemoteEndpoint = true;
  return true;
}

bool Udp::hasRemote() const
{
  return _hasRemoteEndpoint;
}

bool Udp::hasLastSender() const
{
  return _hasLastSenderEndpoint;
}

std::string Udp::lastSenderAddress() const
{
  if (!_hasLastSenderEndpoint)
    return {};

  return _lastSenderEndpoint.address().to_string();
}

uint16_t Udp::lastSenderPort() const
{
  if (!_hasLastSenderEndpoint)
    return 0;

  return _lastSenderEndpoint.port();
}

bool Udp::useLastSenderAsRemote()
{
  if (!_hasLastSenderEndpoint)
    return false;

  _remoteEndpoint = _lastSenderEndpoint;
  _hasRemoteEndpoint = true;
  return true;
}

std::size_t Udp::receiveBytes(std::vector<uint8_t>* buffer, std::size_t maxBytes)
{
  if (buffer == nullptr || !isOpen())
    return 0;

  try
  {
    maxBytes = std::max<std::size_t>(1, std::min(maxBytes, MAX_SAFE_UDP_PAYLOAD));
    buffer->assign(maxBytes, 0);

    std::size_t n = _socket->receive_from(boost::asio::buffer(*buffer), _lastSenderEndpoint);
    buffer->resize(n);
    _hasLastSenderEndpoint = true;
    return n;
  }
  catch (const std::exception& e)
  {
    logException("receiveBytes", e.what());
    buffer->clear();
    return 0;
  }
}

std::size_t Udp::sendBytes(const uint8_t* bytes, std::size_t nbytes)
{
  if (bytes == nullptr || nbytes == 0 || !isOpen())
    return 0;

  try
  {
    const udp_t::endpoint* endpoint = nullptr;

    if (_hasRemoteEndpoint)
      endpoint = &_remoteEndpoint;
    else if (_hasLastSenderEndpoint)
      endpoint = &_lastSenderEndpoint;

    if (endpoint == nullptr)
    {
      logError("sendBytes called without a remote endpoint or last sender");
      return 0;
    }

    return _socket->send_to(boost::asio::buffer(bytes, nbytes), *endpoint);
  }
  catch (const std::exception& e)
  {
    logException("sendBytes", e.what());
    return 0;
  }
}

std::size_t Udp::sendBytes(uint8_t* bytes, std::size_t nbytes)
{
  return sendBytes(static_cast<const uint8_t*>(bytes), nbytes);
}

std::size_t Udp::sendBytes(const std::vector<uint8_t>& bytes)
{
  if (bytes.empty())
    return 0;

  return sendBytes(bytes.data(), bytes.size());
}

void Udp::logError(const std::string& msg) const
{
  std::cerr << "[Udp] " << msg << std::endl;
}

void Udp::logException(const std::string& method, const std::string& what) const
{
  std::cerr << "[Udp::" << method << "] " << what << std::endl;
}
