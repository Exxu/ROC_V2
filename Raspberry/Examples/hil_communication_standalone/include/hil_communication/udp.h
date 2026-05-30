#ifndef SYNC_HIL_UDP_H
#define SYNC_HIL_UDP_H

#include <boost/asio.hpp>
#include <boost/asio/ip/udp.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Udp
{
public:
  using udp_t = boost::asio::ip::udp;
  static constexpr std::size_t MAX_SAFE_UDP_PAYLOAD = 65507;

  Udp();
  virtual ~Udp();

  Udp(const Udp& other) = delete;
  Udp& operator=(const Udp& other) = delete;

  Udp(Udp&& other) noexcept;
  Udp& operator=(Udp&& other) noexcept;

  bool isOpen() const;

  // Bind local UDP socket. Use "0.0.0.0" to listen on all IPv4 interfaces.
  bool open(uint16_t localPort);
  bool open(const std::string& localAddress, uint16_t localPort);

  // Bind local socket and configure the default remote peer.
  bool open(const std::string& localAddress,
            uint16_t localPort,
            const std::string& remoteHost,
            uint16_t remotePort);

  void close();

  // Configure default remote peer for PC-initiated communication.
  bool setRemote(const std::string& remoteHost, uint16_t remotePort);
  bool hasRemote() const;

  // Last endpoint that sent us a datagram. Useful for embedded-initiated protocols.
  bool hasLastSender() const;
  std::string lastSenderAddress() const;
  uint16_t lastSenderPort() const;

  // Force replies to go to the last sender endpoint.
  bool useLastSenderAsRemote();

  // Blocking receive of one complete UDP datagram.
  std::size_t receiveBytes(std::vector<uint8_t>* buffer,
                           std::size_t maxBytes = MAX_SAFE_UDP_PAYLOAD);

  // Blocking send of one UDP datagram to the default remote endpoint.
  // If no remote endpoint was set, it replies to the last sender if available.
  std::size_t sendBytes(const uint8_t* bytes, std::size_t nbytes);
  std::size_t sendBytes(uint8_t* bytes, std::size_t nbytes);
  std::size_t sendBytes(const std::vector<uint8_t>& bytes);

protected:
  std::unique_ptr<boost::asio::io_service> _io;
  std::unique_ptr<udp_t::socket> _socket;

  udp_t::endpoint _remoteEndpoint;
  udp_t::endpoint _lastSenderEndpoint;

  bool _hasRemoteEndpoint;
  bool _hasLastSenderEndpoint;

  void logError(const std::string& msg) const;
  void logException(const std::string& method, const std::string& what) const;

private:
  bool resolveRemote(const std::string& remoteHost, uint16_t remotePort, udp_t::endpoint* endpoint);
};

#endif  // SYNC_HIL_UDP_H
