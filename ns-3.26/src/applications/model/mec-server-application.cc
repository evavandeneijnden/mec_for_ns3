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
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "mec-ue-application.h"
#include <sstream>

namespace ns3 {

    NS_LOG_COMPONENT_DEFINE ("MecServerApplication");

    NS_OBJECT_ENSURE_REGISTERED (MecServerApplication);

    InetSocketAddress m_orcAddress;
    std::vector<InetSocketAddress> m_mecAddresses;
    Event m_sendEvent;

    int MEAS_FREQ = 10;
    int MEC_RATE = 500;
    int UE_SIZE = 200;

//    Ptr<Node> m_thisNode = this->GetNode();
//    Ptr<NetDevice> m_thisNetDevice = m_thisNode.GetDevice(0);
//    Ipv4Address m_thisIpAddress = InetSocketAddress::ConvertFrom(m_thisNetDevice.GetAddress());  //Used for logging purposes

    InetSocketAddress m_orcAddress;

    uint32_t m_expectedWaitingTime = 0;
    int m_noClients = 0;
    int m_noUes = 800; //TODO hardcoded for now, finetune later
    int m_noHandovers = 0;
    Time m_startTime;
    Ptr<LteEnbNetDevice> m_enb;

    TypeId
    MecServerApplication::GetTypeId (void)
    {
        static TypeId tid = TypeId ("ns3::MecServerApplication")
                .SetParent<Application> ()
                .SetGroupName("Applications")
                .AddConstructor<MecServerApplication> ()
                .AddAttribute ("MaxPackets",
                               "The maximum number of packets the application will send",
                               UintegerValue (100),
                               MakeUintegerAccessor (&MecServerApplication::m_count),
                               MakeUintegerChecker<uint32_t> ())
                .AddAttribute ("UpdateInterval",
                               "The time to wait between serviceRequest packets",
                               TimeValue (Seconds (1.0)),
                               MakeTimeAccessor (&MecServerApplication::m_updateInterval),
                               MakeTimeChecker ())
                .AddAttribute ("PacketSize", "Size of echo data in outbound packets",
                               UintegerValue (100),
                               MakeUintegerAccessor (&MecServerApplication::m_packetSize),
                               MakeUintegerChecker<uint32_t> ())
                .AddTraceSource ("Tx", "A new packet is created and is sent",
                                 MakeTraceSourceAccessor (&MecServerApplication::m_txTrace),
                                 "ns3::Packet::TracedCallback")
        ;
        return tid;
    }

    MecServerApplication::MecServerApplication (InetSocketAddress orc, std::vector<InetSocketAddress> mecAddresses, Ptr<LteEnbNetDevice> enb)
    {
        NS_LOG_FUNCTION (this);
        m_sent = 0;
        m_socket = 0;
        m_sendEvent = EventId ();
        m_orcAddress = orc;
        m_mecAddresses = mecAddresses;
        m_startTime = Simulator::Now();
        m_enb = enb;
    }

    MecServerApplication::~MecServerApplication(InetSocketAddress orc, std::vector<InetSocketAddress> mecAddresses, Ptr<LteEnbNetDevice> enb)
    {
        NS_LOG_FUNCTION (this);
        m_socket = 0;

        m_orcAddress = 0;
        m_mecAddresses = 0;
        m_enb = 0;
    }

    void
    MecServerApplication::DoDispose (void)
    {
        NS_LOG_FUNCTION (this);
        Application::DoDispose ();
    }

    void
    MecServerApplication::StartApplication (void)
    {
        NS_LOG_FUNCTION (this);

        if (m_socket == 0)
        {
            TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
            m_socket = Socket::CreateSocket (GetNode (), tid);
            m_socket->Bind ();
            m_socket->Connect (m_orcAddress);
        }

        m_socket->SetRecvCallback (MakeCallback (&MecServerApplication::HandleRead, this));
        m_socket->SetAllowBroadcast (true);
        SendWaitingTimeUpdate();
    }

    void
    MecServerApplication::StopApplication ()
    {
        NS_LOG_FUNCTION (this);

        if (m_socket != 0)
        {
            m_socket->Close ();
            m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
            m_socket = 0;
        }

        Simulator::Cancel (m_sendEvent);
    }

    uint8_t
    MecServerApplication::SetFill (uint8_t *fill, uint32_t fillSize, uint32_t packetSize)
    {
        NS_LOG_FUNCTION (this << fill << fillSize << packetSize);
        uint8_t *result;

        if (fillSize >= packetSize)
        {
            memcpy (result, fill, packetSize);
            m_size = packetSize;
            return;
        }

        //
        // Do all but the final fill.
        //
        uint32_t filled = 0;
        while (filled + fillSize < packetSize)
        {
            memcpy (&result[filled], fill, fillSize);
            filled += fillSize;
        }

        //
        // Last fill may be partial
        //
        memcpy (&result[filled], fill, packetSize - filled);

        //
        // Overwrite packet size attribute.
        //
        m_size = packetSize;
        return result;
    }

