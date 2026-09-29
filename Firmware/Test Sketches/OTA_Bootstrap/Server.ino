/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Server.ino

  Connect to the firmware server. Verbatim copy of the RTK_Everywhere System.ino
  certificate and server helpers - see OTA_Bootstrap_Notes.md.
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

// ---- Copied from RTK_Everywhere/System.ino lines 136-183 ----
//----------------------------------------
// Determine the certificate that should be used with the server
//----------------------------------------
const char * getCertFromServer(const char * server)
{
    const char * cert;
    const char * githubUserContent = "raw.githubusercontent.com";

    // BOOTSTRAP CHANGE: GitHub only. The firmware also maps sparkfun.com to AWS_PUBLIC_CERT.
    cert = nullptr;
    if (server)
    {
        // GitHub
        if (strncmp(server, githubUserContent, strlen(githubUserContent)) == 0)
            cert = GITHUB_RAW_PUBLIC_CERT;
    }
    return cert;
}

//----------------------------------------
// Determine the certificate that should be used with the URL
//----------------------------------------
const char * getCertFromUrl(const char * url)
{
    // Locate the server
    String serverString = getServerFromUrl(url);

    // Return the certificate
    return getCertFromServer(serverString.c_str());
}

//----------------------------------------
// Translate the certificate into a certificate name
//----------------------------------------
const char * getCertName(const char * cert)
{
    if (cert == nullptr)
        return "None";
    if (cert == GITHUB_RAW_PUBLIC_CERT)
        return "github";
    return "Unknown";
}

// ---- Copied from RTK_Everywhere/System.ino lines 187-189 ----
//----------------------------------------
// Get an IP address associated with server
//----------------------------------------

// ---- Copied from RTK_Everywhere/System.ino lines 191-239 ----
String getServerIpAddress(const char * server)
{
    struct addrinfo hints, * res, * p;
    char ipstr[INET6_ADDRSTRLEN];
    String ipAddress;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // Support IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(server, NULL, &hints, &res);
    if (status != 0)
        systemPrintf("getaddrinfo error: %d\r\n", status);
    else
    {
        void * addr = nullptr;
        const char * ipVersion;

        for (p = res; p != NULL; p = p->ai_next)
        {
            // Check for an IPv4 address
            if (p->ai_family == AF_INET)
            {
                struct sockaddr_in * ipv4 = (struct sockaddr_in *)p->ai_addr;
                addr = &(ipv4->sin_addr);
                ipVersion = "IPv4";
                break;
            }

            // Check for an IPv6 address
            else if (p->ai_family == AF_INET6)
            {
                struct sockaddr_in6 * ipv6 = (struct sockaddr_in6 *)p->ai_addr;
                addr = &(ipv6->sin6_addr);
                ipVersion = "IPv6";
                break;
            }
        }

        if (addr)
        {
            inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
            ipAddress = String(ipstr);
        }

        freeaddrinfo(res); // Free the memory
    }
    return ipAddress;
}

// ---- Copied from RTK_Everywhere/System.ino lines 242-245 ----
//----------------------------------------
// Returns true if we successfully establish a secure connection to the
// server or false upon failure.
//----------------------------------------

// ---- Copied from RTK_Everywhere/System.ino lines 247-464 ----
bool securelyConnectToServer(const char * url,
                             NetworkClientSecure &client,
                             const char * cert)
{
    if (settings.debugFirmwareUpdate && otaDebugVerbose)
    {
        systemPrintf("url: %p (%s)\r\n", url, url ? url : "");
        systemPrintf("cert: %p\r\n", cert);
    }

    // Verify a certificate is available
    if ((cert == nullptr) || (strlen(cert) == 0))
    {
        systemPrintf("No certificate specified!\r\n");
        return false;
    }

    // Allocate space to assemble the final URL
    size_t length = strlen(url);
    char urlString[length + 15 + 1];

    // Locate the server
    String serverString = getServerFromUrl(url);
    const char * server = serverString.c_str();
    if (settings.debugFirmwareUpdate && otaDebugVerbose)
        systemPrintf("server: %s\r\n", server);

    // Translate the server name into an IP address
    String ipAddressString = getServerIpAddress(server);
    const char * ipAddress = ipAddressString.c_str();
    if (settings.debugFirmwareUpdate && otaDebugVerbose)
        systemPrintf("ipAddress: %s\r\n", ipAddress);

    // Use the certificate for the connection to the server
    if (settings.debugFirmwareUpdate)
        systemPrintf("Using TLS certificate: %s\r\n", getCertName(cert));
    client.setCACert(cert);

    // Bound the connect/read/write and TLS handshake time so a stalled
    // server fails fast instead of blocking on the library defaults
    // (30 s socket / 120 s handshake).
    client.setTimeout(10000);       // milliseconds: TCP connect + socket read/write
    client.setHandshakeTimeout(15); // seconds: TLS handshake

    // Preflight TLS handshake using the expected host name.
    // With CA configured, connect() fails if certificate validation fails.
    if (settings.debugFirmwareUpdate)
        systemPrintf("Checking TLS connection to %s (%s:443)\r\n",
                     server, ipAddress);
    if (!client.connect(server, 443))
    {
        systemPrintln("TLS socket connect failed");
        return false;
    }

    systemPrintf("TLS certificate verified for %s (%s)\r\n", server, ipAddress);

    client.stop();
    return true;
}

