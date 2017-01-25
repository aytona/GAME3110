#include <stdio.h>
#include <string.h>
#include "RakPeerInterface.h"
#include "MessageIdentifiers.h"
#include "BitStream.h"
#include "RakNetTypes.h"  // MessageID
#include "Gets.h"
#include "RakSleep.h"
#include <conio.h>

#define MAX_CLIENTS 10
#define SERVER_PORT 60000

RakNet::RakPeerInterface *peer = RakNet::RakPeerInterface::GetInstance();

enum UserInputResult {
    UIR_BREAK,
    UIR_CONTINUE,
    UIR_POS,
    UIR_COUNT,
};

enum GameMessages {
    ID_GAME_MESSAGE_CHAT = ID_USER_PACKET_ENUM,
    ID_GAME_MESSAGE_POS
};

struct SPos {
    float x, y, z;
};

unsigned int CheckForCommands(char* message) {
    if (strcmp(message, "quit") == 0) {
        puts("Quitting.");
        return UIR_BREAK;
    }

    if (strcmp(message, "pingip") == 0) {
        char userInput[30];
        printf("Enter IP: ");
        Gets(message, sizeof(message));
        printf("Enter port: ");
        Gets(userInput, sizeof(userInput));
        if (userInput[0] == 0)
            strcpy(userInput, "1234");
        peer->Ping(message, atoi(userInput), false);

        return UIR_CONTINUE;
    }

    if (strcmp(message, "getconnectionlist") == 0) {
        RakNet::SystemAddress systems[10];
        unsigned short numConnections = 10;
        peer->GetConnectionList((RakNet::SystemAddress*) &systems, &numConnections);
        for (int i = 0; i < numConnections; i++) {
            printf("%i. %s\n", i + 1, systems[i].ToString(true));
        }
        return UIR_CONTINUE;
    }

    if (strcmp(message, "ban") == 0) {
        printf("Enter IP to ban.  You can use * as a wildcard\n");
        Gets(message, sizeof(message));
        peer->AddToBanList(message);
        printf("IP %s added to ban list.\n", message);
        return UIR_CONTINUE;
    }

    if (strcmp(message, "pos") == 0) {
        return UIR_POS;
    }

    return UIR_COUNT;
}

