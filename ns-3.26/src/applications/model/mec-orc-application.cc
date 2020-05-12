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
#include <regex>
#include <math.h>
#include <iostream>

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
                .AddAttribute ("DistanceThreshold", "Minimum distance in meters before handover can be considered",
                               UintegerValue (),
                               MakeUintegerAccessor (&MecOrcApplication::distance_threshold),
                               MakeUintegerChecker<uint32_t> ())
                .AddAttribute ("MecPositions", "String of the positions of all MECs",
                               StringValue(),
                               MakeStringAccessor (&MecOrcApplication::mecPositions),
                               MakeStringChecker())
                .AddTraceSource ("Tx", "A new packet is created and is sent",
                                 MakeTraceSourceAccessor (&MecOrcApplication::m_txTrace),
                                 "ns3::Packet::TracedCallback")
                .AddAttribute ("Logfile", "Name of the file to which we log",
                               StringValue(""),
                               MakeStringAccessor (&MecOrcApplication::m_filename),
                               MakeStringChecker())
        ;
        return tid;
    }

    MecOrcApplication::MecOrcApplication () {
        NS_LOG_FUNCTION (this);
        m_socket = 0;
        m_sendUeEvent = EventId ();
        m_sendMecEvent = EventId ();
        m_data_ue = 0;
        m_data_mec = 0;

        ueHandoverCounter = 0;
        mecHandoverCounter = 0;
        measurementReportCounter = 0;
        responseTimeUpdateCounter = 0;
        locationUpdateCounter = 0;

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

        outfile << "Sanity check: " << std::to_string(ueHandoverCounter) << "/" << std::to_string(mecHandoverCounter) <<
            "/" << std::to_string(measurementReportCounter) << "/" << std::to_string(responseTimeUpdateCounter) << "/" <<
            std::to_string(locationUpdateCounter) << std::endl;
        outfile.close();

        Application::DoDispose ();
    }

    void
    MecOrcApplication::StartApplication (void)
    {
        NS_LOG_FUNCTION (this);

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
            std::regex re("([0-9]+\\.[0-9+]\\.[0-9]+\\.[0-9]+)");
            std::smatch match;
            NS_ASSERT(std::regex_search(args[i], match, re));
            Ipv4Address ipv4 = Ipv4Address();
            std::string addrString = args[i];
            char cstr[addrString.size() + 1];
            addrString.copy(cstr, addrString.size()+1);
            cstr[addrString.size()] = '\0';
            ipv4.Set(cstr);
            m_allServers.push_back(InetSocketAddress(ipv4, 1000));
        }
        NS_ASSERT(args.size() == m_allServers.size());

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
            std::regex re("([0-9]+\\.[0-9+]\\.[0-9]+\\.[0-9]+)");
            std::smatch match;
            NS_ASSERT(std::regex_search(args2[i], match, re));
            Ipv4Address ipv4 = Ipv4Address();
            std::string addrString = args2[i];
            char cstr[addrString.size() + 1];
            addrString.copy(cstr, addrString.size()+1);
            cstr[addrString.size()] = '\0';
            ipv4.Set(cstr);
            m_allUes.push_back(InetSocketAddress(ipv4, 1000));
        }


        std::vector<std::string> args3;
        std::string tempString3;
        for (int i = 0 ; i < int(mecPositions.length()); i++){
            char c = mecPositions[i];
            if(c == '/'){
                args3.push_back(tempString3);
                tempString3 = "";
            }
            else{
                tempString3.push_back(c);
            }
        }
        for(int i = 0; i< int(args3.size()) ; i++){
            std::regex re("\\d+,\\d+,\\d+");
            std::smatch match;
            NS_ASSERT(std::regex_search(args3[i], match, re));
            std::string currentStringVector = args3[i];

            std::vector<std::string> args4;
            std::string tempString4;
            for(int j = 0; j<int(currentStringVector.size()); j++){
                char c = currentStringVector[j];
                if(c == ','){
                    args4.push_back(tempString4);
                    tempString4 = "";
                }
                else{
                    tempString4.push_back(c);
                }
            }
            Vector mecPosition = Vector(std::stoi(args4[0]), std::stoi(args4[1]), std::stoi(args4[2]));
            allMecPositions.push_back(mecPosition);
        }




        NS_ASSERT(args2.size() == m_allUes.size());

        TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
        Ptr<Socket> tempSocket;
        tempSocket = Socket::CreateSocket (GetNode(), tid);
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 1000);
        tempSocket->Bind(local);
        tempSocket->Connect(InetSocketAddress("0.0.0.0", 1000));
        tempSocket->SetRecvCallback(MakeCallback(&MecOrcApplication::HandleRead, this));
        tempSocket->SetAllowBroadcast(false);
        m_socket = tempSocket;

        outfile.open(m_filename, std::ios::app);
    }

    void
    MecOrcApplication::StopApplication () {
        NS_LOG_FUNCTION (this);

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
        int currentValue = responseTimes[currentMecAddress];
        int newValue = responseTimes[newMecAddress];



        int handoverTime = -1;
        if ((currentValue != -1) && (newValue != -1)){
            handoverTime = currentValue + newValue;
        }
        else {
            outfile << "At least one param not found: " << std::to_string(currentValue) <<  "/" << std::to_string(newValue) << std::endl;
        }

        std::string fillString = "6/" + addrString + "/" + portString + "/" + std::to_string(handoverTime) + "/";
        std::regex re("6/([0-9]+\\.[0-9+]\\.[0-9]+\\.[0-9]+)/[1-9][0-9]*/[0-9]+/");
        std::smatch match;
        NS_ASSERT(std::regex_search(fillString, match, re));

        uint8_t *buffer = GetFilledString(fillString, m_packetSize);


        //Create packet
        Ptr<Packet> p = Create<Packet> (buffer, m_packetSize);
        // call to the trace sinks before the packet is actually sent,
        // so that tags added to the packet can be sent as well
        free(buffer);
        m_txTrace(p);

        m_socket->SendTo(p, 0, InetSocketAddress(ueAddress.GetIpv4(), 1000));
        ueHandoverCounter++;
    }

    void
    MecOrcApplication::SendMecHandover (InetSocketAddress ueAddress, InetSocketAddress newMecAddress, InetSocketAddress currentMecAddress)
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
        std::string newMecAddrString = ss2.str();

        //Create packet payload
        std::string fillString = "4/" + ueAddrString + "/" + std::to_string(ueAddress.GetPort()) + "/" + newMecAddrString + "/" + std::to_string(newMecAddress.GetPort()) + "/";
        std::regex re("4/([0-9]+\\.[0-9+]\\.[0-9]+\\.[0-9]+)/[1-9][0-9]*/([0-9]+\\.[0-9+]\\.[0-9]+\\.[0-9]+)/[1-9][0-9]*/");
        std::smatch match;
        NS_ASSERT(std::regex_search(fillString, match, re));
        uint8_t *buffer = GetFilledString(fillString, m_packetSize);

        //Create packet
        Ptr <Packet> p = Create<Packet>(buffer, m_packetSize);
        // call to the trace sinks before the packet is actually sent,
        // so that tags added to the packet can be sent as well
        free(buffer);
        m_txTrace(p);

        m_socket->SendTo(p, 0, currentMecAddress);
        mecHandoverCounter++;
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
                        //Metric is delay and measurement report has come in
                        //This is a measurement report from a UE
                        Ipv4Address currentIpv4 = Ipv4Address();
                        currentIpv4.Set(args[1].c_str());

                        InetSocketAddress currentMecAddr = InetSocketAddress(currentIpv4, 1000);

                        //Make map from args[2] content
                        std::string mapString = args[2];
                        std::string tempString2;
                        std::vector<std::string> map_args;
                        std::map<Ipv4Address, int> delayMap;
                        for (int i = 0 ; i < int(mapString.length()); i++){
                            char c = mapString[i];
                            if(c == '!'){
                                map_args.push_back(tempString2);
                                tempString2 = "";
                            }
                            else{
                                tempString2.push_back(c);
                            }
                        }
                        NS_ASSERT(map_args.size() == (2*m_allServers.size()));
                        for(int i = 0; i < int(map_args.size()); i = i+2){
                            Ipv4Address address =  Ipv4Address();
                            address.Set((map_args[i]).c_str());
                            delayMap.insert(std::pair<Ipv4Address, int>(address, stoi(map_args[i+1])));
                        }
                        NS_ASSERT(delayMap.size() == m_allServers.size());

                        int currentDelay = delayMap.find(currentIpv4)->second;
                        NS_ASSERT(currentDelay != 0);

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
                                    triggerEquation = (delay<optimalDelay && (currentDelay > (int)delay_threshold));
                                    break;
                                case 3:
                                    triggerEquation = delay<optimalDelay && (delay < currentDelay*(1-hysteresis)) && (currentDelay > (int)delay_threshold);
                                    break;
                                default:
                                    NS_LOG_ERROR("Unsupported trigger type");
                                    triggerEquation = false;
                                    StopApplication();
                            }
                            if (triggerEquation){
                                optimalDelay = delay;
                                bestMec = address;
                            }
                        }

                        if (bestMec != currentIpv4){
                            //Initiate a handover to bestMec
                            InetSocketAddress ueAddress = InetSocketAddress::ConvertFrom(from);
                            //Find InetSocketAddress for bestMec
                            bool found = false;

                            std::map<InetSocketAddress, int>::iterator it;
                            for (it = responseTimes.begin(); it != responseTimes.end() && !found; ++it){
                                InetSocketAddress current = it->first;
                                Ipv4Address currentIpv4 = current.GetIpv4();

                                if(currentIpv4 == bestMec){
                                    InetSocketAddress newMecAddress = current;
                                    SendUeHandover(ueAddress, newMecAddress, currentMecAddr);
                                    SendMecHandover(ueAddress, newMecAddress, currentMecAddr);
                                    found = true;
                                }
                            }
                        }
                        measurementReportCounter++;
                        break;
                    }
                    case 5: {
                        // Response time update

                        int newResponseTime = stoi(args[1]);
                        InetSocketAddress sendAddress = InetSocketAddress::ConvertFrom(from);

                        if (responseTimes.find(sendAddress) != responseTimes.end()) {
                            //Element already exists in map
                            responseTimes[sendAddress] = newResponseTime;

                        } else {
                            //New MEC, add entry in map
                            responseTimes.insert(std::pair<InetSocketAddress, int>(sendAddress, newResponseTime));

                        }
                        responseTimeUpdateCounter++;
                        break;
                    }
                    case 9: {
                        //Location update
                        //Metric is distance; position update from a UE has just come in
                        Vector uePosition = Vector(std::stoi(args[2]), std::stoi(args[3]),0);

                        //Calculate distances
                        std::vector<double> distances;
                        for(int i = 0; i<int(allMecPositions.size()); i++){
                            Vector mecPosition = allMecPositions[i];
                            double distanceSum = pow((double)(mecPosition.x - uePosition.x), 2.0) + pow((double)(mecPosition.y - uePosition.y), 2.0);
                            double distance = sqrt(distanceSum);
                            distances.push_back(distance);
                        }

                        //Get UEs current MEC address index
                        Ipv4Address currentIpv4 = Ipv4Address();
                        currentIpv4.Set(args[1].c_str());

                        InetSocketAddress currentMecAddr = InetSocketAddress(currentIpv4, 1000);

                        //Get UEs current distance from MEC
                        double currentDistance = 0;
                        int currentMecIndex = -1;

                        for(int i  = 0; i < int(m_allServers.size()); i++) {
                            if(m_allServers[i].GetIpv4() == currentIpv4) {
                                currentDistance = distances[i];
                                currentMecIndex = i;
                                break;
                            }
                        }
                        NS_ASSERT(currentDistance != 0);
                        NS_ASSERT(currentMecIndex != -1);

                        double optimalDistance = currentDistance;
                        int bestMecIndex = currentMecIndex;

                        //Find best MEC
                        for (int i = 0 ; i<int(distances.size()); i++){
                            double distance = distances[i];

                            bool triggerEquation;

                            switch(trigger){
                                case 0:
                                    triggerEquation = (distance<optimalDistance);
                                    break;
                                case 1:
                                    triggerEquation = (distance<optimalDistance && (distance < currentDistance*(1-hysteresis)));
                                    break;
                                case 2:
                                    triggerEquation = (distance<optimalDistance && (currentDistance > distance_threshold));
                                    break;
                                case 3:
                                    triggerEquation = distance<optimalDistance && (distance < currentDistance*(1-hysteresis)) && (currentDistance > distance_threshold);
                                    break;
                                default:
                                    NS_LOG_ERROR("Unsupported trigger type");
                                    triggerEquation = false;
                                    StopApplication();
                            }
                            if (triggerEquation){
                                optimalDistance = distance;
                                bestMecIndex = i;
                            }
                        }

                        if (bestMecIndex != currentMecIndex){
                            //Initiate a handover to bestMec
                            InetSocketAddress ueAddress = InetSocketAddress::ConvertFrom(from);

                            InetSocketAddress newMecAddress = m_allServers[bestMecIndex];
                            SendUeHandover(ueAddress, newMecAddress, currentMecAddr);
                            SendMecHandover(ueAddress, newMecAddress, currentMecAddr);
                        }
                        locationUpdateCounter++;
                        break;
                    }
                    default:
                        NS_LOG_ERROR("Non-existent message type received");
                        StopApplication();
                }

                delete[] buffer;
            }
        }
    }
} // Namespace ns3
