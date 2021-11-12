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

#include <pthread.h>
#include <signal.h>

#define SIZE 100000
using namespace std;

struct Request
{
    vector<string> request;
    vector<string> headers;
};

vector<Request> handleCommands(string path);
string handleMsgReq(Request req);
vector<string> parseResponsebuffer(char buffer[]);

int main(int argc, char *argv[])
{
    int clientSocket;
    unsigned short serverPort = stoi(argv[2]);

    sockaddr_in serverAddr;

    /* build the server Addr structure */
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = *((unsigned long *)gethostbyname(argv[1])->h_addr_list[0]);
    serverAddr.sin_port = htons(serverPort);

    /* building the requests by parsing the commands first */
    vector<Request> reqs = handleCommands("../Client/ClientData/Commands.txt");
    int result;

    /* create a new socket. */
    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    cout << "clientSocket: " << clientSocket << endl;

    /* Open a connection with the server. */
    result = connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (result < 0)
    {
        cout << "Connection Error." << endl;
        return 0;
    }
    cout << "Connection estabilished with server " << inet_ntoa(serverAddr.sin_addr) << endl;
    cout << "------------------------------------------------------------------------------" << endl;

    /* loop on the requests. */
    for (int i = 0; i < reqs.size(); i++)
    {
        /* build the response message. */
        string msgreq = handleMsgReq(reqs[i]);
        cout << "The Request message from client to the server. " << endl;
        cout << msgreq << endl;

        int n = msgreq.length();
        char msgbuffer[n + 1];
        strcpy(msgbuffer, msgreq.c_str());

        /* Sending the Request message to the Server.*/
        cout << "Sending the Request message to the Server. " << endl;
        result = send(clientSocket, &msgbuffer, sizeof(msgbuffer), 0);
        if (result < 0)
        {
            cout << "client Send Error." << endl;
            return 0;
        }

        char buffer[SIZE + 1];
        bzero(buffer, sizeof(buffer));

        /* Receiving the response from the server. */
        result = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (result < 0)
        {
            cout << "client recieve Error." << endl;
            return 0;
        }
        cout << "The Response recieved from the Server: " << endl;
        cout << buffer << endl;

        /* parsing the respose message to get the data recieved in case of get. */
        vector<string> data = parseResponsebuffer(buffer);

        if (data[0] != "404" && reqs[i].request[0] == "GET")
        {
            string filetype = reqs[i].request[1].substr(reqs[i].request[1].find_last_of(".") + 1);
            string path = "../Client/ClientData" + reqs[i].request[1];
            ofstream file(path);
            for (int i = 1; i < data.size(); i++)
            {
                file << data[i];
                file << "\n";
            }
            file.close();
        }
        cout << "------------------------------------------------------------------------------" << endl;
    }
    /* Close the connection if the requests have finished */
    close(clientSocket);
}

vector<Request> handleCommands(string path)
{
    ifstream file(path);
    string line;
    vector<Request> reqs;
    while (getline(file, line))
    {
        istringstream ss(line);
        vector<string> command;
        string s;
        while (getline(ss, s, ' '))
        {
            command.push_back(s);
        }

        Request req;
        vector<string> request;
        if (command.size() > 2)
        {

            //method    0
            if (command[0] == "client_get")
            {
                request.push_back("GET");
            }
            else if (command[0] == "client_post")
            {
                request.push_back("POST");
            }
            //URI   1
            request.push_back(command[1]);
            //hostname  2
            request.push_back(command[2]);
            //port      3
            if (command.size() > 3)
            {
                request.push_back(command[3]);
            }
            else
            {
                request.push_back("80");
            }
        }
        req.request = request;
        vector<string> header;
        header.push_back("Host: " + command[2] + " (" + command[3] + ").");
        header.push_back("Connection: open.");
        req.headers = header;
        reqs.push_back(req);
    }
    return reqs;
}

string handleMsgReq(Request req)
{
    string msg = "";
    vector<string> request = req.request;
    vector<string> header = req.headers;
    if (request[0] == "GET")
    {
        msg += "GET ";
        msg += request[1]; //URI
        msg += " HTTP/1.1\n";
        for (int i = 0; i < header.size(); i++)
        {
            msg += header[i];
            msg += "\n";
        }
        msg += "\n\n";
    }
    else if (request[0] == "POST")
    {
        msg += "POST ";
        msg += request[1]; //URI
        msg += " HTTP/1.1\n";
        for (int i = 0; i < header.size(); i++)
        {
            msg += header[i];
            msg += "\n";
        }
        msg += "\n\n";
        string path = "../Client/ClientData" + request[1];
        ifstream file(path);
        string line;
        int size = 0;
        while (getline(file, line))
        {
            size += line.length();
            msg += line;
            msg += "\n";
        }
    }
    return msg;
}

vector<string> parseResponsebuffer(char buffer[])
{
    string req(buffer);
    istringstream lines(req);
    string line;
    vector<string> data;
    getline(lines, line);
    istringstream ss(line);
    string s;
    while (getline(ss, s, ' '))
    {
        if (isdigit(s[0]) != 0)
        {
            data.push_back(s);
        }
    }
    bool isntheader = false;
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
    return data;
}