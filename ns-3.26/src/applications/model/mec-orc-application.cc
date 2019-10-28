/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2007 University of Washington
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "mec-orc-application.h"
#include <sstream>
#include <fstream>

namespace ns3 {

    NS_LOG_COMPONENT_DEFINE ("MecOrcApplication");

    NS_OBJECT_ENSURE_REGISTERED (MecOrcApplication);

    TypeId
    MecOrcApplication::GetTypeId (void)
    {
        static TypeId tid = TypeId ("ns3::MecOrcApplication")
                .SetParent<Application> ()
                .SetGroupName("Applications")
                .AddConstructor<MecOrcApplication> ()
                .AddAttribute ("MaxPackets",
                               "The maximum number of packets the application will send",
                               UintegerValue (100),
                               MakeUintegerAccessor (&MecOrcApplication::m_count),
                               MakeUintegerChecker<uint32_t> ())
                .AddAttribute ("PacketSize", "Size of echo data in outbound packets",
                               UintegerValue (100),
                               MakeUintegerAccessor (&MecOrcApplication::m_packetSize),
                               MakeUintegerChecker<uint32_t> ())
                .AddAttribute ("AllServers", "Container of all server addresses",
                               StringValue("x"),
                               MakeStringAccessor (&MecOrcApplication::m_serverString),
                               MakeStringChecker())
                .AddAttribute ("AllUes", "Container of all UE addresses",
                               StringValue("x"),
                               MakeStringAccessor (&MecOrcApplication::m_ueString),
                               MakeStringChecker())
                .AddAttribute ("UePort", "Port that UEs use",
                               UintegerValue (),
                               MakeUintegerAccessor (&MecOrcApplication::m_uePort),
                               MakeUintegerChecker<uint32_t> ())
                .AddTraceSource ("Tx", "A new packet is created and is sent",
                                 MakeTraceSourceAccessor (&MecOrcApplication::m_txTrace),
                                 "ns3::Packet::TracedCallback")
        ;
        return tid;
    }

    MecOrcApplication::MecOrcApplication () {
        NS_LOG_FUNCTION (this);
        m_sent = 0;
        m_socket = 0;
        m_sendUeEvent = EventId ();
        m_sendMecEvent = EventId ();
        m_data_ue = 0;
        m_data_mec = 0;

        //Parse server string into InetSocketAddress vector
        std::vector<std::string> args;
        std::string tempString;
        for (int i = 0 ; i < int(m_serverString.length()); i++){
            char c = m_serverString[i];
            if(c == '/'){
                args.push_back(tempString);
                tempString = "";
            }
            else{
                tempString.push_back(c);
            }
        }
        for(int i = 0; i< int(args.size()) ; i++){
            Ipv4Address ipv4 = Ipv4Address();
            std::string addrString = args[i];
            char cstr[addrString.size() + 1];
            addrString.copy(cstr, addrString.size()+1);
            cstr[addrString.size()] = '\0';
            ipv4.Set(cstr);
            m_allServers.push_back(InetSocketAddress(ipv4, 1001));
        }

        std::vector<std::string> args2;
        std::string tempString2;
        for (int i = 0 ; i < int(m_serverString.length()); i++){
            char c = m_serverString[i];
            if(c == '/'){
                args2.push_back(tempString2);
                tempString2 = "";
            }
            else{
                tempString2.push_back(c);
            }
        }
        for(int i = 0; i< int(args2.size()) ; i++){
            Ipv4Address ipv4 = Ipv4Address();
            std::string addrString = args2[i];
            char cstr[addrString.size() + 1];
            addrString.copy(cstr, addrString.size()+1);
            cstr[addrString.size()] = '\0';
            ipv4.Set(cstr);
            m_allUes.push_back(InetSocketAddress(ipv4, 1002));
        }
    }

    MecOrcApplication::~MecOrcApplication() {
        NS_LOG_FUNCTION (this);
        m_socket = 0;

        delete [] m_data_ue;
        delete [] m_data_mec;
        m_data_ue = 0;
        m_data_mec = 0;
    }

    void
    MecOrcApplication::DoDispose (void) {
        NS_LOG_FUNCTION (this);
        Application::DoDispose ();
    }

    void
    MecOrcApplication::StartApplication (void)
    {
        NS_LOG_FUNCTION (this);

        NS_LOG_DEBUG("ueString: " << m_ueString);
        NS_LOG_DEBUG("serverString: " << m_serverString);

        //Make socket for each server
        for (std::vector<InetSocketAddress>::iterator it = m_allServers.begin(); it != m_allServers.end(); ++it){
            TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
            Ptr<Socket> tempSocket;
            InetSocketAddress inet = (*it);
            tempSocket = Socket::CreateSocket (GetNode (), tid);
            tempSocket->Bind ();
            tempSocket->Connect (inet);
            tempSocket->SetRecvCallback (MakeCallback (&MecOrcApplication::HandleRead, this));
            tempSocket->SetAllowBroadcast (true);
            std::pair<InetSocketAddress, Ptr<Socket>>  newPair = std::pair<InetSocketAddress, Ptr<Socket>>(inet, tempSocket);
            serverSocketMap.insert(newPair);
        }

        //Make socket for each UE
        for (std::vector<InetSocketAddress>::iterator it = m_allUes.begin(); it != m_allUes.end(); ++it){
            TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
            Ptr<Socket> tempSocket;
            InetSocketAddress inet2 = (*it);
            tempSocket = Socket::CreateSocket (GetNode (), tid);
            tempSocket->Bind ();
            tempSocket->Connect (inet2);
            tempSocket->SetRecvCallback (MakeCallback (&MecOrcApplication::HandleRead, this));
            tempSocket->SetAllowBroadcast (true);
            std::pair<InetSocketAddress, Ptr<Socket>>  newPair = std::pair<InetSocketAddress, Ptr<Socket>>(inet2, tempSocket);
            ueSocketMap.insert(newPair);
        }
    }

