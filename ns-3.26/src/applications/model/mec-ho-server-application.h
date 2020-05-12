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

#ifndef MEC_SERVER_APPLICATION_H
#define MEC_SERVER_APPLICATION_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include "ns3/traced-callback.h"
#include "ns3/inet-socket-address.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/random-variable-stream.h"
#include "ns3/double.h"
#include <fstream>

namespace ns3 {

    class MecHoServerApplication : public Application
    {
    public:
        /**
         * \brief Get the type ID.
         * \return the object TypeId
         */
        static TypeId GetTypeId (void);

        MecHoServerApplication ();

        virtual ~MecHoServerApplication ();

        /**
        * Add string to result, then fill the rest of the packet with '#' characters
        */
        uint8_t* GetFilledString(std::string filler, int size);

    protected:
        virtual void DoDispose (void);

    private:

        virtual void StartApplication (void);
        virtual void StopApplication (void);

        void HandleRead (Ptr<Socket> socket);

        void SendResponseTimeUpdate();
        void SendUeTransfer(InetSocketAddress ueAddress, InetSocketAddress newMecAddress);
        void SendEcho (Ipv4Address echoAddress, Ptr<Packet> packet, std::string packetId);
        void LogResponseTime(void);
        Time HandleQueue (Ptr<Packet> newPacket);
//        void SendRequestEcho(void);


        uint32_t m_count; //!< Maximum number of packets the application will send
        uint32_t m_updateInterval; //!< Packet inter-send time in ms
        uint32_t m_serviceInterval;
        uint32_t m_measurementInterval;
//        uint32_t m_size; //!< Size of the sent packet

        uint32_t m_sent; //!< Counter for sent packets

        Ptr<Socket> m_socket; //!< Socket
        std::map<InetSocketAddress, Ptr<Socket>> serverSocketMap;
        std::map<InetSocketAddress, Ptr<Socket>> clientSocketMap;

        EventId m_sendEvent; //!< Event to send the next packet
        EventId m_transferEvent;
        EventId m_echoEvent;
        EventId logResponseTimeEvent;
        EventId sendResponseTimeUpdateEvent;

        /// Callbacks for tracing the packet Tx events
        TracedCallback<Ptr<const Packet> > m_txTrace;


        //Added
        Ipv4Address m_orcAddress;
        uint32_t m_orcPort;
        Ptr<Packet> m_packet;
        Ipv4Address m_newAddress;
        int m_newPort;
        int m_mecPort;
        int m_uePort;
        int m_packetSize;
        uint32_t m_cellId;

       double MSG_FREQ; //in messages/millisecond
       double MEAS_FREQ; //in measurements/millisecond
       double MEC_RATE; //in jobs per millisecond
       int UE_SIZE;

        uint32_t m_expectedResponseTime = 0;
        int m_noUes ;
        int m_noHandovers ;
        Time m_startTime;
        Time noSendUntil;
        std::string m_serverString;
        std::vector<InetSocketAddress> m_allServers;
        std::string m_ueString;
        std::vector<InetSocketAddress> m_allUes;
        Ipv4Address m_echoAddress;
        std::set<InetSocketAddress> myClients;
        uint32_t metric;
        Ipv4Address m_thisIpAddress;
        std::string m_filename;

        int responseTimeUpdateCounter;
        int ueTransferCounter;
        int echoCounter;
        int serviceRequestCounter;
        int pingRequestCounter;
        int handoverCommandCounter;
        int handoverDataCounter;
        int firstRequestCounter;

        Ptr<UniformRandomVariable> randomness;

        std::vector<std::pair<Ptr<Packet>, Time>> processingQueue;
        Ptr<ExponentialRandomVariable> processingTimer;
        std::fstream outfile;
    };


} // namespace ns3

#endif /* MEC_SERVER_APPLICATION_H */
