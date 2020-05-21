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
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "mec-ue-application.h"
#include "ns3/udp-socket-factory.h"
#include <sstream>
#include <fstream>
#include <regex>
#include "ns3/vector.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>

namespace ns3 {

    NS_LOG_COMPONENT_DEFINE ("MecUeApplication");

    NS_OBJECT_ENSURE_REGISTERED (MecUeApplication);

    TypeId
    MecUeApplication::GetTypeId (void)
    {
        static TypeId tid = TypeId ("ns3::MecUeApplication")
                .SetParent<Application> ()
                .SetGroupName("Applications")
                .AddConstructor<MecUeApplication> ()
                .AddAttribute ("PacketSize",
                               "The size of payload of a packet",
                               UintegerValue (),
                               MakeUintegerAccessor (&MecUeApplication::m_size),
                               MakeUintegerChecker<uint32_t> ())
                .AddAttribute ("ServiceInterval",
                               "The time to wait between serviceRequest packets",
                               TimeValue (Seconds (1.0)),
                               MakeTimeAccessor (&MecUeApplication::m_serviceInterval),
                               MakeTimeChecker ())
                .AddAttribute ("PingInterval",
                               "The time to wait between pingRequest packets",
                               TimeValue (Seconds (1.0)),
                               MakeTimeAccessor (&MecUeApplication::m_pingInterval),
                               MakeTimeChecker ())
                .AddAttribute ("MecIp",
                               "Ipv4Address of the server ti which this UE is connected",
                               Ipv4AddressValue (),
                               MakeIpv4AddressAccessor (&MecUeApplication::m_mecIp),
                               MakeIpv4AddressChecker ())
               .AddAttribute ("MecPort",
                               "Port of the server ti which this UE is connected",
                               UintegerValue (100),
                               MakeUintegerAccessor (&MecUeApplication::m_mecPort),
                               MakeUintegerChecker<uint32_t> ())
                .AddAttribute ("OrcIp",
                               "Ipv4Address of the ORC to which this UE is connected",
                               Ipv4AddressValue (),
                               MakeIpv4AddressAccessor (&MecUeApplication::m_orcIp),
                               MakeIpv4AddressChecker ())
                .AddAttribute ("OrcPort",
                               "Port of the ORC to which this UE is connected",
                               UintegerValue (),
                               MakeUintegerAccessor (&MecUeApplication::m_orcPort),
                               MakeUintegerChecker<uint32_t> ())
                .AddAttribute ("Local",
                               "Local IP address",
                               Ipv4AddressValue (),
                               MakeIpv4AddressAccessor (&MecUeApplication::m_thisIpAddress),
                               MakeIpv4AddressChecker ())
                .AddAttribute ("AllServers", "Container of all server addresses",
                               StringValue("x"),
                               MakeStringAccessor (&MecUeApplication::m_serverString),
                               MakeStringChecker())
                .AddAttribute ("Enb0", "nth enb in the system",
                               PointerValue(),
                               MakePointerAccessor (&MecUeApplication::m_enb0),
                               MakePointerChecker<LteEnbNetDevice> ())
                .AddAttribute ("Enb1", "nth enb in the system",
                               PointerValue(),
                               MakePointerAccessor (&MecUeApplication::m_enb1),
                               MakePointerChecker<LteEnbNetDevice> ())
                .AddAttribute ("Enb2", "nth enb in the system",
                               PointerValue(),
                               MakePointerAccessor (&MecUeApplication::m_enb2),
                               MakePointerChecker<LteEnbNetDevice> ())
                .AddAttribute ("Node", "Node on which this application will run",
                               PointerValue(),
                               MakePointerAccessor (&MecUeApplication::m_thisNode),
                               MakePointerChecker<Node> ())
                .AddAttribute ("ueImsi",
                               "IMSI of this UE",
                               UintegerValue (),
                               MakeUintegerAccessor (&MecUeApplication::ueImsi),
                               MakeUintegerChecker<uint64_t> ())
                .AddTraceSource ("Tx", "A new packet is created and is sent",
                                 MakeTraceSourceAccessor (&MecUeApplication::m_txTrace),
                                 "ns3::Packet::TracedCallback")
                .AddAttribute ("PingOffset",
                               "Extra waiting time to avoid ping collisions",
                               TimeValue (),
                               MakeTimeAccessor (&MecUeApplication::m_pingOffset),
                               MakeTimeChecker ())
                .AddAttribute ("ServiceOffset",
                               "Extra waiting time to avoid service collisions",
                               TimeValue (),
                               MakeTimeAccessor (&MecUeApplication::m_serviceOffset),
                               MakeTimeChecker ())
                .AddAttribute ("Metric", "Metric used for handover decision making",
                               UintegerValue (),
                               MakeUintegerAccessor (&MecUeApplication::metric),
                               MakeUintegerChecker<uint32_t> ())
                .AddAttribute ("Router", "Router node",
                               PointerValue(),
                               MakePointerAccessor (&MecUeApplication::router),
                               MakePointerChecker<Node> ())
                .AddAttribute ("Logfile", "Name of the file to log to",
                               StringValue(""),
                               MakeStringAccessor (&MecUeApplication::m_filename),
                               MakeStringChecker())
        ;
        return tid;
    }

