/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
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
 *
 * Adapted from lena-simple-epc by Eva van den Eijnden
 */

#include "ns3/lte-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/config-store.h"
//#include "ns3/gtk-config-store.h"

using namespace ns3;

/**
 * Sample simulation script for LTE+EPC. It instantiates several eNodeB,
 * attaches one UE per eNodeB starts a flow for each UE to  and from a remote host.
 * It also  starts yet another flow between each UE pair.
 */

NS_LOG_COMPONENT_DEFINE ("HandoverTest");


int
main (int argc, char *argv[])
{

    double simTime = 5.0;
    double interPacketInterval = 100;
    int numberOfUes = 2;
    int numberOfEnbs = 3; //Hardcoded to 3 at the moment!
    double enb_distance = 4000.0;
    int numberOfMecs = 4;
    double mec_distance = 3000.0;
    double numberOfRemoteHosts = numberOfMecs +1; //One extra for the orchestrator
    std::map<Ptr<Node>, Ptr<Node>> mecEnbMap;

    // Command line arguments
    CommandLine cmd;
    cmd.AddValue("numberOfMecs", "Number of MECs in the simulation", numberOfMecs);
    cmd.AddValue("simTime", "Total duration of the simulation [s])", simTime);
    cmd.AddValue("interPacketInterval", "Inter packet interval [ms])", interPacketInterval);
    cmd.Parse(argc, argv);

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
    Ptr<PointToPointEpcHelper>  epcHelper = CreateObject<PointToPointEpcHelper> ();
    lteHelper->SetEpcHelper (epcHelper);

    ConfigStore inputConfig;
    inputConfig.ConfigureDefaults();

    // parse again so you can override default values from the command line
    cmd.Parse(argc, argv);

    Ptr<Node> pgw = epcHelper->GetPgwNode ();
    NS_LOG_DEBUG("PGW is node " << pgw->GetId());

    // Create RemoteHosts
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create (numberOfRemoteHosts);
    InternetStackHelper internet;
    internet.Install (remoteHostContainer);

    // Create the Internet
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
    p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
    p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.000)));
    NetDeviceContainer internetDevices;
    NodeContainer::Iterator i;
    for (i = remoteHostContainer.Begin (); i != remoteHostContainer.End (); ++i) {
        internetDevices.Add(p2ph.Install (pgw, *i));
    }
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
    // interface 0 is localhost, 1 is the p2p device
    std::vector<Ipv4Address> remoteAddresses;
    for(uint16_t i = 1; i<internetIpIfaces.GetN(); i+=2){
        remoteAddresses.push_back(internetIpIfaces.GetAddress(i));
    }
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    NodeContainer::Iterator j;
    for (j = remoteHostContainer.Begin (); j != remoteHostContainer.End (); ++j) {
        NS_LOG_DEBUG("Setting IP addresses for server/ORC");
        Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting ((*j)->GetObject<Ipv4> ());
        remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);
    }

    //Logging for who-is-who detection
    NodeContainer::Iterator k;
    for (k = remoteHostContainer.Begin (); k != remoteHostContainer.End (); ++k) {
        Ptr<Ipv4> ipv4 = (*k)->GetObject<Ipv4>();
        Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1,0);
        Ipv4Address address = iaddr.GetLocal();
        NS_LOG_DEBUG("Node (ORC/SERVER) " << (*k)->GetId() << ",address: " << address);
    }

    NodeContainer ueNodes;
    NodeContainer enbNodes;
    enbNodes.Create(numberOfEnbs);
    ueNodes.Create(numberOfUes);

    // Install Mobility Model
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
    for (uint16_t i = 0; i < numberOfEnbs; i++)
    {
        enbPositionAlloc->Add (Vector((0.5*enb_distance + enb_distance * i), 9, 0));
    }
    Ptr<ListPositionAllocator> mecPositionAlloc = CreateObject<ListPositionAllocator> ();
    for (uint16_t i = 0; i < numberOfMecs; i++)
    {
        mecPositionAlloc->Add (Vector((0.5*mec_distance + i*mec_distance), 9, 0));
    }
    MobilityHelper constantPositionMobility;
    constantPositionMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    constantPositionMobility.SetPositionAllocator(mecPositionAlloc);
    constantPositionMobility.Install(remoteHostContainer);
    constantPositionMobility.SetPositionAllocator(enbPositionAlloc);
    constantPositionMobility.Install(enbNodes);

    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    ueMobility.Install(ueNodes);

    for(int i = 0; i<numberOfUes; i++){
        Ptr<Node> node = ueNodes.Get(i);
        Ptr<ConstantVelocityMobilityModel> mm = node->GetObject<ConstantVelocityMobilityModel>();
        int strat_int = (i%4);
        if(strat_int < 2){
            mm->SetPosition({0.0,double(strat_int*2 + 1),0.0});
            mm->SetVelocity({27.8, 0, 0});
        }
        else {
            mm->SetPosition({12000.0,double(strat_int*2 + 1),0.0});
            mm->SetVelocity({-27.8, 0, 0});
        }
    }

    // Install LTE Devices to the nodes

    NS_LOG_DEBUG("Install eNBs");
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
    Ptr<NetDevice> thisDevice = (enbLteDevs.Get(0));
    Ptr<LteUeNetDevice> lteDevice = (LteUeNetDevice*) &thisDevice;
    NS_LOG_DEBUG("Install UEs");
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);


    // Link each MEC to an eNB
    for (int i = 0; i<numberOfEnbs; i++){
        //Assign a MEC to each eNB
        mecEnbMap[remoteHostContainer.Get(i+1)] = enbNodes.Get(i);
    }

    for (int i = numberOfEnbs; i<int(remoteHostContainer.GetN()); i++){
        //Assign every "extra" MEC to a random eNB
        int index = rand()%(numberOfEnbs);
        Ptr<Node> node = remoteHostContainer.Get(i);
        Ptr<Node> value = enbNodes.Get(index);
        mecEnbMap[node] = value;
    }

    // Install the IP stack on the UEs
    internet.Install (ueNodes);
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));
    // Assign IP address to UEs, and install applications
    for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
        NS_LOG_DEBUG("Give UE IP stack");
        Ptr<Node> ueNode = ueNodes.Get (u);
        // Set the default gateway for the UE
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
        ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

    // Attach one UE per eNodeB
    for (uint16_t i = 0; i < numberOfUes; i++)
    {
        lteHelper->Attach (ueLteDevs.Get(i), enbLteDevs.Get(1));
        // side effect: the default EPS bearer will be activated
    }

    NodeContainer::Iterator l;
    for (l = enbNodes.Begin (); l != enbNodes.End (); ++l) {
        Ptr<Ipv4> ipv4 = (*l)->GetObject<Ipv4>();
        Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1,0);
        Ipv4Address address = iaddr.GetLocal();
        NS_LOG_DEBUG("Node (ENB) " << (*l)->GetId() << ", address: " << address);

    }
    NodeContainer::Iterator m;
    for (m = ueNodes.Begin (); m != ueNodes.End (); ++m) {
        Ptr<Ipv4> ipv4 = (*m)->GetObject<Ipv4>();
        Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1,0);
        Ipv4Address address = iaddr.GetLocal();
        NS_LOG_DEBUG("Node (UE) " << (*m)->GetId() << ", address: " << address);
    }

    // Install and start applications on UEs and remote hosts

    //UE applications: pass along argument for ORC inetsocketaddress; always first in remoteHost list?
    ApplicationContainer mecApps;
    ApplicationContainer orcApp;
    ApplicationContainer ueApps;


    //Install ORC application
    Ptr<Node> orcNode = remoteHostContainer.Get(0);
    InetSocketAddress orcAddress = InetSocketAddress(remoteAddresses[0], 1000);

