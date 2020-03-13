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

#ifndef MEC_ORC_APPLICATION_H
#define MEC_ORC_APPLICATION_H

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
#include "ns3/double.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/vector.h"
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
    class MecOrcApplication : public Application
    {
    public:
        /**
         * \brief Get the type ID.
         * \return the object TypeId
         */
        static TypeId GetTypeId (void);

        MecOrcApplication ();

        virtual ~MecOrcApplication ();

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

        void SendMecHandover(InetSocketAddress ueAddress, InetSocketAddress newMecAddress, InetSocketAddress currentMecAddress);
        void SendUeHandover (InetSocketAddress ueAddress, InetSocketAddress newMecAddress, InetSocketAddress currentMecAddress);

        uint32_t m_count; //!< Maximum number of packets the application will send

        uint32_t m_sent; //!< Counter for sent packets
        Ptr<Socket> m_socket; //!< Socket
        EventId m_sendUeEvent;
        EventId m_sendMecEvent;

        /// Callbacks for tracing the packet Tx events
        TracedCallback<Ptr<const Packet> > m_txTrace;

        //Added
        uint8_t *m_data_ue;
        uint8_t *m_data_mec;
        uint32_t m_packetSize;

        char trigger;
        double hysteresis;
        uint32_t delay_threshold;
        uint32_t distance_threshold;
        std::string mecPositions;

        std::string m_serverString;
        std::string m_ueString;
        uint32_t m_uePort;
        uint32_t m_mecPort;
        std::vector<InetSocketAddress> m_allServers;
        std::vector<InetSocketAddress> m_allUes;
        std::vector<Vector> allMecPositions;
        std::map<InetSocketAddress, int> responseTimes; //Current waiting times for each MEC by InetSocketAddress. In ms.
        std::string m_filename;

        int ueHandoverCounter;
        int mecHandoverCounter;
        int measurementReportCounter;
        int responseTimeUpdateCounter;
        int locationUpdateCounter;

        std::fstream outfile;

    };

} // namespace ns3

#endif /* MEC_ORC_APPLICATION_H */
