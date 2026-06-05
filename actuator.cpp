#include "protocol.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <random>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

class Actuator {
public:
  int id;
  DeviceType type;
  int sock;

  Actuator(int id, DeviceType type)
      : id(id), type(type), sock(-1), gen(std::random_device{}()),
        dist(25.0, 5.0) {}

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
      msg.deviceClass = DEVICE_CLASS_SENSOR;
      msg.deviceType = type;

      if (send(sock, &msg, sizeof(msg), 0) < 0) {
        perror("send");
      }

      std::cout << "REGISTER sent" << std::endl;

      ssize_t n = recv(sock, &msg, sizeof(msg), 0);

      if (n <= 0) {
        continue;
      }
      std::cout << "REGISTER acknowledged\n";
      break;
    }
  }

  void send_data(void) {
    while (true) {
      float data = dist(gen);

      SensorData msg;
      msg.header.first_byte = (PROTOCOL_ID << 4) | SENSOR_DATA;
      msg.id = id;
      msg.data = data;

      send(sock, &msg, sizeof(msg), 0);
      sleep(1);
    }
  }
};

int main(int argc, char *argv[]) {
  Sensor sensor(1, DEVICE_TEMP_SENSOR_OR_HEATER);

  sensor.connect_to_manager();

  std::cout << "Connected!" << std::endl;

  sensor.send_register();
  sensor.send_data();

  sleep(1000);
}