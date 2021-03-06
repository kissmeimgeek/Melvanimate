

#include "UDP_broadcast.h"

// OLD
// #define UDP_PING_TIMEOUT 30000
// #define UDP_TASK_TIMEOUT 5000
// #define UDP_STALE_TIMEOUT 100000

#define UDP_PING_TIMEOUT 5 * 60 * 1000   //  5min ping interval
#define UDP_TASK_TIMEOUT 60 * 1000       //  1 min tasker
#define UDP_STALE_TIMEOUT 20 * 60 * 1000 // 20min stale remove...

static const IPAddress MELVANIMATE_MULTICAST_ADDR(224, 0, 0, 251);

void UDP_broadcast::begin(const char * host, uint16_t port)  {

        _host = host;
        _port = port;

        if (WiFi.isConnected()) {
                _state = _udp.beginMulticast( WiFi.localIP(),  MELVANIMATE_MULTICAST_ADDR, _port);
        }

        _gotIPHandler = WiFi.onStationModeGotIP([this](const WiFiEventStationModeGotIP& event){
                DebugUDPf("[UDP_broadcast::_gotIPHandler]\n");
                _restart();

        });

        _disconnectedHandler = WiFi.onStationModeDisconnected([this](const WiFiEventStationModeDisconnected& event) {
                DebugUDPf("[UDP_broadcast::_disconnectedHandler]\n");
                _restart();

        });

        _sendRequest(PING);

        DebugUDPf("[UDP_broadcast::begin] Finished\n");
}

void UDP_broadcast::loop() {

        if (_udp.parsePacket()) {
                _parsePacket();
        }

        // if (millis() - _lastmessage > 10000) {
        //         _sendRequest(PONG);
        //         _lastmessage = millis();
        // }

#ifdef UDP_TEST_SENDER
        _test_sender();
#endif

        if (devices.size()) {

                uint32_t last_PING = (**devices.begin()).lastseen;

                if (millis() - _checkTimeOut > UDP_TASK_TIMEOUT) {
                        // purge stale entries
                        for (UDPList::iterator it=devices.begin(); it!=devices.end(); ++it) {

                                UDP_item & item = **it;

                                if (item.lastseen > last_PING) {
                                        last_PING = item.lastseen;
                                }

                                if (  millis() - item.lastseen > UDP_STALE_TIMEOUT) {

                                        //  std::delete me from the list.....
                                        DebugUDPf("[UDP_broadcast::loop] Removed %s (%u.%u.%u.%u) not seen for %u\n", item.name.get(), item.IP[0],item.IP[1],item.IP[2],item.IP[3], millis() - item.lastseen );
                                        //DebugUDPf("     NEED TO ADD DELETE \n");
                                        it = devices.erase(it);
                                        _sendRequest(PING);
                                }
                        }

                        // get earliest last PING...

                        if ( millis() - last_PING > UDP_PING_TIMEOUT && !_sendPong) {
                                DebugUDPf("[UDP_broadcast::loop] Sending PING\n");
                                _sendRequest(PING);
                        }

                        //only run the loop every so many seconds...
                        _checkTimeOut = millis();
                }
        } else {

                if (millis() - _checkTimeOut > UDP_TASK_TIMEOUT) {
                        DebugUDPf("[UDP_broadcast::loop] No Devices found sending PING\n");
                        _sendRequest(PING);
                        _checkTimeOut = millis();
                }


        }

//  send replies with delay
        if (_sendPong && _sendPong < millis()) {
                _sendRequest(PONG);
                _sendPong = 0;
        }


}

void UDP_broadcast::_restart() {
        DebugUDPf("[UDP_broadcast::_restart]\n");
        if (_udp) {
                _udp.stop();
        }

        _listen();


}

void UDP_broadcast::setHost(const char * host ) {
        _host = host;
}

bool UDP_broadcast::_listen() {
        DebugUDPf("[UDP_broadcast::_listen]\n");

        _state = _udp.beginMulticast( WiFi.localIP(),  MELVANIMATE_MULTICAST_ADDR, _port);

}

