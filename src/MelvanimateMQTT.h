
#pragma once

#include <AsyncMqttClient.h>

#include <ESP8266WiFi.h>
#include "Melvanimate.h"
#include "IPAddress.h"
#include <functional>
#include <ArduinoJson.h>

//#define DebugMelvanimateMQTT Serial

#if  defined(DebugMelvanimateMQTT)
#define DebugMelvanimateMQTTf(...) DebugMelvanimateMQTT.printf(__VA_ARGS__)
//#define DebugMelvanimateMQTTf(_1, ...) DebugMelvanimateMQTT.printf_P( PSTR(_1), ##__VA_ARGS__) //  this saves around 5K RAM...

#else
#define DebugMelvanimateMQTTf(...) {}
#endif

class Melvanimate;

class MelvanimateMQTT
{
public:

    MelvanimateMQTT(Melvanimate * lights, IPAddress Addr, uint16_t Port = 1883);

    ~MelvanimateMQTT()
    {
        _mqttClient.disconnect();

        if (_deviceid) {
            free( (void*)_deviceid);
        }
    }

    void loop();

    operator bool()
    {
        return _mqttClient.connected();
    }

    //bool publish(const char * topic, const char * payload);
    bool publish(const char * topic, const char * payload, size_t length, bool retained = false );
    bool publish(const char * topic, const char * payload, bool retained = false );

    void subscribe ( const char * topic, uint8_t qos )
    {
        _mqttClient.subscribe( topic, qos);
    }

    void onConnect(std::function <void(void)> func)
    {
        _connectCB = func;
    }

    void sendJson(bool onlychanged); //  { _send_changed_flag = millis();  _onlychanged = onlychanged; }
    void sendPresets();

    bool addJson(JsonObject & root);
    bool parseJson(JsonObject & root)
    {
    };

    IPAddress getIP()
    {
        return _addr;
    }

    uint16_t getPort()
    {
        return _port;
    }

    void connect()
    {
        _mqttClient.connect();
    }


private:

    void _handle(char* topic, byte* payload, unsigned int length);

    void _onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);

    uint32_t _reconnectTimer {0};
    Melvanimate * _melvanimate {nullptr};
    AsyncMqttClient _mqttClient;
    bool _disconnected{false};
    uint32_t _timeout{0};
    IPAddress _addr;
    uint16_t _port;
    bool _onlychanged {nullptr};
    uint16_t _reconnecttries {0};
    std::function <void(void)>  _connectCB;
    const char * _deviceid{nullptr};

};
