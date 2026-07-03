#include "protocol.hpp"
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <mutex>

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
  int udp_socket;
  bool sensor_registered;

  //Variaveis de controle do manager
  float max_temp;
  float min_temp;
  float max_co2;
  float min_co2;
  float max_humidity;
  float min_humidity;

  unordered_map<int, DeviceInfo> sensors;
  unordered_map<int, DeviceInfo> actuators;

  mutex state_mutex;
  mutex device_mutex;

  Manager() {
    temp = 0;
    co2 = 0;
    humidity = 0;

    max_temp = 35;
    min_temp = 15;
    max_co2 = 9999999;
    min_co2 = -1;
    max_humidity = 9999999;
    min_humidity = -1;
    sensor_registered = false;
  }

  void handle_sensor_data(DeviceType type, float data) {
    lock_guard<mutex> lock(state_mutex);
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
    //Destrava automaticamente o mutex
  }

  void udp_listener() {
    while(true) {
      SensorData msg;

        sockaddr_in sender;
        socklen_t len = sizeof(sender);
        std::cout << "Listening\n";

        ssize_t n = recvfrom(
            udp_socket,
            &msg,
            sizeof(msg),
            0,
            (sockaddr*)&sender,
            &len);

        if (n < 0) {
            perror("recvfrom");
            continue;
        }

        if (n != sizeof(msg)) {
            continue;
        }
        
        uint8_t host_id = msg.id;
        uint32_t host_data = ntohl(msg.data);

        if(sensors.find(host_id) == sensors.end()) continue;

        float value;
        memcpy(&value, &host_data, sizeof(float));
        printf("Received data from %d: %f\n", host_id, value);
        handle_sensor_data(sensors[host_id].type, value);
    } 
  }

  void open_sockets() {
    listening_socket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in tcp_addr{};
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_addr.sin_port = htons(MANAGER_PORT);

    if (::bind(listening_socket, reinterpret_cast<sockaddr *>(&tcp_addr),
               sizeof(tcp_addr)) < 0) {
      perror("bind");
      exit(1);
    }

    if (listen(listening_socket, SOMAXCONN) < 0) {
      perror("listen");
      exit(1);
    }

    cout << "Listening to port " << MANAGER_PORT << endl;

    

  }

  int register_device(const DeviceInfo &device, DeviceClass cls) {
    lock_guard<mutex> lock(device_mutex);
    if (cls == DEVICE_CLASS_SENSOR) {
      if(sensors.find(device.id) != sensors.end()){
        //ID ja existe
        return 1;
      }
      sensors[device.id] = device;
      if(!sensor_registered) {
        sensor_registered = true;

        udp_socket = socket(AF_INET, SOCK_DGRAM, 0);

        sockaddr_in udp_addr{};
        udp_addr.sin_family = AF_INET;
        udp_addr.sin_addr.s_addr = INADDR_ANY;
        udp_addr.sin_port = htons(SENSOR_PORT);

        if (::bind(udp_socket,
                reinterpret_cast<sockaddr*>(&udp_addr),
                sizeof(udp_addr)) < 0)
        {
            perror("bind");
            exit(1);
        } 
        
        std::thread(&Manager::udp_listener, this).detach();
      }
    } else if (cls == DEVICE_CLASS_ACTUATOR) {
      if(actuators.find(device.id) != actuators.end()){
        //ID ja existe
        return 1;
      }
      actuators[device.id] = device;
    }

    return 0;
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

      if(register_device(device, msg.deviceClass)){
        std::cout << "Tentativa de conexao de ID ja existente: " << device.id << endl;
        continue;
      }

      RegisterAck ack;
      ack.header.first_byte = (PROTOCOL_ID << 4) | REGISTER_ACK;
      ack.id = msg.id;

      if (send(client_socket, &ack, sizeof(ack), 0) < 0) {
        perror("send");
      }

      std::thread(&Manager::connection_handler, this, device).detach();
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
      case (ACTUATOR_STATUS): {
        ActuatorStatusMsg msg;
        ssize_t bytes_read = recv(device.socket, &msg, sizeof(msg), MSG_WAITALL);
        if (bytes_read == sizeof(msg)) {
          // Atualiza o estado do atuador na memória do servidor
          lock_guard<mutex> lock(device_mutex);
          actuators[msg.id].status = msg.status;
          std::cout << "Actuator status received: " << (int)msg.id << " -> " 
                    << (msg.status == ACTUATOR_ON ? "ON" : "OFF") << std::endl;
        } else {
          return; //Atuador desconectou ou erro
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

  void find_set_actuators(DeviceType tipo, ActuatorStatus status){
    lock_guard<mutex> lock(device_mutex);
    for(auto &[id, device] : actuators){
      if(device.type == tipo && device.status != status){
        ActuatorSet msg;
        msg.header.first_byte = (PROTOCOL_ID << 4) | ACTUATOR_SET;
        msg.id = device.id;
        msg.status = status;
        send(device.socket, &msg, sizeof(msg), 0);
        device.status = status;
      }
    }
  };

  void control_loop(){
    float local_temp, local_co2, local_humidity;
    while(true){
      //Checar se algum valor esta fora do padrao
      
      {
        lock_guard<mutex> lock(state_mutex);
        local_temp = temp;
        local_co2 = co2;
        local_humidity = humidity;
      }
      if(local_temp > max_temp){ //Desliga os aquecedores e liga os coolers
        find_set_actuators(DEVICE_TEMP_SENSOR_OR_HEATER, ACTUATOR_OFF);
        find_set_actuators(DEVICE_COOLER, ACTUATOR_ON);
      }
      if(local_temp < min_temp){ //Liga os aquecedores e desliga os coolers
        find_set_actuators(DEVICE_TEMP_SENSOR_OR_HEATER, ACTUATOR_ON);
        find_set_actuators(DEVICE_COOLER, ACTUATOR_OFF);
      }
      if(local_co2 > max_co2){ //Desliga os injetores
        find_set_actuators(DEVICE_CO2_SENSOR_OR_INJECTOR, ACTUATOR_OFF);
      }
      if(local_co2 < min_co2){ //Liga os injetores
        find_set_actuators(DEVICE_CO2_SENSOR_OR_INJECTOR, ACTUATOR_ON);
      }
      if(local_humidity > max_humidity){ //Desliga os irrigadores
        find_set_actuators(DEVICE_SOIL_MOISTURE_OR_IRRIGATION, ACTUATOR_OFF);
      }
      if(local_humidity < min_humidity){ //Liga os irrigadores
        find_set_actuators(DEVICE_SOIL_MOISTURE_OR_IRRIGATION, ACTUATOR_ON);
      }

      sleep(1);
    }
  }
  void wait_for_quit() {
    std::cout << "Press 'q' + Enter to shut down the manager.\n";
    char c;
    while (std::cin.get(c)) {
      if (c == 'q' || c == 'Q') {
        std::cout << "Shutting down...\n";
        for (auto &[id, dev] : sensors)   { close(dev.socket); }
        for (auto &[id, dev] : actuators) { close(dev.socket); }
        if (udp_socket >= 0)      { close(udp_socket); }
        if (listening_socket >= 0){ close(listening_socket); }
        exit(0);
      }
    }
  }
};

int main(int argc, char *argv[]) {
  Manager manager = Manager();
  
  manager.open_sockets();
  std::thread(&Manager::control_loop, &manager).detach();
  std::thread quit_thread(&Manager::wait_for_quit, &manager);
  quit_thread.detach();

  manager.accept_connection();
}