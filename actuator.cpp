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
      : id(id), type(type), sock(-1) {}

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

      RegisterAck ack;
      ssize_t n = recv(sock, &ack, sizeof(ack), MSG_WAITALL);

      if (n < (ssize_t)sizeof(ack)) {
        continue;
      }
      std::cout << "REGISTER acknowledged\n";
      break;
    }
  }

  void receive_commands() {
    Header header;
    while (true) {
      ssize_t n = recv(sock, &header, sizeof(header), MSG_PEEK);
      if (n <= 0) return;

      int msg_type = header.first_byte & 0x0f;

      // Dummy logic for testing PEEK and consuming bytes
      recv(sock, &header, sizeof(header), 0);
    }
  }
};

int main(int argc, char *argv[]) {
  Actuator actuator(2, DEVICE_COOLER);

  actuator.connect_to_manager();

  std::cout << "Connected!" << std::endl;

  actuator.send_register();
  actuator.receive_commands();

  return 0;
}