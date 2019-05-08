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

namespace ns3 {

    NS_LOG_COMPONENT_DEFINE ("MecUeApplication");

    NS_OBJECT_ENSURE_REGISTERED (MecUeApplication);

    InetSocketAddress m_orcAddress;
    Time m_noSendUntil;
    Time m_requestSent;
    std::map<Ipv4Address,int64_t> m_measurementReport; //First argument is MECs address, second is observed delay in ms
    std::map<Ipv4Address,Time> m_pingSent;

    Ptr<Node> m_thisNode = this->GetNode();
    Ptr<NetDevice> m_thisNetDevice = m_thisNode.GetDevice(0);
    Ipv4Address m_thisIpAddress = InetSocketAddress::ConvertFrom(m_thisNetDevice.GetAddress());  //Used for logging purposes

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
                .AddAttribute ("Interval",
                               "The time to wait between packets",
                               TimeValue (Seconds (1.0)),
                               MakeTimeAccessor (&MecUeApplication::m_interval),
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
                               MakeUintegerAccessor (&MecUeApplication::SetDataSize,
                                                     &MecUeApplication::GetDataSize),
                               MakeUintegerChecker<uint32_t> ())
                .AddTraceSource ("Tx", "A new packet is created and is sent",
                                 MakeTraceSourceAccessor (&MecUeApplication::m_txTrace),
                                 "ns3::Packet::TracedCallback")
        ;
        return tid;
    }

    MecUeApplication::MecUeApplication (InetSocketAddress mec, InetSocketAddress orc)
    {
        NS_LOG_FUNCTION (this);
        m_sent = 0;
        m_socket = 0;
        m_sendEvent = EventId ();
        m_data = 0;
        m_dataSize = 0;
        this.setMec(mec.GetIpv4(), mec.GetPort());
        m_orcAddress = orc;
    }

    MecUeApplication::~MecUeApplication(InetSocketAddress mec, InetSocketAddress orc)
    {
        NS_LOG_FUNCTION (this);
        m_socket = 0;

        delete [] m_data;
        m_data = 0;
        m_dataSize = 0;

        this.setMec(mec.GetIpv4(), mec.GetPort());
        m_orcAddress = orc;
    }

    void
    MecUeApplication::SetMec (Address ip, uint16_t port)
    {
        NS_LOG_FUNCTION (this << ip << port);
        m_mecAddress = ip;
        m_mecPort = port;
    }

//    void
//    MecUeApplication::SetRemote (Address addr)
//    {
//        NS_LOG_FUNCTION (this << addr);
//        m_mecAddress = addr;
//    }

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
//            else if (Ipv6Address::IsMatchingType(m_mecAddress) == true)
//            {
//                m_socket->Bind6();
//                m_socket->Connect (Inet6SocketAddress (Ipv6Address::ConvertFrom(m_mecAddress), m_mecPort));
//            }
            else if (InetSocketAddress::IsMatchingType (m_mecAddress) == true)
            {
                m_socket->Bind ();
                m_socket->Connect (m_mecAddress);
            }