int main(void) {
    char str[512];

    
    bool isServer; 
    RakNet::Packet *packet;
    char message[2048];
    char nameInput[2048];

    printf("My IP addresses:\n");
    for (unsigned int i = 0; i < peer->GetNumberOfAddresses(); i++) {
        RakNet::SystemAddress sa = peer->GetInternalID(RakNet::UNASSIGNED_SYSTEM_ADDRESS, i);
        printf("%i. %s (LAN=%i)\n", i + 1, sa.ToString(false), sa.IsLANAddress());
    }

    printf("\nEnter Username: ");
    Gets(nameInput, sizeof(nameInput));
    int i = 0;
    while (i < sizeof(nameInput) / sizeof(nameInput[0])) {
        if (nameInput[i] == '\0' || nameInput[i] == ' ') {
            nameInput[i] = ':';
            ++i;
            nameInput[i] = ' ';
            ++i;
            nameInput[i] = '\0';
            break;
        }
        ++i;
    }

    printf("\n(C) or (S)erver?\n");
    Gets(str, sizeof(str));

    if ((str[0] == 'c') || (str[0] == 'C')) {
        RakNet::SocketDescriptor sd;
        peer->Startup(1, &sd, 1);
        isServer = false;
    } else {
        RakNet::SocketDescriptor sd(SERVER_PORT, 0);
        peer->Startup(MAX_CLIENTS, &sd, 1);
        isServer = true;
    }

    if (isServer) {
        printf("Starting the server.\n");
        // We need to let the server accept incoming connections from the clients
        peer->SetMaximumIncomingConnections(MAX_CLIENTS);
    } else {
        printf("Enter server IP or hit enter for 127.0.0.1\n");
        Gets(str, sizeof(str));
        if (str[0] == 0) {
            strcpy(str, "127.0.0.1");
        }
        printf("Starting the client.\n");
        peer->Connect(str, SERVER_PORT, 0, 0);

    }

    while (1) {
        RakSleep(30);
        char broadcastMsg[2048];
        broadcastMsg[0] = ID_GAME_MESSAGE_CHAT;

        if (_kbhit()) {
            Gets(message, sizeof(message));

            unsigned int result = CheckForCommands(message);
            switch (result) {
            case UIR_BREAK:
            {
                break;
            }
            case UIR_CONTINUE:
            {
                continue;
            }
            case UIR_COUNT:
            {

            }break;
            case UIR_POS:
            {
                RakNet::BitStream bs;
                bs.Write((unsigned char)ID_GAME_MESSAGE_POS);
                bs.Write(7.0f);
                peer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
                /*SPos pos;
                pos.x = 1.0f;
                pos.y = 2.0f;
                pos.z = 6.0f;
                broadcastMsg[0] = ID_GAME_MESSAGE_POS;
                memcpy(&broadcastMsg[1], &pos, sizeof(SPos));*/
            }break;
            default:
            {
                broadcastMsg[0] = ID_GAME_MESSAGE_CHAT;
                broadcastMsg[1] = '\0';
                strncat(broadcastMsg, message, sizeof(message));
            }
            }
            strncat(broadcastMsg, nameInput, sizeof(broadcastMsg));
            strncat(broadcastMsg, message, sizeof(broadcastMsg));
            peer->Send(broadcastMsg, (const int)strlen(broadcastMsg) + 1, HIGH_PRIORITY, RELIABLE_ORDERED, 0, RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
        }
        
        for (packet = peer->Receive(); packet; peer->DeallocatePacket(packet), packet = peer->Receive()) {
            switch (packet->data[0]) {
            case ID_REMOTE_DISCONNECTION_NOTIFICATION:
                printf("Another client has disconnected.\n");
                break;
            case ID_REMOTE_CONNECTION_LOST:
                printf("Another client has lost the connection.\n");
                break;
            case ID_REMOTE_NEW_INCOMING_CONNECTION:
                printf("Another client has connected.\n");
                break;
            case ID_CONNECTION_REQUEST_ACCEPTED:
            {
                printf("Our connection request has been accepted.\n");

                // Use a BitStream to write a custom user message
                // Bitstreams are easier to use than sending casted structures, and handle endian swapping automatically
                RakNet::BitStream bsOut;
                bsOut.Write((RakNet::MessageID)ID_GAME_MESSAGE_CHAT);
                bsOut.Write("Hello world");
                peer->Send(&bsOut, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);
            }
            break;
            case ID_NEW_INCOMING_CONNECTION:
                printf("A connection is incoming.\n");
                break;
            case ID_NO_FREE_INCOMING_CONNECTIONS:
                printf("The server is full.\n");
                break;
            case ID_DISCONNECTION_NOTIFICATION:
                if (isServer) {
                    printf("A client has disconnected.\n");
                } else {
                    printf("We have been disconnected.\n");
                }
                break;
            case ID_CONNECTION_LOST:
                if (isServer) {
                    printf("A client lost the connection.\n");
                } else {
                    printf("Connection lost.\n");
                }
                break;

            case ID_GAME_MESSAGE_CHAT:
            {
                RakNet::RakString rs;
                RakNet::BitStream bsIn(packet->data, packet->length, false);
                bsIn.IgnoreBytes(sizeof(RakNet::MessageID));
                bsIn.Read(rs);
                printf("%s\n", rs.C_String());
            }
            break;

            case ID_GAME_MESSAGE_POS:
            {
                RakNet::BitStream bs(packet->data, packet->length, false);
                bs.IgnoreBytes(sizeof(RakNet::MessageID));
                float xPos;
                bs.Read(xPos);
                printf("Position X: %f\n", xPos);
                /*unsigned char *temp = packet->data;
                ++temp;
                SPos *pos = (SPos*)temp;
                pos->x;*/
            }break;

            default:
                printf("%s\n", packet->data);
                sprintf(broadcastMsg, "%s", packet->data);
                peer->Send(broadcastMsg, (const int)strlen(broadcastMsg) + 1, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, true);
                break;
            }
        }
    }


    RakNet::RakPeerInterface::DestroyInstance(peer);

    return 0;
}

