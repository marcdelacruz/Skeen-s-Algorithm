/*
 *  SkeenProcess.cpp
 *  
 *  Marc DelaCruz
 *  CS 6378-5U1 AOS Project 1 Summer 2010 
 *  Created by Marc on 6/13/10.
 *
 */
#include <algorithm>
#include <fstream>

#include "SkeenProcess.h"
#include "ConnSocket.h"

#define TTL_TIME           30
#define TTL_CHECK_INTERVAL 1

typedef std::map<int, ConnSocket*>::iterator FdMapItr;

void parse(std::string const& rStr, std::vector<std::string>& rCmds, char delim='>')
{
    string::size_type lastPos = rStr.find_first_not_of(delim, 0);
    string::size_type pos     = rStr.find_first_of(delim, lastPos);

    while (string::npos != pos || string::npos != lastPos)
    {
        rCmds.push_back(rStr.substr(lastPos, pos - lastPos));
        lastPos = rStr.find_first_not_of(delim, pos);
        pos = rStr.find_first_of(delim, lastPos);
    }
}

void StdIn::read()
{
     //get the Singleton 
     Singleton<SkeenProcess>::getInstance().read();
}

void SkeenProcess::handleTermSignal(int sigNum, siginfo_t* pSigInfo, void* pContext)
{
    if (sigNum == SIGINT || sigNum == SIGTERM || 
        sigNum == SIGHUP || sigNum == SIGQUIT)
    {
        Singleton<SkeenProcess>::getInstance().mShouldRun = false;
    }
    //ignore SIGPIPE
}//end Router::handleTermSignal

SkeenProcess::SkeenProcess() : mId(-1), mIsSystemReady(false),
                               mQueue(), mUnAckedMsgs(),
                               mShouldSendAck(true),
                               mCurTime(0),
                               mSenderData(),
                               mConfigFile(),
                               mTimeToLiveTimer(),
                               mTtlCallBack(this, &SkeenProcess::processTimeToLive),
                               mNumNodes(0)
{
    mPrompt = "Enter Message: " ;
}

SkeenProcess::~SkeenProcess()
{
}

void SkeenProcess::initialize()
{
    setSignalHandler(SIGINT, SA_SIGINFO|SA_RESTART, &SkeenProcess::handleTermSignal);
    setSignalHandler(SIGTERM, SA_SIGINFO|SA_RESTART, &SkeenProcess::handleTermSignal);
    setSignalHandler(SIGHUP, SA_SIGINFO|SA_RESTART, &SkeenProcess::handleTermSignal);
    setSignalHandler(SIGQUIT, SA_SIGINFO|SA_RESTART, &SkeenProcess::handleTermSignal);
    setSignalHandler(SIGPIPE, SA_SIGINFO|SA_RESTART, &SkeenProcess::handleTermSignal);
    Process::initialize();
    std::make_heap(mQueue.begin(), mQueue.end(), SkeenData::isLessThan);
}

void SkeenProcess::deInitialize()
{
    Process::deInitialize();
    FdMapItr itr = mUnknownNodeList.begin();
    for (; itr != mUnknownNodeList.end(); ++itr)
    {
        ConnSocket* pConnSock = itr->second;
        if (pConnSock != 0)
            delete pConnSock;
    }
    mUnknownNodeList.clear();
    itr = mConnList.begin();
    for (; itr != mConnList.end(); ++itr)
    {
        ConnSocket* pConnSock = itr->second;
        if (pConnSock != 0)
            delete pConnSock;
    }
    mConnList.clear();
    std::map<int, ClientSocket*>::iterator cItr = mClientList.begin();
    for (; cItr != mClientList.end(); ++cItr)
    {
        ClientSocket* pClientSock = cItr->second;
        if (pClientSock != 0)
            delete pClientSock;
    }
    mClientList.clear();
    for (size_t i = 0; i < mRemoveList.size(); ++i)
    {
        ConnSocket* pConn = mRemoveList.at(i);
        delete pConn;
        mRemoveList.erase(mRemoveList.begin() + i);
    }
}

void SkeenProcess::processPostLoopAction()
{
    for (size_t i = 0; i < mRemoveList.size(); ++i)
    {
        ConnSocket* pConn = mRemoveList.at(i);
        delete pConn;
        mRemoveList.erase(mRemoveList.begin() + i);
    }
}

