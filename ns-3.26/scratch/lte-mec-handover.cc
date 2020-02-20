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
#include "ns3/traffic-control-helper.h"
#include "ns3/traffic-control-layer.h"
#include "ns3/double.h"
#include "ns3/ns2-mobility-helper.h"
#include "ns3/packet.h"
#include "ns3/vector.h"
#include "ns3/queue.h"
#include <ns3/boolean.h>
#include <fstream>
#include <iostream>
//#include "ns3/gtk-config-store.h"

using namespace ns3;

/**
 * Sample simulation script for LTE+EPC. It instantiates several eNodeB,
 * attaches one UE per eNodeB starts a flow for each UE to  and from a remote host.
 * It also  starts yet another flow between each UE pair.
 */

NS_LOG_COMPONENT_DEFINE ("MecHandover");

//Experiment settings. DO NOT change between experiments
const double simTime = 5; //in seconds (it takes +- 900 seconds to drive a full circle)
const int numberOfUes = 2;

//Application-mimicking settings. DO NOT change between experiments
const uint32_t ORC_PACKET_SIZE = 256;
const uint32_t MEC_PACKET_SIZE = 512;
const uint32_t UE_PACKET_SIZE = 256;
const uint32_t PING_INTERVAL = 1000; //in ms
const uint32_t SERVICE_INTERVAL = 500; //in ms
const uint32_t WAITING_TIME_UPDATE_INTERVAL = 1000; //in ms
double MEC_RATE = 1; //in jobs per millisecond
const uint32_t UE_HANDOVER_SIZE = 200;


//do not change these values, they are hardcoded (for now)
unsigned int numberOfEnbs = 3;
double enb_distance = 4000.0;
unsigned int numberOfMecs = 3;
double mec_distance = 4000.0;
unsigned int numberOfRemoteHosts = numberOfMecs + 1; //One extra for the orchestrator


//Handover strategy settings. Change between experiments
int METRIC = 0; //Valid options are 0 for delay, 1 for distance
int TRIGGER = 0; //Valid options are 0 for optimal, 1 for hysteresis, 2 for threshold and 3 for threshold AND hysteresis
double HYSTERESIS = 0.3; //Value between 0 and 1 for setting the percentage another candidate's performance must be better than the current
int DELAY_THRESHOLD = 15; //If delay is higher than threshold, switch. In ms.
int DISTANCE_THRESHOLD = 0.5*mec_distance; //If distance is more than half the distance between MECs, switch. In meters.


//Mobility model variables. DO NOT change between experiments
std::string traceFile = "handoverMobility.tcl";

std::string EXPERIMENT_NAME = "/home/ubuntu/vtt_model/ns3-MEC/ns-3.26/results/results_ues" + std::to_string(numberOfUes) + "metric" + std::to_string(METRIC) + "trigger" + std::to_string(TRIGGER);
std::string timefile = EXPERIMENT_NAME + "TIMEFILE.txt";

// -------------------------------------------------------

InternetStackHelper internet;
PointToPointHelper p2ph;
ApplicationContainer mecApps;
ApplicationContainer orcApp;
ApplicationContainer ueApps;
NetDeviceContainer ueLteDevs;
NetDeviceContainer enbLteDevs;
NodeContainer remoteHostContainer;
Ptr <LteHelper> lteHelper;
Ptr <PointToPointEpcHelper> epcHelper;
Ipv4StaticRoutingHelper ipv4RoutingHelper;
std::vector<Ipv4Address> remoteAddresses;
NodeContainer ueNodes;
NodeContainer enbNodes;
std::map<Ptr<Node>, uint64_t> ueImsiMap;
std::vector<Ipv4Address> ueAddresses;
std::map <Ptr<Node>, Ptr<Node>> mecEnbMap;
Ptr<Node> pgw;
Ptr<Node> router;
std::string mecPositions;