//----------------------------------------
// Connect to the remote web server specified by the URL.
// Return the file length if possible.
//----------------------------------------
bool serverConnectUsingUrl(const char * subsystem,
                           const char * chip,
                           const char * url,
                           NetworkClientSecure &secureClient,
                           NetworkClient &unsecureClient,
                           NetworkClient * &stream,
                           HTTPClient &https,
                           void (*addHeaders)(HTTPClient &https),
                           t_http_codes expectedResponseCode,
                           size_t &fileBytes)
{
    int attempt;
    int httpResponseCode;
    bool success;

    do
    {
        success = false;
        stream = nullptr;
        fileBytes = 0;

        // Verify that a URL was specified
        if(settings.debugFirmwareUpdate)
            systemPrintf("URL: %s\r\n", url ? url : "[nullptr]");
        if ((url == nullptr) || (strlen(url) == 0))
        {
            systemPrintln("ERROR: No URL was specified!");
            break;
        }

        // Locate the server for this URL
        String serverString = getServerFromUrl(url);
        if (serverString.length() == 0)
        {
            systemPrintln("ERROR: Failed to find server name in URL string");
            break;
        }
        const char * server = serverString.c_str();

        // Translate the server name into an IP address
        String ipAddressString = getServerIpAddress(server);
        if (ipAddressString.length() == 0)
        {
            systemPrintln("Failed to get the IP address for the server");
            break;
        }
        const char * ipAddress = ipAddressString.c_str();

        // Determine if the certificate is known for this server
        const char * cert = getCertFromUrl(url);
        if(settings.debugFirmwareUpdate)
            systemPrintf("Certificate: %s\r\n", cert ? "available" : "none");

        // Select the network connection depending upon the presents of the certificate
        stream = cert ? &secureClient : &unsecureClient;

        // Bound the connect/read/write and TLS handshake time. HTTPClient's
        // defaults (30 s socket / 120 s handshake) mean a stalled server can
        // block a single attempt for up to two minutes, times 3 retries below.
        stream->setTimeout(10000);   // milliseconds: TCP connect + socket read/write

        // Verify the server using the certificate
        if (cert)
        {
            secureClient.setHandshakeTimeout(15); // seconds: TLS handshake

            // Set the certificate
            secureClient.setCACert(cert);

            // Hand the not-yet-connected client straight to HTTPClient rather than
            // preflighting a connect() here: HTTPClient::begin() unconditionally
            // stops any already-connected socket it's handed (beginInternal() in
            // arduino-esp32's HTTPClient.cpp forces _canReuse = false and calls
            // disconnect() the first time a client is bound), so a separate
            // connect-then-stop pass here would just pay for the TLS handshake
            // twice. The GET retry loop below performs the (single) real connect
            // and already retries 3x on failure.
        }

        // Retry the connection up to 3 times
        const int attemptMax = 3;
        for (int attempt = 1; attempt <= attemptMax; attempt++)
        {
            // Build the request for the web server
            if (https.begin(*stream, url) == false)
            {
                systemPrintln("ERROR: Failed to set the URL for the web server!\r\n");
                break;
            }

            // Tell the HTTP layer to follow redirect links returned by the web server
            https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

            // Add additional request headers
            if (addHeaders)
                addHeaders(https);

            // Send the request to the web server and get the web server's response
            httpResponseCode = https.GET();
            if (httpResponseCode >= 0)
                break;

            // Display the error message
            if (settings.debugFirmwareUpdate)
            {
                systemPrintf("ERROR: HTTP GET failed, attempt %d of 3: %d (%s)\r\n",
                             attempt, httpResponseCode,
                             https.errorToString(httpResponseCode).c_str());
                if (attempt < 3)
                    delay(500);
            }

            // Handle the error
            https.end();
            stream->stop();
        }
        if (attempt > attemptMax)
            break;

        // Display the error
        if ((httpResponseCode == expectedResponseCode) && settings.debugFirmwareUpdate)
            systemPrintf("HTTP Response code: %d (%s)\r\n", httpResponseCode,
                         https.errorToString(httpResponseCode).c_str());

        // Handle the error from the web server
        if (httpResponseCode != expectedResponseCode)
        {
            systemPrintf("Web server response %d: %s\r\n", httpResponseCode,
                         https.errorToString(httpResponseCode).c_str());
            if (httpResponseCode == HTTP_CODE_OK)
                // A 200 here means the server ignored our Range request and is about to send
                // the whole file from byte 0 - streaming that into this offset would corrupt
                // the image, so bail rather than guess.
                systemPrintf("HTTP range request failed, code: %d, fileBytes: %d\r\n",
                             httpResponseCode, https.getSize());
            break;
        }

        // Get the file size
        fileBytes = https.getSize();
        if (settings.debugFirmwareUpdate)
            systemPrintf("File size: %d (0x%08x) bytes\r\n", fileBytes, fileBytes);
        if (fileBytes <= 0)
        {
            systemPrintln("ERROR: Web server did not report a file size.");
            break;
        }

        success = true;
    } while (0);

    return success;
}

// ---- Copied from RTK_Everywhere/System.ino lines 468-504 ----
//----------------------------------------
// Extract the web server from the URL, without any port number
//----------------------------------------
String getServerFromUrl(const char * url)
{
    const char * http = "http://";
    const char * https = "https://";
    size_t index;
    size_t length;
    size_t pos;

    // Locate the third slash
    if (url == nullptr)
        return String("");

    index = 0;
    pos = 0;
    length = strlen(url);
    char server[length + 1];
    if (strncmp(url, https, strlen(https)) == 0)
        pos = strlen(https);
    else if (strncmp(url, http, strlen(http)) == 0)
        pos = strlen(http);
    if (pos)
    {
        strcpy(server, &url[pos]);
        for (index = 0; index < length - pos; index++)
        {
            if (server[index] == 0)
                break;
            if ((server[index] == '/') || (server[index] == ':')) // End of the host name
                break;
        }
    }
    server[index] = 0;
    return String(server);
}