void SkeenProcess::read()
{
    //
    DEBUG("Read...");
    char cBuffer[STD_IN_BUFFER_SIZE];
    bzero(&cBuffer, sizeof(cBuffer));
    cin.getline(cBuffer, sizeof(cBuffer));
    string buffer(cBuffer);
    if (buffer.length() > 0)
    {
        DEBUG("got message: " << buffer );
        std::vector<std::string> tokens;
        parse(buffer, tokens);

        if (tokens.size() > 0)
        {
            std::string& rCmd = tokens.at(0);
            if (rCmd.compare("m-send") == 0 or rCmd.compare("snd") == 0)
            {
                if (tokens.size() > 1)
                {
                    int msgId = ++mCurTime;
                    DEBUG("msgId = " << msgId );
                    for (size_t i = 2; i < tokens.size(); ++i)
                    {
                        int nodeId = atoi(tokens.at(i).c_str());
                        DEBUG("Sending to nodeId=" << nodeId);
                        Message textMsg;
                        textMsg.createTextMsg(msgId, mCurTime, nodeId, tokens.at(1));
                        sendMsg(textMsg, nodeId);
                        //create an entry 
                        mSenderData[msgId].first[nodeId] = false;
                        mSenderData[msgId].second = 0;
                    }
                }
                else
                    STATUS("No message...");
            }
            else if (rCmd.compare("m-receive") == 0 or rCmd.compare("rcv") == 0)
            {
                // get the top then pop
                if (mQueue.size() <= 0)
                {
                    STATUS("No messages in queue...");
                }
                else
                {
                    for (size_t j=0;j<mQueue.size();++j)
                    {
                        DEBUG("\t\tQueue.at(" << j
                               << " ts=" << mQueue.at(j).mTimestamp 
                               << " deliverable=" << mQueue.at(j).mIsDeliverable
                               << ")=" << mQueue.at(j).mMsg.getTextMsg() );
                    }
                    if (mQueue.at(0).mIsDeliverable)
                    {
                        STATUS("\t\tReceived Message (from process " << 
                               mQueue.at(0).mSenderId << "): \"" << 
                               mQueue.at(0).mMsg.getTextMsg() << "\" " );
                        std::stable_sort(mQueue.begin(),mQueue.end());
                        mQueue.erase(mQueue.begin());
                    }
                    else
                        STATUS("No deliverable messages...");
                }
            }
            else if (rCmd.compare("addTs") == 0)
            {
                int incrementAmt = 100;
                if (tokens.size() > 1)
                    incrementAmt = atoi(tokens.at(1).c_str());
                mCurTime += incrementAmt;
            }
            else if (rCmd.compare("skipAck") == 0)
            {
                mShouldSendAck = false;
            }
            else if (rCmd.compare("ack") == 0)
            {
                mShouldSendAck = true;
                std::map< int /*senderId*/, std::queue<int> >::iterator itr = mUnAckedMsgs.begin();
                //for each mUnAckedMsgs
                for (; itr != mUnAckedMsgs.end(); ++itr)
                {
                    while(!itr->second.empty())
                    {
                        int msgId = itr->second.front();
                        Message timestampAck;
                        timestampAck.createTimestampMsg(true, itr->first, 
                                                        msgId, 
                                                        mCurTime++);
                        sendMsg(timestampAck, itr->first);
                        itr->second.pop();
                    }
                }
            }
            else if (rCmd.compare("exit") == 0 or rCmd.compare("quit") == 0 or 
                     rCmd.compare("q") == 0 or rCmd.compare("x") == 0)
            {
                 mShouldRun = false;
            }
            else if (rCmd.compare("debug") == 0)
            {
                if (tokens.size() > 1)
                    mHasDebugsOn = atoi(tokens.at(1).c_str());
                else
                    mHasDebugsOn = !mHasDebugsOn;
            }
            else
                STATUS("Unknown Command: " << rCmd);
        }//end if (tokens.size() > 0)
    }//end buffer.length() > 0
    
    showPrompt();
}//end read()

void SkeenProcess::intializeArgProcessorMap()
{
    addProcessArg(string("-n"), 
                  string("process ID (positive integer)"),
                  &SkeenProcess::handleNumArg, REQUIRED_ARG, INPUT_NEEDED,
                  string("process ID"));
    addProcessArg(string("-d"), 
                  string("enable debugs"),
                  &SkeenProcess::handleDebugArg, OPTIONAL_ARG, NO_INPUT_NEEDED,
                  string("debugs"));
    addProcessArg(string("-c"), 
                  string("config file name"),
                  &SkeenProcess::handleConfigFileArg, REQUIRED_ARG, INPUT_NEEDED,
                  string("config file"));
}

