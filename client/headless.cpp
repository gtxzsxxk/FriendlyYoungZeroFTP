#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#define FTP_DIRECT_CMD_4(cmd)    else if (command.substr(0, 4) == #cmd) { \
        if (!txCtrlCommand(command)) { \
            std::cerr << "Error sending "#cmd" command." << std::endl; \
        } else if (!rxCtrlResponse(response)) { \
            std::cerr << "Error receiving response for USER command." << std::endl; \
        } \
    }

class FTPClient {
public:
    FTPClient(const std::string &ip, int port);

    ~FTPClient();

    void run();

private:
    enum TransferMode {
        PORT, PASV
    };

    int sockCtrl;
    std::string svrIp;
    int svrPort;
    std::string cltIp;
    TransferMode transMode;

    /* PORT 模式 */
    int sockDataPort;
    int portDataNumber;

    // For passive mode
    std::string pasvDataIP;
    int pasvDataPort;

    bool connectToServer();

    bool routeCommand(const std::string &command);

    bool txCtrlCommand(const std::string &cmd);

    bool rxCtrlResponse(std::string &response);

    bool netReadLine(int socket, std::string &line);

    bool enterPassiveMode();

    bool enterPortMode();

    int dataChannelInit();

    bool retrieveFile(const std::string &filename);

    bool storeFile(const std::string &filename);

    bool listDirectory(const std::string &pathname);
};

FTPClient::FTPClient(const std::string &ip, int port)
        : sockCtrl(-1),
          svrIp(ip),
          svrPort(port),
          transMode(PORT),
          sockDataPort(-1),
          portDataNumber(0) {}

FTPClient::~FTPClient() {
    if (sockCtrl != -1) {
        close(sockCtrl);
    }
    if (sockDataPort != -1) {
        close(sockDataPort);
    }
}

