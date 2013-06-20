/*
 *  MessageDefs.h
 *  Marc DelaCruz
 *  CS 6378-5U1 AOS Project 1 Summer 2010 
 */

//define message structures
#ifndef MESSAGE_DEFS_H
#define MESSAGE_DEFS_H
 
#include <string>
#include <cstdlib>

using namespace std;

struct Header
{
    unsigned char msgType;
    unsigned char destNodeId;
    int timestamp;
    int msgId;
    int senderId;
    short payloadSize;
};

#define HDR_SIZE           sizeof(Header)
#define MAX_PAYLOAD_SIZE   2048

enum MsgType
{
    MIN_MSG_TYPE         = 64,
    NODE_ID              = MIN_MSG_TYPE, //denotes the client's unique ID is
    RESTART              = 65, //a node died so restart the system
    EXIT                 = 66,
    NODE_READY           = 67,
    KEEP_ALIVE           = 68,
    SYSTEM_READY         = 68,
    FINAL_TIMESTAMP      = 69,
    PROPOSED_TIMESTAMP   = 70,
    TEXT_MSG             = 71,
    MAX_MSG_TYPE_EX,
    MAX_MSG_TYPE         = MAX_MSG_TYPE_EX-1
};

struct NodeIdTag
{
    unsigned char nodeId;
};

struct MessageTag
{
    Header hdr;
    union
    {
        NodeIdTag nodeIdMsg;
        char textMsg[MAX_PAYLOAD_SIZE];
    };
};

#define MAX_MSG_SIZE sizeof(Message)


#endif // MESSAGE_DEFS_H


