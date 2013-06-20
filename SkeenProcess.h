/*
 *  SkeenProcess.h
 *  Marc DelaCruz
 *  CS 6378-5U1 AOS Project 1 Summer 2010 
 *
 *  Created by Marc on 6/13/10.
 *
 */

#ifndef _SKEEN_PROCESS_H_
#define _SKEEN_PROCESS_H_

#include <queue>
#include <vector>
#include "Process.h"
#include "ClientSocket.h"
#include "ServerSocket.h"

struct SkeenData
{
    bool mIsDeliverable;
    int mTimestamp;
    int mMsgId;
    int mSenderId;
    Message mMsg;
    bool operator< (SkeenData const& rSkeenData) const
    {
        return mTimestamp < rSkeenData.mTimestamp;
        /*((mIsDeliverable and mTimestamp < rSkeenData.mTimestamp) or
                (!mIsDeliverable and !rSkeenData.mIsDeliverable and
                 mTimestamp < rSkeenData.mTimestamp));*/
    }
    static bool isLessThan(SkeenData const& rSkeenData1, SkeenData const& rSkeenData2)
    {
        return rSkeenData1 < rSkeenData2;
    }
};

#define STD_IN_BUFFER_SIZE  1024
class StdIn : public Fd
{
    public:
    StdIn() : Fd(STD_IN_BUFFER_SIZE, true, true)
    {
        setId(0);
    }
    virtual ~StdIn() { }

    //define the interface for the Process class
    void open() { mFd = 0; }
    void read();
    void write() {}
    void close() {} //don't close
    bool hasBytesToWrite() const { return false; }
};

class SkeenProcess : public Process, public Singleton<SkeenProcess>
{
    public:
        static void handleTermSignal(int sigNum, siginfo_t* pSigInfo, void* pContext);
        virtual void initialize();
        virtual void deInitialize();
        virtual void processPostLoopAction();
        virtual void read();
    protected:
        friend class Singleton<SkeenProcess>;
        SkeenProcess();
        virtual ~SkeenProcess();
        virtual void intializeArgProcessorMap();
        virtual void deInitializeArgProcessorMap();

        void handleNumArg(std::string argString);
        void handleDebugArg(string argString);
        void handleConfigFileArg(string argString);
        void readConfig();
        void processServerAccept(int fdId);
        void addProcessArg(string argPrefix, string helpString,
                           void (SkeenProcess::*pFunc)(string), 
                           bool isRequired,  
                           bool hasNoInputRequired, string paramName);
        void processClientOpen(int nodeId);
        void processRead(ReadMessage* pMsg);

        void processTimeToLive();
        void processUnknownRead(ReadMessage* pMsg);
        void processUnknownClose(int nodeId);
        void processClientClose(int nodeId);
        void processKnownClose(int nodeId);
        void checkSystemReady();
        void sendMsg(Message& rMsg, int destNode);
        
        int mId;
        bool mIsSystemReady;
        std::vector<SkeenData> mQueue;
        std::map< int /*senderId*/, std::queue<int> > mUnAckedMsgs;
        bool mShouldSendAck; //proposed timestamp
        int mCurTime;
        typedef std::pair < std::map< int /*node Id*/, bool /*rxd*/ >  , int /*maxTime*/ > EntryType;
        typedef std::map<int /*msgId*/, EntryType > SenderDataType;
        SenderDataType mSenderData;
        StdIn mCmdLine;
        std::string mConfigFile;
        ServerSocket mServerSocket;
        Timer mTimeToLiveTimer;
        VoidObjectCallBack<SkeenProcess> mTtlCallBack;
        std::map<int, ConnSocket*> mUnknownNodeList;
        std::vector<ConnSocket*> mRemoveList;
        std::map<int, ConnSocket*> mConnList;
        std::map<int, ClientSocket*> mClientList; // never clean up until the end
        int mNumNodes;
};//end SkeenProcess

#endif// _SKEEN_PROCESS_H_