bool FTPClient::connectToServer() {
    sockCtrl = socket(AF_INET, SOCK_STREAM, 0);
    if (sockCtrl < 0) {
        perror("socket");
        return false;
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(svrPort);

    if (inet_pton(AF_INET, svrIp.c_str(), &serverAddr.sin_addr) <= 0) {
        perror("inet_pton");
        return false;
    }

    if (connect(sockCtrl, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) {
        perror("connect");
        return false;
    }

    /* 获取本机IP地址 */
    struct sockaddr_in localAddr;
    socklen_t addrLen = sizeof(localAddr);
    if (getsockname(sockCtrl, (struct sockaddr *) &localAddr, &addrLen) == -1) {
        perror("getsockname");
        return false;
    }
    char clientIp[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &localAddr.sin_addr, clientIp, sizeof(clientIp)) == NULL) {
        perror("inet_ntop");
        return false;
    }
    cltIp = clientIp;

    /* 获取欢迎信息 */
    std::string response;
    if (!rxCtrlResponse(response)) {
        return false;
    }
    return true;
}

bool FTPClient::txCtrlCommand(const std::string &cmd) {
    std::string command = cmd + "\r\n";
    ssize_t sent = send(sockCtrl, command.c_str(), command.length(), 0);
    if (sent < 0) {
        perror("send");
        return false;
    }
    return true;
}

bool FTPClient::netReadLine(int socket, std::string &line) {
    line.clear();
    char ch;
    while (true) {
        ssize_t n = recv(socket, &ch, 1, 0);
        if (n > 0) {
            if (ch == '\r') {
                /* 分包、防止粘包 */
                n = recv(socket, &ch, 1, MSG_PEEK);
                if (n > 0 && ch == '\n') {
                    /* 吃掉\n */
                    recv(socket, &ch, 1, 0);
                }
                break;
            } else if (ch == '\n') {
                break;
            } else {
                line += ch;
            }
        } else if (n == 0) {
            /* 长度为0，应当关闭连接 */
            return false;
        } else {
            perror("recv");
            return false;
        }
    }
    return true;
}

bool FTPClient::rxCtrlResponse(std::string &response) {
    response.clear();
    std::string &line = response;
    bool multiLine = false;
    std::string code;

    while (true) {
        if (!netReadLine(sockCtrl, line)) {
            return false;
        }
        std::cout << line << std::endl;
        if (line.length() >= 3 && isdigit(line[0]) && isdigit(line[1]) && isdigit(line[2])) {
            if (!multiLine) {
                if (line[3] == '-') {
                    multiLine = true;
                    code = line.substr(0, 3);
                } else if (line[3] == ' ') {
                    break;
                }
            } else {
                if (line.substr(0, 3) == code && line[3] == ' ') {
                    break;
                }
            }
        }
    }
    return true;
}

bool FTPClient::routeCommand(const std::string &command) {
    std::string response;
    if (command == "QUIT") {
        if (!txCtrlCommand("QUIT")) {
            std::cerr << "Error sending QUIT command." << std::endl;
        } else if (!rxCtrlResponse(response)) {
            std::cerr << "Error receiving response for QUIT command." << std::endl;
        }
        return false;  // Exit the command loop
    } else if (command.substr(0, 4) == "USER") {
        if (!txCtrlCommand(command)) {
            std::cerr << "Error sending USER command." << std::endl;
        } else if (!rxCtrlResponse(response)) {
            std::cerr << "Error receiving response for USER command." << std::endl;
        }
    } else if (command.substr(0, 4) == "PASS") {
        if (!txCtrlCommand(command)) {
            std::cerr << "Error sending PASS command." << std::endl;
        } else if (!rxCtrlResponse(response)) {
            std::cerr << "Error receiving response for PASS command." << std::endl;
        }
    } else if (command.substr(0, 4) == "SYST") {
        if (!txCtrlCommand("SYST")) {
            std::cerr << "Error sending SYST command." << std::endl;
        } else if (!rxCtrlResponse(response)) {
            std::cerr << "Error receiving response for SYST command." << std::endl;
        }
    } else if (command.substr(0, 4) == "TYPE") {
        if (!txCtrlCommand(command)) {
            std::cerr << "Error sending TYPE command." << std::endl;
        } else if (!rxCtrlResponse(response)) {
            std::cerr << "Error receiving response for TYPE command." << std::endl;
        }
    } else if (command.substr(0, 4) == "LIST") {
        std::string pathname = command.length() > 5 ? command.substr(5) : "";
        if (!listDirectory(pathname)) {
            std::cerr << "Error executing LIST command." << std::endl;
        }
    } else if (command.substr(0, 4) == "RETR") {
        std::string filename = command.length() > 5 ? command.substr(5) : "";
        if (!retrieveFile(filename)) {
            std::cerr << "Error executing RETR command." << std::endl;
        }
    } else if (command.substr(0, 4) == "STOR") {
        std::string filename = command.length() > 5 ? command.substr(5) : "";
        if (!storeFile(filename)) {
            std::cerr << "Error executing STOR command." << std::endl;
        }
    } else if (command.substr(0, 3) == "CWD") {
        if (!txCtrlCommand(command)) {
            std::cerr << "Error sending CWD command." << std::endl;
        } else if (!rxCtrlResponse(response)) {
            std::cerr << "Error receiving response for CWD command." << std::endl;
        }
    } else if (command.substr(0, 3) == "PWD") {
        if (!txCtrlCommand("PWD")) {
            std::cerr << "Error sending PWD command." << std::endl;
        } else if (!rxCtrlResponse(response)) {
            std::cerr << "Error receiving response for PWD command." << std::endl;
        }
    } else if (command.substr(0, 3) == "MKD") {
        if (!txCtrlCommand(command)) {
            std::cerr << "Error sending MKD command." << std::endl;
        } else if (!rxCtrlResponse(response)) {
            std::cerr << "Error receiving response for MKD command." << std::endl;
        }
    } else if (command.substr(0, 3) == "RMD") {
        if (!txCtrlCommand(command)) {
            std::cerr << "Error sending RMD command." << std::endl;
        } else if (!rxCtrlResponse(response)) {
            std::cerr << "Error receiving response for RMD command." << std::endl;
        }
    } else if (command == "PASV") {
        if (!enterPassiveMode()) {
            std::cerr << "Error entering passive mode." << std::endl;
        }
    } else if (command.substr(0, 4) == "PORT") {
        if (!enterPortMode()) {
            std::cerr << "Error entering active mode." << std::endl;
        }
    } else {
        std::cerr << "Unknown command." << std::endl;
    }
    return true;
}

bool FTPClient::enterPassiveMode() {
    if (!txCtrlCommand("PASV")) {
        std::cerr << "Error sending PASV command." << std::endl;
        return false;
    }
    std::string response;
    if (!rxCtrlResponse(response)) {
        std::cerr << "Error receiving response for PASV command." << std::endl;
        return false;
    }

    // Parse the response to extract IP and port
    size_t start = response.find('(');
    size_t end = response.find(')');
    std::cerr << response << std::endl;
    if (start == std::string::npos || end == std::string::npos) {
        std::cerr << "Invalid PASV response format." << std::endl;
        return false;
    }

    std::string pasvData = response.substr(start + 1, end - start - 1);
    std::replace(pasvData.begin(), pasvData.end(), ',', ' ');
    std::stringstream ss(pasvData);
    int h1, h2, h3, h4, p1, p2;
    ss >> h1 >> h2 >> h3 >> h4 >> p1 >> p2;

    if (ss.fail()) {
        std::cerr << "Invalid PASV response data." << std::endl;
        return false;
    }

    pasvDataIP = std::to_string(h1) + "." + std::to_string(h2) + "." + std::to_string(h3) + "." + std::to_string(h4);
    pasvDataPort = p1 * 256 + p2;
    transMode = PASV;
    return true;
}

bool FTPClient::enterPortMode() {
    // Close any existing data listen socket
    if (sockDataPort != -1) {
        close(sockDataPort);
        sockDataPort = -1;
    }

    // Create a new socket for listening
    sockDataPort = socket(AF_INET, SOCK_STREAM, 0);
    if (sockDataPort < 0) {
        perror("socket");
        return false;
    }

    // Bind to any available port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Accept connections on any IP address
    addr.sin_port = 0;                         // Bind to any available port

    if (bind(sockDataPort, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sockDataPort);
        sockDataPort = -1;
        return false;
    }

    // Get the port number assigned
    socklen_t addrLen = sizeof(addr);
    if (getsockname(sockDataPort, (struct sockaddr *) &addr, &addrLen) == -1) {
        perror("getsockname");
        close(sockDataPort);
        sockDataPort = -1;
        return false;
    }
    portDataNumber = ntohs(addr.sin_port);

    if (listen(sockDataPort, 1) < 0) {
        perror("listen");
        close(sockDataPort);
        sockDataPort = -1;
        return false;
    }

    // Prepare the PORT command
    std::string ip = cltIp;
    std::replace(ip.begin(), ip.end(), '.', ',');
    int p1 = portDataNumber / 256;
    int p2 = portDataNumber % 256;
    std::stringstream portCmd;
    portCmd << "PORT " << ip << "," << p1 << "," << p2;

    if (!txCtrlCommand(portCmd.str())) {
        std::cerr << "Error sending PORT command." << std::endl;
        close(sockDataPort);
        sockDataPort = -1;
        return false;
    }

    std::string response;
    if (!rxCtrlResponse(response)) {
        std::cerr << "Error receiving response for PORT command." << std::endl;
        close(sockDataPort);
        sockDataPort = -1;
        return false;
    }

    if (response.substr(0, 3) != "200") {
        std::cerr << "PORT command failed: " << response << std::endl;
        close(sockDataPort);
        sockDataPort = -1;
        return false;
    }

    transMode = PORT;
    return true;
}

int FTPClient::dataChannelInit() {
    if (transMode == PASV) {
        // Passive Mode
        int dataSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (dataSocket < 0) {
            perror("socket");
            return -1;
        }

        struct sockaddr_in dataAddr;
        memset(&dataAddr, 0, sizeof(dataAddr));
        dataAddr.sin_family = AF_INET;
        dataAddr.sin_port = htons(pasvDataPort);

        if (inet_pton(AF_INET, pasvDataIP.c_str(), &dataAddr.sin_addr) <= 0) {
            perror("inet_pton");
            close(dataSocket);
            return -1;
        }

        if (connect(dataSocket, (struct sockaddr *) &dataAddr, sizeof(dataAddr)) < 0) {
            perror("connect");
            close(dataSocket);
            return -1;
        }

        return dataSocket;
    } else {
        // Active Mode
        struct sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);
        int dataSocket = accept(sockDataPort, (struct sockaddr *) &clientAddr, &addrLen);
        if (dataSocket < 0) {
            perror("accept");
            return -1;
        }

        return dataSocket;
    }
}

