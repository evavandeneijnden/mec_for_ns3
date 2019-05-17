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
         * \brief set the remote address and port
         * \param ip remote IP address
         * \param port remote port
         */
        void SetMec (Address ip, uint16_t port);


        /**
         * Set the data fill of the packet (what is sent as data to the server) to
         * the contents of the fill buffer, repeated as many times as is required.
         *
         * Initializing the packet to the contents of a provided single buffer is
         * accomplished by setting the fillSize set to your desired dataSize
         * (and providing an appropriate buffer).
         *
         * \warning The size of resulting echo packets will be automatically adjusted
         * to reflect the dataSize parameter -- this means that the PacketSize
         * attribute of the Application may be changed as a result of this call.
         *
         * \param fill The fill pattern to use when constructing packets.
         * \param fillSize The number of bytes in the provided fill pattern.
         * \param dataSize The desired size of the final echo data.
         */
        void SetFill (uint8_t *fill, uint32_t fillSize, uint32_t dataSize);

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
        void SendMeasurementReport (std::map<Ipv4Address, uint64_t> measurementReport);



        uint32_t m_count; //!< Maximum number of packets the application will send
        Time m_serviceInterval; //!< Packet inter-send time
        Time m_pingInterval;
        uint32_t m_size; //!< Size of the sent packet


        uint32_t m_sent; //!< Counter for sent packets
        Ptr<Socket> m_socket; //!< Socket
        Address m_mecAddress; //!< Remote peer address
        uint16_t m_mecPort; //!< Remote peer port
        m_sendPingEvent;
        m_sendServiceEvent;

        /// Callbacks for tracing the packet Tx events
        TracedCallback<Ptr<const Packet> > m_txTrace;


        //Added
        InetSocketAddress m_orcAddress;
        Time m_noSendUntil;
        Time m_requestSent;
        bool m_requestBlocked;
        std::map<Ipv4Address,int64_t> m_measurementReport; //First argument is MECs address, second is observed delay in ms
        std::map<Ipv4Address,Time> m_pingSent;
        std::vector<InetSocketAddress> m_mecAddresses;
        uint8_t *m_data_request;
        uint8_t *m_data_ping;
        uint8_t *m_data_report;
        Ptr<Node> m_thisNode;
        Ptr<NetDevice> m_thisNetDevice;
        Ipv4Address m_thisIpAddress;
        uint32_t m_packetSize;
    };

} // namespace ns3

#endif /* MEC_UE_APPLICATION_H */
