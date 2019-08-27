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
        m_sendEvent = EventId ();\
        m_transferEvent = EventId();
        m_echoEvent = EventId();
        m_startTime = Simulator::Now();

        m_noUes = 800; //TODO hardcoded for now, finetune later
        m_noHandovers = 0;
        noSendUntil = Simulator::Now();
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

        if (m_socket == 0)
        {
            TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
            NS_LOG_DEBUG("GetNode: " << GetNode());
            m_socket = Socket::CreateSocket (GetNode (), tid);
            m_socket->Bind ();
            int connect_code = m_socket->Connect (InetSocketAddress(m_orcAddress, m_orcPort));
            NS_LOG_DEBUG("Server connecting to " << m_orcAddress << " on startup with code " << connect_code);
        }
        NS_LOG_DEBUG("Server socket: " << m_socket);
        m_socket->SetRecvCallback (MakeCallback (&MecHoServerApplication::HandleRead, this));
        m_socket->SetAllowBroadcast (true);
        SendWaitingTimeUpdate();
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
            //Bind to correct destination (ORC)
//            m_socket->Bind();
            int connect_code = m_socket->Connect(InetSocketAddress(m_orcAddress, m_orcPort));
            NS_LOG_DEBUG("Server connecting to " << m_orcAddress << "on SendWaitingTimeUpdate with code " << connect_code);

            //Calculate waiting time (in ms)
            NS_LOG_DEBUG("myClients.size(): " << myClients.size());
            double serviceRho = (myClients.size() * MSG_FREQ) / MEC_RATE;
            double expectedServiceWaitingTime = (serviceRho / (1 - serviceRho)) * (1 / MEC_RATE);
            double pingRho = (m_noUes * MEAS_FREQ) / MEC_RATE;
            double expectedPingWaitingTime = (pingRho / (1 - pingRho)) * (1 / MEC_RATE);
            double handoverFrequency = m_noHandovers / ((Simulator::Now() - m_startTime).GetSeconds());
            double handoverRho = handoverFrequency / MEC_RATE;
            double expectedHandoverWaitingTime = (handoverRho / (1 - handoverRho)) * (1 / MEC_RATE);
            NS_LOG_DEBUG("serviceRho: " << serviceRho << ", expectedServiceWaitingTime:" << expectedServiceWaitingTime << ", pingRho: "
                          << pingRho << ", expectedPingWaitingTime: " << expectedPingWaitingTime << ",handoverFrequency: " << handoverFrequency
                          << ", handoverRho: " << handoverRho << ", expectedHandoverWaitingTime: " << expectedHandoverWaitingTime);
            m_expectedWaitingTime = int((expectedServiceWaitingTime + expectedPingWaitingTime + expectedHandoverWaitingTime) * 1000);

            //Create packet payload
            std::string fillString = "5/" + std::to_string(m_expectedWaitingTime) + "/";
//            NS_LOG_DEBUG("Expected waiting time: " << m_expectedWaitingTime);
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
    MecHoServerApplication::SendUeTransfer (void)
    {
        if (Simulator::Now() > noSendUntil){
//            m_socket->Bind();
            int connect_code = m_socket->Connect(InetSocketAddress(m_newAddress, m_newPort));
            NS_LOG_DEBUG("Server connecting to " << m_newAddress << " on UE transfer with code " << connect_code);

            //Create packet payload
            uint8_t *buffer = GetFilledString("", UE_SIZE);

            //Create packet
            Ptr <Packet> p = Create<Packet>(buffer, UE_SIZE);
            // call to the trace sinks before the packet is actually sent,
            // so that tags added to the packet can be sent as well
            m_txTrace(p);
            m_socket->Send(p);

            ++m_sent;
        }
        else {
            m_transferEvent = Simulator::Schedule(noSendUntil, &MecHoServerApplication::SendUeTransfer, this);
        }

    }


    void
    MecHoServerApplication::SendEcho (void)
    {
        if(Simulator::Now() > noSendUntil){
            m_txTrace(m_packet);
            m_socket->Send(m_packet);

            ++m_sent;
        }
        else {
            m_echoEvent = Simulator::Schedule(noSendUntil, &MecHoServerApplication::SendEcho, this);
        }
    }

    void
    MecHoServerApplication::HandleRead (Ptr<Socket> socket)
    {
        NS_LOG_FUNCTION (this << socket);
        Ptr<Packet> packet;
        m_packet = packet;
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
                        int delay = 0; //in ms

                        if (int(m_cellId) == ue_cellId){
                            if (myClients.find(inet_from) == myClients.end()){
                                myClients.insert(inet_from);
                            }
                            delay = m_expectedWaitingTime;
                        }
                        else {
                            //UE is connected to another eNB; add "penalty" for having to go through network
                            delay = m_expectedWaitingTime + 15;
                        }
                        //Bind to correct socket
//                        m_socket->Bind();
                        int connect_code = m_socket->Connect(inet_from);
                        NS_LOG_DEBUG("Server connecting to " << inet_from.GetIpv4() << "on receive service request with code " << connect_code);

                        //Echo packet back to sender with appropriate delay
                        Simulator::Schedule(Seconds(delay / 1000), &MecHoServerApplication::SendEcho, this);

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

                        //TODO remove UE from client list --> check that this works as expected!
                        NS_LOG_DEBUG("before erase: " << myClients.size());
                        myClients.erase(newInet);
                        NS_LOG_DEBUG("after erase: " << myClients.size());
                        //Initiate handover
                        Simulator::Schedule(Seconds(0), &MecHoServerApplication::SendUeTransfer, this);
                        break;
                    }
                    case 7: {
                        //handover data from other mec
                        noSendUntil = Simulator::Now() + Seconds(m_expectedWaitingTime/1000);
                        break;
                    }
                }
            }
        }
    }
} // Namespace ns3
