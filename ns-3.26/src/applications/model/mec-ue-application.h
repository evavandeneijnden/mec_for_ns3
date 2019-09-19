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

#ifndef MEC_UE_APPLICATION_H
#define MEC_UE_APPLICATION_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include "ns3/traced-callback.h"
#include "ns3/node.h"
#include "ns3/application.h"
#include "ns3/lte-enb-net-device.h"
#include "ns3/lte-ue-net-device.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include "ns3/net-device-container.h"
#include "ns3/inet-socket-address.h"
#include "ns3/socket.h"
#include "ns3/lte-enb-rrc.h"
#include <thread>
#include <chrono>

namespace ns3 {

    class Socket;
    class Packet;

/**
 * \ingroup udpecho
 * \brief A Udp Echo client
 *
 * Every packet sent should be returned by the server and received here.
 */
    class MecUeApplication : public Application
    {
    public:
        /**
         * \brief Get the type ID.
         * \return the object TypeId
         */
        static TypeId GetTypeId (void);

        MecUeApplication ();

        virtual ~MecUeApplication ();

        /**
         * Add string to result, then fill the rest of the packet with '#' characters
         */
        uint8_t* GetFilledString(std::string filler);

    protected:
        virtual void DoDispose (void);

    private:

        virtual void StartApplication (void);
        virtual void StopApplication (void);

        /**
         * \brief Handle a packet reception.
         *
         * This function is called by lower layers.
         *
         * \param socket the socket the packet was received to.
         */
        void HandleRead (Ptr<Socket> socket);


        void SendServiceRequest(void);
        void SendPing(void);
        void SendMeasurementReport(void);

        uint16_t GetCellId(void);
        uint16_t CheckEnb(Ptr<LteEnbNetDevice> enb);


        uint32_t m_count; //!< Maximum number of packets the application will send
        Time m_serviceInterval; //!< Packet inter-send time
        Time m_pingInterval;
        uint32_t m_size; //!< Size of the sent packet



        uint32_t m_sent; //!< Counter for sent packets
        Ptr<Socket> m_orcSocket; //!< Socket
        EventId m_sendPingEvent;
        EventId m_sendServiceEvent;
        uint8_t *m_data_request;
        uint8_t *m_data_ping;
        uint8_t *m_data_report;
        Ipv4Address m_mecIp;
        uint8_t m_mecPort;
        Ipv4Address m_orcIp;
        uint8_t m_orcPort;
        Ptr<Node> m_thisNode;
        bool m_requestBlocked;
        Ptr<NetDevice> m_thisNetDevice;
        Ipv4Address m_thisIpAddress;
        std::string m_serverString;




        /// Callbacks for tracing the packet Tx events
        TracedCallback<Ptr<const Packet> > m_txTrace;


        //Added
        Time m_noSendUntil;
        Time m_requestSent;
        std::map<Ipv4Address,int64_t> m_measurementReport; //First argument is MECs address, second is observed delay in ms
        std::map<Ipv4Address,Time> m_pingSent;
        std::vector<InetSocketAddress> m_allServers;
        std::map<InetSocketAddress, Ptr<Socket>> serverSocketMap;
        Ptr<Socket> currentMecSocket;

        Ptr<LteEnbNetDevice> m_enb0;
        Ptr<LteEnbNetDevice> m_enb1;
        Ptr<LteEnbNetDevice> m_enb2;
        std::vector<Ptr<LteEnbNetDevice>> m_enbDevices;

    };

} // namespace ns3

#endif /* MEC_UE_APPLICATION_H */
