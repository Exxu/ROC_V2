#include "roc_radio_tool_bridge/hil_frame.hpp"
#include "roc_radio_tool_bridge/mavlink_minimal.hpp"
#include "roc_radio_tool_bridge/serial_port.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{
using roc_radio_tool_bridge::HilFrame;
using roc_radio_tool_bridge::MavlinkMessage;
using roc_radio_tool_bridge::MavlinkMinimalEndpoint;
using roc_radio_tool_bridge::SerialPort;

constexpr int kCmdUp = 1;
constexpr int kCmdDown = 2;
constexpr int kCmdStatus = 3;

constexpr int kResponseAck = 1;
constexpr int kResponseStatus = 2;

std::atomic<bool> g_running{true};

void handleSignal(int)
{
  g_running = false;
}

enum class SwitchPosition
{
  Unknown,
  Down,
  Up  
};

struct Config
{
  std::string mavlink_bind_ip = "127.0.0.1";
  uint16_t mavlink_bind_port = 14542;
  std::string mavlink_request_ip = "127.0.0.1";
  uint16_t mavlink_request_port = 14540;
  bool request_rc_stream = true;
  float request_rc_rate_hz = 10.0f;
  uint8_t target_system = 1;
  uint8_t target_component = 1;

  int rc_channel = 8;
  int switch_threshold_us = 1500;
  int switch_hysteresis_us = 80;
  bool invert_switch = true;

  std::string tool_port = "/dev/ttyAMA0";
  int tool_baud = 9600;
  int tool_id = 1;
  unsigned int ack_timeout_ms = 400;
  bool dry_run = false;
  bool request_status_after_command = false;
};

const char* switchName(SwitchPosition position)
{
  switch (position)
  {
    case SwitchPosition::Down: return "DOWN";
    case SwitchPosition::Up: return "UP";
    default: return "UNKNOWN";
  }
}

const char* commandName(int command)
{
  switch (command)
  {
    case kCmdUp: return "UP";
    case kCmdDown: return "DOWN";
    case kCmdStatus: return "STATUS";
    default: return "UNKNOWN";
  }
}

const char* resultName(int result)
{
  switch (result)
  {
    case 0: return "OK";
    case 1: return "INVALID_COMMAND";
    case 2: return "REJECTED_AT_TOP";
    case 3: return "REJECTED_AT_BOTTOM";
    case 4: return "RELAY_CONFLICT";
    default: return "UNKNOWN_RESULT";
  }
}

bool parseInt(const std::string& text, int& out)
{
  try
  {
    std::size_t used = 0;
    const int value = std::stoi(text, &used, 10);
    if (used != text.size())
      return false;
    out = value;
    return true;
  }
  catch (...)
  {
    return false;
  }
}

bool parseUInt16(const std::string& text, uint16_t& out)
{
  int value = 0;
  if (!parseInt(text, value) || value < 0 || value > 65535)
    return false;
  out = static_cast<uint16_t>(value);
  return true;
}

bool parseUInt8(const std::string& text, uint8_t& out)
{
  int value = 0;
  if (!parseInt(text, value) || value < 0 || value > 255)
    return false;
  out = static_cast<uint8_t>(value);
  return true;
}

bool parseFloat(const std::string& text, float& out)
{
  try
  {
    std::size_t used = 0;
    const float value = std::stof(text, &used);
    if (used != text.size())
      return false;
    out = value;
    return true;
  }
  catch (...)
  {
    return false;
  }
}

void printUsage(const char* program)
{
  std::cout
      << "Usage:\n"
      << "  " << program << " [options]\n\n"
      << "Default use for ROC:\n"
      << "  " << program << " --tool-port /dev/ttyAMA0\n\n"
      << "Important defaults:\n"
      << "  RC channel:              8\n"
      << "  Two-position switch:     low=DOWN, high=UP\n"
      << "  Startup command:         UP\n"
      << "  MAVLink receive UDP:     127.0.0.1:14542\n"
      << "  MAVLink request UDP:     127.0.0.1:14540\n"
      << "  Tool serial port:        /dev/ttyAMA0 at 9600 bps\n\n"
      << "Options:\n"
      << "  --dry-run                         Do not open the RS485 port; only print actions\n"
      << "  --tool-port <path>                RS485 serial port [default: /dev/ttyAMA0]\n"
      << "  --tool-baud <baud>                RS485 baudrate [default: 9600]\n"
      << "  --tool-id <id>                    Tool firmware ID [default: 1]\n"
      << "  --rc-channel <1..18>              RC channel [default: 8]\n"
      << "  --switch-threshold <us>           Threshold between DOWN and UP [default: 1500]\n"
      << "  --switch-hysteresis <us>          Hysteresis around threshold [default: 80]\n"
      << "  --invert-switch                   Map low PWM to UP and high PWM to DOWN\n"
      << "  --mavlink-bind-ip <ip>            UDP bind IP [default: 127.0.0.1]\n"
      << "  --mavlink-bind-port <port>        UDP bind port [default: 14542]\n"
      << "  --mavlink-request-ip <ip>         UDP destination for COMMAND_LONG [default: 127.0.0.1]\n"
      << "  --mavlink-request-port <port>     UDP destination port [default: 14540]\n"
      << "  --request-rc-rate-hz <hz>         Requested RC_CHANNELS rate [default: 10]\n"
      << "  --no-request-rc-stream            Do not request RC_CHANNELS rate from ArduPilot\n"
      << "  --target-system <id>              MAVLink target system [default: 1]\n"
      << "  --target-component <id>           MAVLink target component [default: 1]\n"
      << "  --ack-timeout-ms <ms>             Tool ACK timeout [default: 400]\n"
      << "  --status-after-command            Request STATUS after each UP/DOWN command\n"
      << "  --help                            Show this help\n";
}