void printNodeConfiguration(std::string name, Ptr<Node> node) {
    NS_LOG_DEBUG("________FOR " << name << "(" << node->GetId() << ")___________");

    NS_LOG_DEBUG("" << name << " MAC ADDRESSES________");
    for(unsigned int i_device = 0; i_device < node->GetNDevices(); i_device++) {
        Ptr<NetDevice> netDevice = node->GetDevice(i_device);
        NS_LOG_DEBUG("INTERFACE " << i_device << ":" << " MAC-ADDRESS " << (*netDevice).GetAddress());
    }

    NS_LOG_DEBUG("" << name <<" IP ADDRESSES________");
    Ptr<Ipv4> node_ipvs = node->GetObject<Ipv4>();

    for(unsigned int node_interface = 0; node_interface < node_ipvs->GetNInterfaces(); node_interface++) {
        unsigned int n_address = node_ipvs->GetNAddresses(node_interface);
        NS_LOG_DEBUG("INTERFACE " << node_interface << " HAS " << n_address << " ADDRESSES");
        for(unsigned int address_i = 0; address_i < n_address; address_i++) {
            NS_LOG_DEBUG("    IP: " << node_ipvs->GetAddress(node_interface, address_i));
        }
    }

    NS_LOG_DEBUG("node ROUTES________");
    Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper>(&std::cout);
    Ipv4GlobalRoutingHelper ipv4RoutingHelper;
    ipv4RoutingHelper.PrintRoutingTableAt(Seconds(0), node, routingStream);
}

Ipv4InterfaceAddress AssignIpv4Address (Ipv4Address ipv4Address, Ipv4Mask mask, Ptr<NetDevice> device) {
    Ptr<Node> node = device->GetNode ();
    NS_ASSERT_MSG (node, "Ipv4AddressHelper::Assign(): NetDevice is not not associated "
                         "with any node -> fail");

    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
    NS_ASSERT_MSG (ipv4, "Ipv4AddressHelper::Assign(): NetDevice is associated"
                         " with a node without IPv4 stack installed -> fail "
                         "(maybe need to use InternetStackHelper?)");

    int32_t interface = ipv4->GetInterfaceForDevice(device);
    if (interface == -1)
    {
        interface = ipv4->AddInterface(device);
    }
    NS_ASSERT_MSG (interface >= 0, "Ipv4AddressHelper::Assign(): "
                                   "Interface index not found");

    Ipv4InterfaceAddress ipv4Addr = Ipv4InterfaceAddress(ipv4Address, mask.Get());
    ipv4->AddAddress (interface, ipv4Addr);
    ipv4->SetMetric (interface, 1);
    ipv4->SetUp (interface);

    // Install the default traffic control configuration if the traffic
    // control layer has been aggregated, if this is not
    // a loopback interface, and there is no queue disc installed already
    Ptr<TrafficControlLayer> tc = node->GetObject<TrafficControlLayer> ();
    if (tc && DynamicCast<LoopbackNetDevice> (device) == 0 && tc->GetRootQueueDiscOnDevice (device) == 0)  {
        TrafficControlHelper tcHelper = TrafficControlHelper::Default ();
        tcHelper.Install (device);
    }
    return ipv4Addr;
}

