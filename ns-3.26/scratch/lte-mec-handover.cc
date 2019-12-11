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
//#include "ns3/gtk-config-store.h"

using namespace ns3;

/**
 * Sample simulation script for LTE+EPC. It instantiates several eNodeB,
 * attaches one UE per eNodeB starts a flow for each UE to  and from a remote host.
 * It also  starts yet another flow between each UE pair.
 */

NS_LOG_COMPONENT_DEFINE ("MecHandover");

//Experiment settings. DO NOT change between experiments
double simTime = 5; //in seconds
int numberOfUes = 2;

//Application-mimicking settings. DO NOT change between experiments
uint32_t ORC_PACKET_SIZE = 512;
uint32_t MEC_PACKET_SIZE = 512;
uint32_t MEC_UPDATE_INTERVAL = 1000; // in ms
uint32_t UE_PACKET_SIZE = 1024;
uint32_t PING_INTERVAL = 1000; //in ms
uint32_t SERVICE_INTERVAL = 100; //in ms


//Handover strategy settings. Change between experiments
int TRIGGER = 0; //Valid options are 0 for optimal, 1 for hysteresis, 2 for threshold and 3 for threshold AND hysteresis
double HYSTERESIS = 0.3; //Value between 0 and 1 for setting the percentage another candidate's performance must be better than the current
int DELAY_THRESHOLD = 20; //If delay is higher than threshold, switch. In ms.


//do not change these values, they are hardcoded (for now)
int numberOfEnbs = 3;
double enb_distance = 4000.0;
int numberOfMecs = 4;
double mec_distance = 3000.0;
double numberOfRemoteHosts = numberOfMecs + 1; //One extra for the orchestrator
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
    Ptr<Node> pgw = epcHelper->GetPgwNode();

        //Setup router

    Ptr<Node> router = CreateObject<Node>();
    internet.Install(router);

        //Setup remote hosts
    std::vector<Ptr<Node>> remoteHosts;
    for(unsigned int i_remote_host = 0; i_remote_host < numberOfRemoteHosts; i_remote_host++) {
//        NS_LOG_DEBUG("Setting up server/ORC" << i_remote_host);
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
                                                      "1.0.0.9"};
    std::vector<std::string> router_addresses = {"1.0.0.2",
                                                 "1.0.0.4",
                                                 "1.0.0.6",
                                                 "1.0.0.8",
                                                 "1.0.0.10"};
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
    routerStaticRouting->AddHostRouteTo(Ipv4Address("1.0.0.9"), Ipv4Address("1.0.0.9"), 6);

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

//    NS_LOG_DEBUG("Show resulting configuration__________________");
//    printNodeConfiguration("ROUTER", router);
//    printNodeConfiguration("PGW", pgw);
//
//    for(unsigned int i_remote_host = 0; i_remote_host < numberOfRemoteHosts; i_remote_host++) {
//        Ptr<Node> remote_host = remoteHosts[i_remote_host];
//        printNodeConfiguration("Node " + std::to_string(remote_host->GetId()), remote_host);
//    }


    for(unsigned int i_remote_host = 0; i_remote_host < numberOfRemoteHosts; i_remote_host++) {
        Ptr<Node> remote_host = remoteHosts[i_remote_host];
        remoteHostContainer.Add(remote_host);
    }
}

void InstallMobilityModels(){
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
    // Attach one UE per eNodeB
    //TODO when scaling the experiment up, check that this works
    for (uint16_t i = 0; i < numberOfUes; i++)
    {
        Ptr<NetDevice> ueDev = ueLteDevs.Get(i);
        Ptr<NetDevice> enbDev = enbLteDevs.Get(0);
        lteHelper->Attach (ueDev, enbDev);
        // side effect: the default EPS bearer will be activated
    }

    NodeContainer::Iterator l;
    for (l = enbNodes.Begin (); l != enbNodes.End (); ++l) {
//        Ptr<Ipv4> ipv4 = (*l)->GetObject<Ipv4>();
//        Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1,0);
//        Ipv4Address address = iaddr.GetLocal();
//        NS_LOG_DEBUG("Node (ENB) " << (*l)->GetId() << ", address: " << address);

//        NS_LOG_DEBUG("FOR ENB (ID: " << (*l)->GetId() << ")");
//        unsigned int t = 0;
//        while(t < (*l)->GetNDevices()) {
//            Ptr<NetDevice> netDevice = (*l)->GetDevice(t);
//            NS_LOG_DEBUG("FOUND NETDEVICE:" << " ADDRESS " << (*netDevice).GetAddress());
//            t++;
//        }
//
//        Ptr<Ipv4> pgw_ipvs = (*l)->GetObject<Ipv4>();
//        unsigned int pgw_interface = 0;
//        while(pgw_interface < pgw_ipvs->GetNInterfaces()) {
//            unsigned int n_address = pgw_ipvs->GetNAddresses(pgw_interface);
//            NS_LOG_DEBUG("INTERFACE " << pgw_interface << " HAS " << n_address << " ADDRESSES ");
//            unsigned int address_i = 0;
//            while(address_i < n_address) {
//                NS_LOG_DEBUG("HAS IP ADDRESS: " << pgw_ipvs->GetAddress(pgw_interface, address_i));
//                address_i++;
//            }
//            pgw_interface++;
//        }
    }
    NodeContainer::Iterator m;
    for (m = ueNodes.Begin (); m != ueNodes.End (); ++m) {
        Ptr<Ipv4> ipv4 = (*m)->GetObject<Ipv4>();
        Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1,0);
        Ipv4Address address = iaddr.GetLocal();
        ueAddresses.push_back(address);
//        NS_LOG_DEBUG("Node (UE) " << (*m)->GetId() << ", address: " << address);
    }
}