    MecUeApplication::MecUeApplication ()
    {
        NS_LOG_FUNCTION (this);
        m_sendPingEvent = EventId ();
        m_sendServiceEvent = EventId ();
        m_sendPositionEvent = EventId();
        m_data_request = 0;
        m_data_ping = 0;
        m_data_report = 0;
        m_requestBlocked = false;
        myCellId = 0;
        packetIdCounter = 0;

        serviceRequestCounter = 0;
        serviceResponseCounter = 0;
        pingRequestCounter = 0;
        pingResponseCounter = 0;
        handoverCommandCounter = 0;
        firstRequestCounter = 0;
        sendPositionCounter = 0;
        sendMeasurementReportCounter = 0;
        requestCounter = 0;



    }

    MecUeApplication::~MecUeApplication()
    {
        NS_LOG_FUNCTION (this);



        delete [] m_data_request;
        delete [] m_data_ping;
        delete [] m_data_report;
        m_data_request = 0;
        m_data_ping = 0;
        m_data_report = 0;

    }

    void
    MecUeApplication::DoDispose (void)
    {
        NS_LOG_FUNCTION (this);
        std::map<int, std::tuple<int,int,double>>::iterator delayIt;
        for (delayIt = delays.begin(); delayIt != delays.end(); delayIt++){
            outfile << "delay/" << m_thisIpAddress << "/" << delayIt->first << "/" << std::get<2>(delayIt->second) << std::endl;
        }

        std::map<int,int>::iterator handoverIt;
        for(handoverIt = handovers.begin(); handoverIt != handovers.end(); handoverIt++){
            outfile << "handovers/" << m_thisIpAddress << "/" << handoverIt->first << "/" << handoverIt->second << std::endl;
        }

        std::vector<Vector>::iterator positionIt;
        for(positionIt = handoverPositions.begin(); positionIt != handoverPositions.end(); positionIt++){
            outfile << "handoverPosition/" << m_thisIpAddress << "/" << (*positionIt) << std::endl;
        }


//        outfile << "Sanity check: " << std::to_string(serviceRequestCounter) << "/" << std::to_string(serviceResponseCounter) <<
//                "/" << std::to_string(pingRequestCounter) << "/" << std::to_string(pingResponseCounter) << "/" <<
//                std::to_string(handoverCommandCounter) << "/" << std::to_string(firstRequestCounter) << "/" << std::to_string(sendPositionCounter) <<
//                "/" << std::to_string(sendMeasurementReportCounter) << std::endl;
        outfile.flush();
        outfile.close();

        Application::DoDispose ();
    }

    void
    MecUeApplication::StartApplication (void)
    {
        NS_LOG_FUNCTION (this);

        //Create random variable for variation in scheduling
        randomness = CreateObject<UniformRandomVariable>();
        randomness->SetAttribute("Min", DoubleValue(0.0));
        randomness->SetAttribute("Max", DoubleValue(10.0));

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
        NS_ASSERT(m_allServers.size() == args.size());

        //Make ORC socket
        if (m_socket == 0) {
            TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
            m_socket = Socket::CreateSocket (GetNode (), tid);
            InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_orcPort);
            m_socket->Bind (local);
            m_socket->Connect(InetSocketAddress(m_orcIp, m_orcPort));
        }
        m_socket->SetRecvCallback (MakeCallback (&MecUeApplication::HandleRead, this));
        m_socket->SetAllowBroadcast(false);
        NS_ASSERT(m_socket);