void CreateRemoteHosts() {
    p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
    p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
    p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.000)));

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");

    // NODES
        //Setup PGW
    pgw = epcHelper->GetPgwNode();

        //Setup router
    router = CreateObject<Node>();
    internet.Install(router);

        //Setup remote hosts
    std::vector<Ptr<Node>> remoteHosts;
    for(unsigned int i_remote_host = 0; i_remote_host < numberOfRemoteHosts; i_remote_host++) {
        Ptr<Node> remoteHost_p = CreateObject<Node>();
        internet.Install(remoteHost_p);
        remoteHosts.push_back(remoteHost_p);
    }

    // LINKS
        //Setup interfaces between pgw & router
    NetDeviceContainer routerPGWLink = p2ph.Install(pgw, router);
    Ptr<NetDevice> pgw_iface = routerPGWLink.Get(0);
    Ptr<NetDevice> router_iface = routerPGWLink.Get(1);
    AssignIpv4Address(Ipv4Address("1.0.0.11"), Ipv4Mask("255.0.0.0"), pgw_iface);
    AssignIpv4Address(Ipv4Address("1.0.0.12"), Ipv4Mask("255.0.0.0"), router_iface);


        //Setup interfaces between router & remote hosts
    std::vector<std::string> remote_host_addresses = {"1.0.0.1",
                                                      "1.0.0.3",
                                                      "1.0.0.5",
                                                      "1.0.0.7",
//                                                      "1.0.0.9"
                                                        };
    std::vector<std::string> router_addresses = {"1.0.0.2",
                                                 "1.0.0.4",
                                                 "1.0.0.6",
                                                 "1.0.0.8",
//                                                 "1.0.0.10"
                                                };
    for(unsigned int i_remote_host = 0; i_remote_host < numberOfRemoteHosts; i_remote_host++) {
        Ptr<Node> remote_host = remoteHosts[i_remote_host];

        // Setup router & remote host interfaces
        NetDeviceContainer netDeviceContainer = p2ph.Install(router, remote_host);
        Ptr<NetDevice> router_iface = netDeviceContainer.Get(0);
        Ptr<NetDevice> remote_host_iface = netDeviceContainer.Get(1);

        // Assign ipv4 address to router and remote host interfaces
        Ipv4Address remote_host_address = Ipv4Address(remote_host_addresses[i_remote_host].c_str());
        AssignIpv4Address(Ipv4Address(router_addresses[i_remote_host].c_str()), Ipv4Mask("255.255.0.0"), router_iface);
        AssignIpv4Address(remote_host_address, Ipv4Mask("255.255.0.0"), remote_host_iface);

        remoteAddresses.push_back(remote_host_address);
    }

    // ROUTES
        //Router to Remote host routes
    Ptr<Ipv4StaticRouting> routerStaticRouting = ipv4RoutingHelper.GetStaticRouting(router->GetObject<Ipv4>());
    routerStaticRouting->AddHostRouteTo(Ipv4Address("1.0.0.1"), Ipv4Address("1.0.0.1"), 2);
    routerStaticRouting->AddHostRouteTo(Ipv4Address("1.0.0.3"), Ipv4Address("1.0.0.3"), 3);
    routerStaticRouting->AddHostRouteTo(Ipv4Address("1.0.0.5"), Ipv4Address("1.0.0.5"), 4);
    routerStaticRouting->AddHostRouteTo(Ipv4Address("1.0.0.7"), Ipv4Address("1.0.0.7"), 5);
//    routerStaticRouting->AddHostRouteTo(Ipv4Address("1.0.0.9"), Ipv4Address("1.0.0.9"), 6);

        //Router to PGW
    routerStaticRouting->AddNetworkRouteTo(Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.255.0.0"), Ipv4Address("1.0.0.11"), 1);

        //PGW routes
    Ptr<Ipv4StaticRouting> pgwStaticRouting = ipv4RoutingHelper.GetStaticRouting(pgw->GetObject<Ipv4>());
    pgwStaticRouting->AddNetworkRouteTo (Ipv4Address("7.0.0.0"), Ipv4Mask ("255.255.0.0"), 1);
    pgwStaticRouting->AddNetworkRouteTo (Ipv4Address("1.0.0.0"), Ipv4Mask ("255.255.0.0"), 2);

        //Routes from remote_host to network
    unsigned int n = 0;
    for(unsigned int i_remote_host = 0; i_remote_host < numberOfRemoteHosts; i_remote_host++) {
        Ptr<Node> remote_host = remoteHosts[i_remote_host];

        Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remote_host->GetObject<Ipv4>());
        std::string router_address_str = router_addresses[i_remote_host];
        remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask ("255.255.0.0"), Ipv4Address(router_address_str.c_str()), 1);
        remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("1.0.0.0"), Ipv4Mask ("255.255.0.0"), Ipv4Address(router_address_str.c_str()), 1);
        n+=2;
    }

    for(unsigned int i_remote_host = 0; i_remote_host < numberOfRemoteHosts; i_remote_host++) {
        Ptr<Node> remote_host = remoteHosts[i_remote_host];
        remoteHostContainer.Add(remote_host);
    }
}

void InstallInfrastructureMobility(){
    // Install Mobility Model
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
    for (uint16_t i = 0; i < numberOfEnbs; i++)
    {
        enbPositionAlloc->Add (Vector((0.5*enb_distance + enb_distance * i), 5, 0));
    }
    Ptr<ListPositionAllocator> mecPositionAlloc = CreateObject<ListPositionAllocator> ();
    for (uint16_t i = 0; i < numberOfMecs; i++)
    {
        int x = (0.5*mec_distance + i*mec_distance);
        int y = 4;
        int z = 0;
        mecPositionAlloc->Add (Vector(x, y, z));
        mecPositions.append(std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(z) + ",/");
    }
    MobilityHelper constantPositionMobility;
    constantPositionMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    constantPositionMobility.SetPositionAllocator(mecPositionAlloc);
    constantPositionMobility.Install(remoteHostContainer);
    constantPositionMobility.SetPositionAllocator(enbPositionAlloc);
    constantPositionMobility.Install(enbNodes);
}