void SkeenProcess::deInitializeArgProcessorMap()
{
}

void SkeenProcess::handleNumArg(std::string argString)
{
    string processNumStr = getArgSubStr(argString);
    mId = atoi(processNumStr.c_str());
    DEBUG("Process Number=" << mId );
    Message::setSrcId(mId);
    if (mConfigFile.length() > 0)
        readConfig();
}

void SkeenProcess::handleDebugArg(string argString)
{
    mHasDebugsOn = true;
    DEBUG("Debugs On...");
}

void SkeenProcess::handleConfigFileArg(string argString)
{
    mConfigFile = getArgSubStr(argString);
    DEBUG("ConfigFile=" << mConfigFile);
    if (mId > 0)
        readConfig();
}

void SkeenProcess::readConfig()
{
    std::ifstream configFile;
    configFile.open(mConfigFile.c_str());
	if (configFile.is_open())
	{
        STATUS("Reading ConfigFile...");
        mNumNodes = 1; //count this process
		while (!configFile.eof() )
        {
            std::string line;
            std::getline (configFile, line);
            std::vector<std::string> tokens;
            parse(line, tokens, ' ');
            if (tokens.size() < 3)
            {
                STATUS("Not enough tokens=" << tokens.size());
                //mShouldRun = false;
                break;
            }
            else
            {
                int num = atoi(tokens.at(0).c_str());
                int port = atoi(tokens.at(2).c_str());
                std::string serverAddr = tokens.at(1);
                STATUS("num=" << num << 
                       " serverAddr=" << serverAddr << 
                       " port=" << port);
                
                if (num != mId and num > 0)
                {
                    mNumNodes++;
                    //save and add to list for sockets to connect
                    if (mId < num)
                    {
                        //num will be the client
                        //add to client list
                        DEBUG("Creating Client sock");
                        ClientSocket* pClient = new ClientSocket(serverAddr.c_str(), port);
                        pClient->setId(num);
                        pClient->setSecondsToLive(TTL_TIME);
                        CallBack<int>* pOpenCallBack  = dynamic_cast<CallBack<int>*>(
                                                  new ObjectCallBack<SkeenProcess, int>(this, 
                                                                &SkeenProcess::processClientOpen));
                        pClient->setOpenCallBack(pOpenCallBack);

                        CallBack<ReadMessage* >* pReadCallBack  = \
                                                dynamic_cast<CallBack<ReadMessage* >*>(
                                                  new ObjectCallBack<SkeenProcess, ReadMessage*>(this, 
                                                                &SkeenProcess::processRead));
                        pClient->setReadCallBack(pReadCallBack);

                        CallBack<int>* pCloseCallBack  = dynamic_cast<CallBack<int>*>(
                                                  new ObjectCallBack<SkeenProcess, int>(this, 
                                                                &SkeenProcess::processClientClose));
                        pClient->setCloseCallBack(pCloseCallBack);
                        mClientList[num] = pClient;
                        addToList(dynamic_cast<Fd*> (pClient));
                    }
                    else
                        DEBUG("Not creating Client sock");
                }
                else if (mId == num)
                {
                    //save the port Id and address
                    mServerSocket.setPortNumber(port);
                    
                    CallBack<int> * pAcceptCallBack = (dynamic_cast<CallBack<int>*> 
                                         (new ObjectCallBack<SkeenProcess, int>(this, 
                                                        &SkeenProcess::processServerAccept)));
                                         
                    mServerSocket.setAcceptCallBack(pAcceptCallBack);
                    mTimeToLiveTimer.start((VoidCallBack*)&mTtlCallBack, 
                                                  TTL_CHECK_INTERVAL, true);
                }
            }
        }
        mServerSocket.setId(-32000);
        addToList(dynamic_cast<Fd*> (&mServerSocket));
        configFile.close();
    }
    else
        DEBUG("config file not open");
    /*
    the file is in the form
    ID address serverport
    1 net24 25571
    2 net34 25572
    3 net25 25573
    */
}//end SkeenProcess::readConfig

