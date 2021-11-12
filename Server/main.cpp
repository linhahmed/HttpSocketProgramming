#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <string.h>
#include <sstream>
#include <fstream>
#include <vector>
#include <chrono>

#include <pthread.h>
#include <signal.h>

#define SIZE 100000
using namespace std;

struct Request
{
    vector<string> request;
    vector<string> postData;
};

void *handleClients(void *clientptr);
Request parseRequestbuffer(char buffer[]);
string handleResponsemsg(string path, string state, string msg);

int main(int argc, char *argv[])
{
    int result;
    int serverSocket, newSocket;
    sockaddr_in serverAdrr;
    sockaddr_in clientAddr;

    socklen_t addrSize;

    unsigned short serverPort = stoi(argv[1]); //string to int

    /* create a new socket. */
    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    cout << "serverSocket: " << serverSocket << endl;
    if (serverSocket < 0)
    {
        cout << "Error with socket()." << endl;
    }

    /* fill the serverAdrr structure. */
    serverAdrr.sin_addr.s_addr = htonl(INADDR_ANY); //translates a long int from host byte order to network byte order.
    serverAdrr.sin_family = AF_INET;
    serverAdrr.sin_port = htons(serverPort); //translates a short int from host byte order to network byte order.

    /* bind to local addr. */
    result = bind(serverSocket, (struct sockaddr *)&serverAdrr, sizeof(serverAdrr));
    if (result < 0)
    {
        cout << "Error with bind()." << endl;
    }

    /* loop forever. */
    while (true)
    {
        string s = argv[0];
        string Server = s.substr(s.find("/") + 1);

        /* serversocket listens for incoming connections. */
        result = listen(serverSocket, 50);
        if (result == 0)
            cout << Server << " Listening on port " << argv[1] << endl;

        /* accept any coming connections on the server socket. */
        addrSize = sizeof(clientAddr);
        newSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &addrSize);
        if (newSocket < 0)
        {
            cout << "Error Accepting new requests." << endl;
        }
   
        /* inet_ntoa() convets internet host addr in network byte order to a string dotted decimal */
        cout << "Accepting connection from client " << inet_ntoa(clientAddr.sin_addr) << " and the new socket to communicate with it: " << newSocket << endl;

        pthread_t tid;
        /* make a thread worker acts like one client to handle multiple clients. */
        int ret = pthread_create(&tid, NULL, handleClients, &newSocket);
        if (ret < 0)
        {
            cout << "Thread create failed." << endl;
        }
    }
}

void *handleClients(void *clientptr)
{
    int client = *((int *)clientptr);
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now(); 
    /* loop on duration = 5 after nothing recieved. */
    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - begin).count() < 5)
    {
        int result;
        char buffer[SIZE + 1];
        bzero(buffer, sizeof(buffer));

        /* receive any coming requests from the client.*/
        result = recv(client, &buffer, sizeof(buffer), 0);
        if (result < 0)
        {
            cout << "Server recieve Error." << endl;
        }
        else if (result == 0)
        {
            continue;
        }
        else if (result > 0)
        {
            /* restart the timer if sth received. */
            begin = std::chrono::steady_clock::now();
        }
        cout << buffer << endl;
        /* parse the request buffer */
        Request req = parseRequestbuffer(buffer);

        string response;
        string path = "../Server/ServerData" + req.request[1];
        /* then build the response message. */
        if (req.request[0] == "GET")
        {
            ifstream file(path);
            bool exists = file.good();
            file.close();
            if (exists)
            {
                response = handleResponsemsg(path, "200", "OK");
            }
            else
            {
                response = handleResponsemsg("", "404", "Not Found");
            }
        }
        else if (req.request[0] == "POST")
        {
            ofstream file(path);
            for (int i = 0; i < req.postData.size(); i++)
            {
                file << req.postData[i];
                file << "\n";
            }
            file.close();
            response = handleResponsemsg(path, "200", "OK");
        }
        cout << "The Response sent to the Client: " << endl;
        cout << response << endl;
        /* send the response */
        int n = response.length();
        char responsebuffer[n + 1];
        strcpy(responsebuffer, response.c_str());

        cout << "Sending the Response message." << endl;
        result = send(client, &responsebuffer, sizeof(responsebuffer), 0);
        if (result <= 0)
        {
            cout << "Server Send Error." << endl;
            break;
        }
        else if (result > 0)
        {
            begin = std::chrono::steady_clock::now();
        }
        cout << "--------------------------------------------------------------------------" << endl;
    }
    cout << std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - begin).count() << endl;
    cout << "Timeout!! Closing The Client Connection socket." << endl;
    cout << "--------------------------------------------------------------------------" << endl;
    close(client);
    return 0;
}

Request parseRequestbuffer(char buffer[])
{
    string req(buffer);

    istringstream lines(req);
    string line;
    getline(lines, line);
    istringstream ss(line);
    string s;

    //0 get or post
    //1 path
    //2 protocol
    //3 post data
    vector<string> request;
    Request reqData;

    while (getline(ss, s, ' '))
    {
        request.push_back(s);
    }
    reqData.request = request;

    bool isntheader = false;
    vector<string> data;
    int i = 0;
    while (getline(lines, line))
    {
        if (isntheader)
        {
            data.push_back(line);
        }
        if (line.empty())
        {
            if (i >= 1)
            {
                isntheader = true;
            }
            i++;
        }
    }
    reqData.postData = data;
    return reqData;
}

string handleResponsemsg(string path, string state, string msg)
{
    string response = "HTTP/1.1 ";
    response += state + " " + msg + "\n";
    response += "Connection: Open.\n";
    
    string filetype = path.substr(path.find_last_of(".") + 1);
    response += "Content-Type: " + filetype + ".\n";
    response += "\n\n";
    if (!path.empty())
    {
        ifstream file(path);
        string line;
        while (getline(file, line))
        {
            response += line;
            response += "\n";
        }
        file.close();
    }

    return response;
}