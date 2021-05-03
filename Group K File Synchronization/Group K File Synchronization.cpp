// Group K File Synchronization.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>
#include <string> 
#include <stdexcept>
#include <stdio.h>
#include <cstdio>
#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>

using namespace std;
using namespace boost;

//namespace fs = std::filesystem;

using boost::asio::ip::tcp;

const unsigned short PORT = 3333;

//writes message to a socket synchronously
void writeToSocket(asio::ip::tcp::socket& socket, std::string message)
{
    std::size_t bytesWritten = 0;

    while (bytesWritten != message.length())
    {
        bytesWritten += socket.write_some(asio::buffer(message.c_str() + bytesWritten, message.length() - bytesWritten));
    }
}

std::vector<std::string> findIP()
{
    //create a vector to store the IPs in
    std::vector<std::string> ipList;
    //Open a command prompt and use arp -a to retrieve a list of IP addresses and hteir interfaces
    FILE* pipe = _popen("arp -a", "r");
    //If the pupe was not created print the error
    if (!pipe)
    {
        fprintf(stderr, "Unable to open IP Scan pipe");
    }
    //if the pipe was created continue
    else
    {
        //instantiate buffer and string
        char buffer[128];
        std::string bufferString = "";

        //until the pipe contains no more data add the data to the string using the buffer
        while (!feof(pipe))
        {
            if (fgets(buffer, 128, pipe) != NULL)
            {
                bufferString += buffer;
            }
        }
        //Close the pipe
        _pclose(pipe);

        std::cout << bufferString;
        //create int for tracking index in string
        int startIndex = 0;
        //iterate through bufferString
        for (int i = 0; i < bufferString.size(); i++)
        {
            //if the current value is an integer or a '.' continue
            if (isdigit(bufferString[i]) || bufferString[i] == '.')
            {
                continue;
            }
            //if a space is found and the discovered string is of sufficient length to be an IP, add it to the vector
            else if (bufferString[i] == ' ' && i - startIndex > 3)
            {
                ipList.push_back(bufferString.substr(startIndex + 1, i - startIndex - 1));
            }
            //if an x occurs, pop the last IP out, an x only appears in arp -a
            //after a reference to this computer, this will make it impossible to
            //connect to a different instance of this app on this machine
            else if (bufferString[i] == 'x')
            {
                ipList.pop_back();
            }
            //set start index to the current index
            startIndex = i;
        }

    }


    return ipList;

}

//Searches the local subnet/vLAN for IP addresses and attempts to connect to each one
//Creates a socket and adds it to the socketVector for each connection
void findAndConnect(std::vector<asio::ip::tcp::socket> &socketVector, asio::io_context &ios)
{
    //create a vector to store the IPs in
    std::vector<std::string> ipList = findIP();

    for (int i = 0; i < ipList.size(); i++)
    {
        try
        {
            // Create an endpoint designating target server application.
            // this will be used by the sockets as an access point for communication using TCP
            std::cout << "Attempting to Connect to " << ipList[i] << "...";
            asio::ip::tcp::endpoint endpoint(asio::ip::address::from_string(ipList[i]), PORT);

            //Create a socket attached to theendpoint
            asio::ip::tcp::socket socket(ios, endpoint.protocol());

            //Attempt to connect the socket to the endpoint
            socket.connect(endpoint);

            std::cout << "Connected" << std::endl;
        }
        catch (system::system_error& e) 
        {
            std::cout << "Connection Failed " << e.what() << std::endl;
            //std::cout << "Error occured! Error code = " << e.code()
            //    << ". Message: " << e.what() << std::endl;;

            //return e.code().value();
        }
    }


}

std::vector<asio::ip::tcp::endpoint> findEndpoints()
{
    std::vector<asio::ip::tcp::endpoint> endpoints;

    //get the IPs
    std::vector<std::string> ipList = findIP();

    for (int i = 0; i < ipList.size(); i++)
    {
        asio::ip::tcp::endpoint ep(asio::ip::address::from_string(ipList[i]), PORT);
        endpoints.push_back(ep);
    }

    return endpoints;
}

class tcpConnection : public boost::enable_shared_from_this<tcpConnection>
{
public:
    typedef boost::shared_ptr<tcpConnection> pointer;

    static pointer create(asio::io_context& ios)
    {
        return pointer(new tcpConnection(ios));
    }

    asio::ip::tcp::socket& socket()
    {
        return socket_;
    }