bool FTPClient::listDirectory(const std::string &pathname) {
    if (transMode == PASV) {
        if (pasvDataIP.empty()) {
            if (!enterPassiveMode()) {
                return false;
            }
        }
    } else {
        if (sockDataPort == -1) {
            if (!enterPortMode()) {
                return false;
            }
        }
    }

    std::string cmd = "LIST";
    if (!pathname.empty()) {
        cmd += " " + pathname;
    }

    if (!txCtrlCommand(cmd)) {
        std::cerr << "Error sending LIST command." << std::endl;
        return false;
    }

    std::string response;
    if (!rxCtrlResponse(response)) {
        std::cerr << "Error receiving response for LIST command." << std::endl;
        return false;
    }

    if (response.substr(0, 3) != "150" && response.substr(0, 3) != "125") {
        std::cerr << "LIST command failed: " << response << std::endl;
        return false;
    }

    int dataSocket = dataChannelInit();
    if (dataSocket < 0) {
        return false;
    }

    // Receive data
    char buffer[4096];
    ssize_t bytesReceived;
    while ((bytesReceived = recv(dataSocket, buffer, sizeof(buffer), 0)) > 0) {
        std::cout.write(buffer, bytesReceived);
    }

    if (bytesReceived < 0) {
        perror("recv");
        close(dataSocket);
        return false;
    }

    close(dataSocket);

    // Receive final response
    if (!rxCtrlResponse(response)) {
        std::cerr << "Error receiving final response for LIST command." << std::endl;
        return false;
    }

    return true;
}

