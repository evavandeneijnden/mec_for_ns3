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
                .AddAttribute ("MecPort", "Port that MECs use",
                               UintegerValue (),
                               MakeUintegerAccessor (&MecOrcApplication::m_mecPort),
                               MakeUintegerChecker<uint32_t> ())
                .AddAttribute ("Trigger", "Trigger used to decide handover",
                               UintegerValue (),
                               MakeUintegerAccessor (&MecOrcApplication::trigger),
                               MakeUintegerChecker<uint32_t> ())
                .AddAttribute ("Hysteresis", "Percentage performance must be better to consider handover",
                               DoubleValue (),
                               MakeDoubleAccessor (&MecOrcApplication::hysteresis),
                               MakeDoubleChecker<double> ())
                .AddAttribute ("DelayThreshold", "Minimum delay in ms before handover can be considered",
                               UintegerValue (),
                               MakeUintegerAccessor (&MecOrcApplication::delay_threshold),
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
            m_allServers.push_back(InetSocketAddress(ipv4, 1000));
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
            m_allUes.push_back(InetSocketAddress(ipv4, 1000));
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

        TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
        Ptr<Socket> tempSocket;
        tempSocket = Socket::CreateSocket (GetNode(), tid);
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 1000);
        tempSocket->Bind(local);
        tempSocket->Connect(InetSocketAddress("0.0.0.0", 1000));
        tempSocket->SetRecvCallback(MakeCallback(&MecOrcApplication::HandleRead, this));
        tempSocket->SetAllowBroadcast(false);
        m_socket = tempSocket;

//        //Make socket for each server
//        for (std::vector<InetSocketAddress>::iterator it = m_allServers.begin(); it != m_allServers.end(); ++it){
//            TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
//            Ptr<Socket> tempSocket;
//            InetSocketAddress inet = (*it);
//            tempSocket = Socket::CreateSocket (GetNode(), tid);
//            InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny(), m_mecPort);
//            tempSocket->Bind(local);
//            tempSocket->Connect(inet);
//            tempSocket->SetRecvCallback(MakeCallback(&MecOrcApplication::HandleRead, this));
//            tempSocket->SetAllowBroadcast (true);
//            std::pair<InetSocketAddress, Ptr<Socket>>  newPair = std::pair<InetSocketAddress, Ptr<Socket>>(inet, tempSocket);
//            serverSocketMap.insert(newPair);
//        }
//
//        //Make socket for each UE
//        for (std::vector<InetSocketAddress>::iterator it = m_allUes.begin(); it != m_allUes.end(); ++it){
//            TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
//            Ptr<Socket> tempSocket;
//            InetSocketAddress inet2 = (*it);
//            tempSocket = Socket::CreateSocket (GetNode (), tid);
//            InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_uePort);
//            tempSocket->Bind (local);
//            tempSocket->Connect (inet2);
//            tempSocket->SetRecvCallback (MakeCallback (&MecOrcApplication::HandleRead, this));
//            tempSocket->SetAllowBroadcast (true);
//            std::pair<InetSocketAddress, Ptr<Socket>>  newPair = std::pair<InetSocketAddress, Ptr<Socket>>(inet2, tempSocket);
//            ueSocketMap.insert(newPair);
//        }
    }

    void
    MecOrcApplication::StopApplication () {
        NS_LOG_FUNCTION (this);

//        std::map<InetSocketAddress, Ptr<Socket>>::iterator it;
//        for (it = serverSocketMap.begin(); it != serverSocketMap.end(); ++it){
//            Ptr<Socket> tempSocket = it->second;
//            tempSocket->Close ();
//            tempSocket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
//            tempSocket = 0;
//        }
//        std::map<InetSocketAddress,Ptr<Socket>>::iterator it2;
//        for (it2 = ueSocketMap.begin(); it2 != ueSocketMap.end(); ++it2){
//            Ptr<Socket> tempSocket = it2->second;
//            tempSocket->Close ();
//            tempSocket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
//            tempSocket = 0;
//        }
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>> ());

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
    MecOrcApplication::SendUeHandover (InetSocketAddress ueAddress, InetSocketAddress newMecAddress, InetSocketAddress currentMecAddress) {
        NS_LOG_FUNCTION (this);
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

        //Used to provide a no-send time (in ms) to the UE so it doesn't try to send messages mid handover; wait until handover has been processed by current AND new MEC
        int handoverTime = waitingTimes.find(currentMecAddress)->second + waitingTimes.find(newMecAddress)->second;

        std::string fillString = "6/" + addrString + "/" + portString + "/" + std::to_string(handoverTime) + "/";
        uint8_t *buffer = GetFilledString(fillString, m_packetSize);

        //Create packet
        Ptr<Packet> p = Create<Packet> (buffer, m_packetSize);
        // call to the trace sinks before the packet is actually sent,
        // so that tags added to the packet can be sent as well
        m_txTrace(p);

        m_socket->SendTo(p, 0, InetSocketAddress(ueAddress.GetIpv4(), 1000));
        ++m_sent;
    }

    void
    MecOrcApplication::SendMecHandover (InetSocketAddress ueAddress, InetSocketAddress newMecAddress)
    {
        NS_LOG_FUNCTION (this);
        //Convert ueAddress into string
        Ipv4Address ueAddr = ueAddress.GetIpv4();
        std::stringstream ss;
        std::stringstream os;
        ueAddr.Print(os);
        ss << os.rdbuf();
        std::string ueAddrString = ss.str();

        //Convert newMecAddress into string
        Ipv4Address newMecAddr = newMecAddress.GetIpv4();
        std::stringstream ss2;
        std::stringstream os2;
        newMecAddr.Print(os2);
        ss2 << os2.rdbuf();
        std::string newMecAddrString = ss.str();

        //Create packet payload
        std::string fillString = "4/" + ueAddrString + "/" + std::to_string(ueAddress.GetPort()) + "/" + newMecAddrString + "/" + std::to_string(newMecAddress.GetPort()) + "/";
        uint8_t *buffer = GetFilledString(fillString, m_packetSize);

        //Create packet
        Ptr <Packet> p = Create<Packet>(buffer, m_packetSize);
        // call to the trace sinks before the packet is actually sent,
        // so that tags added to the packet can be sent as well
        m_txTrace(p);

        m_socket->SendTo(p, 0, InetSocketAddress(ueAddress.GetIpv4(), 1000));
        ++m_sent;
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

                            switch(trigger){
                                case 0:
                                    triggerEquation = (delay<optimalDelay);
                                    break;
                                case 1:
                                    triggerEquation = (delay<optimalDelay && (delay < currentDelay*(1-hysteresis)));
                                    break;
                                case 2:
                                    triggerEquation = (delay<optimalDelay && (delay < currentDelay - delay_threshold));
                                    break;
                                case 3:
                                    triggerEquation = delay<optimalDelay && (delay < currentDelay*(1-hysteresis)) && (delay < currentDelay - delay_threshold);
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
                                    InetSocketAddress newMecAddress = current;
                                    SendUeHandover(ueAddress, newMecAddress, currentMecAddr);
                                    SendMecHandover(ueAddress, newMecAddress);
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
                        NS_LOG_ERROR("Non-existent message type received");
                        StopApplication();
                }
            }
        }
    }
} // Namespace ns3
