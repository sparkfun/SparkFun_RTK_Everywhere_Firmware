#ifndef __NETWORK_CLIENT_H__
#define __NETWORK_CLIENT_H__

extern uint8_t networkGetType(uint8_t user);

class RTKNetworkClient : public Client
{
  protected:
    Client *_client; // Ethernet or WiFi client
    bool _friendClass;
    uint8_t _networkType;

  public:
    //------------------------------
    // Create the network client
    //------------------------------
    RTKNetworkClient(Client *client, uint8_t networkType)
    {
        _friendClass = true;
        _networkType = networkType;
        _client = client;
    }

    RTKNetworkClient(uint8_t user)
    {
        _friendClass = false;
        _networkType = networkGetType(user);
#if defined(COMPILE_ETHERNET)
        if (_networkType == NETWORK_TYPE_ETHERNET)
            _client = new EthernetClient;
        else
#endif // COMPILE_ETHERNET
#if defined(COMPILE_WIFI)
            _client = new WiFiClient;
#else  // COMPILE_WIFI
        _client = nullptr;
#endif // COMPILE_WIFI
    };

    //------------------------------
    // Delete the network client
    //------------------------------
    ~RTKNetworkClient()
    {
        if (_client)
        {
            _client->stop();
            if (!_friendClass)
                delete _client;
            _client = nullptr;
        }
    };

    //------------------------------
    // Determine if receive data is available
    //------------------------------

    int available()
    {
        if (_client)
            return _client->available();
        return 0;
    }

    //------------------------------
    // Determine if the network client was allocated
    //------------------------------

    operator bool()
    {
        return _client;
    }

    //------------------------------
    // Connect to the server
    //------------------------------

    int connect(IPAddress ip, uint16_t port)
    {
        if (_client)
            return _client->connect(ip, port);
        return 0;
    }

    int connect(const char *host, uint16_t port)
    {
        if (_client)
            return _client->connect(host, port);
        return 0;
    }

    //------------------------------
    // Determine if the client is connected to the server
    //------------------------------

    uint8_t connected()
    {
        if (_client)
            return _client->connected();
        return 0;
    }

    //------------------------------
    // Finish transmitting all the data to the server
    //------------------------------

    void flush()
    {
        if (_client)
            _client->flush();
    }

    //------------------------------
    // Look at the next received byte in the data stream
    //------------------------------

    int peek()
    {
        if (_client)
            return _client->peek();
        return -1;
    }

    //------------------------------
    // Display the network client status
    //------------------------------
    size_t print(const char *printMe)
    {
        if (_client)
            return _client->print(printMe);
        return 0;
    };

    //------------------------------
    // Receive a data byte from the server
    //------------------------------

    int read()
    {
        if (_client)
            return _client->read();
        return 0;
    }

    //------------------------------
    // Receive a buffer of data from the server
    //------------------------------

    int read(uint8_t *buf, size_t size)
    {
        if (_client)
            return _client->read(buf, size);
        return 0;
    }

    //------------------------------
    // Get the remote IP address
    //------------------------------

    IPAddress remoteIP()
    {
#if defined(COMPILE_ETHERNET)
        if (_networkType == NETWORK_TYPE_ETHERNET)
            return ((EthernetClient *)_client)->remoteIP();
#endif // COMPILE_ETHERNET
#if defined(COMPILE_WIFI)
        if (_networkType == NETWORK_TYPE_WIFI)
            return ((WiFiClient *)_client)->remoteIP();
#endif // COMPILE_WIFI
        return IPAddress((uint32_t)0);
    }

    //------------------------------
    // Get the remote port number
    //------------------------------

    uint16_t remotePort()
    {
#if defined(COMPILE_ETHERNET)
        if (_networkType == NETWORK_TYPE_ETHERNET)
            return ((EthernetClient *)_client)->remotePort();
#endif // COMPILE_ETHERNET
#if defined(COMPILE_WIFI)
        if (_networkType == NETWORK_TYPE_WIFI)
            return ((WiFiClient *)_client)->remotePort();
#endif // COMPILE_WIFI
        return 0;
    }

    //------------------------------
    // Stop the network client
    //------------------------------

    void stop()
    {
        if (_client)
            _client->stop();
    }

    //------------------------------
    // Send a data byte to the server
    //------------------------------

    size_t write(uint8_t b)
    {
        if (_client)
            return _client->write(b);
        return 0;
    }

    //------------------------------
    // Send a buffer of data to the server
    //------------------------------

