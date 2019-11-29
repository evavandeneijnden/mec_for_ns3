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

NS_LOG_COMPONENT_DEFINE ("MecHandover");

double simTime = 2.2;
double interPacketInterval = 100;
int numberOfUes = 2;
int numberOfEnbs = 3; //Hardcoded to 3 at the moment!
double enb_distance = 4000.0;
int numberOfMecs = 4;
double mec_distance = 3000.0;
double numberOfRemoteHosts = numberOfMecs + 1; //One extra for the orchestrator
std::map <Ptr<Node>, Ptr<Node>> mecEnbMap;

uint32_t ORC_PACKET_SIZE = 512;
uint32_t MEC_PACKET_SIZE = 512;
uint32_t MEC_UPDATE_INTERVAL = 1000;
uint32_t UE_PACKET_SIZE = 1024;
uint32_t PING_INTERVAL = 100;
uint32_t SERVICE_INTERVAL = 10;


//TODO refactor so that these do not have to be global variables
InternetStackHelper internet;
PointToPointHelper p2ph;
NodeContainer remoteHostContainer;
NodeContainer routerNode;
ApplicationContainer mecApps;
ApplicationContainer orcApp;
ApplicationContainer ueApps;
NetDeviceContainer ueLteDevs;
NetDeviceContainer enbLteDevs;
Ptr <LteHelper> lteHelper;
Ptr <PointToPointEpcHelper> epcHelper;
Ptr <Node> pgw;
Ipv4StaticRoutingHelper ipv4RoutingHelper;
std::vector<Ipv4Address> remoteAddresses;
NodeContainer ueNodes;
NodeContainer enbNodes;
std::map<Ptr<Node>, uint64_t> ueImsiMap;
std::vector<Ipv4Address> ueAddresses;
Ptr<Node> router;

void CreateRemoteHosts() {
//    NS_LOG_DEBUG("Create remote hosts");

    p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
    p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
    p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.000)));

    //install IP stack on router and add link to pgw
    Ptr<Node> router = routerNode.Get(0);
    NS_LOG_DEBUG("Node " << pgw->GetId() << " is PGW");
    NS_LOG_DEBUG("Node " << router->GetId() << " is ROUTER");
    internet.Install(router);

    NetDeviceContainer routerDevContainer = p2ph.Install(pgw, router);


    //Create remote hosts and install IP stack on them
    remoteHostContainer.Create (numberOfRemoteHosts);
    internet.Install (remoteHostContainer);


    //Create P2P links between router and each remote host
    NetDeviceContainer internetDevices;
    NodeContainer::Iterator i;
    for (i = remoteHostContainer.Begin (); i != remoteHostContainer.End (); ++i) {
//        NS_LOG_DEBUG("Setting up server/ORC");
        NetDeviceContainer netDeviceContainer = p2ph.Install (router, *i);
        NetDeviceContainer::Iterator d;
//        NS_LOG_DEBUG("New P2P group");
//        for (d = netDeviceContainer.Begin(); d != netDeviceContainer.End(); ++d) {
//            NS_LOG_DEBUG("CREATED NET DEVICE: " << (**d).GetAddress());
//        }

        internetDevices.Add(netDeviceContainer);
    }


    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
    Ipv4InterfaceContainer routerIpIfaces = ipv4h.Assign (routerDevContainer);


    unsigned int t = 0;
    NS_LOG_DEBUG("FOR PGW");
    while(t < pgw->GetNDevices()) {
        Ptr<NetDevice> netDevice = pgw->GetDevice(t);
        NS_LOG_DEBUG("FOUND NETDEVICE:" << " ADDRESS " << (*netDevice).GetAddress());
        t++;
    }

    Ptr<Ipv4> pgw_ipvs = pgw->GetObject<Ipv4>();
    unsigned int pgw_interface = 0;
    while(pgw_interface < pgw_ipvs->GetNInterfaces()) {
        unsigned int n_address = pgw_ipvs->GetNAddresses(pgw_interface);
        NS_LOG_DEBUG("INTERFACE " << pgw_interface << " HAS " << n_address << " ADDRESSES ");
        unsigned int address_i = 0;
        while(address_i < n_address) {
            NS_LOG_DEBUG("HAS IP ADDRESS: " << pgw_ipvs->GetAddress(pgw_interface, address_i));
            address_i++;
        }
        pgw_interface++;
    }