bool FTPClient::retrieveFile(const std::string &filename) {
    if (filename.empty()) {
        std::cerr << "Filename is required for RETR command." << std::endl;
        return false;
    }

    std::string response;

    std::string cmd = "RETR " + filename;
    if (!txCtrlCommand(cmd)) {
        std::cerr << "Error sending RETR command." << std::endl;
        return false;
    }

    if (!rxCtrlResponse(response)) {
        std::cerr << "Error receiving response for RETR command." << std::endl;
        return false;
    }

    if (response.substr(0, 3) != "150" && response.substr(0, 3) != "125") {
        std::cerr << "RETR command failed: " << response << std::endl;
        return false;
    }

    int dataSocket = dataChannelInit();
    if (dataSocket < 0) {
        return false;
    }

    // Open file for writing
    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile) {
        std::cerr << "Cannot open file for writing: " << filename << std::endl;
        close(dataSocket);
        return false;
    }

    char buffer[4096];
    ssize_t bytesReceived;
    while ((bytesReceived = recv(dataSocket, buffer, sizeof(buffer), 0)) > 0) {
        outFile.write(buffer, bytesReceived);
    }

    if (bytesReceived < 0) {
        perror("recv");
        outFile.close();
        close(dataSocket);
        return false;
    }

    outFile.close();
    close(dataSocket);

    // Receive final response
    if (!rxCtrlResponse(response)) {
        std::cerr << "Error receiving final response for RETR command." << std::endl;
        return false;
    }

    return true;
}

