#ifndef SYNC_HIL_HIL_UDP_H
#define SYNC_HIL_HIL_UDP_H

#include <memory>
#include <mutex>

#include "frame.h"
#include "udp.h"

class HilUdp
{
public:
  explicit HilUdp(Udp* udp);
  virtual ~HilUdp();

  HilUdp(const HilUdp& other) = delete;
  HilUdp& operator=(const HilUdp& other) = delete;

  HilUdp(HilUdp&& other) noexcept;
  HilUdp& operator=(HilUdp&& other) noexcept;

  const Udp* getUdp() const;
  Udp* getUdp();

  // Blocking receive of one UDP datagram containing one complete encoded Frame.
  // The returned Frame is still encoded; caller should call frame.unbuild().
  bool readFrame(Frame* frame);

  // Blocking send of one UDP datagram containing one complete encoded Frame.
  // This calls frame.build() internally.
  bool sendFrame(Frame& frame);

private:
  std::unique_ptr<Udp> _udp;
  std::mutex _mutex;
};

#endif  // SYNC_HIL_HIL_UDP_H
