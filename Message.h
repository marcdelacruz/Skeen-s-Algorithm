/*
 *  Message.h
 *  Marc DelaCruz
 *  CS 6378-5U1 AOS Project 1 Summer 2010 
 */

#ifndef MESSAGE_H
#define MESSAGE_H

#include "MessageDefs.h"

class Message
{
public:
    explicit Message(MessageTag& rMsg);
    explicit Message(MsgType msgType, short destNodeId);

    Message();
    ~Message();
    
    void createNodeIdMsg(short nodeId);
    
    short getNodeId() const;

    void createTextMsg(int msgId, int timestamp, 
                       short destNode, std::string const& rTextMsg);
    void createTimestampMsg(bool isProposedTimestamp, short destNodeId, 
                            int msgId, int timestamp);

    MsgType getMsgType() const;
    short getDestNodeId() const;

    inline int getTimestamp() const { return mMessage.hdr.timestamp; }
    inline int getMsgId() const { return mMessage.hdr.msgId; }

    std::string getTextMsg();
    short getSize() const;

    const MessageTag& getMsg();
    
    short getPayloadSize() const;

    void setMsgType(MsgType msgType);
    void setDestNodeId(short nodeId);
    
    void setSrcNode(int nodeId);
    int getSrcNode();

    static void setSrcId(int srcId);

protected:
    MessageTag mMessage;
    static int smSrcId;
};

#endif //MESSAGE_H