void InstallConstantMobilityModels(){

    InstallInfrastructureMobility();

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
}

void InstallTraceMobilityModels(){
    InstallInfrastructureMobility();

    Ns2MobilityHelper ns2 = Ns2MobilityHelper (traceFile);
    ns2.Install(ueNodes.Begin(), ueNodes.End());
}

void InstallConstantPositionMobilityModels() {
    InstallInfrastructureMobility();
    Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator> ();
    for (uint16_t i = 0; i < numberOfUes; i++)
    {
        uePositionAlloc->Add (Vector(2000, 10, 0));
    }
    MobilityHelper constantPositionMobility;
    constantPositionMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    constantPositionMobility.SetPositionAllocator(uePositionAlloc);
    constantPositionMobility.Install(ueNodes);
}

void InstallLteDevices(){
    // Install LTE Devices to the nodes

    enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);


    for (uint32_t i = 0; i < ueNodes.GetN(); i++){
        Ptr<Node> ue = ueNodes.Get(i);
        std::pair<Ptr<NetDevice>, uint64_t> installResult = lteHelper->InstallSingleUeDeviceMec(ue);
        Ptr<NetDevice> netDevice = installResult.first;
        uint64_t imsi = installResult.second;
        ueImsiMap.insert(std::pair<Ptr<Node>, uint64_t>(ue,imsi));
        ueLteDevs.Add(netDevice);
    }

    // Link each MEC to an eNB
    for (unsigned int i = 0; i < numberOfEnbs; i++){
        //Assign a MEC to each eNB
        mecEnbMap[remoteHostContainer.Get(i+1)] = enbNodes.Get(i);
    }

    lteHelper->AddX2Interface (enbNodes);

//    for (unsigned int i = numberOfEnbs; i < remoteHostContainer.GetN(); i++){
//        //Assign every "extra" MEC to a random eNB
//        int index = rand()%(numberOfEnbs);
//        Ptr<Node> node = remoteHostContainer.Get(i);
//        Ptr<Node> value = enbNodes.Get(index);
//        mecEnbMap[node] = value;
//    }
}


void InstallUeNodes(){
    // Install the IP stack on the UEs
    internet.Install (ueNodes);
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));
    // Assign IP address to UEs, and install applications
    for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
        Ptr<Node> ueNode = ueNodes.Get (u);
        // Set the default gateway for the UE
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
        ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }
    for (uint16_t i = 0; i < numberOfUes; i++)
    {
        Ptr<NetDevice> ueDev = ueLteDevs.Get(i);
//        Ptr<NetDevice> enbDev = enbLteDevs.Get(0);
        Ptr<NetDevice> enbDev = enbLteDevs.Get(0);
        lteHelper->Attach (ueDev, enbDev);
        // side effect: the default EPS bearer will be activated
    }

    NodeContainer::Iterator m;
    for (m = ueNodes.Begin (); m != ueNodes.End (); ++m) {
        Ptr<Ipv4> ipv4 = (*m)->GetObject<Ipv4>();
        Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1,0);
        Ipv4Address address = iaddr.GetLocal();
        ueAddresses.push_back(address);
    }
}

