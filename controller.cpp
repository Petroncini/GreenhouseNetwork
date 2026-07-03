#include "protocol.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

// Helpers for converting between float and network uint32

static uint32_t float_to_net(float f) {
    uint32_t tmp;
    memcpy(&tmp, &f, sizeof(tmp));
    return htonl(tmp);
}

static float net_to_float(uint32_t raw) {
    uint32_t host = ntohl(raw);
    float f;
    memcpy(&f, &host, sizeof(f));
    return f;
}

class Controller {
public:
    int sock;

    Controller() : sock(-1) {}

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
        cout << "Controller connected to manager on port " << MANAGER_PORT << endl;
    }

    // Sends a query for the last known value of a sensor type and prints
    // the SENSOR_RESPONSE returned by the manager
    void query_sensor(DataType type) {
        SensorQuery msg;
        msg.header.first_byte = (PROTOCOL_ID << 4) | SENSOR_QUERY;
        msg.type = type;

        if (send(sock, &msg, sizeof(msg), 0) < 0) {
            perror("send SENSOR_QUERY");
            return;
        }

        SensorResponse resp;
        ssize_t n = recv(sock, &resp, sizeof(resp), MSG_WAITALL);
        if (n != sizeof(resp)) {
            cerr << "Error: incomplete SENSOR_RESPONSE (got " << n << " bytes)\n";
            return;
        }

        int resp_type = resp.header.first_byte & 0x0f;
        if (resp_type != SENSOR_RESPONSE) {
            cerr << "Unexpected message type: " << resp_type << endl;
            return;
        }

        float value = net_to_float(resp.data);

        const char *labels[] = {"Temperature (°C)", "Humidity (%)", "CO2 (ppm)"};
        cout << "Sensor reading — "
             << labels[resp.type] << ": " << value << endl;
    }

    // Sends a new min/max limit for a parameter and prints the CONFIG_ACK
    void set_config(DataType type, Boundary boundary, float value) {
        ConfigSet msg;
        msg.header.first_byte = (PROTOCOL_ID << 4) | CONFIG_SET;
        msg.type   = type;
        msg.minMax = boundary;
        msg.value  = float_to_net(value);

        if (send(sock, &msg, sizeof(msg), 0) < 0) {
            perror("send CONFIG_SET");
            return;
        }

        ConfigAck ack;
        ssize_t n = recv(sock, &ack, sizeof(ack), MSG_WAITALL);
        if (n != sizeof(ack)) {
            cerr << "Error: incomplete CONFIG_ACK (got " << n << " bytes)\n";
            return;
        }

        int resp_type = ack.header.first_byte & 0x0f;
        if (resp_type != CONFIG_ACK) {
            cerr << "Unexpected message type: " << resp_type << endl;
            return;
        }

        const char *labels[]    = {"Temperature", "Humidity", "CO2"};
        const char *boundaries[] = {"min", "max"};
        cout << "Config ACK — "
             << labels[ack.type] << " " << boundaries[ack.minMax]
             << " set to " << net_to_float(ack.value) << endl;
    }

    // Queries the manager for the current min/max limits of a parameter
    void query_config(DataType type) {
        QueryConfig msg;
        msg.header.first_byte = (PROTOCOL_ID << 4) | QUERY_CONFIG;
        msg.type = type;

        if (send(sock, &msg, sizeof(msg), 0) < 0) {
            perror("send QUERY_CONFIG");
            return;
        }

        ConfigResponse resp;
        ssize_t n = recv(sock, &resp, sizeof(resp), MSG_WAITALL);
        if (n != sizeof(resp)) {
            cerr << "Error: incomplete CONFIG_RESPONSE (got " << n << " bytes)\n";
            return;
        }

        int resp_type = resp.header.first_byte & 0x0f;
        if (resp_type != CONFIG_RESPONSE) {
            cerr << "Unexpected message type: " << resp_type << endl;
            return;
        }

        const char *labels[] = {"Temperature (°C)", "Humidity (%)", "CO2 (ppm)"};
        cout << "Config for " << labels[resp.type] << ":\n"
             << "  min = " << net_to_float(resp.minValue) << "\n"
             << "  max = " << net_to_float(resp.maxValue) << endl;
    }

    void run() {
        cout << "\n=== Greenhouse Controller ===\n"
             << "Commands:\n"
             << "  1 <data_type>                     — query sensor (0 = temperature, 1 = humidity, 2 = CO2)\n"
             << "  2 <data_type> <0=min|1=max> <val> — set config limit\n"
             << "  3 <data_type>                     — query current config limits\n"
             << "  q                                  — quit\n\n";

        string cmd;
        while (true) {
            cout << "> ";
            if (!(cin >> cmd))
				break;

			// Quit
            if (cmd == "q" || cmd == "Q") {
                cout << "Disconnecting.\n";
                break;
            }

            int choice = 0;
			
			// Handling of invalid commands

            try { choice = stoi(cmd); } catch (...) {
                cout << "Unknown command. Type 1, 2, 3, or q.\n";
                continue;
            }

            int type_int;
            if (!(cin >> type_int) || type_int < 0 || type_int > 2) {
                cout << "Invalid data type (0=temp, 1=humidity, 2=co2)\n";
                cin.clear();
				cin.ignore(1024, '\n');
                continue;
            }
            DataType dtype = static_cast<DataType>(type_int);

            switch (choice) {
                case 1: {
                    query_sensor(dtype);
                    break;
                }
                case 2: {
                    int boundary_int;
                    float value;
                    if (!(cin >> boundary_int) || boundary_int < 0 || boundary_int > 1) {
                        cout << "Invalid boundary (0=min, 1=max)\n";
                        cin.clear();
						cin.ignore(1024, '\n');
                        break;
                    }
                    if (!(cin >> value)) {
                        cout << "Invalid value\n";
                        cin.clear();
						cin.ignore(1024, '\n');
                        break;
                    }
                    set_config(dtype, static_cast<Boundary>(boundary_int), value);
                    break;
                }
                case 3: {
                    query_config(dtype);
                    break;
                }
                default:
                    cout << "Unknown command. Type 1, 2, 3, or q.\n";
                    break;
            }
        }

        close(sock);
    }
};

int main(int argc, char *argv[]) {
    Controller controller;
    controller.connect_to_manager();
    controller.run();
    return 0;
}