bool parseArguments(int argc, char** argv, Config& cfg)
{
  for (int i = 1; i < argc; ++i)
  {
    const std::string arg = argv[i];

    auto needValue = [&](const std::string& option) -> const char* {
      if (i + 1 >= argc)
      {
        std::cerr << "ERROR: missing value after " << option << std::endl;
        return nullptr;
      }
      return argv[++i];
    };

    if (arg == "--help" || arg == "-h")
    {
      printUsage(argv[0]);
      std::exit(0);
    }
    else if (arg == "--dry-run")
    {
      cfg.dry_run = true;
    }
    else if (arg == "--invert-switch")
    {
      cfg.invert_switch = true;
    }
    else if (arg == "--no-request-rc-stream")
    {
      cfg.request_rc_stream = false;
    }
    else if (arg == "--status-after-command")
    {
      cfg.request_status_after_command = true;
    }
    else if (arg == "--tool-port")
    {
      const char* value = needValue(arg);
      if (!value) return false;
      cfg.tool_port = value;
    }
    else if (arg == "--mavlink-bind-ip")
    {
      const char* value = needValue(arg);
      if (!value) return false;
      cfg.mavlink_bind_ip = value;
    }
    else if (arg == "--mavlink-request-ip")
    {
      const char* value = needValue(arg);
      if (!value) return false;
      cfg.mavlink_request_ip = value;
    }
    else if (arg == "--tool-baud")
    {
      const char* value = needValue(arg);
      if (!value || !parseInt(value, cfg.tool_baud)) return false;
    }
    else if (arg == "--tool-id")
    {
      const char* value = needValue(arg);
      if (!value || !parseInt(value, cfg.tool_id)) return false;
    }
    else if (arg == "--rc-channel")
    {
      const char* value = needValue(arg);
      if (!value || !parseInt(value, cfg.rc_channel)) return false;
    }
    else if (arg == "--switch-threshold")
    {
      const char* value = needValue(arg);
      if (!value || !parseInt(value, cfg.switch_threshold_us)) return false;
    }
    else if (arg == "--switch-hysteresis")
    {
      const char* value = needValue(arg);
      if (!value || !parseInt(value, cfg.switch_hysteresis_us)) return false;
    }
    else if (arg == "--mavlink-bind-port")
    {
      const char* value = needValue(arg);
      if (!value || !parseUInt16(value, cfg.mavlink_bind_port)) return false;
    }
    else if (arg == "--mavlink-request-port")
    {
      const char* value = needValue(arg);
      if (!value || !parseUInt16(value, cfg.mavlink_request_port)) return false;
    }
    else if (arg == "--request-rc-rate-hz")
    {
      const char* value = needValue(arg);
      if (!value || !parseFloat(value, cfg.request_rc_rate_hz)) return false;
    }
    else if (arg == "--target-system")
    {
      const char* value = needValue(arg);
      if (!value || !parseUInt8(value, cfg.target_system)) return false;
    }
    else if (arg == "--target-component")
    {
      const char* value = needValue(arg);
      if (!value || !parseUInt8(value, cfg.target_component)) return false;
    }
    else if (arg == "--ack-timeout-ms")
    {
      int value = 0;
      const char* text = needValue(arg);
      if (!text || !parseInt(text, value) || value < 0) return false;
      cfg.ack_timeout_ms = static_cast<unsigned int>(value);
    }
    else
    {
      std::cerr << "ERROR: unknown option: " << arg << std::endl;
      return false;
    }
  }

  if (cfg.rc_channel != 8)
  {
    std::cerr << "WARNING: this ROC test is intended for RC channel 8. Current channel: "
              << cfg.rc_channel << std::endl;
  }

  if (cfg.rc_channel < 1 || cfg.rc_channel > 18)
  {
    std::cerr << "ERROR: --rc-channel must be between 1 and 18" << std::endl;
    return false;
  }

  if (cfg.switch_hysteresis_us < 0 || cfg.switch_hysteresis_us >= 400)
  {
    std::cerr << "ERROR: --switch-hysteresis must be in the range [0, 399] us" << std::endl;
    return false;
  }

  return true;
}