bool FTPClient::storeFile(const std::string &filename) {
    if (filename.empty()) {
        std::cerr << "Filename is required for STOR command." << std::endl;
        return false;
    }

    // Set binary mode
    if (!txCtrlCommand("TYPE I")) {
        std::cerr << "Error setting binary mode." << std::endl;
        return false;
    }
    std::string response;
    if (!rxCtrlResponse(response)) {
        std::cerr << "Error receiving response for TYPE command." << std::endl;
        return false;
    }

    if (transMode == PASV) {
        if (pasvDataIP.empty()) {
            if (!enterPassiveMode()) {
                return false;
            }
        }
    } else {
        if (sockDataPort == -1) {
            if (!enterPortMode()) {
                return false;
            }
        }
    }

    std::string cmd = "STOR " + filename;
    if (!txCtrlCommand(cmd)) {
        std::cerr << "Error sending STOR command." << std::endl;
        return false;
    }

    if (!rxCtrlResponse(response)) {
        std::cerr << "Error receiving response for STOR command." << std::endl;
        return false;
    }

    if (response.substr(0, 3) != "150" && response.substr(0, 3) != "125") {
        std::cerr << "STOR command failed: " << response << std::endl;
        return false;
    }

    int dataSocket = dataChannelInit();
    if (dataSocket < 0) {
        return false;
    }

    // Open file for reading
    std::ifstream inFile(filename, std::ios::binary);
    if (!inFile) {
        std::cerr << "Cannot open file for reading: " << filename << std::endl;
        close(dataSocket);
        return false;
    }

    char buffer[4096];
    while (inFile.read(buffer, sizeof(buffer))) {
        ssize_t bytesSent = send(dataSocket, buffer, inFile.gcount(), 0);
        if (bytesSent < 0) {
            perror("send");
            inFile.close();
            close(dataSocket);
            return false;
        }
    }

    // Send any remaining bytes
    if (inFile.gcount() > 0) {
        ssize_t bytesSent = send(dataSocket, buffer, inFile.gcount(), 0);
        if (bytesSent < 0) {
            perror("send");
            inFile.close();
            close(dataSocket);
            return false;
        }
    }

    inFile.close();
    close(dataSocket);

    // Receive final response
    if (!rxCtrlResponse(response)) {
        std::cerr << "Error receiving final response for STOR command." << std::endl;
        return false;
    }

    return true;
}

void FTPClient::run() {
    if (!connectToServer()) {
        std::cerr << "Failed to connect to FTP server." << std::endl;
        return;
    }

    transMode = PASV;

//  std::cout << "FTP Client is On!" << std::endl;

    // Command loop
    std::string command;
    while (true) {
//    std::cout << "> ";
        if (!std::getline(std::cin, command)) {
            break;
        }

        if (command.empty()) {
            continue;
        }

        if (!routeCommand(command)) {
            break;
        }
    }
}

int main(int argc, char *argv[]) {
    std::string serverIP = "127.0.0.1";
    int serverPort = 21;
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-ip") == 0 && i + 1 < argc) {
            serverIP = argv[i + 1];
            i++;
        } else if (std::strcmp(argv[i], "-port") == 0 && i + 1 < argc) {
            serverPort = std::atoi(argv[i + 1]);
            i++;
        } else {
            std::cerr << "Unknown argument: " << argv[i] << std::endl;
            return 1;
        }
    }


    FTPClient ftpClient(serverIP, serverPort);
    ftpClient.run();

    return 0;
}
