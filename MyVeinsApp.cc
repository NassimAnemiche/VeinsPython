////To place in veins/src/veins/modules/application/traci/MyVeinsApp.cc
// Copyright (C) 2016 David Eckhoff <david.eckhoff@fau.de>
//
// Documentation for these modules is at http://veins.car2x.org/
//
// SPDX-License-Identifier: GPL-2.0-or-later
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "veins/modules/application/traci/MyVeinsApp.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
    #define CLOSE_SOCKET(s) closesocket(s)
    #define GET_SOCKET_ERROR() WSAGetLastError()
    #define SOCKET_ERROR_VAL SOCKET_ERROR
    #define INVALID_SOCKET_VAL INVALID_SOCKET
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <errno.h>
    typedef int socket_t;
    #define CLOSE_SOCKET(s) close(s)
    #define GET_SOCKET_ERROR() errno
    #define SOCKET_ERROR_VAL -1
    #define INVALID_SOCKET_VAL -1
#endif

#include <sstream>
#include <cstring>

using namespace veins;

Define_Module(veins::MyVeinsApp);

void MyVeinsApp::initialize(int stage)
{
    DemoBaseApplLayer::initialize(stage);
    if (stage == 0) {
        // Initialize Winsock on Windows
        #ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
            EV << "Error: WSAStartup failed" << endl;
            udpSocket = INVALID_SOCKET_VAL;
            return;
        }
        #endif

        // Initialize UDP socket
        initializeUDPSocket();
        
        // Initialize TraCI components
        try {
            mobility = TraCIMobilityAccess().get(getParentModule());
            traci = mobility->getCommandInterface();
            traciVehicle = mobility->getVehicleCommandInterface();
            
            if (!mobility || !traci || !traciVehicle) {
                EV << "Error: Failed to initialize TraCI components" << endl;
                return;
            }
            
            EV << "\n=== VEHICLE INITIALIZED ===" << endl;
            EV << "ID: [" << getParentModule()->getFullName() << "]" << endl;
            EV << "=========================" << endl;
        } catch (std::exception& e) {
            EV << "Error in initialize: " << e.what() << endl;
        }
    }
}

void MyVeinsApp::initializeUDPSocket()
{
    try {
        // Create UDP socket
        udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
        if (udpSocket == INVALID_SOCKET_VAL) {
            EV << "Error creating UDP socket. Error code: " << GET_SOCKET_ERROR() << endl;
            udpSocket = INVALID_SOCKET_VAL;
            return;
        }

        // Setup destination address (Python listener)
        memset(&pythonAddr, 0, sizeof(pythonAddr));
        pythonAddr.sin_family = AF_INET;
        pythonAddr.sin_port = htons(5005);  // Python listening port

        #ifdef _WIN32
        pythonAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
        #else
        pythonAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        #endif

        EV << "UDP socket initialized for Python communication" << endl;
    } catch (std::exception& e) {
        EV << "Error initializing UDP socket: " << e.what() << endl;
        udpSocket = INVALID_SOCKET_VAL;
    }
}

void MyVeinsApp::sendToPython(const std::string& message)
{
    if (udpSocket == INVALID_SOCKET_VAL) {
        return; // Socket not initialized
    }

    try {
        int result = sendto(udpSocket, message.c_str(), message.length(), 0,
                           (struct sockaddr*)&pythonAddr, sizeof(pythonAddr));
        if (result == SOCKET_ERROR_VAL) {
            #ifdef _WIN32
            EV << "Error sending to Python. Error code: " << WSAGetLastError() << endl;
            #else
            EV << "Error sending to Python: " << strerror(errno) << endl;
            #endif
        }
    } catch (std::exception& e) {
        EV << "Exception sending to Python: " << e.what() << endl;
    }
}

void MyVeinsApp::finish()
{
    DemoBaseApplLayer::finish();
    try {
        // Close UDP socket
        if (udpSocket != INVALID_SOCKET_VAL) {
            CLOSE_SOCKET(udpSocket);
            udpSocket = INVALID_SOCKET_VAL;
        }

        #ifdef _WIN32
        WSACleanup();
        #endif

        EV << "\n=== VEHICLE FINISHED ===" << endl;
        EV << "ID: [" << getParentModule()->getFullName() << "]" << endl;
        EV << "TIME: " << simTime() << endl;
        EV << "=========================" << endl;
    } catch (std::exception& e) {
        EV << "Error in finish: " << e.what() << endl;
    }
}

