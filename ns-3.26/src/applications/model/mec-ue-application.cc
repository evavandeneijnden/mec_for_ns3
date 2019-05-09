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

    NS_LOG_COMPONENT_DEFINE ("MecUeApplication");

    NS_OBJECT_ENSURE_REGISTERED (MecUeApplication);

    InetSocketAddress m_orcAddress;
    Time m_noSendUntil;
    Time m_requestSent;
    bool m_requestBlocked = false;
    std::map<Ipv4Address,int64_t> m_measurementReport; //First argument is MECs address, second is observed delay in ms
    std::map<Ipv4Address,Time> m_pingSent;
    std::vector<InetSocketAddress> m_mecAddresses;
    uint8_t *m_data_request;
    uint8_t *m_data_ping;
    uint8_t *m_data_report;

    Ptr<Node> m_thisNode = this->GetNode();
    Ptr<NetDevice> m_thisNetDevice = m_thisNode.GetDevice(0);
    Ipv4Address m_thisIpAddress = InetSocketAddress::ConvertFrom(m_thisNetDevice.GetAddress());  //Used for logging purposes

    InetSocketAddress m_mecAddress;
    InetSocketAddress m_orcAddress;

    TypeId
    MecUeApplication::GetTypeId (void)
    {
        static TypeId tid = TypeId ("ns3::MecUeApplication")
                .SetParent<Application> ()
                .SetGroupName("Applications")
                .AddConstructor<MecUeApplication> ()
                .AddAttribute ("MaxPackets",
                               "The maximum number of packets the application will send",
                               UintegerValue (100),
                               MakeUintegerAccessor (&MecUeApplication::m_count),
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
                .AddAttribute ("MecAddress",
                               "The destination Address of the outbound packets",
                               AddressValue (),
                               MakeAddressAccessor (&MecUeApplication::m_mecAddress),
                               MakeAddressChecker ())
                .AddAttribute ("MecPort",
                               "The destination port of the outbound packets",
                               UintegerValue (0),
                               MakeUintegerAccessor (&MecUeApplication::m_mecPort),
                               MakeUintegerChecker<uint16_t> ())
                .AddAttribute ("PacketSize", "Size of echo data in outbound packets",
                               UintegerValue (100),
                               MakeUintegerAccessor (&MecUeApplication::m_packetSize),
                               MakeUintegerChecker<uint32_t> ())
                .AddTraceSource ("Tx", "A new packet is created and is sent",
                                 MakeTraceSourceAccessor (&MecUeApplication::m_txTrace),
                                 "ns3::Packet::TracedCallback")
        ;
        return tid;
    }

    MecUeApplication::MecUeApplication (InetSocketAddress mec, InetSocketAddress orc, std::vector<InetSocketAddress> mecAddresses)
    {
        NS_LOG_FUNCTION (this);
        m_sent = 0;
        m_socket = 0;
        m_sendPingEvent = EventId ();
        m_sendServiceEvent = EventId ();
        m_data_request = 0;
        m_data_ping = 0;
        m_data_report = 0;
        this.setMec(mec.GetIpv4(), mec.GetPort());
        m_orcAddress = orc;
        m_mecAddresses = mecAddresses;
    }

    MecUeApplication::~MecUeApplication(InetSocketAddress mec, InetSocketAddress orc, std::vector<InetSocketAddress> mecAddresses)
    {
        NS_LOG_FUNCTION (this);
        m_socket = 0;

        delete [] m_data_request;
        delete [] m_data_ping;
        delete [] m_data_report;
        m_data_request = 0;
        m_data_ping = 0;
        m_data_report = 0;

        m_orcAddress = 0;
        m_mecAddresses = 0;
    }

    void
    MecUeApplication::SetMec (Address ip, uint16_t port)
    {
        NS_LOG_FUNCTION (this << ip << port);
        m_mecAddress = ip;
        m_mecPort = port;
    }


    void
    MecUeApplication::DoDispose (void)
    {
        NS_LOG_FUNCTION (this);
        Application::DoDispose ();
    }

    void
    MecUeApplication::StartApplication (void)
    {
        NS_LOG_FUNCTION (this);

        if (m_socket == 0)
        {
            TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
            m_socket = Socket::CreateSocket (GetNode (), tid);
            if (Ipv4Address::IsMatchingType(m_mecAddress) == true)
            {
                m_socket->Bind();
                m_socket->Connect (InetSocketAddress (Ipv4Address::ConvertFrom(m_mecAddress), m_mecPort));
            }
            else if (InetSocketAddress::IsMatchingType (m_mecAddress) == true)
            {
                m_socket->Bind ();
                m_socket->Connect (m_mecAddress);
            }
            else
            {
                NS_ASSERT_MSG (false, "Incompatible address type: " << m_mecAddress);
            }
        }

        m_socket->SetRecvCallback (MakeCallback (&MecUeApplication::HandleRead, this));
        m_socket->SetAllowBroadcast (true);
        ScheduleTransmitServiceRequest (Seconds (0.0));
        ScheduleTransmitServicePing (Seconds (0.0));
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

        Simulator::Cancel (m_sendPingEvent);
        Simulator::Cancel (m_sendServiceEvent);
    }

    void
    MecUeApplication::SetFill (uint8_t *fill, uint32_t fillSize, uint8_t *dest)
    {
        //dest can either be m_data_request or m_data_ping or m_data_report
        NS_LOG_FUNCTION (this << fill << fillSize << m_packetSize);

        if (fillSize >= m_packetSize)
        {
            memcpy (dest, fill, m_packetSize);
            m_size = m_packetSize;
            return;
        }

        //
        // Do all but the final fill.
        //
        uint32_t filled = 0;
        while (filled + fillSize < m_packetSize)
        {
            memcpy (&dest[filled], fill, fillSize);
            filled += fillSize;
        }

        //
        // Last fill may be partial
        //
        memcpy (&dest[filled], fill, m_packetSize - filled);

        //
        // Overwrite packet size attribute.
        //
        m_size = m_packetSize;
    }

    void
    MecUeApplication::SendServiceRequest (void) {
        NS_ASSERT (m_sendServiceEvent.IsExpired ());

        if (Simulator::Now() < m_noSenduntil){
            m_requestSent = Simulator::Now();
            m_requestBlocked = true;
        }
        else {
            if (!m_requestBlocked){
                m_requestSent = Simulator::Now();
            }
            //Create packet payload
            std::string fillString = "lorem ipsum";
            uint8_t *buffer = fillString.c_str();
            SetFill(buffer, m_packetSize, m_data_request);

            //Create packet
            Ptr<Packet> p = Create<Packet> (m_data_request, m_packetSize);
            // call to the trace sinks before the packet is actually sent,
            // so that tags added to the packet can be sent as well
            m_txTrace (p);
            m_socket->Send (p);

            ++m_sent;

            if (m_sent < m_count)
            {
                m_sendServiceEvent = Simulator::Schedule (m_serviceInterval, &MecUeApplication::SendServiceRequest, this);
            }

            m_requestBlocked = false;
        }
    }

    void
    MecUeApplication::SendPing (void)
    {
        NS_ASSERT (m_sendPingEvent.IsExpired ());
        m_pingSent.clear();
        for (InetSocketAddress mec: m_mecAddresses){
            if (Simulator::Now() < m_noSenduntil){
                m_requestSent = Simulator::Now();
                m_requestBlocked = true;
            }
            else {
                m_pingSent.insert(Pair<Ipv4Address, Time>(mec, Simulator::Now()));
                m_socket->Bind();
                m_socket->Connect(mec);


                //Create packet payload
                std::string fillString = "lorem ipsum";
                uint8_t *buffer = fillString.c_str();
                SetFill(buffer, m_packetSize, m_data_ping);

                //Create packet
                Ptr <Packet> p = Create<Packet>(m_data_ping, m_packetSize);
                // call to the trace sinks before the packet is actually sent,
                // so that tags added to the packet can be sent as well
                m_txTrace(p);
                m_socket->Send(p);

                ++m_sent;

                if (m_sent < m_count) {
                    m_sendServiceEvent = Simulator::Schedule(m_serviceInterval, &MecUeApplication::SendServiceRequest, this);
                }

                m_requestBlocked = false;
            }
        }
    }

    void
    MecUeApplication::SendMeasurementReport (std::map<Ipv4Address, uint64_t> measurementReport){

        //Bind to ORC address

        //Create packet payload
        std::string payload;
        for(int i = 0; i < measurementReport.size(); i++){
            std::pair<Ipv4Address, uint64_t> item = measurementReport[i];

            //Convert Address into string
            Ipv4Address addr = item.first();
            std::stringstream ss;
            std:ostream os;
            addr.Print(os);
            ss << os.rdbuf();
            std:string addrString = ss.str();

            payload.push_back(addrString + "/" + std::to_string(item.second()) + "/");
        }
        payload.push_back("!"); //! is end character for the measurementReport


        std::string fillString = payload;
        uint8_t *buffer = fillString.c_str();
        SetFill(buffer, m_packetSize, m_data_report);

        //Create packet
        Ptr<Packet> p = Create<Packet> (m_data_report, m_packetSize);
        // call to the trace sinks before the packet is actually sent,
        // so that tags added to the packet can be sent as well
        m_txTrace (p);
        m_socket->Send (p);

        ++m_sent;

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

                    //Update MEC address
                    NS_LOG_INFO("Handover," Simulator::Now()<< "," << m_thisIpAddress << "," << mecAddress << "," << newAddress << "\n");
                    setMec(newAddress, newPort);
                    m_socket->Bind();
                    m_socket->Connect (InetSocketAddress (newAddress, newPort));

                    //Set no-send period
                    m_noSendUntil = Simulator::Now() + Time(args[2]);


                }
                else {
                    //This request came from a MEC
                    if (from_ipv4 == m_mecAddress && inet_from.GetPort() == m_mecPort){
                        int64_t delay = (m_requestSent - Simulator::Now()).GetMilliSeconds();
                        NS_LOG_INFO("Delay," << Simulator::Now() << "," << m_thisIpAddress << "," << from_ipv4 << "," << delay << "\n");
                    }
                    else{
                        Time sendTime;
                        for(int i = 0; i < m_pingSent.size(); i++){
                            InetSocketAddress itemAddress = m_pingSent[i].first();
                            if (itemAddress.GetIpv4() == from_ipv4){
                                sendTime = m_pingSent[i].second();
                                break;
                            }

                        }

                        int64_t delay = (sendTime - Simulator::Now()).GetMilliSeconds();
                        sendTime = 0;
                        m_measurementReport.insert(std::pair<Ipv4Address, int64_t>(from_ipv4, delay));

                        if(m_measurementReport.size() == m_mecAddresses.size()){
                            //There is a measurement for each mec, i.e. the report is now complete and ready to be sent to the ORC
                            SendMeasurementReport(m_measurementReport);
                            m_measurementReport.clear(); //Start with an empty report for the next iteration


                        }
                    }
                }
            }
        }
    }

} // Namespace ns3