    void
    MecOrcApplication::StopApplication () {
        NS_LOG_FUNCTION (this);

        std::map<InetSocketAddress, Ptr<Socket>>::iterator it;
        for (it = serverSocketMap.begin(); it != serverSocketMap.end(); ++it){
            Ptr<Socket> tempSocket = it->second;
            tempSocket->Close ();
            tempSocket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
            tempSocket = 0;
        }
        std::map<InetSocketAddress,Ptr<Socket>>::iterator it2;
        for (it2 = ueSocketMap.begin(); it2 != ueSocketMap.end(); ++it2){
            Ptr<Socket> tempSocket = it2->second;
            tempSocket->Close ();
            tempSocket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
            tempSocket = 0;
        }


    }

    uint8_t*
    MecOrcApplication::GetFilledString (std::string filler, int size) {
        //dest can either be m_data_request or m_data_ping or m_data_report
        NS_LOG_FUNCTION(this << filler);

        std::string result;
        uint8_t *val = (uint8_t *) malloc(size + 1);

        int fillSize = filler.size();

        if (fillSize >= int(size)) {
            NS_LOG_ERROR("Filler for packet larger than packet size");
            StopApplication();
        } else {
            result.append(filler);
            for (int i = result.size(); i < int(size); i++) {
                result.append("#");
            }

            std::memset(val, 0, size + 1);
            std::memcpy(val, result.c_str(), size + 1);
        }
        return val;
    }

    void
    MecOrcApplication::SendUeHandover (InetSocketAddress ueAddress, InetSocketAddress newMecAddress) {


        //Convert Address into string
        Ipv4Address addr = newMecAddress.GetIpv4();
        std::stringstream ss;
        std::stringstream os;
        addr.Print(os);
        ss << os.rdbuf();
        std::string addrString = ss.str();

        //Create packet payload

        std::stringstream ss2;
        ss2 << (newMecAddress.GetPort());
        std::string portString = ss2.str();

        std::string fillString = "6/" + addrString + "/" + portString + "/";
        uint8_t *buffer = GetFilledString(fillString, m_packetSize);

        //Create packet
        Ptr<Packet> p = Create<Packet> (buffer, m_packetSize);
        // call to the trace sinks before the packet is actually sent,
        // so that tags added to the packet can be sent as well
        m_txTrace (p);

        //Find correct UE to connect to and send message trough the matching socket
        std::map<InetSocketAddress, Ptr<Socket>>::iterator it;
        Ptr<Socket> newSocket = 0;
        for (it = ueSocketMap.begin(); it != ueSocketMap.end() && newSocket == 0; ++it){
            InetSocketAddress current = it->first;
            if(current.GetIpv4() == ueAddress.GetIpv4() && current.GetPort() == ueAddress.GetPort()){
                newSocket = it->second;
            }
        }
        if (newSocket != 0){
            newSocket->Send(p);
            ++m_sent;
        }
        else {
            NS_LOG_ERROR("Attempt to send to non-existing server socket");
        }
    }

    void
    MecOrcApplication::SendMecHandover (InetSocketAddress ueAddress, InetSocketAddress newMecAddress)
    {
        //Convert ueAddress into string
        Ipv4Address ueAddr = ueAddress.GetIpv4();
        std::stringstream ss;
        std::ofstream os ("temp.txt", std::ofstream::out);
        ueAddr.Print(os);
        ss << os.rdbuf();
        std::string ueAddrString = ss.str();
        os.flush();
        ss.flush();

        //Convert newMecAddress into string
        Ipv4Address newMecAddr = newMecAddress.GetIpv4();
        newMecAddr.Print(os);
        ss << os.rdbuf();
        std::string newMecAddrString = ss.str();

        //Create packet payload
        std::string fillString = "4/" + ueAddrString + "/" + std::to_string(ueAddress.GetPort()) + "/" + newMecAddrString + "/" + std::to_string(newMecAddress.GetPort()) + "/";
        uint8_t *buffer = GetFilledString(fillString, m_packetSize);

        //Create packet
        Ptr <Packet> p = Create<Packet>(buffer, m_packetSize);
        // call to the trace sinks before the packet is actually sent,
        // so that tags added to the packet can be sent as well
        m_txTrace(p);

        //Find correct MEC to connect to and send message trough the matching socket
        std::map<InetSocketAddress, Ptr<Socket>>::iterator it;
        Ptr<Socket> newSocket = 0;
        for (it = ueSocketMap.begin(); it != ueSocketMap.end() && newSocket == 0; ++it){
            InetSocketAddress current = it->first;
            if(current.GetIpv4() == newMecAddress.GetIpv4() && current.GetPort() == newMecAddress.GetPort()){
                newSocket = it->second;
            }
        }
        if (newSocket != 0){
            newSocket->Send(p);
            ++m_sent;
        }
        else {
            NS_LOG_ERROR("Attempt to send to non-existing server socket");
        }
    }

