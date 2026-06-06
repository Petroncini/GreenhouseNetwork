#include "protocol.hpp"
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>

using namespace std;

struct DeviceInfo {
  int socket;
  int id;
  DeviceClass cls;
  DeviceType type;
  ActuatorStatus status;

  DeviceInfo() = default;
  DeviceInfo(int socket, int id, DeviceClass cls, DeviceType type)
      : socket(socket), id(id), cls(cls), type(type) {
    status = ACTUATOR_OFF;
  }
};

class Manager {
public:
  float temp;
  float co2;
  float humidity;
  int listening_socket;

  unordered_map<int, DeviceInfo> sensors;
  unordered_map<int, DeviceInfo> actuators;

  Manager() {
    temp = 0;
    co2 = 0;
    humidity = 0;
  }

  void open_listening_socket() {
    listening_socket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(MANAGER_PORT);

    if (::bind(listening_socket, reinterpret_cast<sockaddr *>(&addr),
               sizeof(addr)) < 0) {
      perror("bind");
      exit(1);
    }

    if (listen(listening_socket, SOMAXCONN) < 0) {
      perror("listen");
      exit(1);
    }

    cout << "Listening to port " << MANAGER_PORT << endl;
  }

  void register_device(const DeviceInfo &device, DeviceClass cls) {
    if (cls == DEVICE_CLASS_SENSOR) {
      sensors[device.id] = device;
    } else if (cls == DEVICE_CLASS_ACTUATOR) {
      actuators[device.id] = device;
    }
  }

  void accept_connection() {
    while (true) {
      int client_socket = accept(listening_socket, nullptr, nullptr);

      cout << "New connection: " << client_socket << endl;

      RegisterMessage msg;

      ssize_t n = recv(client_socket, &msg, sizeof(msg), MSG_WAITALL);

      if (n < (ssize_t)sizeof(msg)) {
        close(client_socket);
        continue;
      }

      printf("New device register: %d\n", msg.id);

      DeviceInfo device =
          DeviceInfo(client_socket, msg.id, msg.deviceClass, msg.deviceType);

      register_device(device, msg.deviceClass);

      std::thread(&Manager::connection_handler, this, device).detach();

      RegisterAck ack;
      ack.header.first_byte = (PROTOCOL_ID << 4) | REGISTER_ACK;
      ack.id = msg.id;

      if (send(client_socket, &ack, sizeof(ack), 0) < 0) {
        perror("send");
      }
    }
  }

  void handle_sensor_data(DeviceType type, float data) {
    switch (type) {
      case (DEVICE_TEMP_SENSOR_OR_HEATER):
        temp = data;
        break;
      case (DEVICE_SOIL_MOISTURE_OR_IRRIGATION):
        humidity = data;
        break;
      case (DEVICE_CO2_SENSOR_OR_INJECTOR):
        co2 = data;
        break;
      default:
        break;
    }
  }

  void connection_handler(DeviceInfo device) {
    Header header;

    while (true) {
      ssize_t n = recv(device.socket, &header, sizeof(header), MSG_PEEK);
      if (n <= 0)
        return;

      int msg_type = header.first_byte & 0x0f;

      switch (msg_type) {
      case (SENSOR_DATA): {
        SensorData msg;
        ssize_t bytes_read = recv(device.socket, &msg, sizeof(msg), MSG_WAITALL);
        //Confere para ver se chegou o total de bytes esperados
        if (bytes_read == sizeof(msg)) {
          handle_sensor_data(device.type, msg.data);
        } else {
          return; //Sensor desconectou ou houve outro erro
        }
        break;
      }
      default: {
        //Lixo de rede ou mensagem não implementada (recebe só pra limpar a fila)
        recv(device.socket, &header, sizeof(header), 0);
        break;
      }
      }
    }
  }
};

int main(int argc, char *argv[]) {
  Manager manager = Manager();

  manager.open_listening_socket();
  manager.accept_connection();
}