SwitchPosition classifyTwoPositionSwitch(uint16_t pwm_us, SwitchPosition previous, const Config& cfg)
{
  const int pwm = static_cast<int>(pwm_us);
  const int low_limit = cfg.switch_threshold_us - cfg.switch_hysteresis_us;
  const int high_limit = cfg.switch_threshold_us + cfg.switch_hysteresis_us;

  SwitchPosition raw = SwitchPosition::Unknown;

  if (previous == SwitchPosition::Up)
  {
    raw = (pwm <= low_limit) ? SwitchPosition::Down : SwitchPosition::Up;
  }
  else if (previous == SwitchPosition::Down)
  {
    raw = (pwm >= high_limit) ? SwitchPosition::Up : SwitchPosition::Down;
  }
  else
  {
    raw = (pwm >= cfg.switch_threshold_us) ? SwitchPosition::Up : SwitchPosition::Down;
  }

  if (!cfg.invert_switch)
    return raw;

  if (raw == SwitchPosition::Up)
    return SwitchPosition::Down;

  if (raw == SwitchPosition::Down)
    return SwitchPosition::Up;

  return raw;
}

bool readAndPrintToolResponse(SerialPort& serial, unsigned int timeout_ms)
{
  std::vector<uint8_t> raw_frame;
  if (!serial.readFrame(raw_frame, timeout_ms))
  {
    std::cout << "[tool] RX timeout waiting for response" << std::endl;
    return false;
  }

  HilFrame response;
  if (!response.decodeFromBytes(raw_frame))
  {
    std::cout << "[tool] RX invalid HIL frame" << std::endl;
    return false;
  }

  float f_device = 0.0f;
  float f_response_type = 0.0f;

  if (!response.getFloat(0, f_device) || !response.getFloat(1, f_response_type))
  {
    std::cout << "[tool] RX payload too short" << std::endl;
    return false;
  }

  const int device = static_cast<int>(f_device);
  const int response_type = static_cast<int>(f_response_type);

  if (response_type == kResponseAck)
  {
    float f_command = 0.0f;
    float f_result = 0.0f;
    if (!response.getFloat(2, f_command) || !response.getFloat(3, f_result))
    {
      std::cout << "[tool] RX ACK payload too short" << std::endl;
      return false;
    }

    const int command = static_cast<int>(f_command);
    const int result = static_cast<int>(f_result);

    std::cout << "[tool] ACK device=" << device
              << " command=" << commandName(command)
              << " result=" << resultName(result) << std::endl;
    return true;
  }

  if (response_type == kResponseStatus)
  {
    float f_hl = 0.0f;
    float f_ll = 0.0f;
    if (!response.getFloat(2, f_hl) || !response.getFloat(3, f_ll))
    {
      std::cout << "[tool] RX STATUS payload too short" << std::endl;
      return false;
    }

    const int hl = static_cast<int>(f_hl);
    const int ll = static_cast<int>(f_ll);

    std::cout << "[tool] STATUS device=" << device
              << " HL=" << hl
              << " LL=" << ll << std::endl;
    return true;
  }

  std::cout << "[tool] RX unknown response type: " << response_type << std::endl;
  return false;
}

bool sendToolCommand(SerialPort* serial, const Config& cfg, int command)
{
  std::cout << "[tool] TX command=" << commandName(command)
            << " device=" << cfg.tool_id << std::endl;

  if (cfg.dry_run)
  {
    std::cout << "[dry-run] command not sent to RS485" << std::endl;
    return true;
  }

  if (serial == nullptr || !serial->isOpen())
  {
    std::cerr << "ERROR: serial port is not open" << std::endl;
    return false;
  }

  HilFrame request;
  request.addFloat(static_cast<float>(cfg.tool_id));
  request.addFloat(static_cast<float>(command));

  if (!request.build())
  {
    std::cerr << "ERROR: cannot build HIL frame" << std::endl;
    return false;
  }

  if (!serial->writeAll(request.encoded().data(), request.encoded().size()))
  {
    std::cerr << "ERROR: cannot write HIL frame to RS485" << std::endl;
    return false;
  }

  return readAndPrintToolResponse(*serial, cfg.ack_timeout_ms);
}