void UDP_broadcast::_parsePacket() {

        IPAddress IP;

        size_t size = _udp.available();

        UDP_REQUEST_TYPE method = static_cast<UDP_REQUEST_TYPE>(_udp.read());   //byte 1

        char tmp[2];

        tmp[0] = _udp.read(); // byte 2
        tmp[1] = _udp.read(); // byte 3

        uint16_t port = ((uint16_t)tmp[1] << 8) | tmp[0];

        for (uint8_t i = 0; i < 4; i++)  {
                IP[i] = _udp.read();  //bytes 4,5,6,7
        }
        uint8_t host_len = _udp.read();  // byte 8

        std::unique_ptr<char[]> buf(new char[host_len + 1]);

        _udp.read( buf.get(),host_len);  // bytes 8;
        buf[host_len] = '\0';
        _udp.flush();
        DebugUDPf("[UDP_broadcast::_parsePacket] UDP RECIEVED [%u] %s (%u.%u.%u.%u:%u) %s\n", millis(),(method == PING) ? "PING" : "PONG",IP[0], IP[1], IP[2], IP[3], port, buf.get());
        _addToList(IP, std::move(buf) );



        if (method == PING) {
                DebugUDPf("[UDP_broadcast::_parsePacket] Recieved PING: RESPONDING WITH PONG\n");
                _sendPong = millis() + random(0,50);
        }
}

void UDP_broadcast::_sendRequest(UDP_REQUEST_TYPE method) {

        DebugUDPf("[UDP_broadcast::_sendRequest] type = %s, state=%u, _udp=%u, port=%u :", (method == PING) ? "PING" : "PONG", (_udp) ? true : false, _state, _port);

        if (!_udp || !_state) {
                DebugUDPf(" failed\n");
                return;
        }

        IPAddress IP = WiFi.localIP();

        if (_udp.beginPacketMulticast(MELVANIMATE_MULTICAST_ADDR, _port, WiFi.localIP())) {

                const char ip[4] = { IP[0],IP[1],IP[2],IP[3] };

                _udp.write(reinterpret_cast<const uint8_t *>(&method), 1);
                _udp.write(reinterpret_cast<const uint8_t *>(&_port), 2);
                _udp.write( ip, 4);

                if (_host) {
                        //DebugUDPf("[UDP_broadcast::_sendRequest] host = %s\n", _host);
                        uint8_t host_len = strlen(_host);
                        _udp.write(reinterpret_cast<const uint8_t *>(&host_len), 1);
                        _udp.write(_host, strlen(_host) + 1);
                } else {
                        //DebugUDPf("[UDP_broadcast::_sendRequest] No Host\n");
                        const char * no_host = "No Host";
                        _udp.write(reinterpret_cast<const uint8_t *>(&no_host), strlen(no_host) + 1);
                }

                if (_udp.endPacket()) {
                        DebugUDPf(" success\n");
                        return;
                }
        }


        DebugUDPf(" failed\n");





}

void UDP_broadcast::_addToList(IPAddress IP, std::unique_ptr<char[]>(ID) ) {

        if (!ID) {
                return;
        }

        if (strlen(ID.get()) > 33 ) {
                return;
        }

        for (UDPList::iterator it=devices.begin(); it!=devices.end(); ++it) {
                //UDP_item & item = *(*it).get();
                UDP_item & item = **it;

                if (IP == item.IP) {
                        if ( strcmp(ID.get(), item.name.get() )  )  {
                                DebugUDPf("[UDP_broadcast::_addToList] name different reassigning %s->%s\n", item.name.get(), ID.get());
                                std::unique_ptr<char[]> p( new char[strlen(ID.get()) + 1]  );
                                item.name = std::move(p);
                                strcpy(  item.name.get(), ID.get() );
                        }
                        item.lastseen = millis(); //  last seen time set whenever packet recieved....
                        return;
                }
        }

        devices.push_back( std::unique_ptr<UDP_item> (new UDP_item(IP, ID.get() ) )  );

}

