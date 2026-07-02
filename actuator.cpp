#include "protocol.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <random>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <string>
#include <algorithm>
#include <stdexcept>

class Actuator {
public:
  int id;
  DeviceType type;
  int sock;
  ActuatorStatus current_status;

  Actuator(int id, DeviceType type)
      : id(id), type(type), sock(-1), current_status(ACTUATOR_OFF) {}

  void connect_to_manager() {
    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0) {
      perror("socket");
      exit(1);
    }

    sockaddr_in manager_addr{};
    manager_addr.sin_family = AF_INET;
    manager_addr.sin_port = htons(MANAGER_PORT);

    inet_pton(AF_INET, "127.0.0.1", &manager_addr.sin_addr);

    if (connect(sock, reinterpret_cast<sockaddr *>(&manager_addr),
                sizeof(manager_addr)) < 0) {
      perror("connect");
      exit(1);
    }
  }

  void send_register() {
    while (true) {
      if (sock == -1)
        return;
      RegisterMessage msg;
      msg.header.first_byte = (PROTOCOL_ID << 4) | REGISTER;

      msg.id = id;
      msg.deviceClass = DEVICE_CLASS_ACTUATOR;
      msg.deviceType = type;

      if (send(sock, &msg, sizeof(msg), 0) < 0) {
        perror("send");
      }

      std::cout << "REGISTER sent" << std::endl;

      RegisterAck ack;
      ssize_t n = recv(sock, &ack, sizeof(ack), MSG_WAITALL);

      if (n < (ssize_t)sizeof(ack)) {
        continue;
      }
      std::cout << "REGISTER acknowledged\n";
      break;
    }
  }

  void send_status() {
    ActuatorStatusMsg msg;
    msg.header.first_byte = (PROTOCOL_ID << 4) | ACTUATOR_STATUS;
    msg.id = id;
    msg.status = current_status;
    send(sock, &msg, sizeof(msg), 0);
    std::cout << "ACTUATOR_STATUS sent (" << (current_status == ACTUATOR_ON ? "ON" : "OFF") << ")" << std::endl;
  }

  void receive_commands() {
    Header header;
    while (true) {
      ssize_t n = recv(sock, &header, sizeof(header), MSG_PEEK);
      if (n <= 0) return;

      int msg_type = header.first_byte & 0x0f;

      switch (msg_type) {
        case ACTUATOR_STATUS_REQ: {
          ActuatorStatusReq req;
          ssize_t bytes_read = recv(sock, &req, sizeof(req), MSG_WAITALL);
          if (bytes_read == sizeof(req)) {
             if (req.id == this->id) {
               std::cout << "ACTUATOR_STATUS_REQ received." << std::endl;
               send_status();
             }
          } else { 
             return; 
          }
          break;
        }
        case ACTUATOR_SET: {
          ActuatorSet set_cmd;
          ssize_t bytes_read = recv(sock, &set_cmd, sizeof(set_cmd), MSG_WAITALL);
          if (bytes_read == sizeof(set_cmd)) {
             if (set_cmd.id == this->id) {
               current_status = set_cmd.status;
               std::cout << "ACTUATOR_SET received. State changed to " << (current_status == ACTUATOR_ON ? "ON" : "OFF") << std::endl;
               send_status();
             }
          } else { 
             return; 
          }
          break;
        }
        default: {
          //Consumindo bytes desconhecidos
          recv(sock, &header, sizeof(header), 0);
          break;
        }
      }
    }
  }
};

int main(int argc, char *argv[]) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <device_id> <device_type>\n"
              << "Device types:\n"
              << "  0 / temp / heater / temperature\n"
              << "  1 / soil / moisture / irrigation\n"
              << "  2 / co2 / injector\n"
              << "  3 / cooler\n";
    return 1;
  }

  int id_val;
  try {
    id_val = std::stoi(argv[1]);
    if (id_val < 0 || id_val > 255) {
      throw std::out_of_range("ID must be between 0 and 255");
    }
  } catch (const std::exception& e) {
    std::cerr << "Invalid device ID: " << argv[1] << " (" << e.what() << ")\n";
    return 1;
  }

  DeviceType type;
  try {
    std::string type_arg = argv[2];
    std::transform(type_arg.begin(), type_arg.end(), type_arg.begin(), ::tolower);
    if (type_arg == "0" || type_arg == "temp" || type_arg == "heater" || type_arg == "temperature") {
      type = DEVICE_TEMP_SENSOR_OR_HEATER;
    } else if (type_arg == "1" || type_arg == "soil" || type_arg == "moisture" || type_arg == "irrigation") {
      type = DEVICE_SOIL_MOISTURE_OR_IRRIGATION;
    } else if (type_arg == "2" || type_arg == "co2" || type_arg == "injector") {
      type = DEVICE_CO2_SENSOR_OR_INJECTOR;
    } else if (type_arg == "3" || type_arg == "cooler") {
      type = DEVICE_COOLER;
    } else {
      throw std::invalid_argument("Unknown device type");
    }
  } catch (const std::exception& e) {
    std::cerr << "Invalid device type: " << argv[2] << " (" << e.what() << ")\n"
              << "Allowed values: 0 (temp), 1 (soil), 2 (co2), 3 (cooler)\n";
    return 1;
  }

  Actuator actuator(id_val, type);

  actuator.connect_to_manager();

  std::cout << "Connected!" << std::endl;

  actuator.send_register();
  actuator.receive_commands();

  return 0;
}