void MyVeinsApp::onBSM(DemoSafetyMessage* bsm)
{
    if (!bsm) {
        EV << "Error: Received null BSM" << endl;
        return;
    }

    try {
        // Log received beacon message with clear separators
        EV << "\n=== BSM RECEIVED ===" << endl;
        EV << "RECEIVER: [" << getParentModule()->getFullName() << "]" << endl;
        
        // Get sender info safely
        std::string senderName = "Unknown";
        cModule* senderMod = bsm->getSenderModule();
        if (senderMod) {
            senderName = senderMod->getFullName();
            EV << "SENDER: [" << senderName << "]" << endl;
        } else {
            EV << "SENDER: Unknown" << endl;
        }

        // Get position info safely
        Coord senderPos = bsm->getSenderPos();
        EV << "SENDER POS: (" << senderPos.x << ", " << senderPos.y << ")" << endl;
        
        double distance = 0.0;
        std::string receiverName = getParentModule()->getFullName();
        
        // Only calculate distance if we have valid position
        if (mobility && mobility->getPositionAt(simTime()).length() > 0) {
            curPosition = mobility->getPositionAt(simTime());
            distance = curPosition.distance(senderPos);
            EV << "DISTANCE: " << distance << " m" << endl;
        }
        
        EV << "===================" << endl;

        // Send BSM data to Python
        std::ostringstream pythonMsg;
        pythonMsg << "BSM,"
                  << simTime().dbl() << ","
                  << receiverName << ","
                  << senderName << ","
                  << senderPos.x << ","
                  << senderPos.y << ","
                  << curPosition.x << ","
                  << curPosition.y << ","
                  << distance;
        
        sendToPython(pythonMsg.str());
        
    } catch (std::exception& e) {
        EV << "Error in onBSM: " << e.what() << endl;
    }
}

void MyVeinsApp::onWSM(BaseFrame1609_4* wsm)
{
    if (!wsm) {
        EV << "Error: Received null WSM" << endl;
        return;
    }

    try {
        // Log received wireless message with clear separators
        EV << "\n=== WSM RECEIVED ===" << endl;
        EV << "RECEIVER: [" << getParentModule()->getFullName() << "]" << endl;
        EV << "MESSAGE: " << wsm->getName() << endl;
        EV << "TIME: " << simTime() << endl;

        std::string receiverName = getParentModule()->getFullName();
        std::string msgName = wsm->getName();
        
        DemoSafetyMessage* bsm = dynamic_cast<DemoSafetyMessage*>(wsm);
        if (bsm) {
            Coord senderPos = bsm->getSenderPos();
            EV << "SENDER POS: (" << senderPos.x << ", " << senderPos.y << ")" << endl;
            
            double distance = 0.0;
            if (mobility) {
                curPosition = mobility->getPositionAt(simTime());
                distance = curPosition.distance(senderPos);
                EV << "DISTANCE: " << distance << " m" << endl;
            }

            // Send WSM data to Python
            std::ostringstream pythonMsg;
            pythonMsg << "WSM,"
                      << simTime().dbl() << ","
                      << receiverName << ","
                      << msgName << ","
                      << senderPos.x << ","
                      << senderPos.y << ","
                      << curPosition.x << ","
                      << curPosition.y << ","
                      << distance;
            
            sendToPython(pythonMsg.str());
        }
        EV << "===================" << endl;
    } catch (std::exception& e) {
        EV << "Error in onWSM: " << e.what() << endl;
    }
}

void MyVeinsApp::onWSA(DemoServiceAdvertisment* wsa)
{
    if (!wsa) {
        EV << "Error: Received null WSA" << endl;
        return;
    }

    try {
        EV << "\n=== WSA RECEIVED ===" << endl;
        EV << "RECEIVER: [" << getParentModule()->getFullName() << "]" << endl;
        EV << "SERVICE INFO: " << wsa->getServiceDescription() << endl;
        EV << "===================" << endl;

        // Send WSA data to Python
        std::string receiverName = getParentModule()->getFullName();
        std::string serviceDesc = wsa->getServiceDescription();
        
        std::ostringstream pythonMsg;
        pythonMsg << "WSA,"
                  << simTime().dbl() << ","
                  << receiverName << ","
                  << serviceDesc;
        
        sendToPython(pythonMsg.str());
        
    } catch (std::exception& e) {
        EV << "Error in onWSA: " << e.what() << endl;
    }
}

void MyVeinsApp::handleSelfMsg(cMessage* msg)
{
    if (msg) {
        DemoBaseApplLayer::handleSelfMsg(msg);
    }
}

void MyVeinsApp::handlePositionUpdate(cObject* obj)
{
    DemoBaseApplLayer::handlePositionUpdate(obj);
    // Update current position
    if (mobility) {
        curPosition = mobility->getPositionAt(simTime());
    }
}
