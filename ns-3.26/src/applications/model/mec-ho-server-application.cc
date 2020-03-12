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
#include <regex>
#include "ns3/double.h"
#include "ns3/vector.h"
#include <fstream>
#include <iostream>

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
                .AddAttribute ("UpdateInterval",
                               "The time to wait between measurement updates",
                               UintegerValue (),
                               MakeUintegerAccessor (&MecHoServerApplication::m_updateInterval),
                               MakeUintegerChecker<uint32_t> ())
                .AddAttribute ("ServiceInterval",
                               "The time to wait between service packets",
                               UintegerValue (),
                               MakeUintegerAccessor (&MecHoServerApplication::m_serviceInterval),
                               MakeUintegerChecker<uint32_t> ())
                .AddAttribute ("MeasurementInterval",
                               "The time to wait between ping packets",
                               UintegerValue (),
                               MakeUintegerAccessor (&MecHoServerApplication::m_measurementInterval),
                               MakeUintegerChecker<uint32_t> ())
                .AddAttribute ("PacketSize", "Size of echo data in outbound packets",
                               UintegerValue (),
                               MakeUintegerAccessor (&MecHoServerApplication::m_packetSize),
                               MakeUintegerChecker<uint32_t> ())
                .AddAttribute ("UeHandoverSize", "Amount of data to be transferred when a UE is handed over",
                               UintegerValue (),
                               MakeUintegerAccessor (&MecHoServerApplication::UE_SIZE),
                               MakeUintegerChecker<uint32_t> ())
                .AddAttribute ("MecRate", "Number of jobs a server can handle per millisecond",
                               DoubleValue (),
                               MakeDoubleAccessor (&MecHoServerApplication::MEC_RATE),
                               MakeDoubleChecker<double> ())
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
                .AddAttribute ("Metric", "Metric to base handover decision on",
                               UintegerValue(),
                               MakeUintegerAccessor (&MecHoServerApplication::metric),
                               MakeUintegerChecker<uint32_t> ())
                .AddAttribute ("Local",
                               "Local IP address",
                               Ipv4AddressValue (),
                               MakeIpv4AddressAccessor (&MecHoServerApplication::m_thisIpAddress),
                               MakeIpv4AddressChecker ())
                .AddTraceSource ("Tx", "A new packet is created and is sent",
                                 MakeTraceSourceAccessor (&MecHoServerApplication::m_txTrace),
                                 "ns3::Packet::TracedCallback")
                .AddAttribute ("Logfile", "Name of the file to log to",
                               StringValue(""),
                               MakeStringAccessor (&MecHoServerApplication::m_filename),
                               MakeStringChecker())
        ;
        return tid;
    }

    MecHoServerApplication::MecHoServerApplication ()
    {
        NS_LOG_FUNCTION (this);
        m_socket = 0;
        m_sendEvent = EventId ();
        m_transferEvent = EventId();
        m_echoEvent = EventId();
        m_startTime = Simulator::Now();

        m_noHandovers = 0;
        noSendUntil = Simulator::Now();

        responseTimeUpdateCounter = 0;
        ueTransferCounter = 0;
        echoCounter = 0;
        serviceRequestCounter = 0;
        pingRequestCounter = 0;
        handoverCommandCounter = 0;
        handoverDataCounter = 0;
        firstRequestCounter = 0;
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

        std::fstream outfile;
        outfile.open(m_filename, std::ios::app);
        outfile << "Sanity check: " << std::to_string(responseTimeUpdateCounter) << ", " << std::to_string(ueTransferCounter) <<
            ", " << std::to_string(echoCounter) << ", " << std::to_string(serviceRequestCounter) << ", " <<
            std::to_string(pingRequestCounter) << ", " << std::to_string(handoverCommandCounter) << ", " <<
            std::to_string(handoverDataCounter) << ", " << std::to_string(firstRequestCounter) << std::endl;
        outfile << "QueueCounter: " << queueCounter << std::endl;

        Application::DoDispose ();
    }

    void
    MecHoServerApplication::StartApplication (void)
    {
        NS_LOG_FUNCTION (this);


        //Create random variable for variation in scheduling
        randomness = CreateObject<UniformRandomVariable>();
        randomness->SetAttribute("Min", DoubleValue(0.0));
        randomness->SetAttribute("Max", DoubleValue(10.0));

        processingTimer = CreateObject<ExponentialRandomVariable>();
        processingTimer->SetAttribute("Mean", DoubleValue(1/MEC_RATE));


        MSG_FREQ = 1.0/(double)m_serviceInterval;
        if (metric == 0){
            MEAS_FREQ = 1.0/(double)m_measurementInterval;
        }
        else if (metric == 1){
            MEAS_FREQ = 0.0;
        }


        //Make all servers list
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
            std::regex re("([0-9]+\\.[0-9+]\\.[0-9]+\\.[0-9]+)");
            std::smatch match;
            NS_ASSERT(std::regex_search(args[i], match, re));
            Ipv4Address ipv4 = Ipv4Address();
            std::string addrString = args[i];
            char cstr[addrString.size() + 1];
            addrString.copy(cstr, addrString.size()+1);
            cstr[addrString.size()] = '\0';
            ipv4.Set(cstr);
            m_allServers.push_back(InetSocketAddress(ipv4, m_mecPort));
        }
        NS_ASSERT(args.size() == m_allServers.size());

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
            std::regex re("([0-9]+\\.[0-9+]\\.[0-9]+\\.[0-9]+)");
            std::smatch match;
            NS_ASSERT(std::regex_search(args2[i], match, re));
            Ipv4Address ipv4 = Ipv4Address();
            std::string addrString = args2[i];
            char cstr[addrString.size() + 1];
            addrString.copy(cstr, addrString.size()+1);
            cstr[addrString.size()] = '\0';
            ipv4.Set(cstr);
            m_allUes.push_back(InetSocketAddress(ipv4, m_uePort));
        }
        NS_ASSERT(args2.size() == m_allUes.size());

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

        //Make socket for each server
        for (std::vector<InetSocketAddress>::iterator it = m_allServers.begin(); it != m_allServers.end(); ++it){
            TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
            Ptr<Socket> tempSocket = 0;
            InetSocketAddress inet = (*it);
            std::pair<InetSocketAddress, Ptr<Socket>>  newPair = std::pair<InetSocketAddress, Ptr<Socket>>(inet, tempSocket);
            serverSocketMap.insert(newPair);
        }

        //Make socket for each client
        for (std::vector<InetSocketAddress>::iterator it = m_allUes.begin(); it != m_allUes.end(); ++it){
            TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
            Ptr<Socket> tempSocket = 0;
            InetSocketAddress inet = (*it);
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
        NS_ASSERT(fillSize <= int(size));

        result.append(filler);
        for (int i = result.size(); i < int(size); i++) {
            result.append("#");
        }
        NS_ASSERT(result.find(filler) == 0);
        std::string filler_part = result.substr(fillSize);
        NS_ASSERT(filler_part.at(0) == '#');
        for (int i = 1; i < int(filler_part.size()); i++){
            NS_ASSERT(filler_part.at(i) == filler_part.at(i-1));
        }
        std::memset(val, 0, size + 1);
        std::memcpy(val, result.c_str(), size + 1);

        return val;
    }

    void
    MecHoServerApplication::SendResponseTimeUpdate (Time responseTime) {
        NS_LOG_FUNCTION(this);
        NS_ASSERT(m_sendEvent.IsExpired());
        std::fstream outfile;
        outfile.open(m_filename, std::ios::app);

        m_expectedResponseTime = responseTime.GetMilliSeconds();

        outfile << "Server response time, " << Simulator::Now().GetSeconds() << ", " << m_thisIpAddress << ", " << std::to_string(myClients.size()) <<  ", " << m_expectedResponseTime << std::endl;

        //Create packet payload
        std::string fillString = "5/" + std::to_string(m_expectedResponseTime) + "/";
        std::regex re("5/[1-9][0-9]*/");
        std::smatch match;
        NS_ASSERT(std::regex_search(fillString, match, re));

        uint8_t *buffer = GetFilledString(fillString, m_packetSize);

        //Send packet
        Ptr <Packet> p = Create<Packet>(buffer, m_packetSize);
        m_txTrace(p);
        m_socket->Send(p);

        responseTimeUpdateCounter++;


    }

    void
    MecHoServerApplication::SendUeTransfer (InetSocketAddress ueAddress, InetSocketAddress newMecAddress)
    {
        NS_LOG_FUNCTION (this);
        if (Simulator::Now() > noSendUntil){
            //Create packet payload
            //Get string representation of UE address
            std::stringstream ss;
            std::stringstream os;
            ueAddress.GetIpv4().Print(os);
            ss << os.rdbuf();
            std::string ueAddrString = ss.str();

            std::string fillString = "7/" + ueAddrString + "/";
            std::regex re("7/([0-9]+\\.[0-9+]\\.[0-9]+\\.[0-9]+)/");
            std::smatch match;
            NS_ASSERT(std::regex_search(fillString, match, re));

            uint8_t *buffer = GetFilledString(fillString, UE_SIZE);

            //Create packet
            Ptr <Packet> p = Create<Packet>(buffer, UE_SIZE);
            // call to the trace sinks before the packet is actually sent,
            // so that tags added to the packet can be sent as well
            m_txTrace(p);
            m_socket->SendTo(p, 0, newMecAddress);
        }
        else {
            m_transferEvent = Simulator::Schedule(noSendUntil, &MecHoServerApplication::SendUeTransfer, this, ueAddress, newMecAddress);
        }
        ueTransferCounter++;

    }


    void
    MecHoServerApplication::SendEcho (Ipv4Address echoAddress, Ptr<Packet> packet)
    {
        NS_LOG_FUNCTION (this);
        if(Simulator::Now() > noSendUntil){
            m_txTrace(packet);
            m_socket->SendTo(packet, 0, InetSocketAddress(echoAddress, 1000));
            echoCounter++;
        }
        else {
            m_echoEvent = Simulator::Schedule(noSendUntil, &MecHoServerApplication::SendEcho, this, echoAddress, packet);
        }
    }

    Time
    MecHoServerApplication::HandleQueue (Ptr<Packet> newPacket)
    {
        NS_LOG_FUNCTION(this);
        //Update existing packet queue and toss old ones
        for (int i = 0 ; i < int(processingQueue.size()) ; i++){
            std::pair<Ptr<Packet>, Time> currentPair = processingQueue[i];
            if (currentPair.second <= Simulator::Now()){
                //This packet has already left the queue; remove it
                processingQueue.erase(processingQueue.begin() + i);
            }
        }
        //Calculate waiting time for packet
        Time responseTime;
        if(processingQueue.size() > 0){
            //Response time == waiting for the previous packet (last in vector) to be done + own processing time
            Time processingTime = MilliSeconds(processingTimer->GetValue() + 1);
            responseTime = MilliSeconds(processingQueue.back().second + processingTime);
            queueCounter++;
        }
        else {
            //There is no queue; response time == own processing time
            responseTime = MilliSeconds(processingTimer->GetValue() + 1);
        }
        //Add new packet to queue
        std::pair<Ptr<Packet>, Time> queueItem = std::make_pair(newPacket, responseTime);
        processingQueue.push_back(queueItem);

        return responseTime;
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
                        m_echoAddress = inet_from.GetIpv4();
                        //Echo packet back to sender with appropriate delay
                        Time responseTime = HandleQueue(packet);
                        m_echoEvent = Simulator::Schedule(responseTime, &MecHoServerApplication::SendEcho, this, m_echoAddress, packet);
                        serviceRequestCounter++;
                        break;
                    }
                    case 2:{
                        // ping request from UE
                        int ue_cellId = stoi(args[1]);
                        uint32_t delay; //in ms

                        if (int(m_cellId) == ue_cellId){
                            //UE is connected to same eNB as MEC
                            if (myClients.find(inet_from) != myClients.end()){
                                //UE is connected to this MEC
                                delay = 0;
                            }
                            else{
                                delay = 15;
                            }
                        }
                        else {
                            //UE is connected to another eNB; add "penalty" for having to go through network
                            delay = 15;
                        }
                        m_echoAddress = inet_from.GetIpv4();
                        //Echo packet back to sender with appropriate delay
                        Time responseTime = HandleQueue(packet);

                        //Make sure SendResponseTimeUpdate gets trigger once each ping interval
                        if (m_allUes[0] == inet_from){
                            SendResponseTimeUpdate(responseTime);
                        }

                        m_echoEvent = Simulator::Schedule(responseTime + MilliSeconds(delay), &MecHoServerApplication::SendEcho, this, m_echoAddress, packet);
                        pingRequestCounter++;
                        break;
                    }

                    case 4: {
                        //handover command from orc
                        //Get ue address
                        std::string ueAddressString = args[1];
                        std::string uePortString = args[2];

                        Ipv4Address ueAddress = Ipv4Address();
                        ueAddress.Set(ueAddressString.c_str());

                        int newPort = int(std::stoi(uePortString));

                        m_newAddress = ueAddress;
                        m_newPort = newPort;
                        InetSocketAddress ueInet = InetSocketAddress(m_newAddress, m_newPort);

                        //Get new MEC address
                        std::string mecAddressString = args[3];
                        std::string mecPortString = args[4];

                        Ipv4Address mecAddress = Ipv4Address();
                        mecAddress.Set(mecAddressString.c_str());

                        int mecPort = int(std::stoi(mecPortString));

                        InetSocketAddress mecInet = InetSocketAddress(mecAddress, mecPort);

                        m_noHandovers++;

                        myClients.erase(ueInet);
                        //Initiate handover
                        Time responseTime = HandleQueue(packet);
                        Simulator::Schedule(responseTime, &MecHoServerApplication::SendUeTransfer, this, ueInet, mecInet);
                        handoverCommandCounter++;
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
                        noSendUntil = Simulator::Now() + Seconds(m_expectedResponseTime/1000);
                        m_noHandovers++;
                        handoverDataCounter++;
                        break;
                    }
                    case 8: {
                        //FIRST service request from ue
                        int ue_cellId = stoi(args[1]);
                        uint32_t delay; //in ms

                        myClients.insert(inet_from);

                        if (int(m_cellId) == ue_cellId){
                            delay = 0;

                        }
                        else {
                            //UE is connected to another eNB; add "penalty" for having to go through network
                            delay = 15;
                        }

                        m_echoAddress = inet_from.GetIpv4();
                        //Echo packet back to sender with appropriate delay
                        //Create packet payload
                        std::string fillString = "1/3/" + args[2] + "/";
                        uint8_t *buffer = GetFilledString(fillString, m_packetSize);

                        //Send packet
                        Ptr <Packet> p = Create<Packet>(buffer, m_packetSize);
                        Time responseTime = HandleQueue(packet);
                        m_echoEvent = Simulator::Schedule(responseTime + MilliSeconds(delay), &MecHoServerApplication::SendEcho, this, m_echoAddress, p);
                        firstRequestCounter++;
                        break;
                    }
                }
            }
        }
    }
} // Namespace ns3