//    NS_LOG_DEBUG("_______________");
//
//    unsigned int e = 0;
//    NS_LOG_DEBUG("FOR ROUTER");
//    while(e < router->GetNDevices()) {
//        Ptr<NetDevice> netDevice = router->GetDevice(e);
//        NS_LOG_DEBUG("FOUND NETDEVICE:" << " ADDRESS " << (*netDevice).GetAddress());
//        e++;
//    }
//
//    Ptr<Ipv4> router_ipvs = router->GetObject<Ipv4>();
//    unsigned int router_interface = 0;
//    while(router_interface < router_ipvs->GetNInterfaces()) {
//        unsigned int n_address = router_ipvs->GetNAddresses(router_interface);
//        NS_LOG_DEBUG("INTERFACE " << router_interface << " HAS " << n_address << " ADDRESSES ");
//        unsigned int address_i = 0;
//        while(address_i < n_address) {
//            NS_LOG_DEBUG("HAS IP ADDRESS: " << router_ipvs->GetAddress(router_interface, address_i));
//            address_i++;
//        }
//        router_interface++;
//    }

//    NS_LOG_DEBUG("_______________");
    // interface 0 is localhost, 1 is the p2p device
//    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

    //Set PGW routes
    Ptr<Ipv4StaticRouting> pgwStaticRouting = ipv4RoutingHelper.GetStaticRouting (pgw->GetObject<Ipv4> ());
    pgwStaticRouting->AddNetworkRouteTo (Ipv4Address ("1.0.0.0"), Ipv4Mask ("255.0.0.0"), 2);
    pgwStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

    //Make vector of remote addresses (skip every second one, because this is our router)
    //Add route from router to each server
    Ptr<Ipv4StaticRouting> routerStaticRouting = ipv4RoutingHelper.GetStaticRouting (router->GetObject<Ipv4> ());
    int if_number = 1;
    for(uint16_t i = 1; i<internetIpIfaces.GetN(); i+=2){
        Ipv4Address address = internetIpIfaces.GetAddress(i);
//        NS_LOG_DEBUG("FOUND INET IFACE: " << address);
        remoteAddresses.push_back(address);
        //Add route from router to remoteHost
//        NS_LOG_DEBUG("Address " << address << " can be reached trough interface " << if_number);
        routerStaticRouting->AddHostRouteTo(address,if_number);
        if_number++;
    }
    //Route towards LTE --> send to PGW
    routerStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 6);

    //Routes from servers to other servers
    NodeContainer::Iterator j;
//    int n = 1;
    for (j = remoteHostContainer.Begin (); j != remoteHostContainer.End (); ++j) {
        Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting ((*j)->GetObject<Ipv4> ());
        remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);
//        std::string addrString = "1.0.0." + n.str();
//        NS_LOG_DEBUG("addr string: " << addrString);
        remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address ("1.0.0.0"), Ipv4Mask ("255.0.0.0"),1);
//        n+=2;
    }

//    //Logging for who-is-who detection
//    NodeContainer::Iterator k;
//    for (k = remoteHostContainer.Begin (); k != remoteHostContainer.End (); ++k) {
//        Ptr<Ipv4> ipv4 = (*k)->GetObject<Ipv4>();
//        Ipv4InterfaceAddress iaddr = ipv4->GetAddress(1,0);
//        Ipv4Address address = iaddr.GetLocal();
//        NS_LOG_DEBUG("Node (ORC/SERVER) " << (*k)->GetId() << ",address: " << address);
//    }
//    NS_LOG_DEBUG("Test 9");
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
    ObjectFactory m_factory = ObjectFactory("ns3::MecOrcApplication");
    m_factory.Set ("MaxPackets", UintegerValue(10000));
    m_factory.Set ("PacketSize", UintegerValue(ORC_PACKET_SIZE));
    m_factory.Set ("AllServers", StringValue(mecString));
    m_factory.Set ("AllUes", StringValue(ueString));
    m_factory.Set ("UePort", UintegerValue(1002));
    m_factory.Set ("MecPort", UintegerValue(1001));

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
        m_factory.Set("MecPort", UintegerValue(1001));
        m_factory.Set("AllUes", StringValue(ueString));
        m_factory.Set("UePort", UintegerValue(1002));

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

        ObjectFactory m_factory = ObjectFactory("ns3::MecUeApplication");
        m_factory.Set("MaxPackets", UintegerValue(10000));
        m_factory.Set("PacketSize", UintegerValue(UE_PACKET_SIZE));
        m_factory.Set("ServiceInterval", TimeValue(MilliSeconds(SERVICE_INTERVAL)));
        m_factory.Set("PingInterval", TimeValue(MilliSeconds(PING_INTERVAL)));
        m_factory.Set ("MecIp", Ipv4AddressValue(remoteAddresses[i+1]));
        m_factory.Set("MecPort", UintegerValue(1001));
        m_factory.Set ("OrcIp", Ipv4AddressValue(orcAddress.GetIpv4()));
        m_factory.Set("OrcPort", UintegerValue(orcAddress.GetPort()));
        //TODO give the below argument the right value, currently is wrong
        m_factory.Set("Local", Ipv4AddressValue(remoteAddresses[i]));
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
    // TODO Servers cannot talk to each other as they are P2P to PGW and not on the same subnet?

    lteHelper = CreateObject<LteHelper>();
    epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    pgw = epcHelper->GetPgwNode();

    routerNode.Create(1);
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