    void startConnect()                                                            // When connected, send your files to the server
    {
        #pragma region Get file directory (User input method -- to be replaced by checks)
        std::cout << "Directory of the file: ";
        std::string fileDirectory = "";
        std::cin >> fileDirectory;
        fileDirectoryChar = fileDirectory.c_str(); // GET FILE from given directory
        #pragma endregion

        std::ifstream fileData(fileDirectoryChar);                                          // Open the file data 

        if(fileData.eof() == false) {                                                       // If you don't have an empty file...
            while (1) {                                                                     // While true...
                fileData.read(sendBuff.data(), BUFFERSIZE);                                 // Read in the data from the file into the buffer
                std::streamsize stmSize = ((fileData) ? BUFFERSIZE : fileData.gcount());    // Set the stream size to the appropriate size based on how much data still needs to be sent
                sendBuff[stmSize] = 0;                                                      // Set the buffer point at the streamsize to 0
                std::cout << "Send Buffer items: " << sendBuff.data() << std::endl;
                // A boost asio function that writes the given data to the given socket asynchronously
                boost::asio::async_write(socket_, boost::asio::buffer(sendBuff, BUFFERSIZE), boost::bind(&tcpConnection::writeHandler, shared_from_this(),
                    boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
                if (!fileData)                                                              // Once you reach the last portion of the file
                    break;                                                                  // Break out of the loop
            }
        }
        fileData.close();                                                                   // Close the file after use
    }

    void startAccept()                                                      // When connected, recieve files from the server
    {
        std::ifstream fileData(fileDirectoryChar);                                          // Open the file data 

        if (fileData.eof() == false) {                                                       // If you don't have an empty file...
            while (1) {                                                                     // While true...
                fileData.read(sendBuff.data(), BUFFERSIZE);                                 // Read in the data from the file into the buffer
                std::streamsize stmSize = ((fileData) ? BUFFERSIZE : fileData.gcount());    // Set the stream size to the appropriate size based on how much data still needs to be sent
                sendBuff[stmSize] = 0;                                                      // Set the buffer point at the streamsize to 0
                std::cout << "Recieved Buffer items: " << sendBuff.data() << std::endl;
                // A boost asio function that writes the given data to the given socket asynchronously
                boost::asio::async_write(socket_, boost::asio::buffer(sendBuff, BUFFERSIZE), boost::bind(&tcpConnection::recieveHandler, shared_from_this(),
                    boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
                if (!fileData)                                                              // Once you reach the last portion of the file
                    break;                                                                  // Break out of the loop
            }
        }
        fileData.close();                                                                   // Close the file after use
    }


private:
    tcpConnection(asio::io_context& ios) : socket_(ios) {};

    void writeHandler(const system::error_code& err, std::size_t bytes_transferred) {           // The method that gets called after all data has been written
        if (!err) {                                                                             // If there are no errors with writing the data to the socket...
            std::cout << "File is sent to the socket!" << std::endl;                            // Send a test-friendly message to indicate that the file has been sent
        }
        else {                                                                                  // Otherwise, if ther IS an error...
            std::cout << "Something went wrong in writeHandler! Error: " << err << std::endl;   // Print out the error and where it is being caused
        }
    }

    void recieveHandler(const system::error_code& err, std::size_t bytes_transferred) {         // The method that gets called after all data has been recieved from the socket
        if (!err) {                                                                             // If there is no error in reciving data from the socket....
            std::cout << "File is recieved!" << std::endl;                                      // Send a test-friendly message to indicate that the file has been recieved
        }
        else {                                                                                  // Otherwise, if ther IS an error...
            std::cout << "Something went wrong in recieveHandler! Error: " << err << std::endl; // Print out the error and where it is being caused
        }
    }

    asio::ip::tcp::socket socket_;
    const int BUFFERSIZE = 1024;
    boost::array<char, 1025> sendBuff;
    boost::array<char, 1025> recvBuff;
    const char* fileDirectoryChar;

};





//Class responsible for accepting connections
class tcpServer
{
public:
    //constructor which creates an acceptor and starts accepting connections
    tcpServer(asio::io_context& ios) : acceptor(ios, tcp::endpoint(tcp::v4(), PORT))
    {
        this->ios = &ios;
        startAccept();
        startConnect();
    }
private:
    //the first portion of acception a connection, starts accepting asynchronously
    void startAccept()
    {
        //create a connection object to represent the connection
        tcpConnection::pointer newConnection = tcpConnection::create(*ios);

        //accept the connection asynchronously and call handleAccept() when done
        acceptor.async_accept(newConnection->socket(), boost::bind(&tcpServer::handleAccept, this, newConnection, asio::placeholders::error));
    }

    //Handles the accpetance of a connection
    void handleAccept(tcpConnection::pointer newConnection, const system::error_code& error)
    {
        //if no error has occurred start the connection
        if (!error)
        {
            newConnection->startAccept();
        }

        //Go back to searching for more connections
        startAccept();
    }

    //Handles the success of a connection
    void handleConnect(tcpConnection::pointer newConnection, const system::error_code& error)
    {
        //if no error has occurred start the connection
        if (!error)
        {
            newConnection->startConnect();
        }

        //Go back to searching for more connections
        startConnect();
    }

    //the first portion of sending a connection asynchronously
    void startConnect()
    {
        //create a connection object to represent the connection
        tcpConnection::pointer newConnection = tcpConnection::create(*ios);

        //send connections asynchronously
        asio::async_connect(newConnection->socket(), findEndpoints(), boost::bind(&tcpServer::handleConnect, this, newConnection, asio::placeholders::error));
    }

    //the acceptor used to accept connections
    tcp::acceptor acceptor;
    asio::io_context* ios;
};

//obsolete file path locator code here
//function that gains the file path based off the filename using cmd prompt coding
//string gainLoc(string fileName)
//{
//    char buffer[256];
//    string result = "";
//
//    //code below is cmd code for finding the filepath of name of file within any directory in comp(/s) without
//    //without any excess information (/b)
//    string nameloc = "dir " + fileName + "* /s /b";
//
//    FILE* pipeName = _popen(nameloc.c_str(), "r");
//    if (!pipeName) {
//        return "popen in fileLocation failed";
//    }
//    while (!feof(pipeName)) {
//
//        // use buffer to read and add to result
//        if (fgets(buffer, 256, pipeName) != NULL)
//            result += buffer;
//    }
//
//    _pclose(pipeName);
//    //make sure that folder File in client and server is removed, also make sure filename is unique for code
//
//    //end result should simply have filepath to the file input
//    return result;
//
//}


//updated code to get the file location; still needs to be edited so location can be outputted
//obsolete code 
//string fileLocation(string fileName)
//{
//    char buffer[256];
//    string fileDirectory = fileName;
//    const char* fileDirectoryChar = fileDirectory.c_str();
//    string result = "Directory is: ";
//
//    FILE* fi;
//    errno_t err;
//
//    if ((err = fopen_s(&fi, fileDirectoryChar, "rb")) != 0) {
//        cout << "File cannot be opened." << endl;
//    }
//    else {
//        while (!feof(fi)) {
//            if (fgets(buffer, 128, fi) == NULL) {
//                cout << "Gained file location" << endl;
//                break;
//            }
//            fputs(buffer, stdout);
//        }
//    }
//    fclose(fi);
//
//    return result;
//
//}

//code which takes the output of the commandline after the call of file comparision 
//string fileCompare(std::string command) {
//    char buffer[128];
//    string result = "";
//
//    // Open pipe to access the file in the system
//    FILE* pipe = _popen(command.c_str(), "r");
//    if (!pipe) {
//        return "popen failed!";
//    }
//
//    // read till end of process of file compare
//    while (!feof(pipe)) {
//
//        // use buffer to read and add to result
//        if (fgets(buffer, 128, pipe) != NULL)
//            result += buffer;
//    }
//
//    _pclose(pipe);
//    //the ending result can be long if files are not the same
//    return result;
//}

string fileComparison(FILE* file1, FILE* file2)
{
    int differ = 0;
    int max_size = 65536;
    char* fi1 = (char*)calloc(1, max_size + 1);
    char* fi2 = (char*)calloc(1, max_size + 1);
    size_t s1, s2;

    //checking if files are the same in do while loop
    do {
        s1 = fread(fi1,1,max_size, file1);
        s2 = fread(fi2,1,max_size, file2);
        //below checks if there is any pointer differences at one point or if 
        // file2 yields a different length than file1
        if (s1 != s2 || memcmp(fi1,fi2,s1))
        {
            differ = 1;
        }

    } while (!feof(file1) || !feof(file2));

    free(fi1);
    free(fi2);

    //if statements here may need to read from the async write/read to alter code
    //based on file comparison results
    if (differ ==1)
    {
        //placeholder string statement for solving merge conflicts
        return "Files were found to be different despite same name.";
    }
    else {
        //placeholder string statement for ignoring identical files
        return "Files were found to be the same. ";
    }
    //have a case or way of seeing if there is no file present with same name so it 
    //can just write in; maybe new function
    return "";
}

int main()
{
    //Creates the I/O context to be used as a basis for communicating I/O commands to the OS
    asio::io_context ios;

    //create vector to hold the active sockets
    std::vector<asio::ip::tcp::socket> socketVector;

    tcpServer server = tcpServer(ios);
    findAndConnect(socketVector, ios);
    
    //code below is commandline necessary for attempting to file file name
    //in cmd string placeholder has been added for the file location
    // can also use HELP FC in commandline to get instructions of how /b and etc works
    //example code line for below: fc "c:\Program Files\placeholder_file\filename1.txt" "c:\Program Files\placeholder_file\filename1.txt" \B
    //another example for folder comparison: fc "c:\Program Files\holderfolder\folder1\*" "c:\Program Files\holderfolder\folder2\*" 
    //example above would check if folder2 has all exact files in folder1, from title names to content in files
    
    //code below was paired with fc file comparison code
    //string cmd = fileCompare("fc \"Files\\file1.txt\" \"Files\\file2.txt\" /B");
    //cout << "the output of command: " << cmd << endl;
    ////string required to be found to see if files are the same 
    //string check = "FC: no differences encountered";
    //if (cmd.find(check) != std::string::npos)
    //{
    //    //files resulted in no differences
    //    cout << "Files are the same.";
    //}
    //else
    //{
    //    //files are found to be different
    //    cout << "Files are not the same.";
    //}

    ios.run();
    std::system("pause");
}
