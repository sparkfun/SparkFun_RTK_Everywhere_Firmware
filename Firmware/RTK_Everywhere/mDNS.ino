/*------------------------------------------------------------------------------
mDNS.ino

  This module implements the local server for the multicast domain name
  service (mDNS).
------------------------------------------------------------------------------*/

#ifdef COMPILE_NETWORK

//----------------------------------------
// Constants
//----------------------------------------

#define MDNS_MIN_POLLING_MSEC       100     // Milliseconds delay for polling
#define MDNS_START_DELAY_MSEC       1000    // Milliseconds delay after network startup

#define MDNS_MAX_BACKOFF_MSEC       (15 * MILLISECONDS_IN_A_MINUTE)
enum
{
    MDNS_STATE_OFF = 0,
    MDNS_STATE_WAIT_FOR_NETWORK,
    MDNS_STATE_START_DELAY,
    MDNS_ADD_SERVICES,
    MDNS_STATE_RUNNING,
    MDNS_STATE_SHUTTING_DOWN,
    // Add new states here
    MDNS_STATE_MAX
} MDNS_STATES;

// List of state names
const char *const mdnsStateNames[] = {
    "MDNS_STATE_OFF",
    "MDNS_STATE_WAIT_FOR_NETWORK",
    "MDNS_STATE_START_DELAY",
    "MDNS_ADD_SERVICES",
    "MDNS_STATE_RUNNING",
    "MDNS_STATE_SHUTTING_DOWN",
};
const int mdnsStateEntries = sizeof(mdnsStateNames) / sizeof(mdnsStateNames[0]);

//----------------------------------------
// Locals
//----------------------------------------

static uint32_t mdnsBackoffMsec;
static uint8_t mdnsState;
static uint32_t mdnsTimerMsec;

//----------------------------------------
// Set the next multicast DNS state
//----------------------------------------
void mdnsSetState(uint8_t newState)
{
    // Display the state transition
    if (settings.mdnsDebugState)
    {
        const char * previousState;
        const char * transition;

        // Get the previous state and the transition
        if (newState != mdnsState)
        {
            previousState = mdnsStateNames[mdnsState];
            transition = " --> ";
        }
        else
        {
            previousState = "*";
            transition = "";
        }

        // Display the new state
        if (newState >= mdnsStateEntries)
            systemPrintf("%s%sUnknown mDNS state (%d)\r\n",
                         previousState, transition, newState);
        else
            systemPrintf("%s%s%s\r\n", previousState, transition,
                         mdnsStateNames[newState]);
    }

    // Validate the state
    if (newState >= MDNS_STATE_MAX)
        reportFatalError("Invalid mDNS state");

    // Set the new state
    mdnsState = newState;
}

//----------------------------------------
// Start multicast DNS for Forms.ino only
//----------------------------------------
void mdnsStartMulticastDNS()
{
    if (settings.mdnsEnable == true)
    {
        if (MDNS.begin(&settings.mdnsHostName[0]) == false) // This should make the device findable from 'rtk.local' in a browser
            systemPrintln("Error setting up MDNS responder!");
        else
            MDNS.addService("http", "tcp", settings.httpPort); // Add service to MDNS
    }
}

//----------------------------------------
// Stop multicast DNS for Forms.ino only
//----------------------------------------
void mdnsStopMulticastDNS()
{
    if (settings.mdnsEnable == true)
        MDNS.end();
}

//----------------------------------------
// Stop multicast DNS
//----------------------------------------
void mdnsStop()
{
    if ((mdnsState == MDNS_ADD_SERVICES) || (mdnsState == MDNS_STATE_RUNNING))
        mdnsSetState(MDNS_STATE_SHUTTING_DOWN);
}