    size_t write(const uint8_t *buf, size_t size)
    {
        if (_client)
            return _client->write(buf, size);
        return 0;
    }

  protected:
    //------------------------------
    // Return the IP address
    //------------------------------

    uint8_t *rawIPAddress(IPAddress &addr)
    {
        return Client::rawIPAddress(addr);
    }

    //------------------------------
    // Declare the friend classes
    //------------------------------

    friend class NetworkEthernetClient;
    friend class NetworkEthernetSslClient;
    friend class NetworkWiFiClient;
    friend class NetworkWiFiSslClient;
};

#ifdef COMPILE_ETHERNET
class NetworkEthernetClient : public RTKNetworkClient
{
  private:
    EthernetClient _ethernetClient;

  public:
    NetworkEthernetClient() : RTKNetworkClient(&_ethernetClient, NETWORK_TYPE_ETHERNET)
    {
    }

    NetworkEthernetClient(EthernetClient &client)
        : _ethernetClient{client}, RTKNetworkClient(&_ethernetClient, NETWORK_TYPE_ETHERNET)
    {
    }

    ~NetworkEthernetClient()
    {
        this->~RTKNetworkClient();
    }
};
#endif // COMPILE_ETHERNET

#ifdef COMPILE_WIFI
class NetworkWiFiClient : public RTKNetworkClient
{
  protected:
    WiFiClient _client;

  public:
    NetworkWiFiClient() : RTKNetworkClient(&_client, NETWORK_TYPE_WIFI)
    {
    }

    NetworkWiFiClient(WiFiClient &client) : _client{client}, RTKNetworkClient(&_client, NETWORK_TYPE_WIFI)
    {
    }

    ~NetworkWiFiClient()
    {
        this->~RTKNetworkClient();
    }
};

class NetworkSecureWiFiClient : public RTKNetworkClient
{
  protected:
    WiFiClientSecure _client;

  public:
    NetworkSecureWiFiClient() : _client{WiFiClientSecure()}, RTKNetworkClient(&_client, NETWORK_TYPE_WIFI)
    {
    }

    NetworkSecureWiFiClient(WiFiClient &client) : _client{client}, RTKNetworkClient(&_client, NETWORK_TYPE_WIFI)
    {
    }

    ~NetworkSecureWiFiClient()
    {
        this->~RTKNetworkClient();
    }

    WiFiClientSecure *getClient()
    {
        return &_client;
    }

    int lastError(char *buf, const size_t size)
    {
        return _client.lastError(buf, size);
    }

    void setInsecure() // Don't validate the chain, just accept whatever is given.  VERY INSECURE!
    {
        _client.setInsecure();
    }

    void setPreSharedKey(const char *pskIdent, const char *psKey) // psKey in Hex
    {
        _client.setPreSharedKey(pskIdent, psKey);
    }

    void setCACert(const char *rootCA)
    {
        _client.setCACert(rootCA);
    }

    void setCertificate(const char *client_ca)
    {
        _client.setCertificate(client_ca);
    }

    void setPrivateKey(const char *private_key)
    {
        _client.setPrivateKey(private_key);
    }

    bool loadCACert(Stream &stream, size_t size)
    {
        return _client.loadCACert(stream, size);
    }

    void setCACertBundle(const uint8_t *bundle)
    {
        _client.setCACertBundle(bundle);
    }

    bool loadCertificate(Stream &stream, size_t size)
    {
        return _client.loadCertificate(stream, size);
    }

    bool loadPrivateKey(Stream &stream, size_t size)
    {
        return _client.loadPrivateKey(stream, size);
    }

    bool verify(const char *fingerprint, const char *domain_name)
    {
        return _client.verify(fingerprint, domain_name);
    }

    void setHandshakeTimeout(unsigned long handshake_timeout)
    {
        _client.setHandshakeTimeout(handshake_timeout);
    }

    void setAlpnProtocols(const char **alpn_protos)
    {
        _client.setAlpnProtocols(alpn_protos);
    }

    /*
        const mbedtls_x509_crt* getPeerCertificate()
        {
            return mbedtls_ssl_get_peer_cert(&_client.sslclient->ssl_ctx);
        }

        bool getFingerprintSHA256(uint8_t sha256_result[32])
        {
            return get_peer_fingerprint(_client.sslclient, sha256_result);
        }
    */

    int setTimeout(uint32_t seconds)
    {
        return _client.setTimeout(seconds);
    }

    int fd() const
    {
        return _client.fd();
    }
};

#endif // COMPILE_WIFI

#endif // __NETWORK_CLIENT_H__
