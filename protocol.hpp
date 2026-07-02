#include <stdint.h>

enum DeviceClass : uint8_t { DEVICE_CLASS_SENSOR = 0, DEVICE_CLASS_ACTUATOR = 1 };

enum DeviceType : uint8_t {
  DEVICE_TEMP_SENSOR_OR_HEATER = 0,
  DEVICE_SOIL_MOISTURE_OR_IRRIGATION = 1,
  DEVICE_CO2_SENSOR_OR_INJECTOR = 2,
  DEVICE_COOLER = 3
};

enum ActuatorStatus : uint8_t { ACTUATOR_OFF = 0, ACTUATOR_ON = 1 };

enum DataType : uint8_t { DATA_TEMPERATURE = 0, DATA_HUMIDITY = 1, DATA_CO2 = 2 };

enum MessageType : uint8_t {
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
};

enum Boundary : uint8_t {  BOUNDARY_MIN = 0, BOUNDARY_MAX = 1 };

#define PROTOCOL_ID 0xF

#define MANAGER_PORT 5007

#define SENSOR_PORT 6006

#pragma pack(push, 1)

struct Header {
  uint8_t first_byte;
};

struct RegisterMessage {
  Header header;
  uint8_t id;
  DeviceClass deviceClass;
  DeviceType deviceType;
};

struct RegisterAck {
  Header header;
  uint8_t id;
};

struct SensorData {
  uint8_t id;
  uint32_t data;
};

struct ActuatorStatusReq {
  Header header;
  uint8_t id;
};

struct ActuatorStatusMsg {
  Header header;
  uint8_t id;
  ActuatorStatus status;
};

struct ActuatorSet {
  Header header;
  uint8_t id;
  ActuatorStatus status;
};

struct SensorQuery {
  Header header;
  DataType type;
};

struct SensorResponse {
  Header header;
  DataType type;
  uint32_t data;
};

struct ConfigSet {
  Header header;
  DataType type;
  Boundary minMax;
  uint32_t value;
};

struct ConfigAck {
  Header header;
  DataType type;
  Boundary minMax;
  uint32_t value;
};

struct QueryConfig {
  Header header;
  DataType type;
};

struct ConfigResponse {
  Header header;
  DataType type;
  uint32_t minValue;
  uint32_t maxValue;
};

#pragma pack(pop)