void SkeenProcess::processServerAccept(int fdId)
{
    //
    ConnSocket* pConn = new ConnSocket(fdId, sizeof(Message), true, true);
    pConn->setId(-fdId);
    DEBUG("processServerAccept fdId=" << fdId 
          << " ID=" << pConn->getId());
    pConn->setFd(fdId);
    //set selectable
    pConn->setNonBlocking();
    pConn->setSelectable(true);
    DEBUG("processServerAccept FD=" << pConn->getFd() 
          << " ID=" << pConn->getId() << " sel=" 
          << pConn->isSelectable() << " open=" << pConn->isOpen() );
    //set TTL
    pConn->setSecondsToLive(TTL_TIME);
    //need to add open and close call back
    CallBack<ReadMessage* >* pReadCallBack  = \
                            dynamic_cast<CallBack<ReadMessage* >*>(
                              new ObjectCallBack<SkeenProcess, ReadMessage*>(this, 
                                            &SkeenProcess::processUnknownRead));
    pConn->setReadCallBack(pReadCallBack);
    CallBack<int>* pCloseCallBack  = dynamic_cast<CallBack<int>*>(
                              new ObjectCallBack<SkeenProcess, int>(this, 
                                            &SkeenProcess::processUnknownClose));
    pConn->setCloseCallBack(pCloseCallBack);
    mUnknownNodeList[-fdId] = pConn;
    addToList(dynamic_cast<Fd*> (pConn));

}//end SkeenProcess::processServerAccept

void SkeenProcess::addProcessArg(string argPrefix, string helpString,
                           void (SkeenProcess::*pFunc)(string), 
                           bool isRequired,  
                           bool hasNoInputRequired, string paramName)
{
    CallBack<string>* pCallBack (dynamic_cast<CallBack<string>*> 
                         (new ObjectCallBack<SkeenProcess, string>(this, pFunc)));
    Process::addProcessArg(argPrefix, helpString, pCallBack, isRequired, 
                           hasNoInputRequired, paramName);
}//end SkeenProcess::addProcessArg



void SkeenProcess::processClientOpen(int nodeId)
{
    //send node ID
    Message msg;
    msg.createNodeIdMsg(mId);
    mClientList[nodeId]->setNonBlocking();
    mClientList[nodeId]->writeMsg(msg);
    mClientList[nodeId]->setSecondsToLive(TTL_TIME);
    checkSystemReady();
}