uint8_t UDP_broadcast::count() {
        return devices.size() + 1;
}

void UDP_broadcast::addJson(JsonArray & root) {

        //  this adds the current device.  Rather than keep it in memory...
        JsonObject & thisdevice = root.createNestedObject();
        JsonArray & IPjson = thisdevice.createNestedArray("IP");

        IPAddress IP = WiFi.localIP();

        IPjson.add(IP[0]);
        IPjson.add(IP[1]);
        IPjson.add(IP[2]);
        IPjson.add(IP[3]);

        thisdevice["name"] = _host;


        for (UDPList::iterator it=devices.begin(); it!=devices.end(); ++it) {
                UDP_item & udp_item = **it;

                JsonObject & item = root.createNestedObject();

                JsonArray & IP = item.createNestedArray("IP");
                IP.add(udp_item.IP[0]);
                IP.add(udp_item.IP[1]);
                IP.add(udp_item.IP[2]);
                IP.add(udp_item.IP[3]);

                item["name"] = udp_item.name.get();

        }
}


#ifdef UDP_TEST_SENDER

void UDP_broadcast::_test_sender() {
        static uint32_t timeout = 0;
        static uint16_t number = 0;
        UDP_REQUEST_TYPE method = PING;

        if (millis() - timeout > 10000) {

                timeout = millis();

                if (_udp.beginPacketMulticast(MELVANIMATE_MULTICAST_ADDR, _port, WiFi.localIP())) {

                        char buf[32] = {'\0'};

                        //strcpy(buf, "Test Sender ");

                        snprintf(buf, 32, "Test Sender %u", number++);

                        const char ip[4] = { 192, 168, 1, (uint8_t)random (1,255) };
                        method = PONG;

                        _udp.write(reinterpret_cast<const uint8_t *>(&method), 1);
                        _udp.write(reinterpret_cast<const uint8_t *>(&_port), 2);
                        _udp.write( ip, 4);

                        if (_host) {
                                //DebugUDPf("[UDP_broadcast::_sendRequest] host = %s\n", _host);
                                uint8_t host_len = strlen(buf);
                                _udp.write(reinterpret_cast<const uint8_t *>(&host_len), 1);
                                _udp.write(buf, host_len + 1);
                        } else {
                                //DebugUDPf("[UDP_broadcast::_sendRequest] No Host\n");
                                const char * no_host = "No Host";
                                _udp.write(reinterpret_cast<const uint8_t *>(&no_host), strlen(no_host) + 1);
                        }

                        if (_udp.endPacket()) {
                                DebugUDPf(" success\n");
                                return;
                        }
                }
        }

///  old
        //         if (!_conn) {
        //                 return;
        //         }
        //
        //         char buf[32] = {'\0'};
        //
        //         char * end = strcpy(buf, "Test Sender ");
        //
        //         snprintf( end, 5, "%u", number++);
        //
        //         const char ip[4] = { 192, 168, 1, (uint8_t)random (1,255) };
        //
        //         _conn->flush();
        //         _conn->append(reinterpret_cast<const char*>(&method), 1);
        //         _conn->append(reinterpret_cast<const char*>(&_port), 2);
        //         _conn->append( ip, 4);
        //
        //         if (_host) {
        //                 //DebugUDPf("[UDP_broadcast::_sendRequest] host = %s\n", _host);
        //                 uint8_t host_len = strlen(_host);
        //                 _conn->append(reinterpret_cast<const char*>(&host_len), 1);
        //                 _conn->append(_host, strlen(_host) + 1);
        //         } else {
        //                 //DebugUDPf("[UDP_broadcast::_sendRequest] No Host\n");
        //                 _conn->append("No Host", 7);
        //         }
        //
        //
        //         _conn->send();
        //
        // }


}

#endif
