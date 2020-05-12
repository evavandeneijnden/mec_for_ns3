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
#include <fstream>
#include <iostream>
#include "ns3/random-variable-stream.h"
#include "ns3/double.h"
#include <tuple>
#include <fstream>

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
        uint8_t* GetFilledString(std::string filler, int size);

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
        void SendFirstRequest(void);
        void SendPing(void);
        void SendMeasurementReport(std::map<Ipv4Address,int64_t> report);

        void NextService(Time);

        uint16_t GetCellId(void);
        bool CheckEnb(Ptr<LteEnbNetDevice> enb);

        void SendIndividualPing(Ptr<Packet> p, InetSocketAddress mec, int packetId);
        void SendPosition(void);
        void PrintRoutes(void);
        void LogServer(void);
        void HandlePingTimeout(std::list<int> batchIds);


        uint32_t m_count; //!< Maximum number of packets the application will send
        Time m_serviceInterval; //!< Packet inter-send time
        Time m_pingInterval;
        Time m_timeoutInterval = MilliSeconds(500);
        uint32_t m_size; //!< Size of the sent packet



        uint32_t m_sent; //!< Counter for sent packets
        Ptr<Socket> m_socket; //!< Socket
        EventId m_sendPingEvent;
        EventId m_sendServiceEvent;
        EventId m_sendPositionEvent;
        EventId m_handleTimeoutEvent;
        EventId logServerEvent;
        uint8_t *m_data_request;
        uint8_t *m_data_ping;
        uint8_t *m_data_report;
        Ipv4Address m_mecIp;
        uint32_t m_mecPort;
        Ipv4Address m_orcIp;
        uint32_t m_orcPort;
        Ptr<Node> m_thisNode;
        bool m_requestBlocked;
        Ptr<NetDevice> m_thisNetDevice;
        Ipv4Address m_thisIpAddress;
        std::string m_serverString;

        /// Callbacks for tracing the packet Tx events
        TracedCallback<Ptr<const Packet> > m_txTrace;

        Time m_noSendUntil;
        Time m_requestSent;
        Time requestBlockedTime;
        std::vector<InetSocketAddress> m_allServers;
        std::map<InetSocketAddress, Ptr<Socket>> serverSocketMap;

        Ptr<LteEnbNetDevice> m_enb0;
        Ptr<LteEnbNetDevice> m_enb1;
        Ptr<LteEnbNetDevice> m_enb2;
        std::vector<Ptr<LteEnbNetDevice>> m_enbDevices;
        uint64_t ueImsi;
        uint16_t myCellId;
        int packetIdCounter;
        int requestCounter;
        Time m_pingOffset;
        Time m_serviceOffset;
        uint32_t metric;
        Ptr<Node> router;
        std::string m_filename;

        int serviceRequestCounter;
        int serviceResponseCounter;
        int pingRequestCounter;
        int pingResponseCounter;
        int handoverCommandCounter;
        int firstRequestCounter;
        int sendPositionCounter;
        int sendMeasurementReportCounter;

        Ptr<UniformRandomVariable> randomness;

        std::map<int,Time> openServiceRequests;
        std::map<int,Time> pingSendTimes;
        std::vector<std::pair<std::list<int>, std::map<Ipv4Address, int64_t>>> openPingRequests;

        std::map<int, std::tuple<int,int,double>> delays; //Total delay, #delays, mean
        std::map<int,int> handovers; //time-slot, number of handovers in this time slot
        std::vector<Vector> handoverPositions;
        std::fstream outfile;
    };

} // namespace ns3

#endif /* MEC_UE_APPLICATION_H */