//            else if (Inet6SocketAddress::IsMatchingType (m_mecAddress) == true)
//            {
//                m_socket->Bind6 ();
//                m_socket->Connect (m_mecAddress);
//            }
            else
            {
                NS_ASSERT_MSG (false, "Incompatible address type: " << m_mecAddress);
            }
        }

        m_socket->SetRecvCallback (MakeCallback (&MecUeApplication::HandleRead, this));
        m_socket->SetAllowBroadcast (true);
        ScheduleTransmit (Seconds (0.0));
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

        Simulator::Cancel (m_sendEvent);
    }

    void
    MecUeApplication::SetDataSize (uint32_t dataSize)
    {
        NS_LOG_FUNCTION (this << dataSize);

        //
        // If the client is setting the echo packet data size this way, we infer
        // that she doesn't care about the contents of the packet at all, so
        // neither will we.
        //
        delete [] m_data;
        m_data = 0;
        m_dataSize = 0;
        m_size = dataSize;
    }

    uint32_t
    MecUeApplication::GetDataSize (void) const
    {
        NS_LOG_FUNCTION (this);
        return m_size;
    }

    void
    MecUeApplication::SetFill (std::string fill)
    {
        NS_LOG_FUNCTION (this << fill);

        uint32_t dataSize = fill.size () + 1;

        if (dataSize != m_dataSize)
        {
            delete [] m_data;
            m_data = new uint8_t [dataSize];
            m_dataSize = dataSize;
        }

        memcpy (m_data, fill.c_str (), dataSize);

        //
        // Overwrite packet size attribute.
        //
        m_size = dataSize;
    }

    void
    MecUeApplication::SetFill (uint8_t fill, uint32_t dataSize)
    {
        NS_LOG_FUNCTION (this << fill << dataSize);
        if (dataSize != m_dataSize)
        {
            delete [] m_data;
            m_data = new uint8_t [dataSize];
            m_dataSize = dataSize;
        }

        memset (m_data, fill, dataSize);

        //
        // Overwrite packet size attribute.
        //
        m_size = dataSize;
    }

    void
    MecUeApplication::SetFill (uint8_t *fill, uint32_t fillSize, uint32_t dataSize)
    {
        NS_LOG_FUNCTION (this << fill << fillSize << dataSize);
        if (dataSize != m_dataSize)
        {
            delete [] m_data;
            m_data = new uint8_t [dataSize];
            m_dataSize = dataSize;
        }

        if (fillSize >= dataSize)
        {
            memcpy (m_data, fill, dataSize);
            m_size = dataSize;
            return;
        }

        //
        // Do all but the final fill.
        //
        uint32_t filled = 0;
        while (filled + fillSize < dataSize)
        {
            memcpy (&m_data[filled], fill, fillSize);
            filled += fillSize;
        }

        //
        // Last fill may be partial
        //
        memcpy (&m_data[filled], fill, dataSize - filled);

        //
        // Overwrite packet size attribute.
        //
        m_size = dataSize;
    }

    void
    MecUeApplication::ScheduleTransmit (Time dt)
    {
        NS_LOG_FUNCTION (this << dt);
        m_sendEvent = Simulator::Schedule (dt, &MecUeApplication::Send, this);
    }

    void
    MecUeApplication::Send (void)
    {
//        Old implementation; substitute for a UE-appropriate one
//        NS_LOG_FUNCTION (this);
//
//        NS_ASSERT (m_sendEvent.IsExpired ());
//
//        Ptr<Packet> p;
//        if (m_dataSize)
//        {
//            //
//            // If m_dataSize is non-zero, we have a data buffer of the same size that we
//            // are expected to copy and send.  This state of affairs is created if one of
//            // the Fill functions is called.  In this case, m_size must have been set
//            // to agree with m_dataSize
//            //
//            NS_ASSERT_MSG (m_dataSize == m_size, "MecUeApplication::Send(): m_size and m_dataSize inconsistent");
//            NS_ASSERT_MSG (m_data, "MecUeApplication::Send(): m_dataSize but no m_data");
//            p = Create<Packet> (m_data, m_dataSize);
//        }
//        else
//        {
//            //
//            // If m_dataSize is zero, the client has indicated that it doesn't care
//            // about the data itself either by specifying the data size by setting
//            // the corresponding attribute or by not calling a SetFill function.  In
//            // this case, we don't worry about it either.  But we do allow m_size
//            // to have a value different from the (zero) m_dataSize.
//            //
//            p = Create<Packet> (m_size);
//        }
//        // call to the trace sinks before the packet is actually sent,
//        // so that tags added to the packet can be sent as well
//        m_txTrace (p);
//        m_socket->Send (p);
//
//        ++m_sent;
//
//        if (Ipv4Address::IsMatchingType (m_mecAddress))
//        {
//            NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
//                                    Ipv4Address::ConvertFrom (m_mecAddress) << " port " << m_mecPort);
//        }
//        else if (Ipv6Address::IsMatchingType (m_mecAddress))
//        {
//            NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
//                                    Ipv6Address::ConvertFrom (m_mecAddress) << " port " << m_mecPort);
//        }
//        else if (InetSocketAddress::IsMatchingType (m_mecAddress))
//        {
//            NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
//                                    InetSocketAddress::ConvertFrom (m_mecAddress).GetIpv4 () << " port " << InetSocketAddress::ConvertFrom (m_mecAddress).GetPort ());
//        }
//        else if (Inet6SocketAddress::IsMatchingType (m_mecAddress))
//        {
//            NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
//                                    Inet6SocketAddress::ConvertFrom (m_mecAddress).GetIpv6 () << " port " << Inet6SocketAddress::ConvertFrom (m_mecAddress).GetPort ());
//        }
//
//        if (m_sent < m_count)
//        {
//            ScheduleTransmit (m_interval);
//        }
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
                        int64_t delay = (m_pingSent.find(from_ipv4) - Simulator::Now()).GetMilliSeconds();
                        m_measurementReport.insert(pair<Ipv4Address, int64_t>(from_ipv4, delay));
                    }
                }
            }
        }
    }

} // Namespace ns3
