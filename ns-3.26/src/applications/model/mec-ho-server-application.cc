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

#include "mec-ho-server-application.h"
#include <sstream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MecHoServerApplication");

NS_OBJECT_ENSURE_REGISTERED (MecHoServerApplication);



    TypeId
    MecHoServerApplication::GetTypeId (void)
    {
        static TypeId tid = TypeId ("ns3::MecHoServerApplication")
                .SetParent<Application> ()
                .SetGroupName("Applications")
                .AddConstructor<MecHoServerApplication> ()
                .AddAttribute ("MaxPackets",
                               "The maximum number of packets the application will send",
                               UintegerValue (),
                               MakeUintegerAccessor (&MecHoServerApplication::m_count),
                               MakeUintegerChecker<uint32_t> ())
                .AddAttribute ("UpdateInterval",
                               "The time to wait between serviceRequest packets",
                               UintegerValue (),
                               MakeUintegerAccessor (&MecHoServerApplication::m_updateInterval),
                               MakeUintegerChecker<uint32_t> ())
                .AddAttribute ("PacketSize", "Size of echo data in outbound packets",
                               UintegerValue (),
                               MakeUintegerAccessor (&MecHoServerApplication::m_packetSize),
                               MakeUintegerChecker<uint32_t> ())
                .AddAttribute ("OrcAddress", "InetSocketAddress of the orchestrator",
                               Ipv4AddressValue(),
                               MakeIpv4AddressAccessor (&MecHoServerApplication::m_orcAddress),
                               MakeIpv4AddressChecker())
                .AddAttribute ("OrcPort", "Port number",
                               UintegerValue(),
                               MakeUintegerAccessor (&MecHoServerApplication::m_orcPort),
                               MakeUintegerChecker<uint32_t> ())
                .AddAttribute ("CellID", "ID of the eNB this server is associated with",
                               UintegerValue(),
                               MakeUintegerAccessor (&MecHoServerApplication::m_cellId),
                               MakeUintegerChecker<uint32_t> ())
                .AddAttribute ("AllServers", "Container of all server addresses",
                               StringValue("x"),
                               MakeStringAccessor (&MecHoServerApplication::m_serverString),
                               MakeStringChecker())
                .AddAttribute ("MecPort", "Port number for MEC sockets",
                               UintegerValue(),
                               MakeUintegerAccessor (&MecHoServerApplication::m_mecPort),
                               MakeUintegerChecker<uint32_t> ())
                .AddAttribute ("AllUes", "Container of all UE addresses",
                               StringValue("x"),
                               MakeStringAccessor (&MecHoServerApplication::m_ueString),
                               MakeStringChecker())
                .AddAttribute ("UePort", "Port number for Ue sockets",
                               UintegerValue(),
                               MakeUintegerAccessor (&MecHoServerApplication::m_uePort),
                               MakeUintegerChecker<uint32_t> ())
                .AddAttribute ("NumberOfUes", "Number of UEs in the system",
                               UintegerValue(),
                               MakeUintegerAccessor (&MecHoServerApplication::m_noUes),
                               MakeUintegerChecker<uint32_t> ())
                .AddTraceSource ("Tx", "A new packet is created and is sent",
                                 MakeTraceSourceAccessor (&MecHoServerApplication::m_txTrace),
                                 "ns3::Packet::TracedCallback")
        ;
        return tid;
    }

    MecHoServerApplication::MecHoServerApplication ()
    {
        NS_LOG_FUNCTION (this);
        m_sent = 0;
        m_socket = 0;
        m_sendEvent = EventId ();
        m_transferEvent = EventId();
        m_echoEvent = EventId();
        m_startTime = Simulator::Now();

        m_noHandovers = 0;
        noSendUntil = Simulator::Now();

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
            m_allServers.push_back(InetSocketAddress(ipv4, m_mecPort));
        }

        //Make allUes list
        std::vector<std::string> args2;
        std::string tempString2;
        for (int i = 0 ; i < int(m_ueString.length()); i++){
            char c = m_ueString[i];
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
            std::string addrString = args[i];
            char cstr[addrString.size() + 1];
            addrString.copy(cstr, addrString.size()+1);
            cstr[addrString.size()] = '\0';
            ipv4.Set(cstr);
            m_allUes.push_back(InetSocketAddress(ipv4, m_uePort));
        }
    }

    MecHoServerApplication::~MecHoServerApplication()
    {
        NS_LOG_FUNCTION (this);
        m_socket = 0;

    }

    void
    MecHoServerApplication::DoDispose (void)
    {
        NS_LOG_FUNCTION (this);
        Application::DoDispose ();
    }

    void
    MecHoServerApplication::StartApplication (void)
    {
        NS_LOG_FUNCTION (this);

        //Make ORC socket
        if (m_socket == 0)
        {
            TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
            m_socket = Socket::CreateSocket(GetNode (), tid);
            InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_orcPort);
            m_socket->Bind(local);
            m_socket->Connect(InetSocketAddress(m_orcAddress, m_orcPort));
        }
        m_socket->SetRecvCallback(MakeCallback (&MecHoServerApplication::HandleRead, this));
        m_socket->SetAllowBroadcast(false);
        SendWaitingTimeUpdate();

        //Make socket for each server
        for (std::vector<InetSocketAddress>::iterator it = m_allServers.begin(); it != m_allServers.end(); ++it){
            TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
            Ptr<Socket> tempSocket = 0;
            //InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 1000);
//            tempSocket->Bind (local);
            InetSocketAddress inet = (*it);
//            tempSocket->Connect (inet);
//            tempSocket->SetRecvCallback (MakeCallback (&MecHoServerApplication::HandleRead, this));
//            tempSocket->SetAllowBroadcast (true);
            std::pair<InetSocketAddress, Ptr<Socket>>  newPair = std::pair<InetSocketAddress, Ptr<Socket>>(inet, tempSocket);
            serverSocketMap.insert(newPair);
        }

        //Make socket for each client
        for (std::vector<InetSocketAddress>::iterator it = m_allUes.begin(); it != m_allUes.end(); ++it){
            TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
            Ptr<Socket> tempSocket = 0;
//            tempSocket = Socket::CreateSocket (GetNode (), tid);
            //InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 1000); //m_uePort);
//            tempSocket->Bind (local);
            InetSocketAddress inet = (*it);
//            tempSocket->Connect (inet);
//            tempSocket->SetRecvCallback (MakeCallback (&MecHoServerApplication::HandleRead, this));
//            tempSocket->SetAllowBroadcast (true);
            std::pair<InetSocketAddress, Ptr<Socket>>  newPair = std::pair<InetSocketAddress, Ptr<Socket>>(inet, tempSocket);
            clientSocketMap.insert(newPair);
        }

    }

    void
    MecHoServerApplication::StopApplication ()
    {
        NS_LOG_FUNCTION (this);

        if (m_socket != 0)
        {
            m_socket->Close ();
            m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
            m_socket = 0;
        }
//
//        std::map<InetSocketAddress, Ptr<Socket>>::iterator it;
//        for (it = serverSocketMap.begin(); it != serverSocketMap.end(); ++it){
//            Ptr<Socket> tempSocket = it->second;
//            tempSocket->Close ();
//            tempSocket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
//            tempSocket = 0;
//        }
//        std::map<InetSocketAddress,Ptr<Socket>>::iterator it2;
//        for (it2 = clientSocketMap.begin(); it2 != clientSocketMap.end(); ++it2){
//            Ptr<Socket> tempSocket = it2->second;
//            //tempSocket->Close ();
//            //tempSocket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
//            tempSocket = 0;
//        }

        Simulator::Cancel (m_sendEvent);
        Simulator::Cancel(m_echoEvent);
    }

    uint8_t*
    MecHoServerApplication::GetFilledString (std::string filler, int size) {
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
    MecHoServerApplication::SendWaitingTimeUpdate (void) {
        NS_LOG_FUNCTION(this << m_updateInterval);
        NS_ASSERT(m_sendEvent.IsExpired());

        if (Simulator::Now() > noSendUntil) {

            //Calculate waiting time (in ms)
            double serviceRho = (double)(myClients.size() * MSG_FREQ) / MEC_RATE;
            double expectedServiceWaitingTime = (serviceRho / (1 - serviceRho)) * (1 / MEC_RATE);
            double pingRho = (double)(m_noUes * MEAS_FREQ) / MEC_RATE;
            double expectedPingWaitingTime = (double)(pingRho / (1 - pingRho)) * (1 / MEC_RATE);
            double handoverFrequency = (double)m_noHandovers / ((Simulator::Now() - m_startTime).GetSeconds());
            double handoverRho = (double)handoverFrequency / MEC_RATE;
            double expectedHandoverWaitingTime = (double)(handoverRho / (1 - handoverRho)) * (1 / MEC_RATE);
//            NS_LOG_DEBUG("serviceRho: " << serviceRho << ", expectedServiceWaitingTime:" << expectedServiceWaitingTime << ", pingRho: "
//                          << pingRho << ", expectedPingWaitingTime: " << expectedPingWaitingTime << ",handoverFrequency: " << handoverFrequency
//                          << ", handoverRho: " << handoverRho << ", expectedHandoverWaitingTime: " << expectedHandoverWaitingTime);
            m_expectedWaitingTime = (expectedServiceWaitingTime + expectedPingWaitingTime + expectedHandoverWaitingTime)*1000;

            //Create packet payload
            std::string fillString = "5/" + std::to_string(m_expectedWaitingTime) + "/";
            uint8_t *buffer = GetFilledString(fillString, m_packetSize);

            //Send packet
            Ptr <Packet> p = Create<Packet>(buffer, m_packetSize);
            m_txTrace(p);
            m_socket->Send(p);

            ++m_sent;

            if (m_sent < m_count) {
                m_sendEvent = Simulator::Schedule(Seconds(m_updateInterval/1000), &MecHoServerApplication::SendWaitingTimeUpdate, this);
            }
        }

         else {
            m_sendEvent = Simulator::Schedule(Seconds(m_updateInterval/1000), &MecHoServerApplication::SendWaitingTimeUpdate, this);
//            SendWaitingTimeUpdate();
        }


    }

    void
    MecHoServerApplication::SendUeTransfer (Ipv4Address newAddress)
    {
        NS_LOG_FUNCTION (this);
        if (Simulator::Now() > noSendUntil){
            //Create packet payload
            //Get string representation of UE address
            std::stringstream ss;
            std::stringstream os;
            newAddress.Print(os);
            ss << os.rdbuf();
            std::string addrString = ss.str();

            std::string fillString = "7/" + addrString + "/";
            uint8_t *buffer = GetFilledString(fillString, UE_SIZE);

            //Create packet
            Ptr <Packet> p = Create<Packet>(buffer, UE_SIZE);
            // call to the trace sinks before the packet is actually sent,
            // so that tags added to the packet can be sent as well
            m_txTrace(p);
            m_socket->SendTo(p, 0, InetSocketAddress(newAddress, 1000));
            ++m_sent;
        }
        else {
            m_transferEvent = Simulator::Schedule(noSendUntil, &MecHoServerApplication::SendUeTransfer, this, newAddress);
        }

    }


    void
    MecHoServerApplication::SendEcho (Ipv4Address echoAddress, Ptr<Packet> packet)
    {
        NS_LOG_FUNCTION (this);
        if(Simulator::Now() > noSendUntil){
            m_txTrace(packet);
            m_socket->SendTo(packet, 0, InetSocketAddress(echoAddress, 1000));

            ++m_sent;
        }
        else {
            m_echoEvent = Simulator::Schedule(noSendUntil, &MecHoServerApplication::SendEcho, this, echoAddress, packet);
        }
    }


    void
    MecHoServerApplication::HandleRead (Ptr<Socket> socket)
    {
        NS_LOG_FUNCTION (this << socket);
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom (from)))
        {
            if (InetSocketAddress::IsMatchingType (from))
            {
                InetSocketAddress inet_from = InetSocketAddress::ConvertFrom(from);

                //Get payload from packet
                uint32_t packetSize = packet->GetSize();
                uint8_t *buffer = new uint8_t[packetSize];
                packet->CopyData(buffer, packetSize);

                std::string payloadString = std::string((char*)buffer);

                //Split the payload string into arguments
                std::string tempString;
                std::vector<std::string> args;
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

                switch(stoi(args[0])){
                    case 1: {
                        //service request from ue
                        int ue_cellId = stoi(args[1]);
                        uint32_t delay = 0; //in ms

                        if (int(m_cellId) == ue_cellId){
                            if (myClients.find(inet_from) != myClients.end()){
                                delay = m_expectedWaitingTime;
                            }
                            else{
                                //UE is connected to another MEC; add "penalty" for having to go through network
                                delay = m_expectedWaitingTime + 15;
                            }
                        }
                        else {
                            //UE is connected to another eNB; add "penalty" for having to go through network
                            delay = m_expectedWaitingTime + 15;
                        }
                        m_echoAddress = inet_from.GetIpv4();
                        //Echo packet back to sender with appropriate delay
                        m_echoEvent = Simulator::Schedule(MilliSeconds(delay), &MecHoServerApplication::SendEcho, this, m_echoAddress, packet);
                        break;
                    }
                    case 2:{
                        // ping request from UE
                        NS_LOG_DEBUG("Received ping request from " << inet_from.GetIpv4());
                        NS_LOG_DEBUG("My cellID is " << m_cellId);
                        int ue_cellId = stoi(args[1]);
                        uint32_t delay = 0; //in ms

                        if (int(m_cellId) == ue_cellId){
                            if (myClients.find(inet_from) != myClients.end()){
                                delay = m_expectedWaitingTime;
                            }
                            else{
                                //UE is connected to another MEC; add "penalty" for having to go through network
                                delay = m_expectedWaitingTime + 15;
                            }
                        }
                        else {
                            //UE is connected to another eNB; add "penalty" for having to go through network
                            delay = m_expectedWaitingTime + 15;
                        }
                        m_echoAddress = inet_from.GetIpv4();
                        //Echo packet back to sender with appropriate delay
                        m_echoEvent = Simulator::Schedule(MilliSeconds(delay), &MecHoServerApplication::SendEcho, this, m_echoAddress, packet);
                        break;
                    }

                    case 4: {
                        //handover command from orc
                        //Get new MEC address
                        std::string addressString = args[1];
                        std::string portString = args[2];

                        Ipv4Address newAddress = Ipv4Address();
                        newAddress.Set(addressString.c_str());

                        int newPort = int(std::stoi(portString));

                        m_newAddress = newAddress;
                        m_newPort = newPort;
                        InetSocketAddress newInet = InetSocketAddress(m_newAddress, m_newPort);

                        myClients.erase(newInet);
                        //Initiate handover
                        Simulator::Schedule(Seconds(0), &MecHoServerApplication::SendUeTransfer, this, m_newAddress);
                        break;
                    }
                    case 7: {
                        //handover data from other mec
                        //Make InetAddress from arg strings
                        Ipv4Address ipv4 = Ipv4Address();
                        std::string addrString = args[1];
                        char cstr[addrString.size() + 1];
                        addrString.copy(cstr, addrString.size()+1);
                        cstr[addrString.size()] = '\0';
                        ipv4.Set(cstr);
                        myClients.insert(InetSocketAddress(ipv4, 1000));
                        noSendUntil = Simulator::Now() + Seconds(m_expectedWaitingTime/1000);
                        break;
                    }
                    case 8: {
                        //FIRST service request from ue
                        int ue_cellId = stoi(args[1]);
                        uint32_t delay = 0; //in ms

                        myClients.insert(inet_from);

                        if (int(m_cellId) == ue_cellId){
                            delay = m_expectedWaitingTime;

                        }
                        else {
                            //UE is connected to another eNB; add "penalty" for having to go through network
                            delay = m_expectedWaitingTime + 15;
                        }

                        m_echoAddress = inet_from.GetIpv4();
                        //Echo packet back to sender with appropriate delay
                        //Create packet payload
                        std::string fillString = "1/3/";
                        uint8_t *buffer = GetFilledString(fillString, m_packetSize);

                        //Send packet
                        Ptr <Packet> p = Create<Packet>(buffer, m_packetSize);
                        m_echoEvent = Simulator::Schedule(MilliSeconds(delay), &MecHoServerApplication::SendEcho, this, m_echoAddress, p);
                        break;
                    }
                }
            }
        }
    }
} // Namespace ns3
