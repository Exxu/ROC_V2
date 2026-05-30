#ifndef SYNC_HIL_HIL_SERIAL_H
#define SYNC_HIL_HIL_SERIAL_H

#include <memory>
#include <mutex>

#include "frame.h"
#include "serial.h"

class HilSerial
{
public:
  explicit HilSerial(Serial* serial);
  virtual ~HilSerial();

  HilSerial(const HilSerial& other) = delete;
  HilSerial& operator=(const HilSerial& other) = delete;

  HilSerial(HilSerial&& other) noexcept;
  HilSerial& operator=(HilSerial&& other) noexcept;

  const Serial* getSerial() const;
  Serial* getSerial();

  // Blocking read. Returns true only after one complete frame was received.
  bool readFrame(Frame* frame);

  // Blocking write. Builds the frame and writes the complete encoded buffer.
  bool sendFrame(Frame& frame);

protected:
  std::unique_ptr<Serial> _serial;

private:
  std::mutex _mutex;
};

#endif  // SYNC_HIL_HIL_SERIAL_H
