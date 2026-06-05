#include <stdint.h>

typedef enum { DEVICE_CLASS_SENSOR = 0, DEVICE_CLASS_ACTUATOR = 1 } DeviceClass;

typedef enum {
  DEVICE_TEMP_SENSOR_OR_HEATER = 0,
  DEVICE_SOIL_MOISTURE_OR_IRRIGATION = 1,
  DEVICE_CO2_SENSOR_OR_INJECTOR = 2,
  DEVICE_COOLER = 3
} DeviceType;

typedef enum { ACTUATOR_OFF = 0, ACTUATOR_ON = 1 } ActuatorStatus;

typedef enum { DATA_TEMPERATURE = 0, DATA_HUMIDITY = 1, DATA_CO2 = 2 } DataType;

typedef enum {
  REGISTER = 1,
  REGISTER_ACK = 2,
  SENSOR_DATA = 3,
  ACTUATOR_STATUS_REQ = 4,
  ACTUATOR_STATUS = 5,
  ACTUATOR_SET = 6,
  SENSOR_QUERY = 7,
  SENSOR_RESPONSE = 8,
  CONFIG_SET = 9,
  CONFIG_ACK = 10,
  QUERY_CONFIG = 11,
  CONFIG_RESPONSE = 12
} MessageType;

#define PROTOCOL_ID 0xF

#define MANAGER_PORT 5003

struct Header {
  uint8_t first_byte;
};

struct RegisterMessage {
  Header header;
  int id;
  DeviceClass deviceClass;
  DeviceType deviceType;
};

struct RegisterAck {
  Header header;
  int id;
};

struct SensorData {
  Header header;
  int id;
  float data;
};