//    for(int i = 0; i<int(TypeId::GetRegisteredN()); i++){
//        TypeId temp = TypeId::GetRegistered(i);
//        std::string name = temp.GetName();
//    }
//
//    ObjectFactory m_factory = ObjectFactory("ns3::MecOrcApplication");
//    m_factory.Set ("MaxPackets", UintegerValue(10000));
//    m_factory.Set ("PacketSize", UintegerValue(ORC_PACKET_SIZE));
//
//    Ptr<Application> app = m_factory.Create<Application> ();
//    orcNode->AddApplication (app);
//    orcApp.Add(app);
//    NS_LOG_DEBUG("Application (type ORC): " << app << " on node " << orcNode->GetId());


    uint16_t dlPort = 1234;
    uint16_t ulPort = 2000;
    uint16_t otherPort = 3000;
    ApplicationContainer clientApps;
    ApplicationContainer serverApps;

    //Install MEC applications
    NS_LOG_DEBUG("N: " << remoteHostContainer.GetN());
    for (uint32_t u = 0; u < ueNodes.GetN (); ++u){
        NS_LOG_DEBUG("Test 1");
        ++ulPort;
        ++otherPort;
        PacketSinkHelper dlPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPort));
        PacketSinkHelper ulPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), ulPort));
        PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), otherPort));
        serverApps.Add (dlPacketSinkHelper.Install (ueNodes.Get(u)));
        serverApps.Add (ulPacketSinkHelper.Install (orcNode));
        serverApps.Add (packetSinkHelper.Install (ueNodes.Get(u)));

        UdpClientHelper dlClient (ueIpIface.GetAddress (u), dlPort);
        dlClient.SetAttribute ("Interval", TimeValue (MilliSeconds(interPacketInterval)));
        dlClient.SetAttribute ("MaxPackets", UintegerValue(1000000));

        UdpClientHelper ulClient (orcAddress, ulPort);
        ulClient.SetAttribute ("Interval", TimeValue (MilliSeconds(interPacketInterval)));
        ulClient.SetAttribute ("MaxPackets", UintegerValue(1000000));

        UdpClientHelper client (ueIpIface.GetAddress (u), otherPort);
        client.SetAttribute ("Interval", TimeValue (MilliSeconds(interPacketInterval)));
        client.SetAttribute ("MaxPackets", UintegerValue(1000000));

        clientApps.Add (dlClient.Install (orcNode));
        clientApps.Add (ulClient.Install (ueNodes.Get(u)));
        if (u+1 < ueNodes.GetN ())
        {
            clientApps.Add (client.Install (ueNodes.Get(u+1)));
        }
        else
        {
            clientApps.Add (client.Install (ueNodes.Get(0)));
        }
        NS_LOG_DEBUG("Test 2");


    }

    //Install UE applications

    for (uint32_t u = 0; u < ueNodes.GetN (); ++u){
        NS_LOG_DEBUG("Test 3");
        ++ulPort;
        ++otherPort;
        PacketSinkHelper dlPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPort));
        PacketSinkHelper ulPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), ulPort));
        PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), otherPort));
        serverApps.Add (dlPacketSinkHelper.Install (ueNodes.Get(u)));
        serverApps.Add (ulPacketSinkHelper.Install (orcNode));
        serverApps.Add (packetSinkHelper.Install (ueNodes.Get(u)));

        UdpClientHelper dlClient (ueIpIface.GetAddress (u), dlPort);
        dlClient.SetAttribute ("Interval", TimeValue (MilliSeconds(interPacketInterval)));
        dlClient.SetAttribute ("MaxPackets", UintegerValue(1000000));

        UdpClientHelper ulClient (orcAddress, ulPort);
        ulClient.SetAttribute ("Interval", TimeValue (MilliSeconds(interPacketInterval)));
        ulClient.SetAttribute ("MaxPackets", UintegerValue(1000000));

        UdpClientHelper client (ueIpIface.GetAddress (u), otherPort);
        client.SetAttribute ("Interval", TimeValue (MilliSeconds(interPacketInterval)));
        client.SetAttribute ("MaxPackets", UintegerValue(1000000));

        clientApps.Add (dlClient.Install (orcNode));
        clientApps.Add (ulClient.Install (ueNodes.Get(u)));
        if (u+1 < ueNodes.GetN ())
        {
            clientApps.Add (client.Install (ueNodes.Get(u+1)));
        }
        else
        {
            clientApps.Add (client.Install (ueNodes.Get(0)));
        }
        NS_LOG_DEBUG("Test 4");


    }

    NS_LOG_DEBUG("Test 5");
    serverApps.Start (Seconds (0.01));
    clientApps.Start (Seconds (0.01));

    lteHelper->EnableTraces ();
//     Uncomment to enable PCAP tracing
//    p2ph.EnablePcapAll("tests");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    /*GtkConfigStore config;
    config.ConfigureAttributes();*/

    Simulator::Destroy();
    return 0;

}