    void
    MecOrcApplication::HandleRead (Ptr<Socket> socket)
    {
        NS_LOG_FUNCTION (this << socket);
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom (from)))
        {
            if (InetSocketAddress::IsMatchingType (from))
            {

                //Get payload from packet
                uint32_t packetSize = packet->GetSize();
                uint8_t *buffer = new uint8_t[packetSize];
                packet->CopyData(buffer, packetSize);
                std::string payloadString = std::string((char*)buffer);

                //Split packet into args by "/" character
                std::vector<std::string> args;
                std::string tempString;
                for (int i = 0 ; i < int(payloadString.length()); i++){
                    char c = payloadString[i];
                    if(c == '/'){
                        args.push_back(tempString);
                        tempString = "";
                    }
                    else{
                        tempString.push_back(c);
                    }
                }

                //Choose which type of message this is based on args[0]
                switch(stoi(args[0])){
                    case 3: {
                        //This is a measurement report from a UE
                        Ipv4Address currentMecAddr = Ipv4Address();
                        currentMecAddr.Set(args[1].c_str());

                        //Make map from args[2] content
                        std::string mapString = args[2];
                        std::vector<std::string> map_args;
                        std::map<Ipv4Address, int> delayMap;
                        for (int i = 0 ; i < int(mapString.length()); i++){
                            char c = mapString[i];
                            if(c == '!'){
                                map_args.push_back(tempString);
                                tempString = "";
                            }
                            else{
                                tempString.push_back(c);
                            }
                        }
                        for(int i = 0; i < int(map_args.size()); i = i+2){
                            Ipv4Address address =  Ipv4Address();
                            address.Set((map_args[i]).c_str());
                            delayMap.insert(std::pair<Ipv4Address, int>(address, stoi(map_args[i+1])));
                        }

                        int currentDelay = delayMap.find(currentMecAddr)->second;

                        //find current delay
                        int optimalDelay = currentDelay;
                        Ipv4Address bestMec;

                        for (std::map<Ipv4Address,int>::iterator it = delayMap.begin(); it != delayMap.end(); ++it){
                            Ipv4Address address =  it->first;

                            int delay = it->second;

                            bool triggerEquation;

                            switch(TRIGGER){
                                case 0:
                                    triggerEquation = (delay<optimalDelay);
                                    break;
                                case 1:
                                    triggerEquation = (delay<optimalDelay && (delay < currentDelay*(1-HYSTERESIS)));
                                    break;
                                case 2:
                                    triggerEquation = (delay<optimalDelay && (delay < currentDelay - DELAY_THRESHOLD));
                                    break;
                                case 3:
                                    triggerEquation = delay<optimalDelay && (delay < currentDelay*(1-HYSTERESIS)) && (delay < currentDelay - DELAY_THRESHOLD);
                                    break;
                                default:
                                    NS_LOG_ERROR("Unsupported message type");
                                    StopApplication();
                            }
                            if (triggerEquation){
                                optimalDelay = delay;
                                bestMec = address;
                            }
                        }

                        if (bestMec != currentMecAddr){
                            //Initiate a handover to bestMec
                            InetSocketAddress ueAddress = InetSocketAddress::ConvertFrom(from);
                            //Find InetSocketAddress for bestMec
                            bool found = false;

                            for (std::map<InetSocketAddress, int>::iterator it = waitingTimes.begin(); it != waitingTimes.end() && !found; ++it){
                                InetSocketAddress current = it->first;
                                Ipv4Address currentIpv4 = current.GetIpv4();

                                if(currentIpv4 == bestMec){
                                    InetSocketAddress mecAddress = current;
                                    SendUeHandover(ueAddress, mecAddress);
                                    SendMecHandover(ueAddress, mecAddress);
                                    found = true;
                                }
                            }
                        }
                        break;
                    }
                    case 5: {
                        int newWaitingTime = stoi(args[1]);
                        InetSocketAddress sendAddress = InetSocketAddress::ConvertFrom(from);

                        if (waitingTimes.find(sendAddress) != waitingTimes.end()) {
                            //Element already exists in map
                            waitingTimes[sendAddress] = newWaitingTime;

                        } else {
                            //New MEC, add entry in map
                            waitingTimes.insert(std::pair<InetSocketAddress, int>(sendAddress, newWaitingTime));

                        }
                        break;
                    }
                    default:
                        NS_LOG_ERROR("Non-exsitent message type received");
                        StopApplication();
                }
            }
        }
    }
} // Namespace ns3