void printStartupSummary(const Config& cfg)
{
  std::cout << "ROC RC tool bridge started" << std::endl;
  std::cout << "  RC channel:       " << cfg.rc_channel << std::endl;
  std::cout << "  switch mode:      two positions, low=DOWN, high=UP"
            << (cfg.invert_switch ? " (inverted)" : "") << std::endl;
  std::cout << "  threshold:        " << cfg.switch_threshold_us
            << " us, hysteresis=" << cfg.switch_hysteresis_us << " us" << std::endl;
  std::cout << "  MAVLink RX:       " << cfg.mavlink_bind_ip << ":" << cfg.mavlink_bind_port << std::endl;
  std::cout << "  MAVLink request:  " << cfg.mavlink_request_ip << ":" << cfg.mavlink_request_port << std::endl;
  std::cout << "  tool serial:      " << cfg.tool_port << " @ " << cfg.tool_baud << " bps" << std::endl;
  std::cout << "  startup action:   send UP once before reading switch edges" << std::endl;
}

}  // namespace

int main(int argc, char** argv)
{
  Config cfg;
  if (!parseArguments(argc, argv, cfg))
  {
    printUsage(argv[0]);
    return 2;
  }

  std::signal(SIGINT, handleSignal);
  std::signal(SIGTERM, handleSignal);

  printStartupSummary(cfg);

  SerialPort serial;
  if (!cfg.dry_run)
  {
    if (!serial.openPort(cfg.tool_port, cfg.tool_baud))
      return 1;
  }

  MavlinkMinimalEndpoint mavlink;
  if (!mavlink.openEndpoint(cfg.mavlink_bind_ip,
                            cfg.mavlink_bind_port,
                            cfg.mavlink_request_ip,
                            cfg.mavlink_request_port))
  {
    return 1;
  }

  std::cout << "[startup] Sending tool UP command once" << std::endl;
  sendToolCommand(cfg.dry_run ? nullptr : &serial, cfg, kCmdUp);

  auto last_request_time = std::chrono::steady_clock::time_point{};
  auto last_rc_log_time = std::chrono::steady_clock::now();
  SwitchPosition last_switch_position = SwitchPosition::Unknown;
  bool baseline_captured = false;

  while (g_running)
  {
    const auto now = std::chrono::steady_clock::now();

    if (cfg.request_rc_stream &&
        (last_request_time.time_since_epoch().count() == 0 ||
         std::chrono::duration_cast<std::chrono::milliseconds>(now - last_request_time).count() >= 2000))
    {
      mavlink.sendSetMessageInterval(65, cfg.request_rc_rate_hz,
                                     cfg.target_system, cfg.target_component);
      last_request_time = now;
    }

    MavlinkMessage message;
    if (!mavlink.receiveMessage(message, 200))
      continue;

    uint16_t pwm_us = 0;
    if (!roc_radio_tool_bridge::extractRcChannelPwm(message, cfg.rc_channel, pwm_us))
      continue;

    const SwitchPosition current_position =
        classifyTwoPositionSwitch(pwm_us, last_switch_position, cfg);

    const auto log_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_rc_log_time).count();
    if (log_elapsed_ms >= 1000)
    {
      std::cout << "[rc] ch" << cfg.rc_channel << "=" << pwm_us
                << " us -> " << switchName(current_position) << std::endl;
      last_rc_log_time = now;
    }

    if (!baseline_captured)
    {
      last_switch_position = current_position;
      baseline_captured = true;
      std::cout << "[rc] baseline captured: ch" << cfg.rc_channel << "=" << pwm_us
                << " us -> " << switchName(current_position)
                << ". No DOWN/UP edge command is sent for the initial switch position."
                << std::endl;
      continue;
    }

    if (current_position == last_switch_position)
      continue;

    std::cout << "[rc] switch changed: " << switchName(last_switch_position)
              << " -> " << switchName(current_position)
              << " at " << pwm_us << " us" << std::endl;

    last_switch_position = current_position;

    const int command = (current_position == SwitchPosition::Up) ? kCmdUp : kCmdDown;
    sendToolCommand(cfg.dry_run ? nullptr : &serial, cfg, command);

    if (cfg.request_status_after_command)
      sendToolCommand(cfg.dry_run ? nullptr : &serial, cfg, kCmdStatus);

    // Avoid immediately sampling an old RC packet while the Arduino is still processing the command.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  std::cout << "Stopping ROC RC tool bridge" << std::endl;
  return 0;
}