void InstallApplications(){
    // Install and start applications on UEs and remote hosts

    //Install MEC applications
    //Make string of all MEC IP addresses (port is always 1001)
    std::string mecString;
    for (int i = 1; i < int(remoteAddresses.size()); i++){
        //Serialize address
        Ipv4Address addr = remoteAddresses[i];
        std::stringstream ss;
        std::stringstream os;
        addr.Print(os);
        ss << os.rdbuf();
        std::string addrString = ss.str();

        //Add address to mecString (with "/" as separator)
        mecString.append(addrString + "/");
    }

    //Make string of all UE IP addresses (port is always 1002)
    std::string ueString;
    for (int i = 0; i < int(ueAddresses.size()); i++){
        //Serialize address
        Ipv4Address addr = ueAddresses[i];
        std::stringstream ss;
        std::stringstream os;
        addr.Print(os);
        ss << os.rdbuf();
        std::string ueAddrString = ss.str();

        //Add address to mecString (with "/" as separator)
        ueString.append(ueAddrString + "/");
    }

    //Install ORC application
    Ptr<Node> orcNode = remoteHostContainer.Get(0);
    InetSocketAddress orcAddress = InetSocketAddress(remoteAddresses[0], 1000);

    for(int i = 0; i<int(TypeId::GetRegisteredN()); i++){
        TypeId temp = TypeId::GetRegistered(i);
        std::string name = temp.GetName();
    }
    ObjectFactory m_factory = ObjectFactory("ns3::MecOrcApplication");
    m_factory.Set ("PacketSize", UintegerValue(ORC_PACKET_SIZE));
    m_factory.Set ("AllServers", StringValue(mecString));
    m_factory.Set ("AllUes", StringValue(ueString));
    m_factory.Set ("UePort", UintegerValue(1000));
    m_factory.Set ("MecPort", UintegerValue(1000));
    m_factory.Set ("Trigger", UintegerValue(TRIGGER));
    m_factory.Set ("Hysteresis", DoubleValue(HYSTERESIS));
    m_factory.Set ("DelayThreshold", UintegerValue(DELAY_THRESHOLD));
    m_factory.Set ("MecPositions", StringValue(mecPositions));
    m_factory.Set ("DistanceThreshold", UintegerValue(DISTANCE_THRESHOLD));
    m_factory.Set("Logfile", StringValue(EXPERIMENT_NAME +  "ORC.txt"));

    Ptr<Application> app = m_factory.Create<Application> ();
    orcNode->AddApplication (app);
    orcApp.Add(app);


    //  Install MEC applications
    for (int i = 1; i < int(remoteHostContainer.GetN()); ++i){
        Ptr<Node> node = remoteHostContainer.Get(i);
        Ptr<Node> enbNode = mecEnbMap[node];
        Ptr<NetDevice> device = enbNode->GetDevice(0);
        Ptr<LteEnbNetDevice> netDevice = dynamic_cast<LteEnbNetDevice*>(PeekPointer (device));
        uint16_t cellId = netDevice->GetCellId();

        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1,0);
        Ipv4Address ipAddr = iaddr.GetLocal();

        ObjectFactory m_factory = ObjectFactory("ns3::MecHoServerApplication");
        m_factory.Set ("UpdateInterval", UintegerValue(WAITING_TIME_UPDATE_INTERVAL));
        m_factory.Set ("ServiceInterval", UintegerValue(SERVICE_INTERVAL));
        m_factory.Set ("MeasurementInterval", UintegerValue(PING_INTERVAL));
        m_factory.Set ("PacketSize", UintegerValue(MEC_PACKET_SIZE));
        m_factory.Set ("UeHandoverSize", UintegerValue(UE_HANDOVER_SIZE));
        m_factory.Set ("MecRate", DoubleValue(MEC_RATE));
        m_factory.Set ("OrcAddress", Ipv4AddressValue(orcAddress.GetIpv4()));
        m_factory.Set("OrcPort", UintegerValue(orcAddress.GetPort()));
        m_factory.Set ("CellID", UintegerValue(cellId) );
        m_factory.Set ("AllServers", StringValue(mecString));
        m_factory.Set("MecPort", UintegerValue(1000));
        m_factory.Set("AllUes", StringValue(ueString));
        m_factory.Set("UePort", UintegerValue(1000));
        m_factory.Set("NumberOfUes", UintegerValue(numberOfUes));
        m_factory.Set ("Metric", UintegerValue(METRIC));
        m_factory.Set("Local", Ipv4AddressValue(ipAddr));
        m_factory.Set("Logfile", StringValue(EXPERIMENT_NAME + "MEC" + std::to_string(i) + ".txt"));

        Ptr<Application> app = m_factory.Create<Application> ();
        node->AddApplication(app);
        mecApps.Add(app);
    }

    //Install UE applications
    for (int i = 0; i < int(remoteAddresses.size()); ++i){
    }
    for (int i = 0; i < int(ueNodes.GetN()); ++i){
        Ptr<Node> node = ueNodes.Get(i);
        uint64_t ueImsi = ueImsiMap.find(node)->second;

        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1,0);
        Ipv4Address ipAddr = iaddr.GetLocal();

        ObjectFactory m_factory = ObjectFactory("ns3::MecUeApplication");
        m_factory.Set("PacketSize", UintegerValue(UE_PACKET_SIZE));
        m_factory.Set("ServiceInterval", TimeValue(MilliSeconds(SERVICE_INTERVAL)));
        m_factory.Set("PingInterval", TimeValue(MilliSeconds(PING_INTERVAL)));
        m_factory.Set ("MecIp", Ipv4AddressValue(remoteAddresses[1]));
        m_factory.Set("MecPort", UintegerValue(1000));
        m_factory.Set ("OrcIp", Ipv4AddressValue(orcAddress.GetIpv4()));
        m_factory.Set("OrcPort", UintegerValue(orcAddress.GetPort()));
        m_factory.Set("Local", Ipv4AddressValue(ipAddr));
        m_factory.Set ("AllServers", StringValue(mecString));
        m_factory.Set ("Enb0", PointerValue(enbLteDevs.Get(0)));
        m_factory.Set ("Enb1", PointerValue(enbLteDevs.Get(1)));
        m_factory.Set ("Enb2", PointerValue(enbLteDevs.Get(2)));
        m_factory.Set("Node", PointerValue(node));
        m_factory.Set("ueImsi", UintegerValue(ueImsi));
        m_factory.Set("PingOffset", TimeValue(MilliSeconds(i*(PING_INTERVAL/numberOfUes))));
        m_factory.Set("ServiceOffset", TimeValue(MilliSeconds(i*(SERVICE_INTERVAL/numberOfUes))));
        m_factory.Set("Metric", UintegerValue(METRIC));
        m_factory.Set("Router", PointerValue(router));
        m_factory.Set("Logfile", StringValue(EXPERIMENT_NAME + "UE" + std::to_string(i) + ".txt"));


        Ptr<Application> app = m_factory.Create<Application> ();
        node->AddApplication (app);

        ueApps.Add(ueApps);
    }
}