void SkeenProcess::processRead(ReadMessage* pMsg)
{
    //process if system ready
    Message msg(pMsg->getMessage());
    int sender = msg.getSrcNode();
    //check for system ready
    if (mIsSystemReady)
    {
        //look for txt msg, proposed timestamp, final timestamps
        int time = std::max(mCurTime, msg.getTimestamp()) + 1;
        bool isValidMsg = true;
        DEBUG("Got Message=" << msg.getMsgType() << " msgId=" << msg.getMsgId());
        switch(msg.getMsgType())
        {
            case TEXT_MSG:
            {
                //put in queue, record initial timestamp
                SkeenData data;
                data.mIsDeliverable = false;
                data.mTimestamp = msg.getTimestamp();
                data.mMsgId = msg.getMsgId();
                data.mMsg = msg;
                data.mSenderId = sender;
                mQueue.push_back(data);
                //std::push_heap(mQueue.begin(), mQueue.end(), SkeenData::isLessThan);
                std::stable_sort(mQueue.begin(), mQueue.end());
                if (mShouldSendAck)
                {
                    //send proposed time stamp
                    //get the max of mCurTime and timestamp in message
                    DEBUG("Sending Proposed Timestamp... sender=" << sender);
                    Message timestampAck;
                    timestampAck.createTimestampMsg(true, sender, 
                                                    data.mMsgId, 
                                                    time);
                    sendMsg(timestampAck, sender);
                }
                else
                {
                    DEBUG("Not sending ACK...");
                    //save message ID in unacked queue
                    mUnAckedMsgs[sender].push(msg.getMsgId());
                    mShouldSendAck = true;
                }
                break;
            }
            case PROPOSED_TIMESTAMP:
            {
                //check if all timestamps have been received
                SenderDataType::iterator sItr = mSenderData.find(msg.getMsgId());
                if (sItr != mSenderData.end())
                {
                    EntryType& rE = sItr->second;
                    std::map<int, bool>::iterator mItr = rE.first.find(sender);
                    if (mItr != rE.first.end())
                    {
                        mItr->second = true;
                        rE.second = std::max(rE.second, msg.getTimestamp());
                        bool allReceived = true;
                        for (mItr = rE.first.begin(); mItr != rE.first.end(); ++mItr)//check if all received
                        {
                            if (!mItr->second)
                            {
                                allReceived = false;
                                break;
                            }
                        }
                        if (allReceived)
                        {
                            DEBUG("PROPOSED TS, All proposed Timestamps rxd...");
                            //send final timestamp
                            for (mItr = rE.first.begin(); mItr != rE.first.end(); ++mItr)
                            {
                                Message finalTimestamp;
                                finalTimestamp.createTimestampMsg(false, 
                                                                  mItr->first, 
                                                                  msg.getMsgId(), 
                                                                  rE.second);
                                sendMsg(finalTimestamp, mItr->first);
                            }
                            //delete this entry
                            mSenderData.erase(sItr);
                        }
                    }//end if sender found in receiver list
                    else
                        DEBUG("Receiver "<< sender << " NOT FOUND");
                }//end if message ID found
                else
                    DEBUG("MessageId "<< msg.getMsgId() << " NOT FOUND");
                break;
            }//end PROPOSED_TIMESTAMP
            case FINAL_TIMESTAMP:
            {
                //find in queue , set final timestamp
                //re-sort
                for (size_t i = 0; i < mQueue.size(); ++i)
                {
                    SkeenData& rData = mQueue.at(i);
                    if (msg.getMsgId() == rData.mMsg.getMsgId() and 
                        sender == rData.mSenderId)
                    {
                        rData.mIsDeliverable = true;
                        rData.mTimestamp = msg.getTimestamp();
                        std::stable_sort(mQueue.begin(), mQueue.end());
                        break;
                    }
                }
                break;
            }
            default:
                isValidMsg = false;
                DEBUG("UNSUPPORTED msgType=" << msg.getMsgType());
                break;
        }//end switch
        if (isValidMsg)
            mCurTime = time;
    }//end is system ready
    else
    {
        std::map<int, ClientSocket*>::iterator cItr = mClientList.find(sender); 
        if (cItr != mClientList.end())
        {
            if (msg.getMsgType() == NODE_ID  and 
                msg.getNodeId() <= mNumNodes)
            {
                DEBUG("Verified Connection to Process " << msg.getNodeId() );
                cItr->second->setSecondsToLive(-1);
                checkSystemReady();
            }//end getting NODE ID message
            else
            {
                DEBUG(__func__ << " msgType=" << msg.getMsgType() << " nodeId=" << msg.getNodeId() << " sender=" << sender);
                cItr->second->close();
            }
        }
        else
            DEBUG("Can't find sender=" << sender << " in list" );
    }
}//end SkeenProcess::processRead(ReadMessage* pMsg)

void SkeenProcess::processTimeToLive()
{
    FdMapItr itr = mUnknownNodeList.begin();
    for (; itr != mUnknownNodeList.end(); ++itr )
    {
        ConnSocket* pSock = itr->second;
        int curTimeToLive = pSock->getSecondsToLive();

        if (curTimeToLive == 0)
        {
            //close this unidentified socket
            DEBUG("CLOSING UNIDENTIFIED Socket ID=" << pSock->getId());
            pSock->close();
        }
        else if (curTimeToLive > 0)
        {
            pSock->setSecondsToLive((curTimeToLive-TTL_CHECK_INTERVAL));
        }
        //else if -1, ignore
    }
    //TODO: also check clients
    std::map<int, ClientSocket*>::iterator cItr = mClientList.begin();
    for (; cItr != mClientList.end(); ++cItr)
    {
        ClientSocket* pClientSock = cItr->second;
        if (pClientSock != 0)
        {
            int curTimeToLive = pClientSock->getSecondsToLive();

            if (curTimeToLive == 0)
            {
                //close this unidentified socket
                DEBUG("CLOSING CLIENT Socket ID=" << pClientSock->getId());
                pClientSock->close();
            }
            else if (curTimeToLive > 0)
            {
                pClientSock->setSecondsToLive((curTimeToLive-TTL_CHECK_INTERVAL));
            }
        }
    }
}//end SkeenProcess::processTimeToLive