        m_sendServiceEvent = Simulator::Schedule (Seconds(0.5) + MilliSeconds(randomness->GetValue()) + m_serviceOffset, &MecUeApplication::SendFirstRequest, this);
        logServerEvent = Simulator::Schedule(Seconds(295), &MecUeApplication::LogServer, this);
        if (metric == 0){
            m_sendPingEvent = Simulator::Schedule((Seconds(295) + MilliSeconds(randomness->GetValue()) + m_pingOffset), &MecUeApplication::SendPing, this);
        }
        else if (metric == 1){
            m_sendPositionEvent = Simulator::Schedule((Seconds(295) + MilliSeconds(randomness->GetValue()) + m_pingOffset), &MecUeApplication::SendPosition, this);
        }
        else {
            NS_LOG_ERROR("An invalid metric parameter was passed to the UE");
            StopApplication();
        }

        outfile.open(m_filename, std::ios::app);


    }


    void
    MecUeApplication::StopApplication ()
    {
        NS_LOG_FUNCTION (this);

        if (m_socket != 0)
        {
            m_socket->Close ();
            m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
            m_socket = 0;
        }

        std::map<InetSocketAddress, Ptr<Socket>>::iterator it;
        for (it = serverSocketMap.begin(); it != serverSocketMap.end(); ++it){
            Ptr<Socket> tempSocket = it->second;
            tempSocket->Close ();
            tempSocket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
            tempSocket = 0;
        }

        Simulator::Cancel (m_sendPingEvent);
        Simulator::Cancel (m_sendServiceEvent);
        Simulator::Cancel (m_sendPositionEvent);

    }

    void
    MecUeApplication::PrintRoutes(void) {
        NS_LOG_DEBUG("ROUTES in router");
        Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>(&std::cout);
        Ipv4GlobalRoutingHelper ipv4RoutingHelper;
        ipv4RoutingHelper.PrintRoutingTableAt(Seconds(0), router, routingStream);
    }

    void
    MecUeApplication::LogServer(void) {
        outfile << "MEC/" << m_thisIpAddress << "/" << Simulator::Now().GetSeconds() << "/" << m_mecIp << std::endl;
        logServerEvent = Simulator::Schedule(Seconds(1), &MecUeApplication::LogServer, this);
    }

    uint8_t*
    MecUeApplication::GetFilledString (std::string filler, int size) {
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

    bool MecUeApplication::CheckEnb(Ptr<LteEnbNetDevice> enb) {
        NS_LOG_FUNCTION(this);
        bool result = false;

        Ptr<LteEnbRrc> rrc = enb->GetRrc();
        std::map<uint16_t, Ptr<UeManager>> ueMap = rrc->GetUeMap();
        std::map<uint16_t, Ptr<UeManager>>::iterator it;
        for(it = ueMap.begin(); it != ueMap.end(); ++it){
            Ptr<UeManager> manager = it->second;
            uint64_t imsi = manager->GetImsi();
            if(ueImsi == imsi){
                result = true;
                break;
            }
        }
        return result;
    }

    uint16_t MecUeApplication::GetCellId(){
        NS_LOG_FUNCTION(this);

        uint16_t result;

        if (CheckEnb(m_enb0)){
            result = m_enb0->GetCellId();
        }
        else if (CheckEnb(m_enb1)){
            result = m_enb1->GetCellId();
        }
        else if (CheckEnb(m_enb2)){
            result = m_enb2->GetCellId();
        }
        else {
            result = -1;
            NS_LOG_ERROR("No CellID found");
            StopApplication();
        }
        //result must be smaller than the number of eNBs, but larger than 0 (eNB cellIDs start at 1)
        NS_ASSERT(result <= 3 && result > 0);
        if (result != myCellId){
            NS_LOG_DEBUG("Node " << m_thisNode->GetId() << " switching from cell id " << myCellId << " to cell id " << result);
            outfile << Simulator::Now().GetSeconds() << "/" << m_thisIpAddress << "/ Radio handover" << std::endl;
            myCellId = result;
        }
        return result;

    }
    //First service request sets special flag so that the MEC will add this UE to its client list
    void
    MecUeApplication::SendFirstRequest (void) {
        NS_LOG_FUNCTION(this);

        //Create packet payload
        std::string fillString = "8/" + std::to_string(GetCellId()) + "/" + std::to_string(packetIdCounter) + "/";
        std::regex re("8/[0-9]+/[0-9]+/");
        std::smatch match;
        NS_ASSERT(std::regex_search(fillString, match, re));
        uint8_t *buffer = GetFilledString(fillString, m_size);

        //Create packet
        Ptr<Packet> p = Create<Packet> (buffer, m_size);
        free(buffer);
        // call to the trace sinks before the packet is actually sent,
        // so that tags added to the packet can be sent as well
        m_txTrace (p);
        m_socket->SendTo(p, 0, InetSocketAddress(m_mecIp, m_mecPort));
//        outfile << Simulator::Now().GetSeconds() << " - " << m_thisIpAddress << ", sent first service request with ID " <<  packetIdCounter << " to " << m_mecIp << std::endl;

        m_sendServiceEvent = Simulator::Schedule (m_serviceInterval, &MecUeApplication::SendServiceRequest, this);

        openServiceRequests[packetIdCounter] = Simulator::Now();
//        outfile << Simulator::Now().GetSeconds() << " - " << m_thisIpAddress << ", sent service request (1) with ID " << packetIdCounter << std::endl;
        packetIdCounter++;

        firstRequestCounter ++;
    }

    void
    MecUeApplication::SendServiceRequest (void) {

        NS_ASSERT (m_sendServiceEvent.IsExpired ());

        if (Simulator::Now() < m_noSendUntil){
            m_requestBlocked = true;
            requestBlockedTime = Simulator::Now();
            Time waitingTime = m_noSendUntil - Simulator::Now();
            m_sendServiceEvent = Simulator::Schedule (waitingTime, &MecUeApplication::SendServiceRequest, this);

        }
        else {
            NS_LOG_FUNCTION(this);
            NS_ASSERT(Simulator::Now() >= m_noSendUntil);
            if (m_requestBlocked){
                //UE has tried to send during no-send period
                openServiceRequests[packetIdCounter] = requestBlockedTime;
//                outfile << requestBlockedTime.GetSeconds() << " - " << m_thisIpAddress << ", sent service request (2) with ID " << packetIdCounter << std::endl;
            }
            else {
                openServiceRequests[packetIdCounter] = Simulator::Now();
//                outfile << Simulator::Now().GetSeconds() << " - " << m_thisIpAddress << ", sent service request (3) with ID " << packetIdCounter << std::endl;
            }
            //Create packet payload
            std::string fillString = "1/" + std::to_string(GetCellId()) + "/" + std::to_string(packetIdCounter) + "/";

            std::regex re("1/[0-9]+/[0-9]+/");
            std::smatch match;
            NS_ASSERT(std::regex_search(fillString, match, re));
            uint8_t *buffer = GetFilledString(fillString, m_size);


            //Create packet
            Ptr<Packet> p = Create<Packet> (buffer, m_size);
            free(buffer);
            // call to the trace sinks before the packet is actually sent,
            // so that tags added to the packet can be sent as well
            m_txTrace (p);
            m_socket->SendTo(p, 0, InetSocketAddress(m_mecIp, m_mecPort));
//            outfile << Simulator::Now().GetSeconds() << " - " << m_thisIpAddress << ", sent service request with ID " <<  packetIdCounter << " to " << m_mecIp << std::endl;
            Time waitingTime = m_serviceInterval + MilliSeconds(randomness->GetValue());
            m_sendServiceEvent = Simulator::Schedule (waitingTime, &MecUeApplication::SendServiceRequest, this);

            m_requestBlocked = false;
            packetIdCounter++;
            serviceRequestCounter++;
        }
    }
    void
    MecUeApplication::SendPosition(void){
        NS_LOG_FUNCTION(this);

        if (Simulator::Now() < m_noSendUntil){
            m_sendPositionEvent = Simulator::Schedule(m_noSendUntil, &MecUeApplication::SendPosition, this);
        }
        else {
            NS_ASSERT(Simulator::Now() >= m_noSendUntil);
            Ptr<MobilityModel> myMobility = m_thisNode->GetObject<MobilityModel>();
            Vector myPosition = myMobility->GetPosition();

            std::stringstream ss;
            std::stringstream os;
            m_mecIp.Print(os);
            ss << os.rdbuf();
            std::string mecString = ss.str();

            std::string fillString = "9/" + mecString + "/" + std::to_string(myPosition.x) + "/" + std::to_string(myPosition.y) + "/";
            std::regex re("9/([0-9]+\\.[0-9+]\\.[0-9]+\\.[0-9]+)/\\d+\\.\\d+/\\d+\\.\\d+/");
            std::smatch match;
            NS_ASSERT(std::regex_search(fillString, match, re));
            uint8_t *buffer = GetFilledString(fillString, m_size);

            //Create packet
            Ptr<Packet> p = Create<Packet> (buffer, m_size);
            free(buffer);
            m_txTrace (p);
            m_socket->SendTo(p, 0, InetSocketAddress(m_orcIp, m_orcPort));

            m_sendPositionEvent = Simulator::Schedule(m_pingInterval + MilliSeconds(randomness->GetValue()), &MecUeApplication::SendPosition, this);
            sendPositionCounter++;

        }
    }

    void
    MecUeApplication::SendIndividualPing(Ptr<Packet> p, InetSocketAddress mec, int packetId){
        NS_LOG_FUNCTION(this);
        m_socket->SendTo(p, 0, mec);
//        outfile << Simulator::Now().GetSeconds() << " - " << m_thisIpAddress << ", sent ping request with ID " <<  packetId << " to " << mec.GetIpv4() << std::endl;
        pingRequestCounter++;
    }

    void
    MecUeApplication::SendPing (void)
    {
        NS_LOG_FUNCTION (this);
//        m_pingSent.clear();
        int mecCounter = 0;
        std::list<int> batchIds;

        if (Simulator::Now() < m_noSendUntil){
            m_requestSent = Simulator::Now();
            m_requestBlocked = true;
            m_sendPingEvent = Simulator::Schedule(m_noSendUntil - Simulator::Now(), &MecUeApplication::SendPing, this);
//            outfile << Simulator::Now().GetSeconds() << " - " << m_thisIpAddress << ", sendPing blocked; no-send period for another " << (m_noSendUntil - Simulator::Now()).GetMilliSeconds() << " ms." << std::endl;
        }
        else {
            NS_ASSERT(Simulator::Now() >= m_noSendUntil);
            for (InetSocketAddress mec: m_allServers) {
                //Create packet payload
                std::string fillString = "2/";
                fillString.append(std::to_string(GetCellId()) + "/" + std::to_string(packetIdCounter) + "/");
                std::regex re("2/[0-9]+/[0-9]+/");
                std::smatch match;
                NS_ASSERT(std::regex_search(fillString, match, re));
                uint8_t *buffer = GetFilledString(fillString, m_size);

                //Create packet
                Ptr <Packet> p = Create<Packet>(buffer, m_size);
                free(buffer);
                // call to the trace sinks before the packet is actually sent,
                // so that tags added to the packet can be sent as well
                m_txTrace(p);

                //Determine correct server socket and send
                Simulator::Schedule(MicroSeconds(10 * mecCounter), &MecUeApplication::SendIndividualPing, this, p, mec, packetIdCounter);
                pingSendTimes[packetIdCounter] = Simulator::Now() + MicroSeconds(10 * mecCounter);
                batchIds.push_back(packetIdCounter);
                packetIdCounter++;
            }
            openPingRequests.push_back(std::make_pair(batchIds, std::map<Ipv4Address,int64_t>()));
            Time sendTime = m_pingInterval + MilliSeconds(randomness->GetValue());
            m_sendPingEvent = Simulator::Schedule(sendTime, &MecUeApplication::SendPing, this);

            m_handleTimeoutEvent = Simulator::Schedule(m_timeoutInterval, &MecUeApplication::HandlePingTimeout, this, batchIds);
            requestCounter ++;
        }
    }

    void
    MecUeApplication::HandlePingTimeout(std::list<int> batchIds){
        std::map<Ipv4Address, int64_t> measurementReport;
        std::vector<std::pair<std::list<int>, std::map<Ipv4Address, int64_t>>>::iterator mapIt;
        for(mapIt = openPingRequests.begin(); mapIt != openPingRequests.end(); mapIt++){
            if(mapIt->first == batchIds){
                measurementReport = mapIt->second;

                if(measurementReport.size() < m_allServers.size()){
                    std::vector<InetSocketAddress>::iterator it;
                    for(it = m_allServers.begin(); it != m_allServers.end(); it++) {
                        Ipv4Address serverAddress = it->GetIpv4();
                        std::map<Ipv4Address, int64_t>::iterator it2 = measurementReport.find(serverAddress);
                        if(it2 == measurementReport.end()){
                            //No entry for this server, add one
//                            outfile << Simulator::Now() << " - " << m_thisIpAddress << ", ping timeout" << std::endl;
                            measurementReport[serverAddress] = m_timeoutInterval.GetMilliSeconds();
                        }
                    }
                    Simulator::Schedule(MilliSeconds(randomness->GetValue()), &MecUeApplication::SendMeasurementReport, this, measurementReport);
                }
                break;
            }
        }
    }

    void
    MecUeApplication::SendMeasurementReport (std::map<Ipv4Address,int64_t> report){
        NS_LOG_FUNCTION (this);
        //Create packet payload
        //Convert current mec address into string
        std::stringstream ss;
        std::stringstream os;
        m_mecIp.Print(os);
        ss << os.rdbuf();
        std::string mecString = ss.str();

        std::string payload = "3/" + mecString + "/";

        for (std::map<Ipv4Address,int64_t>::iterator it = report.begin(); it != report.end(); ++it){
            //Convert Address into string
            Ipv4Address addr = it->first;
            std::stringstream ss2;
            std::stringstream os2;
            addr.Print(os2);
            ss2 << os2.rdbuf();
            std::string addrString = ss2.str();

            payload.append(addrString + "!" + std::to_string(it->second) + "!");
        }

        payload.push_back('/'); //! is separator character for the measurementReport
        std::regex re("3/([0-9]+\\.[0-9+]\\.[0-9]+\\.[0-9]+)/([0-9]+\\.[0-9+]\\.[0-9]+\\.[0-9]+![0-9]+!){" + std::to_string(m_allServers.size()) + "}/");
        std::smatch match;
        NS_ASSERT(std::regex_search(payload, match, re));

        uint8_t *buffer = GetFilledString(payload, m_size);
        //Create packet
        Ptr<Packet> p = Create<Packet> (buffer, m_size);
        free(buffer);
        // call to the trace sinks before the packet is actually sent,
        // so that tags added to the packet can be sent as well
        m_txTrace (p);
        m_socket->SendTo(p, 0, InetSocketAddress(m_orcIp, m_orcPort));
//        outfile << Simulator::Now().GetSeconds() << " - " << m_thisIpAddress << ", sent measurement report to " << m_orcIp << std::endl;
        sendMeasurementReportCounter++;
    }

    void
    MecUeApplication::HandleRead (Ptr<Socket> socket)
    {
        NS_LOG_FUNCTION (this << socket);
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom (from)))
        {
            if (InetSocketAddress::IsMatchingType (from))
            {
                InetSocketAddress inet_from = InetSocketAddress::ConvertFrom(from);
                Ipv4Address from_ipv4 = inet_from.GetIpv4();

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

                switch(std::stoi(args[0])){
                    case 1: {
                        //This is a service response from my MEC
                        int packetId = std::stoi(args[2]);
                        int64_t delay = (Simulator::Now() - openServiceRequests[packetId]).GetMilliSeconds();
//                        outfile << Simulator::Now().GetSeconds() << " - " << m_thisIpAddress << ", Service response with ID " << packetId << ", packet send time: " << openServiceRequests[packetId].GetSeconds() << ", measured delay: " << delay << " ms. " << std::endl;
                        openServiceRequests.erase(packetId);

                        if (Simulator::Now().GetSeconds() >= 300.0){
                            std::tuple<int,int,double> runningMean = delays[int(Simulator::Now().GetSeconds())];
                            int newTotalDelay = std::get<0>(runningMean) + delay;
                            int newCount = std::get<1>(runningMean) + 1;
                            int newMean = double(newTotalDelay)/double(newCount);
//                            outfile << Simulator::Now().GetSeconds() << " - " << m_thisIpAddress << ", Mean calculations. newTotalDelay: " << newTotalDelay << ", newCount: " << newCount << ", newMean: " << newMean << std::endl;
                            std::tuple<int,int,double> newEntry = std::make_tuple(newTotalDelay, newCount, newMean);
                            delays[int(Simulator::Now().GetSeconds())] = newEntry;
                        }

                        serviceResponseCounter++;
                        break;
                    }
                    case 2: {
                        //This is a ping response from a MEC

                        //On receive, add <address, time> entry to measurement report that corresponds with the packetId
                        int packetId = std::stoi(args[2]);
                        std::vector<std::pair<std::list<int>, std::map<Ipv4Address, int64_t>>>::iterator ping_it;
                        for(ping_it = openPingRequests.begin(); ping_it != openPingRequests.end(); ping_it++){
                            std::list<int> batchIds = ping_it->first;
                            for (std::list<int>::iterator i = batchIds.begin(); i != batchIds.end(); i++){
                            }
                            std::map<Ipv4Address, int64_t> measurementReport = ping_it->second;

                            if(std::count(batchIds.begin(), batchIds.end(), packetId)){
                                //packetId is in this list
                                int64_t delay = (Simulator::Now() - pingSendTimes[packetId]).GetMilliSeconds();
                                measurementReport[from_ipv4] = delay;
                                std::replace(openPingRequests.begin(), openPingRequests.end(), *ping_it, std::make_pair(ping_it->first, measurementReport));

                                //If measurementreport is now complete, send it to ORC.
                                if(measurementReport.size() == m_allServers.size()){
                                    //There is a measurement for each mec, i.e. the report is now complete and ready to be sent to the ORC
                                    Simulator::Schedule(MilliSeconds(randomness->GetValue()), &MecUeApplication::SendMeasurementReport, this, measurementReport);

                                    //Remove this entry and any older ones that are now outdated from openPingRequests.
                                    openPingRequests.erase(openPingRequests.begin(), std::find(openPingRequests.begin(), openPingRequests.end(), *ping_it));
                                    openPingRequests.erase(openPingRequests.begin());

                                    //Remove pingSendTimes entries
                                    for(std::list<int>::iterator it2 = batchIds.begin(); it2 != batchIds.end(); it2++){
                                        pingSendTimes.erase(*it2);
                                    }
                                 }
                                break;
                            }
                        }
                        pingResponseCounter++;
                        break;
                    }
                    case 6: {
                        //Handover command
                        //Get new MEC address
                        std::string addressString = args[1];
                        std::string portString = args[2];

                        Ipv4Address newAddress = Ipv4Address();
                        newAddress.Set(addressString.c_str());

                        uint16_t newPort = std::stoi(portString);

                        //Update MEC address
                        //Log handover, time, my address, old MEC, new MEC and no-send period duration in ms

                        if (Simulator::Now().GetSeconds() >= 300.0) {
                            int numberOfHandovers = handovers[int(Simulator::Now().GetSeconds())];
                            handovers[int(Simulator::Now().GetSeconds())] = numberOfHandovers + 1;
                            Ptr<MobilityModel> myMobility = m_thisNode->GetObject<MobilityModel>();
                            handoverPositions.push_back(myMobility->GetPosition());
//                            outfile << Simulator::Now().GetSeconds() << " - " << m_thisIpAddress << ", handovers +1" << std::endl;
                        }


                        m_mecIp = newAddress;
                        m_mecPort = newPort;

                        //Set no-send period
                        m_noSendUntil = Simulator::Now() + MilliSeconds(stoi(args[3]));
                        handoverCommandCounter++;
                        break;
                    }
                }

                delete[] buffer;
            }
        }
    }

} // Namespace ns3