    void
    MecServerApplication::SendWaitingTimeUpdate (void) {
        NS_ASSERT (m_sendEvent.IsExpired ());

        //Bind to correct destination (ORC)
        m_socket->Bind();
        m_socket->Connect (m_orcAddress);

        //Calculate waiting time (in ms)
        double serviceRho = (m_noClients*MSG_FREQ)/MEC_RATE;
        double expectedServiceWaitingTime = (serviceRho/(1-serviceRho))*(1/MEC_RATE);
        int pingRho = (m_noUes*MEAS_FREQ)/MEC_RATE;
        double expectedPingWaitingTime = (pingRho/(1-pingRho))*(1/MEC_RATE);
        double handoverFrequency = m_noHandovers/((Simulator::Now() - m_startTime).GetSeconds());
        int handoverRho = handoverFrequency/MEC_RATE;
        double expectedHandoverWaitingTime = (handoverRho/(1-handoverRho))*(1/MEC_RATE);
        m_expectedWaitingTime = int((expectedServiceWaitingTime + expectedPingWaitingTime + expectedHandoverWaitingTime)*1000);

        //Create packet payload
        std::string fillString = std::to_string(m_expectedWaitingTime) + "/";
        uint8_t *buffer = fillString.c_str();
        uint8_t *payload = SetFill(buffer, buffer.size(), m_packetSize);

        //Send packet
        Ptr<Packet> p = Create<Packet> (payload, m_packetSize);
        m_txTrace (p);
        m_socket->Send (p);

        ++m_sent;

        if (m_sent < m_count)
        {
            m_sendEvent = Simulator::Schedule (m_updateInterval, &MecServerApplication::SendWaitingTimeUpdate, this);
        }

    void
    MecServerApplication::SendUeTransfer (InetSocketAddress newMec)
    {
        m_socket->Bind();
        m_socket->Connect(newMec);

        //Create packet payload
        std::string fillString = "lorem ipsum";
        uint8_t *buffer = fillString.c_str();
        unint8_t *payload = SetFill(buffer, buffer.size(), UE_SIZE);

        //Create packet
        Ptr <Packet> p = Create<Packet>(payload, UE_SIZE);
        // call to the trace sinks before the packet is actually sent,
        // so that tags added to the packet can be sent as well
        m_txTrace(p);
        m_socket->Send(p);

        ++m_sent;
    }


        void
        MecServerApplication::SendEcho (Packet packet)
        {

            m_txTrace(packet);
            m_socket->Send(packet);

            ++m_sent;
        }

    void
    MecServerApplication::HandleRead (Ptr<Socket> socket)
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
                if (from_ipv4 == m_orcAddress.GetIpv4() && inet_from.GetPort() == m_orcAddress.GetPort()){

                    //Get payload from packet
                    uint32_t packetSize = packet->GetSize();
                    uint8_t *buffer = new uint8_t[packetSize];
                    packet->CopyData(buffer, packetSize);
                    std::string payloadString = std::string((char*)buffer);

                    //Split the payload string into arguments
                    std::string tempString;
                    std::vector<std::string> args;
                    for (int i = 0 ; i < payloadString.length(); i++){
                        char c = payloadString[i];
                        if(c == "/"){
                            args.push_back(tempString);
                            tempString = "";
                        }
                        else{
                            tempString.push_back(c);
                        }
                    }
                    //Get new MEC address
                    std::string addressString = args[0];
                    std::string portString = args[1];

                    Ipv4Address newAddress = Ipv4Address();
                    newAdress.Set(addressString.c_str);

                    uint16_t newPort = std::stoi(portString);

                    //Initiate handover
                    Simulator::Schedule(Seconds(0), &SendUeTransfer, InetSocketAddress(newAddress, newPort));

                }
                else {
                    //Get payload from packet
                    uint32_t packetSize = packet->GetSize();
                    uint8_t *buffer = new uint8_t[packetSize];
                    packet->CopyData(buffer, packetSize);
                    std::string payloadString = std::string((char*)buffer);

                    //Split the payload string into arguments
                    std::string tempString;
                    std::vector<std::string> args;
                    for (int i = 0 ; i < payloadString.length(); i++){
                        char c = payloadString[i];
                        if(c == "/"){
                            args.push_back(tempString);
                            tempString = "";
                        }
                        else{
                            tempString.push_back(c);
                        }
                    }

                    Ptr<LteEnbNetDevice> ue_enb = args[0];
                    int delay = 0; //in ms

                    if (m_enb.GetCellId() == ue_enb){
                        delay = m_expectedWaitingTime;
                    }
                    else {
                        //UE is connected to another eNB; add "penalty" for having to go through network
                        delay = m_expectedWaitingTime + 15;
                    }
                }

                //Bind to correct socket
                m_socket->Bind ();
                m_socket->Connect (inet_from);

                //Echo packet back to sender with appropriate delay
                Simulator::Schedule(Milliseconds(delay), &SendEcho, packet);

                }
            }
        }
    }
//
} // Namespace ns3