void SkeenProcess::processUnknownRead(ReadMessage* pMsg)
{
    int senderNode = pMsg->getNodeId();
    Message msg(pMsg->getMessage());
    //find the node
    FdMapItr itr = mUnknownNodeList.find(senderNode);
    ConnSocket* pConn = (itr != mUnknownNodeList.end() ? itr->second : 0);
    bool shouldRemove = true;
    if (pConn)
    {
        //if the message is the node ID, don't remove
        FdMapItr nItr = mConnList.find(msg.getNodeId());
        
        if (msg.getMsgType() == NODE_ID and nItr == mConnList.end() and 
            msg.getNodeId() <= mNumNodes)
        {
            shouldRemove = false;
            STATUS("Accepting Connection to Process " << msg.getNodeId() );
            //move to known nodes
            mConnList[msg.getNodeId()] = pConn;
            pConn->setId(msg.getNodeId());
            Message nodeIdMsg;
            nodeIdMsg.createNodeIdMsg(mId);
            pConn->setNonBlocking();
            pConn->writeMsg(nodeIdMsg);
            mUnknownNodeList.erase(itr);
            //reset callbacks
            CallBack<ReadMessage* >* pReadCallBack  = \
                                    dynamic_cast<CallBack<ReadMessage* >*>(
                                      new ObjectCallBack<SkeenProcess, ReadMessage*>(this, 
                                                    &SkeenProcess::processRead));
            pConn->setReadCallBack(pReadCallBack);
            CallBack<int>* pCloseCallBack  = dynamic_cast<CallBack<int>*>(
                                      new ObjectCallBack<SkeenProcess, int>(this, 
                                                    &SkeenProcess::processKnownClose));
            pConn->setCloseCallBack(pCloseCallBack);
            checkSystemReady();
        }//end getting NODE ID message
        else
            shouldRemove = true;

    }//end if socket exists
    if (shouldRemove and pConn)
    {
        pConn->close();
    }
}

void SkeenProcess::processUnknownClose(int nodeId)
{
    //remove
    std::map<int, ConnSocket*>::iterator itr = mUnknownNodeList.find(nodeId);
    ConnSocket* pConn = (itr != mUnknownNodeList.end() ? itr->second : 0);
    if (pConn)
    {
        pConn->setSelectable(false);
        removeFromList(pConn->getId());
        mUnknownNodeList.erase(itr);
        mRemoveList.push_back(pConn);
    }
}

void SkeenProcess::processClientClose(int nodeId)
{
    STATUS("Client socket ID=" << nodeId << " closed, exiting");
    if (mIsSystemReady)
        mShouldRun = false; 
}

void SkeenProcess::processKnownClose(int nodeId)
{
    //remove
    STATUS("Conn socket ID=" << nodeId << " closed, exiting");
    mShouldRun = false; 
    FdMapItr itr = mConnList.find(nodeId);
    ConnSocket* pConn = (itr != mConnList.end() ? itr->second : 0);
    if (pConn)
    {
        pConn->setSelectable(false);
        removeFromList(pConn->getId());
        mConnList.erase(itr);
        mRemoveList.push_back(pConn);
    }
}

void SkeenProcess::checkSystemReady()
{
    int numOpenClients = 0;
    std::map<int, ClientSocket*>::iterator cItr = mClientList.begin();
    for (; cItr != mClientList.end(); ++cItr)
    {
        if (cItr->second->isOpen() and cItr->second->getSecondsToLive() == -1)
            numOpenClients++;
    }
    DEBUG("FdListSize=" << mConnList.size() << " openClients=" << numOpenClients << " numNodes=" << mNumNodes);
    if ((size_t)mNumNodes == (mConnList.size() + numOpenClients + 1))
    {
        STATUS("...ALL CONNECTIONS MADE...");
        //stop accepting from server
        mServerSocket.setShouldAcceptConnections(false);
        //close all unknown
        for (FdMapItr mItr = mUnknownNodeList.begin() ; 
             mItr != mUnknownNodeList.end() ; mItr++)
        {
            ConnSocket* pConnSocket = mItr->second;
            pConnSocket->close();
        }
        mUnknownNodeList.clear();
        mIsSystemReady = true;
        showPrompt();
        //mCmdLine.setNonBlocking();
        addToList(&mCmdLine);
    }
}

void SkeenProcess::sendMsg(Message& rMsg, int destNode)
{
    std::map<int, ClientSocket*>::iterator cItr = mClientList.find(destNode);
    if (cItr != mClientList.end())
    {
        cItr->second->writeMsg(rMsg);
    }
    else
    {
        std::map<int, ConnSocket*>::iterator itr = mConnList.find(destNode);
        if (itr != mConnList.end())
            itr->second->writeMsg(rMsg);
        else
            DEBUG("Can't route message, can't find destination FD");
    }
}

int main (int argc, char* argv[])
{
    Singleton<SkeenProcess>::getInstance().run(argc, argv);
}

