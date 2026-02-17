/**********************************************************************
  Telnet.h

  Robots-For-All (R4A)
  Telnet client and server declarations
  See: https://www.rfc-editor.org/rfc/pdfrfc/rfc854.txt.pdf
**********************************************************************/

#ifndef __TELNET_H__
#define __TELNET_H__

#include <Arduino.h>
#include <WiFi.h>

//****************************************
// Telnet Client API
//****************************************

// Forward declaration
class R4A_TELNET_SERVER;

//*********************************************************************
// Telnet client
class R4A_TELNET_CLIENT : public WiFiClient
{
private:
    R4A_TELNET_CLIENT * _nextClient; // Next client in the server's client list
    String _command; // User command received via telnet

    // Allow the server to access the command string
    friend R4A_TELNET_SERVER;

public:

    // Constructor
    R4A_TELNET_CLIENT()
    {
    }

    // Allow the R4A_TELNET_SERVER access to the private members
    friend class R4A_TELNET_SERVER;
};

//****************************************
// Telnet Server API
//****************************************

// Telnet server
class R4A_TELNET_SERVER : WiFiServer
{
private:
    enum R4A_TELNET_SERVER_STATE
    {
        R4A_TELNET_STATE_OFF = 0,
        R4A_TELNET_STATE_ALLOCATE_CLIENT,
        R4A_TELNET_STATE_LISTENING,
        R4A_TELNET_STATE_SHUTDOWN,
    };
    uint8_t _state; // Telnet server state
    uint16_t _port; // Port number for the telnet server
    bool _echo;
    R4A_TELNET_CLIENT * _newClient; // New client object, ready for listen call
    R4A_TELNET_CLIENT * _clientList; // Singlely linked list of telnet clients

public:

    Print * _debugState; // Address of Print object to display telnet server state changes
    Print * _displayOptions; // Address of Print object to display telnet options

    // Constructor
    // Inputs:
    //   menuTable: Address of table containing the menu descriptions, the
    //              main menu must be the first entry in the table.
    //   menuEntries: Number of entries in the menu table
    //   blankLineBeforePreMenu: Display a blank line before the preMenu
    //   blankLineBeforeMenuHeader: Display a blank line before the menu header
    //   blankLineAfterMenuHeader: Display a blank line after the menu header
    //   alignCommands: Align the commands
    //   blankLineAfterMenu: Display a blank line after the menu
    //   port: Port number for internet connection
    //   echo: When true causes the input characters to be echoed
    R4A_TELNET_SERVER(uint16_t port = 23,
                      bool echo = false)
        : _state{0}, _port{port}, _echo{echo}, _newClient{nullptr},
          _clientList{nullptr}, _debugState{nullptr}, _displayOptions{nullptr},
          WiFiServer()
    {
    }

    // Destructor
    ~R4A_TELNET_SERVER()
    {
        if (_newClient)
            delete _newClient;
        while (_clientList)
        {
            R4A_TELNET_CLIENT * client = _clientList;
            _clientList = client->_nextClient;
            client->stop();
            delete client;
        }
    }

    // Display the client list
    void displayClientList(Print * display = &Serial);

    // Get the telnet port number
    //  Returns the telnet port number set during initalization
    uint16_t port();

    // Start and update the telnet server
    // Inputs:
    //   wifiConnected: Set true when WiFi is connected to an access point
    //                  and false otherwise
    void update(bool wifiConnected);
};

extern class R4A_TELNET_SERVER telnet;  // Server providing telnet access

#endif  // __TELNET_H__