//----------------------------------------
// Update the multicast DNS state machine
//----------------------------------------
void mdnsUpdate()
{
    uint32_t currentMsec;

    currentMsec = millis();
    switch (mdnsState)
    {
    case MDNS_STATE_OFF:
        // Wait for the backoff delay to expire
        if ((int32_t)(currentMsec - mdnsTimerMsec) > 0)
        {
            // Set the next polling interval
            mdnsTimerMsec = currentMsec + MDNS_MIN_POLLING_MSEC;

            // Start the local multicast DNS server if requested and the
            // network is available
            if (settings.mdnsEnable && networkUserOpen(NETWORK_USER_MDNS_RESPONDER,
                                                        NETWORK_TYPE_ACTIVE))
            {
                mdnsSetState(MDNS_STATE_WAIT_FOR_NETWORK);
            }
        }
        break;

    // Wait for a network media connection
    case MDNS_STATE_WAIT_FOR_NETWORK:
        // Determine if the network has failed
        if ((!settings.mdnsEnable) || networkIsShuttingDown(NETWORK_USER_MDNS_RESPONDER))
        {
            // mDNS was disabled or the network connection failed
            mdnsTimerMsec = currentMsec + MDNS_MIN_POLLING_MSEC;
            mdnsSetState(MDNS_STATE_OFF);
        }

        // Determine if the network is connected to the media
        else if (networkUserConnected(NETWORK_USER_MDNS_RESPONDER))
        {
            // Initialize the start up delay
            mdnsTimerMsec = currentMsec;
            mdnsSetState(MDNS_STATE_START_DELAY);
        }
        break;

    // Delay after the network connection
    case MDNS_STATE_START_DELAY:
        // Determine if the network has failed
        if ((!settings.mdnsEnable) || networkIsShuttingDown(NETWORK_USER_MDNS_RESPONDER))
        {
            // mDNS was disabled or the network failed
            mdnsTimerMsec = currentMsec + MDNS_MIN_POLLING_MSEC;
            mdnsSetState(MDNS_STATE_OFF);
        }

        // Determine if the startup delay is complete
        else if ((currentMsec - mdnsTimerMsec) >= MDNS_START_DELAY_MSEC)
        {
            // Attempt to start the mDNS responder
            if (MDNS.begin(&settings.mdnsHostName[0]) == false)
            {
                systemPrintln("ERROR: mDNS responder failed to start!");

                // Increase the backoff time
                mdnsBackoffMsec += mdnsBackoffMsec;
                if (!mdnsBackoffMsec)
                    mdnsBackoffMsec = MDNS_START_DELAY_MSEC;
                else if (mdnsBackoffMsec > MDNS_MAX_BACKOFF_MSEC)
                    mdnsBackoffMsec = MDNS_MAX_BACKOFF_MSEC;

                mdnsTimerMsec = currentMsec + mdnsBackoffMsec;
                mdnsSetState(MDNS_STATE_OFF);
            }
            else
            {
                // Successfully started mDNS
                mdnsSetState(MDNS_ADD_SERVICES);
            }
        }
        break;

    // Add a list of services to mDNS
    case MDNS_ADD_SERVICES:
        // Determine if the network has failed
        if ((!settings.mdnsEnable) || networkIsShuttingDown(NETWORK_USER_MDNS_RESPONDER))
        {
            // mDNS was disabled or the network failed
            mdnsTimerMsec = currentMsec + MDNS_MIN_POLLING_MSEC;
            mdnsSetState(MDNS_STATE_SHUTTING_DOWN);
        }

        // Attempt to add the services to the mDNS responder
        else if (MDNS.addService("http", "tcp", settings.httpPort))
        {
            // Successfully added the services
            mdnsBackoffMsec = 0;
            mdnsSetState(MDNS_STATE_RUNNING);
        }
        break;

    // Keep the mDNS responder running while possible
    case MDNS_STATE_RUNNING:
        // Determine if the network has failed
        if ((!settings.mdnsEnable) || networkIsShuttingDown(NETWORK_USER_MDNS_RESPONDER))
        {
            // mDNS was disabled or the network failed
            mdnsTimerMsec = currentMsec + MDNS_MIN_POLLING_MSEC;
            mdnsSetState(MDNS_STATE_SHUTTING_DOWN);
        }
        break;

    // Shutdown the mDNS responder
    case MDNS_STATE_SHUTTING_DOWN:
        MDNS.end();
        mdnsSetState(MDNS_STATE_OFF);
        break;
    }
}

//----------------------------------------
// Verify the multicast DNS tables
//----------------------------------------
void mdnsVerifyTables()
{
    // Verify the table lengths
    if (mdnsStateEntries != MDNS_STATE_MAX)
        reportFatalError("Fix mdnsStateNames table to match MDNS_STATES");
}

#endif // COMPILE_NETWORK