void InstallApplications(){
    // Install and start applications on UEs and remote hosts

    //UE applications: pass along argument for ORC inetsocketaddress; always first in remoteHost list?
//        ApplicationContainer mecApps;
//        ApplicationContainer orcApp;
//        ApplicationContainer ueApps;


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
    // trigger/hysterisis/delay_threshold
    ObjectFactory m_factory = ObjectFactory("ns3::MecOrcApplication");
    m_factory.Set ("MaxPackets", UintegerValue(10000));
    m_factory.Set ("PacketSize", UintegerValue(ORC_PACKET_SIZE));
    m_factory.Set ("AllServers", StringValue(mecString));
    m_factory.Set ("AllUes", StringValue(ueString));
    m_factory.Set ("UePort", UintegerValue(1000));
    m_factory.Set ("MecPort", UintegerValue(1000));
    m_factory.Set ("Trigger", UintegerValue(TRIGGER));
    m_factory.Set ("Hysteresis", DoubleValue(HYSTERESIS));
    m_factory.Set ("DelayThreshold", UintegerValue(DELAY_THRESHOLD));

    Ptr<Application> app = m_factory.Create<Application> ();
    orcNode->AddApplication (app);
    orcApp.Add(app);
//    NS_LOG_DEBUG("Application (type ORC): " << app << " on node " << orcNode->GetId());


    //  Install MEC applications
    for (int i = 1; i < int(remoteHostContainer.GetN()); ++i){
        Ptr<Node> node = remoteHostContainer.Get(i);
        Ptr<Node> enbNode = mecEnbMap[node];
        Ptr<NetDevice> device = enbNode->GetDevice(0);
        Ptr<LteEnbNetDevice> netDevice = dynamic_cast<LteEnbNetDevice*>(PeekPointer (device));
        uint16_t cellId = netDevice->GetCellId();

        ObjectFactory m_factory = ObjectFactory("ns3::MecHoServerApplication");
        m_factory.Set ("MaxPackets", UintegerValue(10000));
        m_factory.Set ("UpdateInterval", UintegerValue(MEC_UPDATE_INTERVAL));
        m_factory.Set ("PacketSize", UintegerValue(MEC_PACKET_SIZE));
        m_factory.Set ("OrcAddress", Ipv4AddressValue(orcAddress.GetIpv4()));
        m_factory.Set("OrcPort", UintegerValue(orcAddress.GetPort()));
        m_factory.Set ("CellID", UintegerValue(cellId) );
        m_factory.Set ("AllServers", StringValue(mecString));
        m_factory.Set("MecPort", UintegerValue(1000));
        m_factory.Set("AllUes", StringValue(ueString));
        m_factory.Set("UePort", UintegerValue(1000));
        m_factory.Set("NumberOfUes", UintegerValue(numberOfUes));

        Ptr<Application> app = m_factory.Create<Application> ();
        node->AddApplication(app);
        mecApps.Add(app);
//        NS_LOG_DEBUG("Application (type MEC): " << app << " on node " << node->GetId());
    }

    //Install UE applications
    for (int i = 0; i < int(remoteAddresses.size()); ++i){
//        NS_LOG_DEBUG("remote adress: " << remoteAddresses[i]);
    }
    for (int i = 0; i < int(ueNodes.GetN()); ++i){
        Ptr<Node> node = ueNodes.Get(i);
        uint64_t ueImsi = ueImsiMap.find(node)->second;

        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1,0);
        Ipv4Address ipAddr = iaddr.GetLocal();

        ObjectFactory m_factory = ObjectFactory("ns3::MecUeApplication");
        m_factory.Set("MaxPackets", UintegerValue(10000));
        m_factory.Set("PacketSize", UintegerValue(UE_PACKET_SIZE));
        m_factory.Set("ServiceInterval", TimeValue(MilliSeconds(SERVICE_INTERVAL)));
        m_factory.Set("PingInterval", TimeValue(MilliSeconds(PING_INTERVAL)));
        m_factory.Set ("MecIp", Ipv4AddressValue(remoteAddresses[i+1]));
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


        Ptr<Application> app = m_factory.Create<Application> ();
        node->AddApplication (app);

        ueApps.Add(ueApps);
//        NS_LOG_DEBUG("Application (type UE): " << app << " on node " << node->GetId());
    }
}

int StartSimulation(){
    orcApp.Start(Simulator::Now());
    mecApps.Start (Simulator::Now());
    ueApps.Start (Simulator::Now());


    lteHelper->EnableTraces ();
//     Uncomment to enable PCAP tracing
    p2ph.EnablePcapAll("lena-epc-first");

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
    epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    CreateRemoteHosts();


    enbNodes.Create(numberOfEnbs);
    ueNodes.Create(numberOfUes);


    InstallMobilityModels();

    InstallLteDevices();

    InstallUeNodes();

    InstallApplications();

    return StartSimulation();


    //END MAIN
}

