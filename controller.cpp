#include "protocol.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;


// Funçoes auxiliares para conversao entre float e uint32_t

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

    // Inicializa a conexao com o gerenciador
    void connect_to_manager() {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("socket");
            exit(1);
        }

        // Configura o endereco do gerenciador
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

    // Consulta o ultimo valor registrado por um sensor de dado tipo 
    // e imprime o SENSOR_RESPONSE retornado pelo manager
    void query_sensor(DataType type) {
        // Constroi a mensagem SENSOR_QUERY
        SensorQuery msg;
        msg.header.first_byte = (PROTOCOL_ID << 4) | SENSOR_QUERY;
        msg.type = type;

        if (send(sock, &msg, sizeof(msg), 0) < 0) {
            perror("send SENSOR_QUERY");
            return;
        }

        // Recebe e verifica a resposta SENSOR_RESPONSE

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

        // Converte o valor recebido para float e imprime a resposta formatada
        float value = net_to_float(resp.data);

        const char *labels[] = {"Temperature (°C)", "Humidity (%)", "CO2 (ppm)"};
        cout << "Sensor reading — "
             << labels[resp.type] << ": " << value << endl;
    }

    // Reconfigura o limite maximo ou minimo de um parametro e imprime o CONFIG_ACK
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

    // Consulta os limites atuais de um parametro configurados no gerenciador
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
		// Menu de comandos
        cout << "\n============== Greenhouse Controller ==============\n"
             << "Commands:		— Data types: 0 = Temperature, 1 = Humidity, 2 = CO2\n\n"
			 << " Query sensor:\n"
             << "  [1] <data_type>\n\n"
			 << " Query config limits:\n"
             << "  [2] <data_type>\n\n"
			 << " Set config limit:\n"
             << "  [3] <data_type> <0=min|1=max> <value>\n\n"
			 << " Disconnect and quit:\n"
             << "  [q]\n\n"
			 << "Type the command number and the parameters\n";

        string cmd;
        while (true) {
            cout << "> ";
            if (!(cin >> cmd))
				break;

			// Encerrar o controlador
            if (cmd == "q" || cmd == "Q") {
                cout << "Disconnecting.\n";
                break;
            }

            int choice = 0;
			
			// Tratamento de entradas invalidas

            // Verifica o numero do comandoś
            try { choice = stoi(cmd); } catch (...) {
                cout << "Unknown command. Type 1, 2, 3, or q.\n";
                continue;
            }

            int type_int;

            // Verifica o numero do tipo de dado
            if (!(cin >> type_int) || type_int < 0 || type_int > 2) {
                cout << "Invalid data type. Use: 0 = Temperature, 1 = Humidity, 2 = CO2\n";
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
                    query_config(dtype);
                    break;
                }
                case 3: {
					int boundary_int;
                    float value;
                    // Verifica se o parametro de <min|max> e 0 ou 1
                    if (!(cin >> boundary_int) || boundary_int < 0 || boundary_int > 1) {
                        cout << "Invalid boundary. Use the format: 3 <data_type> <0=min|1=max> <value>\n";
                        cin.clear();
						cin.ignore(1024, '\n');
                        break;
                    }
                    // Verifica se o valor do parametro e valido
                    if (!(cin >> value)) {
                        cout << "Invalid value\n";
                        cin.clear();
						cin.ignore(1024, '\n');
                        break;
                    }
                    // Envia CONFIG_SET ao gerenciador
                    set_config(dtype, static_cast<Boundary>(boundary_int), value);
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