int StartSimulation(){
    orcApp.Start(Simulator::Now());
    mecApps.Start (Simulator::Now());
    ueApps.Start (Simulator::Now());


    lteHelper->EnableTraces ();
//     Uncomment to enable PCAP tracing
//    p2ph.EnablePcapAll("lena-epc-first");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    /*GtkConfigStore config;
    config.ConfigureAttributes();*/

    Simulator::Destroy();
    return 0;
}


int
main (int argc, char *argv[]) {

    lteHelper = CreateObject<LteHelper>();
    lteHelper->SetHandoverAlgorithmType("ns3::A2A4RsrqHandoverAlgorithm");
    epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    CreateRemoteHosts();


    enbNodes.Create(numberOfEnbs);
    ueNodes.Create(numberOfUes);


    InstallTraceMobilityModels();
//    InstallConstantMobilityModels();
//    InstallConstantPositionMobilityModels();

    InstallLteDevices();

    InstallUeNodes();

    InstallApplications();

    Packet::EnablePrinting();

//    NS_LOG_DEBUG("Show resulting configuration__________________");
//    printNodeConfiguration("ROUTER", router);
//    printNodeConfiguration("PGW", pgw);
//
//    // Remove hosts
//    for(unsigned int i_remote_host = 0; i_remote_host < numberOfRemoteHosts; i_remote_host++) {
//        Ptr<Node> remote_host = remoteHostContainer.Get(i_remote_host);
//        printNodeConfiguration("Node " + std::to_string(remote_host->GetId()), remote_host);
//    }
//
//    // ENB
//    for(unsigned int i_enb = 0; i_enb < numberOfEnbs; i_enb++) {
//        Ptr<Node> enb = enbNodes.Get(i_enb);
//        printNodeConfiguration("ENB " + std::to_string(enb->GetId()), enb);
//    }
//
//    // UE
//    for(unsigned int i_ue = 0; i_ue < numberOfUes; i_ue++) {
//        Ptr<Node> ue = ueNodes.Get(i_ue);
//        printNodeConfiguration("UE " + std::to_string(ue->GetId()), ue);
//    }
    auto experiment_start = std::chrono::system_clock::now();
    int simResult = StartSimulation();
    auto experiment_end = std::chrono::system_clock::now();

    std::chrono::duration<double> elapsed_seconds = experiment_end-experiment_start;
    std::time_t end_time = std::chrono::system_clock::to_time_t(experiment_end);

    std::fstream outfile;
    outfile.open(timefile, std::ios::app);
    outfile << "Finished experiment at " << std::ctime(&end_time) << ". Elapsed time: " << elapsed_seconds.count() << " seconds." << std::endl;

    return simResult;


    //END MAIN
}

