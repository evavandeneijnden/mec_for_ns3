///* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
///*
// * Copyright 2007 University of Washington
// *
// * This program is free software; you can redistribute it and/or modify
// * it under the terms of the GNU General Public License version 2 as
// * published by the Free Software Foundation;
// *
// * This program is distributed in the hope that it will be useful,
// * but WITHOUT ANY WARRANTY; without even the implied warranty of
// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// * GNU General Public License for more details.
// *
// * You should have received a copy of the GNU General Public License
// * along with this program; if not, write to the Free Software
// * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
// */
//#include "ns3/log.h"
//#include "ns3/ipv4-address.h"
//#include "ns3/ipv6-address.h"
//#include "ns3/nstime.h"
//#include "ns3/inet-socket-address.h"
//#include "ns3/inet6-socket-address.h"
//#include "ns3/socket.h"
//#include "ns3/simulator.h"
//#include "ns3/socket-factory.h"
//#include "ns3/packet.h"
//#include "ns3/uinteger.h"
//#include "ns3/trace-source-accessor.h"
//#include "mec-orc-application.h"
//#include <sstream>
//
//namespace ns3 {
//
//    NS_LOG_COMPONENT_DEFINE ("MecOrcApplication");
//
////    NS_OBJECT_ENSURE_REGISTERED (MecOrcApplication);
//
//    uint8_t *m_data_ue;
//    uint8_t *m_data_mec;
//
//    char TRIGGER = 'O'; //Valid options are "O" for optimal, "H" for hysteresis, "T" for threshold and "TH" for threshold AND hysteresis
//    double HYSTERESIS = 0.3; //Value between 0 and 1 for setting the percentage another candidate's performance must be better than the current
//    int DELAY_THRESHOLD = 20; //If delay is higher than threshold, switch. In ms.
//    std::map<InetSocketAddress, int> waitingTimes; //Current waiting times for each MEC by InetSocketAddress. In ms.
//
//    TypeId
//    MecOrcApplication::GetTypeId (void)
//    {
//        static TypeId tid = TypeId ("ns3::MecOrcApplication")
//                .SetParent<Application> ()
//                .SetGroupName("Applications")
//                .AddConstructor<MecOrcApplication> ()
//                .AddAttribute ("MaxPackets",
//                               "The maximum number of packets the application will send",
//                               UintegerValue (100),
//                               MakeUintegerAccessor (&MecOrcApplication::m_count),
//                               MakeUintegerChecker<uint32_t> ())
//                .AddAttribute ("PacketSize", "Size of echo data in outbound packets",
//                               UintegerValue (100),
//                               MakeUintegerAccessor (&MecOrcApplication::m_packetSize),
//                               MakeUintegerChecker<uint32_t> ())
//                .AddTraceSource ("Tx", "A new packet is created and is sent",
//                                 MakeTraceSourceAccessor (&MecOrcApplication::m_txTrace),
//                                 "ns3::Packet::TracedCallback")
//        ;
//        return tid;
//    }
//
//    MecOrcApplication::MecOrcApplication ()
//{
//    NS_LOG_FUNCTION (this);
//    m_sent = 0;
//    m_socket = 0;
//    m_sendUeEvent = EventId ();
//    m_sendMecEvent = EventId ();
//    m_data_ue = 0;
//    m_data_mec = 0;
//}
//
//MecOrcApplication::~MecOrcApplication()
//{
//NS_LOG_FUNCTION (this);
//m_socket = 0;
//
//delete [] m_data_ue;
//delete [] m_data_mec;
//m_data_ue = 0;
//m_data_mec = 0;
//}
//
//void
//MecOrcApplication::DoDispose (void)
//{
//    NS_LOG_FUNCTION (this);
//    Application::DoDispose ();
//}
//
//void
//MecOrcApplication::StartApplication (void)
//{
//    NS_LOG_FUNCTION (this);
//
//    if (m_socket == 0)
//    {
//        TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
//        m_socket = Socket::CreateSocket (GetNode (), tid);
//    }
//
//    m_socket->SetRecvCallback (MakeCallback (&MecOrcApplication::HandleRead, this));
//    m_socket->SetAllowBroadcast (true);
//}
//
//void
//MecOrcApplication::StopApplication ()
//{
//    NS_LOG_FUNCTION (this);
//
//    if (m_socket != 0)
//    {
//        m_socket->Close ();
//        m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
//        m_socket = 0;
//    }
//
//}
//
//void
//MecOrcApplication::SetFill (uint8_t *fill, uint32_t fillSize, uint8_t *dest)
//{
//    //dest can either be m_data_request or m_data_ping or m_data_report
//    NS_LOG_FUNCTION (this << fill << fillSize << m_packetSize);
//
//    if (fillSize >= m_packetSize)
//    {
//        memcpy (dest, fill, m_packetSize);
//        m_size = m_packetSize;
//        return;
//    }
//
//    //
//    // Do all but the final fill.
//    //
//    uint32_t filled = 0;
//    while (filled + fillSize < m_packetSize)
//    {
//        memcpy (&dest[filled], fill, fillSize);
//        filled += fillSize;
//    }
//
//    //
//    // Last fill may be partial
//    //
//    memcpy (&dest[filled], fill, m_packetSize - filled);
//
//    //
//    // Overwrite packet size attribute.
//    //
//    m_size = m_packetSize;
//}
//
//void
//MecOrcApplication::SendUeHandover (InetSocketAddress ueAddress, InetSocketAddress newMecAddress) {
//
//    m_socket->Bind();
//    m_socket->Connect (ueAddress);
//
//    //Convert Address into string
//    Ipv4Address addr = newMecAddress.GetIpv4();
//    std::stringstream ss;
//    std::stringstream os;
//    addr.Print(os);
//    ss << os.rdbuf();
//    std::string addrString = ss.str();
//
//    //Create packet payload
//
//    std::stringstream ss2;
//    ss2 << (newMecAddress.GetPort());
//    std::string portString = ss2.str();
//
//    std::string fillString = "6/" + addrString + "/" + portString + "/";
//    uint8_t *buffer = fillString.c_str();
//
//    SetFill(buffer, m_packetSize, m_data_request);
//
//    //Create packet
//    Ptr<Packet> p = Create<Packet> (m_data_ue, m_packetSize);
//    // call to the trace sinks before the packet is actually sent,
//    // so that tags added to the packet can be sent as well
//    m_txTrace (p);
//    m_socket->Send (p);
//
//    ++m_sent;
//
//    if (m_sent < m_count)
//    {
//        m_sendUeEvent = Simulator::Schedule (m_serviceInterval, &MecOrcApplication::SendServiceRequest, this);
//    }
//
//}
//
//void
//MecOrcApplication::SendMecHandover (InetSocketAddress ueAddress, InetSocketAddress newMecAddress)
//{
//    m_socket->Bind();
//    m_socket->Connect (newMecAddress);
//
//    //Convert ueAddress into string
//    Ipv4Address ueAddr = ueAddress.GetIpv4();
//    std::stringstream ss;
//    std:ostream os;
//    ueAddr.Print(os);
//    ss << os.rdbuf();
//    std:string ueAddrString = ss.str();
//
//    //Convert newMecAddress into string
//    Ipv4Address newMecAddr = newMecAddress.GetIpv4();
//    newMecAddr.Print(os);
//    ss << os.rdbuf();
//    std:string newMecAddrString = ss.str();
//
//    //Create packet payload
//    std::string fillString = "4/" + ueAddrString + "/" + str(ueAddress.GetPort()) + "/" + newMecAddrString + "/" + str(newMecAddress.GetPort()) + "/";
//    uint8_t *buffer = fillString.c_str();
//    SetFill(buffer, m_packetSize, m_data_ping);
//
//    //Create packet
//    Ptr <Packet> p = Create<Packet>(m_data_ping, m_packetSize);
//    // call to the trace sinks before the packet is actually sent,
//    // so that tags added to the packet can be sent as well
//    m_txTrace(p);
//    m_socket->Send(p);
//
//    ++m_sent;
//
//    if (m_sent < m_count) {
//        m_sendServiceEvent = Simulator::Schedule(m_serviceInterval, &MecOrcApplication::SendServiceRequest, this);
//    }
//
//    m_requestBlocked = false;
//}
//
//void
//MecOrcApplication::HandleRead (Ptr<Socket> socket)
//{
//    NS_LOG_FUNCTION (this << socket);
//    Ptr<Packet> packet;
//    Address from;
//    while ((packet = socket->RecvFrom (from)))
//    {
//        if (InetSocketAddress::IsMatchingType (from))
//        {
//            InetSocketAddress inet_from = InetSocketAddress::ConvertFrom(from);
//            Ipv4Address from_ipv4 = inet_from.GetIpv4();
//
//            //Split packet into args by "/" character
//            std::vector<std::string> args;
//            std::string tempString;
//            for (int i = 0 ; i < payloadString.length(); i++){
//                char c = payloadString[i];
//                if(c == "/"){
//                    args.push_back(tempString);
//                    tempString = "";
//                }
//                else{
//                    tempString.push_back(c);
//                }
//            }
//
//            //Choose which type of message this is based on args[0]
//            switch(args[0]){
//                case "3":
//                    //This is a measurement report from a UE
//                    Ipv4Address currentMecAddr = Ipv4Address();
//                    currentMecAddr.Set(args[1].c_str());
//
//                    //Make map from args[2] content
//                    std::string mapstring = args[2];
//                    std::vector<std::string> map_args;
//                    std::map<Ipv4Address, int> delayMap;
//                    for (int i = 0 ; i < mapString.length(); i++){
//                        char c = mapString[i];
//                        if(c == "!"){
//                            map_args.push_back(tempString);
//                            tempString = "";
//                        }
//                        else{
//                            tempString.push_back(c);
//                        }
//                    }
//                    for(int i = 0; i < map_args.size(); i = i+2){
//                        Ipv4Address address =  Ipv4Address();
//                        address.Set(map_args[i])
//                        delayMap.insert(std::pair<Ipv4Address, int>(address, int(map_args[i+1])));
//                    }
//
//                    int currentDelay = delayMap.find(currentMecAddr->second());
//
//                    //find current delay
//                    int optimalDelay = currentDelay;
//                    Ipv4Address bestMec;
//                    for(int i = 0; i<delayMap.size(); i++){
//                        Ipv4Address address =  Ipv4Address();
//                        address.Set(delayMap[i].first());
//
//                        int delay = delayMap[i].second();
//
//                        bool triggerEquation;
//
//                        switch(TRIGGER){
//                            case "O":
//                                triggerEquation = (delay<optimalDelay);
//                                break;
//                            case "H":
//                                triggerEquation = (delay<optimalDelay && (delay < currentDelay*(1-HYSTERESIS)));
//                                break;
//                            case "T":
//                                triggerEquation = (delay<optimalDelay && (delay < currentDelay - THRESHOLD));
//                                break;
//                            case "TH":
//                                triggerEquation = delay<optimalDelay && (delay < currentDelay*(1-HYSTERESIS)) && (delay < currentDelay - THRESHOLD));
//                                break;
//                            default:
//                                NS_LOG_ERROR("Unsupported message type");
//                                StopApplication();
//                        }
//                        if (triggerEquation){
//                            smallestDelay = delay;
//                            bestMec = address;
//                        }
//                    }
//                    if (bestMec != currentMecAddr){
//                        //Initiate a handover to bestMec
//                        InetSocketAddress ueAddress = InetSocketAddress::ConvertFrom(from);
//                        //Find InetSocketAddress for bestMec
//                        InetSocketAddress mecAddress;
//                        for (int i = 0; i<waitingTimes.size(); i++){
//                            InetSocketAddress current = waitingTimes[i].first();
//                            Ipv4Address currentIpv4 = current.GetIpv4();
//
//                            if(currentIpv4 == bestMec){
//                                mecAddress = current;
//                            }
//                        }
//                    SendUeHandover(ueAddress, mecAddress);
//                    SendMecHandover(ueAddress, mecAddress);
//                    }
//                    break;
//                case "5":
//                    int newWaitingTime = int(args[1]);
//                    InetSocketAddress sendAddress = InetSocketaddress::ConvertFrom(from);
//
//                    if(waitingTimes.find(sendAddress) != waitingTimes.end()){
//                        //Element already exists in map
//                        waitingTimes[sendAddress] = newWaitingTime;
//
//                    }
//                    else{
//                        //New MEC, add entry in map
//                        waitingTimes.insert(std::pair<InetSocketAddress, int>(sendAddress,newWaitingTime));
//
//                    }
//                    break;
//                default:
//
//
//            }
//        }
//    }
//}
//} // Namespace ns3
