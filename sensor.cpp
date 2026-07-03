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

class Sensor {
public:
  uint8_t id;
  DeviceType type;
  int sock;
  int udp_sock;
  sockaddr_in manager_addr{};
  sockaddr_in udp_manager_addr{};

  std::mt19937 gen;
  std::normal_distribution<float> dist;

  Sensor(int id, DeviceType type)
      : id(id), type(type), sock(-1), gen(std::random_device{}()),
        dist(25.0, 5.0) {}

  void connect_to_manager() {
    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0) {
      perror("socket");
      exit(1);
    }

    manager_addr.sin_family = AF_INET;
    manager_addr.sin_port = htons(MANAGER_PORT);
    inet_pton(AF_INET, "127.0.0.1", &manager_addr.sin_addr);

    // Separate destination for UDP sensor data (manager listens on SENSOR_PORT)
    udp_manager_addr.sin_family = AF_INET;
    udp_manager_addr.sin_port = htons(SENSOR_PORT);
    inet_pton(AF_INET, "127.0.0.1", &udp_manager_addr.sin_addr);

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
      msg.deviceClass = DEVICE_CLASS_SENSOR;
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

      // TCP is no longer needed; all subsequent communication is UDP
      close(sock);
      sock = -1;

      udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
      if (udp_sock < 0) {
          perror("socket (udp)");
          exit(1);
      }
      break;
    }
  }

  void send_data(void) {
    while (true) {
      float raw_data = dist(gen);

      SensorData msg;
      msg.id = id;

      uint32_t bits;
      memcpy(&bits, &raw_data, sizeof(bits));
      msg.data = htonl(bits);
      ssize_t n = sendto(
          udp_sock,
          &msg,
          sizeof(msg),
          0,
          reinterpret_cast<sockaddr*>(&udp_manager_addr),
          sizeof(udp_manager_addr));

      if (n < 0)
          perror("sendto");

      sleep(1);
    }
  }

  void wait_for_quit() {
    std::cout << "Press 'q' + Enter to disconnect and quit.\n";
    char c;
    while (std::cin.get(c)) {
      if (c == 'q' || c == 'Q') {
        std::cout << "Closing connection...\n";
        if (udp_sock >= 0) { close(udp_sock); udp_sock = -1; }
        if (sock >= 0)     { close(sock);     sock = -1; }
        exit(0);
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

  Sensor sensor(id_val, type);

  sensor.connect_to_manager();

  std::cout << "Connected!" << std::endl;

  sensor.send_register();

  std::thread quit_thread(&Sensor::wait_for_quit, &sensor);
  quit_thread.detach();

  sensor.send_data();
}