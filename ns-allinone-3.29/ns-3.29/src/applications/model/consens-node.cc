#include "ns3/address.h"
#include "ns3/address-utils.h"
#include "ns3/log.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/consens-node.h"
#include "../../rapidjson/document.h"
#include "../../rapidjson/writer.h"
#include "../../rapidjson/stringbuffer.h"
#include <fstream>
#include <time.h>
#include <sys/time.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>

static double GetWallTime();

namespace ns3 {

//int m_messageCount;

NS_LOG_COMPONENT_DEFINE ("Consens");

NS_OBJECT_ENSURE_REGISTERED (Consens);

TypeId
Consens::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Consens")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<Consens> ()
    .AddAttribute ("Local",
                   "The Address on which to Bind the rx socket.",
                   AddressValue (),
                   MakeAddressAccessor (&Consens::m_local),
                   MakeAddressChecker ())
    .AddAttribute ("Protocol",
                   "The type id of the protocol to use for the rx socket.",
                   TypeIdValue (UdpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&Consens::m_tid),
                   MakeTypeIdChecker ())
    .AddAttribute ("BlockTorrent",
                   "Enable the BlockTorrent protocol",
                   BooleanValue (false),
                   MakeBooleanAccessor (&Consens::m_blockTorrent),
                   MakeBooleanChecker ())
    .AddAttribute ("SPV",
                   "Enable SPV Mechanism",
                   BooleanValue (false),
                   MakeBooleanAccessor (&Consens::m_spv),
                   MakeBooleanChecker ())
    .AddAttribute ("NumberOfMiners",
                   "The number of miners",
                   UintegerValue (16),
                   MakeUintegerAccessor (&Consens::m_noMiners),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("FixedBlockSize",
                   "The fixed size of the block",
                   UintegerValue (0),
                   MakeUintegerAccessor (&Consens::m_fixedBlockSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("FixedBlockIntervalGeneration",
                   "The fixed time to wait between two consecutive block generations",
                   DoubleValue (0),
                   MakeDoubleAccessor (&Consens::m_fixedBlockTimeGeneration),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("InvTimeoutMinutes",
                   "The timeout of inv messages in minutes",
                   TimeValue (Minutes (20)),
                   MakeTimeAccessor (&Consens::m_invTimeoutMinutes),
                   MakeTimeChecker())
    .AddAttribute ("HashRate",
                   "The hash rate of the miner",
                   DoubleValue (0.2),
                   MakeDoubleAccessor (&Consens::m_hashRate),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("BlockGenBinSize",
                   "The block generation bin size",
                   DoubleValue (-1),
                   MakeDoubleAccessor (&Consens::m_blockGenBinSize),
                   MakeDoubleChecker<double>  ())
    .AddAttribute ("BlockGenParameter",
                   "The block generation distribution parameter",
                   DoubleValue (-1),
                   MakeDoubleAccessor (&Consens::m_blockGenParameter),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("AverageBlockGenIntervalSeconds",
                   "The average block generation interval we aim at (in seconds)",
                   DoubleValue (10*60),
                   MakeDoubleAccessor (&Consens::m_averageBlockGenIntervalSeconds),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("Cryptocurrency",
                   "BITCOIN, LITECOIN, DOGECOIN",
                   UintegerValue (0),
                   MakeUintegerAccessor (&Consens::m_cryptocurrency),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("ChunkSize",
                   "The fixed size of the block chunk",
                   UintegerValue (100000),
                   MakeUintegerAccessor (&Consens::m_chunkSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddTraceSource ("Rx",
                     "A packet has been received",
                     MakeTraceSourceAccessor (&Consens::m_rxTrace),
                     "ns3::Packet::AddressTracedCallback")
  ;
  return tid;
}

Consens::Consens () : BitcoinNode(), m_realAverageBlockGenIntervalSeconds(10*m_secondsPerMin),
                                m_timeStart (0), m_timeFinish (0), m_fistToMine (false)
{
  NS_LOG_FUNCTION (this);


  m_requiredCount = 1;
  m_nodeReqCount = 1;
  m_messageProc = 0.0000000001;
  m_specialCaseProc = 60;
  m_maxNumBlocks = 1000;

  m_nodeCompCount = 0;
  m_blockCount = 0;
  //m_leaderID = 2;
  m_messageCount = 0;
  m_consensState = 0;
  m_timeCompleted = 0;

  m_minerAverageBlockGenInterval = 0;
  m_minerGeneratedBlocks = 0;
  m_previousBlockGenerationTime = 0;

  std::random_device rd;
  int r_seed = rd();
  std::cout << "<SEED> This run is seeded with " << r_seed << "\n";
  m_generator.seed(r_seed);

  if (m_fixedBlockTimeGeneration > 0)
    m_nextBlockTime = m_fixedBlockTimeGeneration;
  else
    m_nextBlockTime = 0;

  if (m_fixedBlockSize > 0)
    m_nextBlockSize = m_fixedBlockSize;
  else
    m_nextBlockSize = 0;

  m_isMiner = true;
}


Consens::~Consens(void)
{
  NS_LOG_FUNCTION (this);
}

void
Consens::StartApplication ()    // Called at time specified by Start
{
  BitcoinNode::StartApplication ();
  //std::ofstream outputFile;
  int tmp_nodeID = GetNode()->GetId();
  //std::cout << " <><><><><><><><><><> the node id is "  << tmp_nodeID;

  std::string outFile;
  outFile=std::string("output");
  outFile+=std::to_string(tmp_nodeID);
  outFile+=".csv";
  //std::string minId = std::to_string(GetNode()->GetId());
  //outFile.append(minId);
  //outFile.append(".csv");
  outputFile.open(outFile);
  outputFile << " first line of of the log \n";
  //std::cout << " the node id is "  << GetNode()->GetId();



  NS_LOG_WARN ("Miner " << GetNode()->GetId() << " m_noMiners = " << m_noMiners << "");
  NS_LOG_WARN ("Miner " << GetNode()->GetId() << " m_realAverageBlockGenIntervalSeconds = " << m_realAverageBlockGenIntervalSeconds << "s");
  NS_LOG_WARN ("Miner " << GetNode()->GetId() << " m_averageBlockGenIntervalSeconds = " << m_averageBlockGenIntervalSeconds << "s");
  NS_LOG_WARN ("Miner " << GetNode()->GetId() << " m_fixedBlockTimeGeneration = " << m_fixedBlockTimeGeneration << "s");
  NS_LOG_WARN ("Miner " << GetNode()->GetId() << " m_hashRate = " << m_hashRate );
  NS_LOG_WARN ("Miner " << GetNode()->GetId() << " m_blockBroadcastType = " << getBlockBroadcastType(m_blockBroadcastType));
  NS_LOG_WARN ("Miner " << GetNode()->GetId() << " m_cryptocurrency = " << getCryptocurrency(m_cryptocurrency));

  if (m_blockGenBinSize < 0 && m_blockGenParameter < 0)
  {
    m_blockGenBinSize = 1./m_secondsPerMin/1000;
    m_blockGenParameter = 0.19 * m_blockGenBinSize / 2;
  }
  else
    m_blockGenParameter *= m_hashRate;

  if (m_fixedBlockTimeGeneration == 0)
    m_blockGenTimeDistribution.param(std::geometric_distribution<int>::param_type(m_blockGenParameter));

  if (m_fixedBlockSize > 0)
    m_nextBlockSize = m_fixedBlockSize;
  else
  {
    switch(m_cryptocurrency)
    {
      case BITCOIN:
      {
        std::array<double,201> intervals {0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100, 105, 110, 115, 120, 125,
                                         130, 135, 140, 145, 150, 155, 160, 165, 170, 175, 180, 185, 190, 195, 200, 205, 210, 215, 220, 225, 230, 235,
                                         240, 245, 250, 255, 260, 265, 270, 275, 280, 285, 290, 295, 300, 305, 310, 315, 320, 325, 330, 335, 340, 345,
                                         350, 355, 360, 365, 370, 375, 380, 385, 390, 395, 400, 405, 410, 415, 420, 425, 430, 435, 440, 445, 450, 455,
                                         460, 465, 470, 475, 480, 485, 490, 495, 500, 505, 510, 515, 520, 525, 530, 535, 540, 545, 550, 555, 560, 565,
                                         570, 575, 580, 585, 590, 595, 600, 605, 610, 615, 620, 625, 630, 635, 640, 645, 650, 655, 660, 665, 670, 675,
                                         680, 685, 690, 695, 700, 705, 710, 715, 720, 725, 730, 735, 740, 745, 750, 755, 760, 765, 770, 775, 780, 785,
                                         790, 795, 800, 805, 810, 815, 820, 825, 830, 835, 840, 845, 850, 855, 860, 865, 870, 875, 880, 885, 890, 895,
                                         900, 905, 910, 915, 920, 925, 930, 935, 940, 945, 950, 955, 960, 965, 970, 975, 980, 985, 990, 995, 1000};
        std::array<double,200> weights {4.96, 0.21, 0.17, 0.25, 0.27, 0.3, 0.34, 0.26, 0.26, 0.33, 0.35, 0.49, 0.42, 0.42, 0.48, 0.41, 0.46, 0.45,
                                       0.58, 0.58, 0.57, 0.52, 0.54, 0.47, 0.53, 0.56, 0.5, 0.48, 0.53, 0.54, 0.49, 0.51, 0.56, 0.53, 0.56, 0.5,
                                       0.47, 0.45, 0.52, 0.43, 0.46, 0.47, 0.6, 0.53, 0.42, 0.48, 0.55, 0.49, 0.63, 2.38, 0.47, 0.53, 0.43, 0.51,
                                       0.44, 0.46, 0.44, 0.41, 0.47, 0.46, 0.45, 0.37, 0.49, 0.4, 0.41, 0.41, 0.41, 0.37, 0.43, 0.47, 0.48, 0.37,
                                       0.4, 0.46, 0.34, 0.35, 0.37, 0.36, 0.37, 0.31, 0.35, 0.39, 0.34, 0.38, 0.29, 0.41, 0.37, 0.34, 0.36, 0.34,
                                       0.29, 0.3, 0.36, 0.26, 0.29, 0.31, 0.3, 0.29, 0.35, 0.5, 0.28, 0.37, 0.31, 0.33, 0.32, 0.28, 0.34, 0.31,
                                       0.26, 0.24, 0.22, 0.25, 0.24, 0.25, 0.26, 0.25, 0.24, 0.33, 0.24, 0.23, 0.2, 0.24, 0.26, 0.27, 0.27, 0.21,
                                       0.22, 0.3, 0.25, 0.21, 0.26, 0.21, 0.21, 0.21, 0.23, 0.48, 0.2, 0.19, 0.21, 0.2, 0.17, 0.19, 0.21, 0.22,
                                       0.24, 0.25, 0.23, 0.31, 0.46, 8.32, 0.22, 0.11, 0.13, 0.17, 0.12, 0.16, 0.15, 0.16, 0.19, 0.21, 0.18, 0.24,
                                       0.19, 0.2, 0.16, 0.17, 0.19, 0.17, 0.22, 0.33, 0.17, 0.22, 0.25, 0.19, 0.2, 0.17, 0.28, 0.25, 0.24, 0.25, 0.3,
                                       0.34, 0.46, 0.49, 0.67, 3.13, 2.94, 0.14, 0.36, 3.88, 0.07, 0.11, 0.11, 0.11, 0.26, 0.12, 0.13, 0.88, 5.84, 4.11};
        m_blockSizeDistribution = std::piecewise_constant_distribution<double> (intervals.begin(), intervals.end(), weights.begin());
        break;
      }
      case LITECOIN:
      {
        std::array<double,201> intervals {0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0, 6.5, 7.0, 7.5, 8.0, 8.5, 9.0, 9.5, 10.0,
										 10.5, 11.0, 11.5, 12.0, 12.5, 13.0, 13.5, 14.0, 14.5, 15.0, 15.5, 16.0, 16.5, 17.0, 17.5, 18.0, 18.5, 19.0, 19.5,
										 20.0, 20.5, 21.0, 21.5, 22.0, 22.5, 23.0, 23.5, 24.0, 24.5, 25.0, 25.5, 26.0, 26.5, 27.0, 27.5, 28.0, 28.5, 29.0,
										 29.5, 30.0, 30.5, 31.0, 31.5, 32.0, 32.5, 33.0, 33.5, 34.0, 34.5, 35.0, 35.5, 36.0, 36.5, 37.0, 37.5, 38.0, 38.5,
										 39.0, 39.5, 40.0, 40.5, 41.0, 41.5, 42.0, 42.5, 43.0, 43.5, 44.0, 44.5, 45.0, 45.5, 46.0, 46.5, 47.0, 47.5, 48.0,
										 48.5, 49.0, 49.5, 50.0, 50.5, 51.0, 51.5, 52.0, 52.5, 53.0, 53.5, 54.0, 54.5, 55.0, 55.5, 56.0, 56.5, 57.0, 57.5,
										 58.0, 58.5, 59.0, 59.5, 60.0, 60.5, 61.0, 61.5, 62.0, 62.5, 63.0, 63.5, 64.0, 64.5, 65.0, 65.5, 66.0, 66.5, 67.0,
										 67.5, 68.0, 68.5, 69.0, 69.5, 70.0, 70.5, 71.0, 71.5, 72.0, 72.5, 73.0, 73.5, 74.0, 74.5, 75.0, 75.5, 76.0, 76.5,
										 77.0, 77.5, 78.0, 78.5, 79.0, 79.5, 80.0, 80.5, 81.0, 81.5, 82.0, 82.5, 83.0, 83.5, 84.0, 84.5, 85.0, 85.5, 86.0,
										 86.5, 87.0, 87.5, 88.0, 88.5, 89.0, 89.5, 90.0, 90.5, 91.0, 91.5, 92.0, 92.5, 93.0, 93.5, 94.0, 94.5, 95.0, 95.5,
										 96.0, 96.5, 97.0, 97.5, 98.0, 98.5, 99.0, 99.5, 100.0};
        std::array<double,200> weights {38.91, 5.76, 4.97, 4.11, 3.4, 3.13, 2.77, 2.36, 2.24, 2.04, 1.85, 1.74, 1.55, 1.47, 1.32, 1.19, 1.1, 1.0, 0.89,
										0.87, 0.82, 0.75, 0.73, 0.63, 0.61, 0.61, 0.53, 0.52, 0.52, 0.56, 0.47, 0.48, 0.45, 0.39, 0.4, 0.37, 0.37, 0.34,
										0.32, 0.34, 0.32, 0.27, 0.32, 0.32, 0.3, 0.26, 0.25, 0.35, 0.89, 0.18, 0.12, 0.11, 0.1, 0.1, 0.09, 0.1, 0.09, 0.1,
										0.09, 0.1, 0.08, 0.08, 0.07, 0.07, 0.05, 0.07, 0.07, 0.06, 0.06, 0.06, 0.05, 0.05, 0.04, 0.05, 0.03, 0.05, 0.04,
										0.04, 0.04, 0.04, 0.04, 0.05, 0.03, 0.03, 0.04, 0.02, 0.03, 0.02, 0.02, 0.03, 0.03, 0.03, 0.03, 0.03, 0.03, 0.02,
										0.05, 0.09, 0.01, 0.02, 0.02, 0.02, 0.01, 0.01, 0.01, 0.02, 0.01, 0.01, 0.02, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01,
										0.01, 0.02, 0.01, 0.01, 0.01, 0.01, 0.02, 0.0, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.0, 0.01, 0.01, 0.01, 0.01,
										0.01, 0.01, 0.01, 0.01, 0.0, 0.01, 0.01, 0.0, 0.0, 0.01, 0.01, 0.01, 0.0, 0.0, 0.0, 0.01, 0.01, 0.01, 0.01, 0.01,
										0.0, 0.0, 0.0, 0.01, 0.0, 0.01, 0.0, 0.0, 0.01, 0.0, 0.0, 0.0, 0.01, 0.01, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
										0.01, 0.0, 0.01, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.01, 0.0, 0.0, 0.0, 0.0, 0.01, 0.0,
										0.0, 0.24};
        m_blockSizeDistribution = std::piecewise_constant_distribution<double> (intervals.begin(), intervals.end(), weights.begin());
        break;
      }
      case DOGECOIN:
      {
        std::array<double,201> intervals {0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0, 6.5, 7.0, 7.5, 8.0, 8.5, 9.0, 9.5, 10.0,
										 10.5, 11.0, 11.5, 12.0, 12.5, 13.0, 13.5, 14.0, 14.5, 15.0, 15.5, 16.0, 16.5, 17.0, 17.5, 18.0, 18.5, 19.0, 19.5,
										 20.0, 20.5, 21.0, 21.5, 22.0, 22.5, 23.0, 23.5, 24.0, 24.5, 25.0, 25.5, 26.0, 26.5, 27.0, 27.5, 28.0, 28.5, 29.0,
										 29.5, 30.0, 30.5, 31.0, 31.5, 32.0, 32.5, 33.0, 33.5, 34.0, 34.5, 35.0, 35.5, 36.0, 36.5, 37.0, 37.5, 38.0, 38.5,
										 39.0, 39.5, 40.0, 40.5, 41.0, 41.5, 42.0, 42.5, 43.0, 43.5, 44.0, 44.5, 45.0, 45.5, 46.0, 46.5, 47.0, 47.5, 48.0,
										 48.5, 49.0, 49.5, 50.0, 50.5, 51.0, 51.5, 52.0, 52.5, 53.0, 53.5, 54.0, 54.5, 55.0, 55.5, 56.0, 56.5, 57.0, 57.5,
										 58.0, 58.5, 59.0, 59.5, 60.0, 60.5, 61.0, 61.5, 62.0, 62.5, 63.0, 63.5, 64.0, 64.5, 65.0, 65.5, 66.0, 66.5, 67.0,
										 67.5, 68.0, 68.5, 69.0, 69.5, 70.0, 70.5, 71.0, 71.5, 72.0, 72.5, 73.0, 73.5, 74.0, 74.5, 75.0, 75.5, 76.0, 76.5,
										 77.0, 77.5, 78.0, 78.5, 79.0, 79.5, 80.0, 80.5, 81.0, 81.5, 82.0, 82.5, 83.0, 83.5, 84.0, 84.5, 85.0, 85.5, 86.0,
										 86.5, 87.0, 87.5, 88.0, 88.5, 89.0, 89.5, 90.0, 90.5, 91.0, 91.5, 92.0, 92.5, 93.0, 93.5, 94.0, 94.5, 95.0, 95.5,
										 96.0, 96.5, 97.0, 97.5, 98.0, 98.5, 99.0, 99.5, 100.0};
        std::array<double,200> weights {16.38, 9.75, 7.9, 6.45, 5.51, 4.78, 4.13, 3.52, 3.12, 2.76, 2.48, 2.2, 1.88, 1.77, 1.59, 1.47, 1.31, 1.22, 1.11,
										1.02, 0.92, 0.86, 0.76, 0.73, 0.68, 0.61, 0.6, 0.56, 0.53, 0.5, 0.52, 0.51, 0.51, 0.47, 0.46, 0.43, 0.41, 0.4,
										0.38, 0.36, 0.34, 0.33, 0.3, 0.29, 0.27, 0.25, 0.27, 0.24, 0.23, 0.2, 0.2, 0.19, 0.17, 0.16, 0.16, 0.15, 0.14,
										0.12, 0.14, 0.13, 0.11, 0.13, 0.11, 0.11, 0.09, 0.1, 0.08, 0.08, 0.07, 0.07, 0.07, 0.07, 0.06, 0.07, 0.07, 0.05,
										0.06, 0.06, 0.05, 0.06, 0.06, 0.05, 0.04, 0.04, 0.04, 0.04, 0.04, 0.03, 0.04, 0.04, 0.03, 0.03, 0.03, 0.03, 0.03,
										0.03, 0.03, 0.04, 0.04, 0.03, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02,
										0.02, 0.01, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.01, 0.02, 0.01, 0.02,
										0.01, 0.01, 0.01, 0.01, 0.02, 0.01, 0.02, 0.02, 0.02, 0.02, 0.01, 0.01, 0.02, 0.01, 0.01, 0.02, 0.01, 0.02, 0.01,
										0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01,
										0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01,
										0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.01, 0.02, 0.41};
        m_blockSizeDistribution = std::piecewise_constant_distribution<double> (intervals.begin(), intervals.end(), weights.begin());
        break;
      }
    }
  }

/*   if (GetNode()->GetId() == 0)
  {
    Block newBlock(1, 0, -1, 500000, 0, 0, Ipv4Address("0.0.0.0"));
    m_blockchain.AddBlock(newBlock);
  } */

  m_nodeStats->hashRate = m_hashRate;
  m_nodeStats->miner = 1;

  if(m_blockCount <= m_maxNumBlocks)
  {
    ScheduleNextMiningEvent ();
  }
}

void
Consens::StopApplication ()
{
  BitcoinNode::StopApplication ();
  Simulator::Cancel (m_nextMiningEvent);

  NS_LOG_WARN ("The miner " << GetNode ()->GetId () << " with hash rate = " << m_hashRate << " generated " << m_minerGeneratedBlocks
                << " blocks "<< "(" << 100. * m_minerGeneratedBlocks / (m_blockchain.GetTotalBlocks() - 1)
                << "%) with average block generation time = " << m_minerAverageBlockGenInterval
                << "s or " << static_cast<int>(m_minerAverageBlockGenInterval) / m_secondsPerMin << "min and "
                << m_minerAverageBlockGenInterval - static_cast<int>(m_minerAverageBlockGenInterval) / m_secondsPerMin * m_secondsPerMin << "s"
                << " and average size " << m_minerAverageBlockSize << " Bytes");

  outputFile.close();
  m_nodeStats->minerGeneratedBlocks = m_minerGeneratedBlocks;
  m_nodeStats->minerAverageBlockGenInterval = m_minerAverageBlockGenInterval;
  m_nodeStats->minerAverageBlockSize = m_minerAverageBlockSize;

  if (m_fistToMine)
  {
    m_timeFinish = GetWallTime();
    std::cout << "Time/Block = " << (m_timeFinish - m_timeStart) / (m_blockchain.GetTotalBlocks() - 1) << "s\n";
  }
}

void
Consens::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  BitcoinNode::DoDispose ();
}

double
Consens::GetFixedBlockTimeGeneration(void) const
{
  NS_LOG_FUNCTION (this);
  return m_fixedBlockTimeGeneration;
}

void
Consens::SetFixedBlockTimeGeneration(double fixedBlockTimeGeneration)
{
  NS_LOG_FUNCTION (this);
  m_fixedBlockTimeGeneration = fixedBlockTimeGeneration;
}

uint32_t
Consens::GetFixedBlockSize(void) const
{
  NS_LOG_FUNCTION (this);
  return m_fixedBlockSize;
}

void
Consens::SetFixedBlockSize(uint32_t fixedBlockSize)
{
  NS_LOG_FUNCTION (this);
  m_fixedBlockSize = fixedBlockSize;
}

double
Consens::GetBlockGenBinSize(void) const
{
  NS_LOG_FUNCTION (this);
  return m_blockGenBinSize;
}

void
Consens::SetBlockGenBinSize (double blockGenBinSize)
{
  NS_LOG_FUNCTION (this);
  m_blockGenBinSize = blockGenBinSize;
}

double
Consens::GetBlockGenParameter(void) const
{
  NS_LOG_FUNCTION (this);
  return m_blockGenParameter;
}

void
Consens::SetBlockGenParameter (double blockGenParameter)
{
  NS_LOG_FUNCTION (this);
  m_blockGenParameter = blockGenParameter;
  m_blockGenTimeDistribution.param(std::geometric_distribution<int>::param_type(m_blockGenParameter));

}

double
Consens::GetHashRate(void) const
{
  NS_LOG_FUNCTION (this);
  return m_hashRate;
}

void
Consens::SetHashRate (double hashRate)
{
  NS_LOG_FUNCTION (this);
  m_hashRate = hashRate;
}

void
Consens::SetBlockBroadcastType (enum BlockBroadcastType blockBroadcastType)
{
  NS_LOG_FUNCTION (this);
  m_blockBroadcastType = blockBroadcastType;
}

void
Consens::ScheduleNextMiningEvent (void)
{
  //std::cout << GetNode()->GetId() << " scheduling next event \n";
  NS_LOG_FUNCTION (this);

  //std::random_device rd{};
  //std::mt19937 gen{rd()};
  //std::normal_distribution<> normD{237907.507 , 232381.693};

  //std::default_random_engine generator;
  std::weibull_distribution<double> distribution(1.354783,249857.5);

  //Debug settings:
  //m_leaderID=1;
  //m_messageCount = m_requiredCount+1;
  if(m_leaderID==0)
  {
    // Send message for special case (example being leader selection)
    //m_nextMiningEvent = Simulator::Schedule (Seconds(m_specialCaseProc), &Consens::SpecialCaseMessage, this);
    std::cout << "<!!!!!!!!!!!!!!!!> unfortunately we did not have the leader id set :( \n";
    m_leaderID=4;
    //ScheduleNextMiningEvent();
  }
  //if the number of blocks required has not been processed
  if(m_blockCount <= m_maxNumBlocks)
  {
    //---------------------------------------------------
    //        If state of consens is complete then
    //        execute normal mine with very little Delay
    //---------------------------------------------------
    //Do a normal message processing event
    if (m_messageCount <= m_requiredCount && m_consensState == 0)
    {
      //Send message for regular consensus message processing
      m_messageCount++;
      //m_messageProc+=0.1;
      double processDelay = distribution(m_generator);
      outputFile << GetNode()->GetId() << ",start," << processDelay << "," << Simulator::Now().GetNanoSeconds() << ", , \n";
      if(GetNode()->GetId() == 8)
      {
        //std::cout << GetNode()->GetId() << " sending process message " << m_peersAddresses[2] << " : " << m_blockCount << " : " << m_messageCount << " : " << Simulator::Now().GetSeconds() << "\n";
      }
      //m_nextMiningEvent = Simulator::Schedule (NanoSeconds(distribution(generator)), &Consens::ConsensMessage, this);
      m_nextMiningEvent = Simulator::Schedule (NanoSeconds(processDelay), &Consens::ConsensMessage, this);
    }
    else  //normal processing is completed for this node
    {
      //Send message for block write
      //If this node is the leader
      if((GetNode()->GetId() == (m_leaderID)))
      {
        //if the number of completed peers is greater than or equal
        // to the required amount
        if (m_nodeCompCount >= m_nodeReqCount)
        {
          m_messageCount = 0;
          //if legacy parameter is set which is not the normal case
          if(m_fixedBlockTimeGeneration > 0)
          {
            m_nextBlockTime = m_fixedBlockTimeGeneration;
            m_blockCount++;

            NS_LOG_DEBUG ("Time " << Simulator::Now ().GetSeconds () << ": Miner " << GetNode ()->GetId ()
                        << " fixed Block Time Generation " << m_fixedBlockTimeGeneration << "s");

            //std::cout << " setting the message count to 0 and writing block " << m_fixedBlockTimeGeneration << " is the delay \n";
            //m_nextMiningEvent = Simulator::Schedule (Seconds(m_fixedBlockTimeGeneration), &Consens::MineBlock, this);

            //m_nextMiningEvent = Simulator::ScheduleNow (&Consens::MineBlock, this);


            std::cout << m_blockCount << " sending FIXED start message " << GetNode()->GetId() << " : " << Simulator::Now().GetSeconds() << " is the sim time \n";
            m_nextMiningEvent = Simulator::ScheduleNow (&Consens::StartMessage, this);
          }
          else
          {

            m_blockCount++;
            m_nextBlockTime = m_blockGenTimeDistribution(m_generator)*m_blockGenBinSize*m_secondsPerMin
                            *( m_averageBlockGenIntervalSeconds/m_realAverageBlockGenIntervalSeconds )/m_hashRate;

            //NS_LOG_DEBUG("m_nextBlockTime = " << m_nextBlockTime << ", binsize = " << m_blockGenBinSize << ", m_blockGenParameter = " << m_blockGenParameter << ", hashrate = " << m_hashRate);
            //std::cout << " setting the message count to 0 by node " << GetNode()->GetId() << " : " << m_nextBlockTime << " is the delay \n";
            //std::cout << " setting the message count to 0 by node " << GetNode()->GetId() << " : " << Simulator::Now().GetSeconds() << " is the sim time \n";
            //m_nextMiningEvent = Simulator::Schedule (Seconds(m_nextBlockTime), &Consens::MineBlock, this);

            //m_nextMiningEvent = Simulator::ScheduleNow (&Consens::MineBlock, this);

            NS_LOG_WARN ("Time " << Simulator::Now ().GetSeconds () << ": Miner " << GetNode ()->GetId () << " will generate a block in "
                         << m_nextBlockTime << "s or " << static_cast<int>(m_nextBlockTime) / m_secondsPerMin
                         << "  min and  " << static_cast<int>(m_nextBlockTime) % m_secondsPerMin
                         << "s using Geometric Block Time Generation with parameter = "<< m_blockGenParameter);

            std::cout << m_blockCount << ", consented, " << m_leaderID << "," << GetNode()->GetId() << "," << Simulator::Now().GetNanoSeconds() << "," << m_nodeCompCount << "\n";
            m_nodeCompCount = 0;
            m_nextMiningEvent = Simulator::ScheduleNow (&Consens::StartMessage, this);
            //m_nextMiningEvent = Simulator::Schedule (Seconds(m_messageProc), &Consens::StartMessage, this);
          }
        }
        return;
      }
      else if (m_consensState == 0)
      {
        m_messageCount = 0;
        m_consensState = 1;

        //if(GetNode()->GetId() == 1)
        //{
          //std::cout << GetNode()->GetId() << " Has had enough " << m_blockCount << " : " << Simulator::Now().GetSeconds() << "\n";
        //}
        m_nextMiningEvent = Simulator::ScheduleNow(&Consens::CompMessage, this);
        // ScheduleNextMiningEvent(); ?
        return;
      }
    }
  }
  else
  {

    //if(m_timeCompleted == 0)
    //{
      m_consensState = 1;
      m_messageCount = m_requiredCount;
      m_timeCompleted = Simulator::Now().GetSeconds();
      return;
      //Simulator::Cancel (m_nextMiningEvent);
      //std::cout << "<!!!!!!!!!!!!!!> " << GetNode()->GetId() << " this node has completed " << Simulator::Now().GetSeconds() << "\n";
    //}
  }
}

void
Consens::SpecialCaseMessage (void)
{

  return;
}

void
Consens::StartMessage (void)
{
  //outputFile << "StartMessage \n";
  //std::cout << m_blockCount << " : " << m_messageCount << " Next time step message for PROC " << Simulator::Now().GetSeconds() << " node id " << GetNode()->GetId() << "\n";
  rapidjson::Document inv;
  rapidjson::Document block;

  inv.SetObject();
  block.SetObject();

  int height =  m_blockchain.GetCurrentTopBlock()->GetBlockHeight() + 1;
  int minerId = GetNode ()->GetId ();
  int parentBlockMinerId = m_blockchain.GetCurrentTopBlock()->GetMinerId();
  double currentTime = Simulator::Now ().GetSeconds ();
  std::ostringstream stringStream;
  std::string blockHash;

  stringStream << height << "/" << minerId;
  //std::cout << "!!! incoming string stream " << stringStream.str() << "\n";
  blockHash = stringStream.str();

  inv.SetObject();
  block.SetObject();



    if (m_fixedBlockSize > 0)
      m_nextBlockSize = m_fixedBlockSize;
    else
    {
      m_nextBlockSize = m_blockSizeDistribution(m_generator) * 1000;	// *1000 because the m_blockSizeDistribution returns KBytes

      if (m_cryptocurrency == BITCOIN)
      {
        // The block size is linearly dependent on the averageBlockGenIntervalSeconds
        if(m_nextBlockSize < m_maxBlockSize - m_headersSizeBytes)
          m_nextBlockSize = m_nextBlockSize*m_averageBlockGenIntervalSeconds / m_realAverageBlockGenIntervalSeconds
                          + m_headersSizeBytes;
        else
          m_nextBlockSize = m_nextBlockSize*m_averageBlockGenIntervalSeconds / m_realAverageBlockGenIntervalSeconds;
      }
    }

    if (m_nextBlockSize < m_averageTransactionSize)
      m_nextBlockSize = m_averageTransactionSize + m_headersSizeBytes;

    Block newBlock (height, minerId, parentBlockMinerId, m_nextBlockSize,
                    currentTime, currentTime, Ipv4Address("127.0.0.1"));


/*   //For attacks
   if (GetNode ()->GetId () == 0)
     height = 2 - m_minerGeneratedBlocks;

   if (GetNode ()->GetId () == 0)
   {
	if (height == 1)
      parentBlockMinerId = -1;
    else
	  parentBlockMinerId = 0;
   } */

  switch(m_blockBroadcastType)
  {
    case STANDARD:
    {
      rapidjson::Value value;
      rapidjson::Value array(rapidjson::kArrayType);
      rapidjson::Value blockInfo(rapidjson::kObjectType);

      value.SetString("block"); //Remove
      inv.AddMember("type", value, inv.GetAllocator());

      if (m_protocolType == STANDARD_PROTOCOL)
      {
          value = START;
          inv.AddMember("message", value, inv.GetAllocator());

          value.SetString(blockHash.c_str(), blockHash.size(), inv.GetAllocator());
          array.PushBack(value, inv.GetAllocator());

          inv.AddMember("inv", array, inv.GetAllocator());
          //std::cout << "Should be a proc message in there somewhere  \n";
      }
      break;
    }
  }

  /**
   * Update m_meanBlockReceiveTime with the timeCreated of the newly generated block
   */
  m_meanBlockReceiveTime = (m_blockchain.GetTotalBlocks() - 1)/static_cast<double>(m_blockchain.GetTotalBlocks())*m_meanBlockReceiveTime
                         + (currentTime - m_previousBlockReceiveTime)/(m_blockchain.GetTotalBlocks());
  m_previousBlockReceiveTime = currentTime;

  m_meanBlockPropagationTime = (m_blockchain.GetTotalBlocks() - 1)/static_cast<double>(m_blockchain.GetTotalBlocks())*m_meanBlockPropagationTime;

  m_meanBlockSize = (m_blockchain.GetTotalBlocks() - 1)/static_cast<double>(m_blockchain.GetTotalBlocks())*m_meanBlockSize
                  + (m_nextBlockSize)/static_cast<double>(m_blockchain.GetTotalBlocks());

  m_blockchain.AddBlock(newBlock);


    // Stringify the DOM
    rapidjson::StringBuffer invInfo;
    rapidjson::Writer<rapidjson::StringBuffer> invWriter(invInfo);
    inv.Accept(invWriter);

    rapidjson::StringBuffer blockInfo;
    rapidjson::Writer<rapidjson::StringBuffer> blockWriter(blockInfo);
    block.Accept(blockWriter);

    int count = 0;

    for (std::vector<Ipv4Address>::const_iterator i = m_peersAddresses.begin(); i != m_peersAddresses.end(); ++i, ++count)
    {

      if(count >= 2)
      {
        //break;
      }
      const uint8_t delimiter[] = "#";

       //std::cout << GetNode()->GetId() << " : " << Simulator::Now().GetNanoSeconds() << " : to : " << *i << " : " << invInfo.GetSize() << " Start message packet info " << invInfo.GetString() << "\n";

          m_peersSockets[*i]->Send (reinterpret_cast<const uint8_t*>(invInfo.GetString()), invInfo.GetSize(), 0);
          m_peersSockets[*i]->Send (delimiter, 1, 0);

          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                       << "s bitcoin miner " << GetNode ()->GetId ()
                       << " sent a packet " << invInfo.GetString()
  			         << " to " << *i);

  /* 	//Send large packet
  	int k;
  	for (k = 0; k < 4; k++)
  	{
        ns3TcpSocket->Send (reinterpret_cast<const uint8_t*>(packetInfo.GetString()), packetInfo.GetSize(), 0);
  	  ns3TcpSocket->Send (delimiter, 1, 0);
  	} */


    }


    //m_messageCount++;

    //std::cout << "Current node " << minerId << " message count " << m_messageCount << " \n";

    //This is now handled in the response of the proc and comp message
    //ScheduleNextMiningEvent ();

}

void
Consens::ConsensMessage (void)
{
  //outputFile << std::string(GetNode()->GetId()) << "," << std::string(Simulator::Now().GetSeconds());
  outputFile << GetNode()->GetId() << ", , ,end," << Simulator::Now().GetNanoSeconds() << " \n";
  //std::cout << m_blockCount << " : " << m_messageCount << " Next time step message for PROC " << Simulator::Now().GetSeconds() << " node id " << GetNode()->GetId() << "\n";
  rapidjson::Document inv;
  rapidjson::Document block;

  inv.SetObject();
  block.SetObject();

  int height =  m_blockchain.GetCurrentTopBlock()->GetBlockHeight() + 1;
  int minerId = GetNode ()->GetId ();
  int parentBlockMinerId = m_blockchain.GetCurrentTopBlock()->GetMinerId();
  double currentTime = Simulator::Now ().GetSeconds ();
  std::ostringstream stringStream;
  std::string blockHash;

  stringStream << height << "/" << minerId;
  blockHash = stringStream.str();

  inv.SetObject();
  block.SetObject();

  if (height == 1)
  {
    m_fistToMine = true;
	m_timeStart = GetWallTime();
  }
/*   //For attacks
   if (GetNode ()->GetId () == 0)
     height = 2 - m_minerGeneratedBlocks;

   if (GetNode ()->GetId () == 0)
   {
	if (height == 1)
      parentBlockMinerId = -1;
    else
	  parentBlockMinerId = 0;
   } */

  switch(m_blockBroadcastType)
  {
    case STANDARD:
    {
      rapidjson::Value value;
      rapidjson::Value array(rapidjson::kArrayType);
      rapidjson::Value blockInfo(rapidjson::kObjectType);

      value.SetString("block"); //Remove
      inv.AddMember("type", value, inv.GetAllocator());

      if (m_protocolType == STANDARD_PROTOCOL)
      {
        if (!m_blockTorrent)
        {
          value = PROC;
          inv.AddMember("message", value, inv.GetAllocator());

          value.SetString(blockHash.c_str(), blockHash.size(), inv.GetAllocator());
          array.PushBack(value, inv.GetAllocator());

          inv.AddMember("inv", array, inv.GetAllocator());
          //std::cout << "Should be a proc message in there somewhere  \n";
        }
      }
      break;
    }
   }

    // Stringify the DOM
    rapidjson::StringBuffer invInfo;
    rapidjson::Writer<rapidjson::StringBuffer> invWriter(invInfo);
    inv.Accept(invWriter);

    rapidjson::StringBuffer blockInfo;
    rapidjson::Writer<rapidjson::StringBuffer> blockWriter(blockInfo);
    block.Accept(blockWriter);

    int count = 0;

    for (std::vector<Ipv4Address>::const_iterator i = m_peersAddresses.begin(); i != m_peersAddresses.end(); ++i)
    {

      const uint8_t delimiter[] = "#";

      switch(m_blockBroadcastType)
      {
        case STANDARD:
        {
          //if(GetNode()->GetId()==6)
	         //std::cout << GetNode()->GetId() << " : " << *i << " : " << m_peersAddresses[m_leaderID-1] << " : " << invInfo.GetString() << "\n";

          m_peersSockets[*i]->Send (reinterpret_cast<const uint8_t*>(invInfo.GetString()), invInfo.GetSize(), 0);
          m_peersSockets[*i]->Send (delimiter, 1, 0);
          //m_peersSockets[*i]->Send (delimiter, 1, 0);

          //NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
          //             << "s bitcoin miner " << GetNode ()->GetId ()
          //             << " sent a packet " << invInfo.GetString()
  			   //      << " to " << *i);
          break;
        }

      }
    }

  /* 	//Send large packet
  	int k;
  	for (k = 0; k < 4; k++)
  	{
        ns3TcpSocket->Send (reinterpret_cast<const uint8_t*>(packetInfo.GetString()), packetInfo.GetSize(), 0);
  	  ns3TcpSocket->Send (delimiter, 1, 0);
  	} */




    //m_messageCount++;

    //std::cout << "Current node " << minerId << " message count " << m_messageCount << " \n";

    //This is now handled in the response of the proc and comp message
    //ScheduleNextMiningEvent ();

}

void
Consens::MineBlock (void)
{
  //std::cout << "!!!!!!!! Next time step message for INV " << Simulator::Now().GetSeconds() << "\n";
  NS_LOG_FUNCTION (this);
  rapidjson::Document inv;
  rapidjson::Document block;

  int height =  m_blockchain.GetCurrentTopBlock()->GetBlockHeight() + 1;
  int minerId = GetNode ()->GetId ();
  int parentBlockMinerId = m_blockchain.GetCurrentTopBlock()->GetMinerId();
  double currentTime = Simulator::Now ().GetSeconds ();
  std::ostringstream stringStream;
  std::string blockHash;

  stringStream << height << "/" << minerId;
  blockHash = stringStream.str();

  inv.SetObject();
  block.SetObject();

  if (height == 1)
  {
    m_fistToMine = true;
	m_timeStart = GetWallTime();
  }
/*   //For attacks
   if (GetNode ()->GetId () == 0)
     height = 2 - m_minerGeneratedBlocks;

   if (GetNode ()->GetId () == 0)
   {
	if (height == 1)
      parentBlockMinerId = -1;
    else
	  parentBlockMinerId = 0;
   } */


  if (m_fixedBlockSize > 0)
    m_nextBlockSize = m_fixedBlockSize;
  else
  {
    m_nextBlockSize = m_blockSizeDistribution(m_generator) * 1000;	// *1000 because the m_blockSizeDistribution returns KBytes

    if (m_cryptocurrency == BITCOIN)
    {
      // The block size is linearly dependent on the averageBlockGenIntervalSeconds
      if(m_nextBlockSize < m_maxBlockSize - m_headersSizeBytes)
        m_nextBlockSize = m_nextBlockSize*m_averageBlockGenIntervalSeconds / m_realAverageBlockGenIntervalSeconds
                        + m_headersSizeBytes;
      else
        m_nextBlockSize = m_nextBlockSize*m_averageBlockGenIntervalSeconds / m_realAverageBlockGenIntervalSeconds;
    }
  }

  if (m_nextBlockSize < m_averageTransactionSize)
    m_nextBlockSize = m_averageTransactionSize + m_headersSizeBytes;

  Block newBlock (height, minerId, parentBlockMinerId, m_nextBlockSize,
                  currentTime, currentTime, Ipv4Address("127.0.0.1"));

  //std::cout << m_blockBroadcastType << " : " << m_protocolType << " : " << m_blockTorrent << " :  !!!!!!!!!!! ";
  switch(m_blockBroadcastType)
  {
    case STANDARD:
    {
      rapidjson::Value value;
      rapidjson::Value array(rapidjson::kArrayType);
      rapidjson::Value blockInfo(rapidjson::kObjectType);

      value.SetString("block"); //Remove
      inv.AddMember("type", value, inv.GetAllocator());

      if (m_protocolType == STANDARD_PROTOCOL)
      {
        if (!m_blockTorrent)
        {
          value = INV;
          inv.AddMember("message", value, inv.GetAllocator());

          value.SetString(blockHash.c_str(), blockHash.size(), inv.GetAllocator());
          array.PushBack(value, inv.GetAllocator());

          inv.AddMember("inv", array, inv.GetAllocator());
        }
        else
        {
          value = EXT_INV;
          inv.AddMember("message", value, inv.GetAllocator());
          value.SetString(blockHash.c_str(), blockHash.size(), inv.GetAllocator());
          blockInfo.AddMember("hash", value, inv.GetAllocator ());

	        value = newBlock.GetBlockSizeBytes ();
          blockInfo.AddMember("size", value, inv.GetAllocator ());

          value = true;
          blockInfo.AddMember("fullBlock", value, inv.GetAllocator ());

          array.PushBack(blockInfo, inv.GetAllocator());
          inv.AddMember("inv", array, inv.GetAllocator());
        }
      }
      else if (m_protocolType == SENDHEADERS)
      {

        value = newBlock.GetBlockHeight ();
        blockInfo.AddMember("height", value, inv.GetAllocator ());

        value = newBlock.GetMinerId ();
        blockInfo.AddMember("minerId", value, inv.GetAllocator ());

        value = newBlock.GetParentBlockMinerId ();
        blockInfo.AddMember("parentBlockMinerId", value, inv.GetAllocator ());

        value = newBlock.GetBlockSizeBytes ();
        blockInfo.AddMember("size", value, inv.GetAllocator ());

        value = newBlock.GetTimeCreated ();
        blockInfo.AddMember("timeCreated", value, inv.GetAllocator ());

        value = newBlock.GetTimeReceived ();
        blockInfo.AddMember("timeReceived", value, inv.GetAllocator ());

        if (!m_blockTorrent)
        {
          value = HEADERS;
          inv.AddMember("message", value, inv.GetAllocator());
        }
        else
        {
          value = EXT_HEADERS;
          inv.AddMember("message", value, inv.GetAllocator());

          value = true;
          blockInfo.AddMember("fullBlock", value, inv.GetAllocator ());
        }

        array.PushBack(blockInfo, inv.GetAllocator());
        inv.AddMember("blocks", array, inv.GetAllocator());
      }
      break;
    }
    case UNSOLICITED:
    {
      rapidjson::Value value (BLOCK);
      rapidjson::Value blockInfo(rapidjson::kObjectType);
      rapidjson::Value array(rapidjson::kArrayType);

      block.AddMember("message", value, block.GetAllocator());

      value.SetString("block"); //Remove
      block.AddMember("type", value, block.GetAllocator());

      value = newBlock.GetBlockHeight ();
      blockInfo.AddMember("height", value, block.GetAllocator ());

      value = newBlock.GetMinerId ();
      blockInfo.AddMember("minerId", value, block.GetAllocator ());

      value = newBlock.GetParentBlockMinerId ();
      blockInfo.AddMember("parentBlockMinerId", value, block.GetAllocator ());

      value = newBlock.GetBlockSizeBytes ();
      blockInfo.AddMember("size", value, block.GetAllocator ());

      value = newBlock.GetTimeCreated ();
      blockInfo.AddMember("timeCreated", value, block.GetAllocator ());

      value = newBlock.GetTimeReceived ();
      blockInfo.AddMember("timeReceived", value, block.GetAllocator ());

      array.PushBack(blockInfo, block.GetAllocator());
      block.AddMember("blocks", array, block.GetAllocator());

      break;
    }
    case RELAY_NETWORK:
    {
      rapidjson::Value value;
      rapidjson::Value headersInfo(rapidjson::kObjectType);
      rapidjson::Value chunkInfo(rapidjson::kObjectType);
      rapidjson::Value blockInfo(rapidjson::kObjectType);
      rapidjson::Value invArray(rapidjson::kArrayType);
      rapidjson::Value blockArray(rapidjson::kArrayType);

      value.SetString("block"); //Remove
      inv.AddMember("type", value, inv.GetAllocator());

      if (m_protocolType == STANDARD_PROTOCOL)
      {
        if (!m_blockTorrent)
        {
          value = INV;
          inv.AddMember("message", value, inv.GetAllocator());

          value.SetString(blockHash.c_str(), blockHash.size(), inv.GetAllocator());
          invArray.PushBack(value, inv.GetAllocator());

          inv.AddMember("inv", invArray, inv.GetAllocator());
        }
        else
        {
          value = EXT_INV;
          inv.AddMember("message", value, inv.GetAllocator());

          value.SetString(blockHash.c_str(), blockHash.size(), inv.GetAllocator());
          chunkInfo.AddMember("hash", value, inv.GetAllocator ());

          value = newBlock.GetBlockSizeBytes ();
          chunkInfo.AddMember("size", value, inv.GetAllocator ());

          value = true;
          chunkInfo.AddMember("fullBlock", value, inv.GetAllocator ());

          invArray.PushBack(chunkInfo, inv.GetAllocator());
          inv.AddMember("inv", invArray, inv.GetAllocator());
        }
      }
      else if (m_protocolType == SENDHEADERS)
      {

        value = newBlock.GetBlockHeight ();
        headersInfo.AddMember("height", value, inv.GetAllocator ());

        value = newBlock.GetMinerId ();
        headersInfo.AddMember("minerId", value, inv.GetAllocator ());

        value = newBlock.GetParentBlockMinerId ();
        headersInfo.AddMember("parentBlockMinerId", value, inv.GetAllocator ());

        value = newBlock.GetBlockSizeBytes ();
        headersInfo.AddMember("size", value, inv.GetAllocator ());

        value = newBlock.GetTimeCreated ();
        headersInfo.AddMember("timeCreated", value, inv.GetAllocator ());

        value = newBlock.GetTimeReceived ();
        headersInfo.AddMember("timeReceived", value, inv.GetAllocator ());

        if (!m_blockTorrent)
        {
          value = HEADERS;
          inv.AddMember("message", value, inv.GetAllocator());
        }
        else
        {
          value = EXT_HEADERS;
          inv.AddMember("message", value, inv.GetAllocator());

          value = true;
          headersInfo.AddMember("fullBlock", value, inv.GetAllocator ());
        }

        invArray.PushBack(headersInfo, inv.GetAllocator());
        inv.AddMember("blocks", invArray, inv.GetAllocator());
      }



      //Unsolicited for miners
      value = BLOCK;
      block.AddMember("message", value, block.GetAllocator());

      value.SetString("compressed-block"); //Remove
      block.AddMember("type", value, block.GetAllocator());

      value = newBlock.GetBlockHeight ();
      blockInfo.AddMember("height", value, block.GetAllocator ());

      value = newBlock.GetMinerId ();
      blockInfo.AddMember("minerId", value, block.GetAllocator ());

      value = newBlock.GetParentBlockMinerId ();
      blockInfo.AddMember("parentBlockMinerId", value, block.GetAllocator ());

      value = newBlock.GetBlockSizeBytes ();
      blockInfo.AddMember("size", value, block.GetAllocator ());

      value = newBlock.GetTimeCreated ();
      blockInfo.AddMember("timeCreated", value, block.GetAllocator ());

      value = newBlock.GetTimeReceived ();
      blockInfo.AddMember("timeReceived", value, block.GetAllocator ());

      blockArray.PushBack(blockInfo, block.GetAllocator());
      block.AddMember("blocks", blockArray, block.GetAllocator());

      break;
    }
    case UNSOLICITED_RELAY_NETWORK:
    {
      rapidjson::Value value;
      rapidjson::Value blockNodesInfo(rapidjson::kObjectType);
      rapidjson::Value blockInfo(rapidjson::kObjectType);
      rapidjson::Value invArray(rapidjson::kArrayType);
      rapidjson::Value blockArray(rapidjson::kArrayType);

      //Unsolicited for nodes
      value = BLOCK;
      inv.AddMember("message", value, inv.GetAllocator());

      value.SetString("block"); //Remove
      inv.AddMember("type", value, inv.GetAllocator());

      value = newBlock.GetBlockHeight ();
      blockNodesInfo.AddMember("height", value, inv.GetAllocator ());

      value = newBlock.GetMinerId ();
      blockNodesInfo.AddMember("minerId", value, inv.GetAllocator ());

      value = newBlock.GetParentBlockMinerId ();
      blockNodesInfo.AddMember("parentBlockMinerId", value, inv.GetAllocator ());

      value = newBlock.GetBlockSizeBytes ();
      blockNodesInfo.AddMember("size", value, inv.GetAllocator ());

      value = newBlock.GetTimeCreated ();
      blockNodesInfo.AddMember("timeCreated", value, inv.GetAllocator ());

      value = newBlock.GetTimeReceived ();
      blockNodesInfo.AddMember("timeReceived", value, inv.GetAllocator ());

      invArray.PushBack(blockNodesInfo, inv.GetAllocator());
      inv.AddMember("blocks", invArray, inv.GetAllocator());


      //Unsolicited for miners
      value = BLOCK;
      block.AddMember("message", value, block.GetAllocator());

      value.SetString("compressed-block"); //Remove
      block.AddMember("type", value, block.GetAllocator());

      value = newBlock.GetBlockHeight ();
      blockInfo.AddMember("height", value, block.GetAllocator ());

      value = newBlock.GetMinerId ();
      blockInfo.AddMember("minerId", value, block.GetAllocator ());

      value = newBlock.GetParentBlockMinerId ();
      blockInfo.AddMember("parentBlockMinerId", value, block.GetAllocator ());

      value = newBlock.GetBlockSizeBytes ();
      blockInfo.AddMember("size", value, block.GetAllocator ());

      value = newBlock.GetTimeCreated ();
      blockInfo.AddMember("timeCreated", value, block.GetAllocator ());

      value = newBlock.GetTimeReceived ();
      blockInfo.AddMember("timeReceived", value, block.GetAllocator ());

      blockArray.PushBack(blockInfo, block.GetAllocator());
      block.AddMember("blocks", blockArray, block.GetAllocator());

      break;
    }
  }

  /**
   * Update m_meanBlockReceiveTime with the timeCreated of the newly generated block
   */
  m_meanBlockReceiveTime = (m_blockchain.GetTotalBlocks() - 1)/static_cast<double>(m_blockchain.GetTotalBlocks())*m_meanBlockReceiveTime
                         + (currentTime - m_previousBlockReceiveTime)/(m_blockchain.GetTotalBlocks());
  m_previousBlockReceiveTime = currentTime;

  m_meanBlockPropagationTime = (m_blockchain.GetTotalBlocks() - 1)/static_cast<double>(m_blockchain.GetTotalBlocks())*m_meanBlockPropagationTime;

  m_meanBlockSize = (m_blockchain.GetTotalBlocks() - 1)/static_cast<double>(m_blockchain.GetTotalBlocks())*m_meanBlockSize
                  + (m_nextBlockSize)/static_cast<double>(m_blockchain.GetTotalBlocks());

  m_blockchain.AddBlock(newBlock);

  // Stringify the DOM
  rapidjson::StringBuffer invInfo;
  rapidjson::Writer<rapidjson::StringBuffer> invWriter(invInfo);
  inv.Accept(invWriter);

  rapidjson::StringBuffer blockInfo;
  rapidjson::Writer<rapidjson::StringBuffer> blockWriter(blockInfo);
  block.Accept(blockWriter);

  int count = 0;

  //std::cout << "!!!!! going into mineblock loop and switch !!!!! " << m_blockBroadcastType << "\n";
  //std::cout << " packet info " << invInfo.GetString() << "\n";
  for (std::vector<Ipv4Address>::const_iterator i = m_peersAddresses.begin(); i != m_peersAddresses.end(); ++i, ++count)
  {

    const uint8_t delimiter[] = "#";

    switch(m_blockBroadcastType)
    {
      case STANDARD:
      {
        m_peersSockets[*i]->Send (reinterpret_cast<const uint8_t*>(invInfo.GetString()), invInfo.GetSize(), 0);
        m_peersSockets[*i]->Send (delimiter, 1, 0);

        if (m_protocolType == STANDARD_PROTOCOL && !m_blockTorrent)
          m_nodeStats->invSentBytes += m_bitcoinMessageHeader + m_countBytes + inv["inv"].Size()*m_inventorySizeBytes;
        else if (m_protocolType == SENDHEADERS && !m_blockTorrent)
          m_nodeStats->headersSentBytes += m_bitcoinMessageHeader + m_countBytes + inv["blocks"].Size()*m_headersSizeBytes;
        else if (m_protocolType == STANDARD_PROTOCOL && m_blockTorrent)
        {
          m_nodeStats->extInvSentBytes += m_bitcoinMessageHeader + m_countBytes + inv["inv"].Size()*m_inventorySizeBytes;
          for (int j=0; j<inv["inv"].Size(); j++)
          {
            m_nodeStats->extInvSentBytes += 5; //1Byte(fullBlock) + 4Bytes(numberOfChunks)
            if (!inv["inv"][j]["fullBlock"].GetBool())
              m_nodeStats->extInvSentBytes += inv["inv"][j]["availableChunks"].Size()*1;
          }
        }
        else if (m_protocolType == SENDHEADERS && m_blockTorrent)
        {
          m_nodeStats->extHeadersSentBytes += m_bitcoinMessageHeader + m_countBytes + inv["blocks"].Size()*m_headersSizeBytes;
          for (int j=0; j<inv["blocks"].Size(); j++)
          {
            m_nodeStats->extHeadersSentBytes += 1;//fullBlock
            if (!inv["blocks"][j]["fullBlock"].GetBool())
              m_nodeStats->extHeadersSentBytes += inv["inv"][j]["availableChunks"].Size();
          }
        }

        NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                     << "s bitcoin miner " << GetNode ()->GetId ()
                     << " sent a packet " << invInfo.GetString()
			         << " to " << *i);
        break;
      }
      case UNSOLICITED:
      {
        m_nodeStats->blockSentBytes += m_bitcoinMessageHeader + block["blocks"][0]["size"].GetInt();

        double sendTime = m_nextBlockSize / m_uploadSpeed;
        double eventTime;

/*                 std::cout << "Node " << GetNode()->GetId() << "-" << InetSocketAddress::ConvertFrom(from).GetIpv4 ()
                          << " " << m_peersDownloadSpeeds[InetSocketAddress::ConvertFrom(from).GetIpv4 ()] << " Mbps , time = "
                          << Simulator::Now ().GetSeconds() << "s \n"; */

        if (m_sendBlockTimes.size() == 0 || Simulator::Now ().GetSeconds() >  m_sendBlockTimes.back())
        {
          eventTime = 0;
        }
        else
        {
          //std::cout << "m_sendBlockTimes.back() = m_sendBlockTimes.back() = " << m_sendBlockTimes.back() << std::endl;
          eventTime = m_sendBlockTimes.back() - Simulator::Now ().GetSeconds();
        }
        m_sendBlockTimes.push_back(Simulator::Now ().GetSeconds() + eventTime + sendTime);


        /* std::cout << sendTime << " " << eventTime << " " << m_sendBlockTimes.size() << std::endl; */
        NS_LOG_INFO("Node " << GetNode()->GetId() << " will start sending the block to " << *i
                    << " at " << Simulator::Now ().GetSeconds() + eventTime << "\n");

        std::string packet = blockInfo.GetString();
        sendTime = 0;
        eventTime = 0;
        Simulator::Schedule (Seconds(eventTime), &Consens::SendBlock, this, packet, m_peersSockets[*i]);
        Simulator::Schedule (Seconds(eventTime + sendTime), &Consens::RemoveSendTime, this);

        break;
      }
      case RELAY_NETWORK:
      {
        if(count < m_noMiners - 1)
        {
          int    noTransactions = static_cast<int>((m_nextBlockSize - m_blockHeadersSizeBytes)/m_averageTransactionSize);
          long   blockSize = m_blockHeadersSizeBytes + m_transactionIndexSize*noTransactions;
          double sendTime = blockSize / m_uploadSpeed;
          double eventTime;

          m_nodeStats->blockSentBytes += m_bitcoinMessageHeader + blockSize;

/* 				std::cout << "Node " << GetNode()->GetId() << "-" << *i
                            << " " << m_peersDownloadSpeeds[*i] << " Mbps , time = "
                            << Simulator::Now ().GetSeconds() << "s \n"; */

          if (m_sendCompressedBlockTimes.size() == 0 || Simulator::Now ().GetSeconds() >  m_sendCompressedBlockTimes.back())
          {
            eventTime = 0;
          }
          else
          {
            //std::cout << "m_sendCompressedBlockTimes.back() = m_sendCompressedBlockTimes.back() = " << m_sendCompressedBlockTimes.back() << std::endl;
            eventTime = m_sendCompressedBlockTimes.back() - Simulator::Now ().GetSeconds();
          }
          m_sendCompressedBlockTimes.push_back(Simulator::Now ().GetSeconds() + eventTime + sendTime);

          //std::cout << sendTime << " " << eventTime << " " << m_sendCompressedBlockTimes.size() << std::endl;
          NS_LOG_INFO("Node " << GetNode()->GetId() << " will start sending the block to " << *i
                      << " at " << Simulator::Now ().GetSeconds() + eventTime << "\n");

          //sendTime = blockSize / m_uploadSpeed * count;
          //std::cout << sendTime << std::endl;

          std::string packet = blockInfo.GetString();
          sendTime = 0;
          eventTime = 0;
          Simulator::Schedule (Seconds(sendTime), &Consens::SendBlock, this, packet, m_peersSockets[*i]);
          Simulator::Schedule (Seconds(eventTime + sendTime), &Consens::RemoveCompressedBlockSendTime, this);

        }
        else
        {
          m_peersSockets[*i]->Send (reinterpret_cast<const uint8_t*>(invInfo.GetString()), invInfo.GetSize(), 0);
          m_peersSockets[*i]->Send (delimiter, 1, 0);

          if (m_protocolType == STANDARD_PROTOCOL && !m_blockTorrent)
            m_nodeStats->invSentBytes += m_bitcoinMessageHeader + m_countBytes + inv["inv"].Size()*m_inventorySizeBytes;
          else if (m_protocolType == SENDHEADERS && !m_blockTorrent)
            m_nodeStats->headersSentBytes += m_bitcoinMessageHeader + m_countBytes + inv["blocks"].Size()*m_headersSizeBytes;
          else if (m_protocolType == STANDARD_PROTOCOL && m_blockTorrent)
          {
            m_nodeStats->extInvSentBytes += m_bitcoinMessageHeader + m_countBytes + inv["inv"].Size()*m_inventorySizeBytes;
            for (int j=0; j<inv["inv"].Size(); j++)
            {
              m_nodeStats->extInvSentBytes += 5; //1Byte(fullBlock) + 4Bytes(numberOfChunks)
              if (!inv["inv"][j]["fullBlock"].GetBool())
                m_nodeStats->extInvSentBytes += inv["inv"][j]["availableChunks"].Size()*1;
            }
          }
          else if (m_protocolType == SENDHEADERS && m_blockTorrent)
          {
            m_nodeStats->extHeadersSentBytes += m_bitcoinMessageHeader + m_countBytes + inv["blocks"].Size()*m_headersSizeBytes;
            for (int j=0; j<inv["blocks"].Size(); j++)
            {
            m_nodeStats->extHeadersSentBytes += 1;//fullBlock
            if (!inv["blocks"][j]["fullBlock"].GetBool())
                m_nodeStats->extHeadersSentBytes += inv["blocks"][j]["availableChunks"].Size()*1;
            }
          }

          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds ()
                       << "s bitcoin miner " << GetNode ()->GetId ()
                       << " sent a packet " << invInfo.GetString()
                       << " to " << *i);
        }
        break;
      }
      case UNSOLICITED_RELAY_NETWORK:
      {
        double sendTime;
        double eventTime;
        std::string packet;

/* 				std::cout << "Node " << GetNode()->GetId() << "-" << *i
                            << " " << m_peersDownloadSpeeds[*i] << " Mbps , time = "
                            << Simulator::Now ().GetSeconds() << "s \n"; */

        if(count < m_noMiners - 1)
        {
          int    noTransactions = static_cast<int>((m_nextBlockSize - m_blockHeadersSizeBytes)/m_averageTransactionSize);
          long   blockSize = m_blockHeadersSizeBytes + m_transactionIndexSize*noTransactions;
          sendTime = blockSize / m_uploadSpeed;

          m_nodeStats->blockSentBytes += m_bitcoinMessageHeader + blockSize;

          if (m_sendCompressedBlockTimes.size() == 0 || Simulator::Now ().GetSeconds() >  m_sendCompressedBlockTimes.back())
          {
            eventTime = 0;
          }
          else
          {
            //std::cout << "m_sendCompressedBlockTimes.back() = m_sendCompressedBlockTimes.back() = " << m_sendCompressedBlockTimes.back() << std::endl;
            eventTime = m_sendCompressedBlockTimes.back() - Simulator::Now ().GetSeconds();
          }
          m_sendCompressedBlockTimes.push_back(Simulator::Now ().GetSeconds() + eventTime + sendTime);

          //std::cout << sendTime << " " << eventTime << " " << m_sendCompressedBlockTimes.size() << std::endl;
          NS_LOG_INFO("Node " << GetNode()->GetId() << " will start sending the block to " << *i
                      << " at " << Simulator::Now ().GetSeconds() + eventTime << "\n");

          //sendTime = blockSize / m_uploadSpeed * count;
          //std::cout << sendTime << std::endl;

          std::string packet = blockInfo.GetString();
          sendTime = 0;
          eventTime = 0;
          Simulator::Schedule (Seconds(sendTime), &Consens::SendBlock, this, packet, m_peersSockets[*i]);
          Simulator::Schedule (Seconds(eventTime + sendTime), &Consens::RemoveCompressedBlockSendTime, this);
        }
        else
        {
          sendTime = m_nextBlockSize / m_uploadSpeed;
          m_nodeStats->blockSentBytes += m_bitcoinMessageHeader + m_nextBlockSize;
          if (m_sendBlockTimes.size() == 0 || Simulator::Now ().GetSeconds() >  m_sendBlockTimes.back())
          {
            eventTime = 0;
          }
          else
          {
            //std::cout << "m_sendBlockTimes.back() = m_sendBlockTimes.back() = " << m_sendBlockTimes.back() << std::endl;
            eventTime = m_sendBlockTimes.back() - Simulator::Now ().GetSeconds();
          }
          m_sendBlockTimes.push_back(Simulator::Now ().GetSeconds() + eventTime + sendTime);
          packet = invInfo.GetString();

          /* std::cout << sendTime << " " << eventTime << " " << m_sendBlockTimes.size() << std::endl; */
          NS_LOG_INFO("Node " << GetNode()->GetId() << " will send the block to " << *i
                      << " at " << Simulator::Now ().GetSeconds() + eventTime << ", eventTime = " << eventTime  << "\n");

                      sendTime = 0;
                      eventTime = 0;
          Simulator::Schedule (Seconds(eventTime), &Consens::SendBlock, this, packet, m_peersSockets[*i]);
          Simulator::Schedule (Seconds(eventTime + sendTime), &Consens::RemoveSendTime, this);

        }
	   break;
      }
    }


/* 	//Send large packet
	int k;
	for (k = 0; k < 4; k++)
	{
      ns3TcpSocket->Send (reinterpret_cast<const uint8_t*>(packetInfo.GetString()), packetInfo.GetSize(), 0);
	  ns3TcpSocket->Send (delimiter, 1, 0);
	} */


  }

  m_minerAverageBlockGenInterval = m_minerGeneratedBlocks/static_cast<double>(m_minerGeneratedBlocks+1)*m_minerAverageBlockGenInterval
                                 + (Simulator::Now ().GetSeconds () - m_previousBlockGenerationTime)/(m_minerGeneratedBlocks+1);
  m_minerAverageBlockSize = m_minerGeneratedBlocks/static_cast<double>(m_minerGeneratedBlocks+1)*m_minerAverageBlockSize
                          + static_cast<double>(m_nextBlockSize)/(m_minerGeneratedBlocks+1);
  m_previousBlockGenerationTime = Simulator::Now ().GetSeconds ();
  m_minerGeneratedBlocks++;

  //m_messageCount++;

  //std::cout << "Current node " << minerId << " message count " << m_messageCount << " \n";
  if(m_blockCount <= m_maxNumBlocks)
  {
    ScheduleNextMiningEvent ();
  }
}

void
Consens::CompMessage ()
{
  //outputFile << "CompMessage \n";
  //std::cout << GetNode()->GetId() << "!!!!!!!! Next time step message for COMP " << Simulator::Now().GetSeconds() << "\n";
  NS_LOG_FUNCTION (this);
  rapidjson::Document inv;
  rapidjson::Document block;

  int height =  m_blockchain.GetCurrentTopBlock()->GetBlockHeight() + 1;
  int minerId = GetNode ()->GetId ();
  int parentBlockMinerId = m_blockchain.GetCurrentTopBlock()->GetMinerId();
  double currentTime = Simulator::Now ().GetSeconds ();
  std::ostringstream stringStream;
  std::string blockHash;

  stringStream << height << "/" << minerId;
  blockHash = stringStream.str();

  inv.SetObject();
  block.SetObject();

  if (height == 1)
  {
    m_fistToMine = true;
	m_timeStart = GetWallTime();
  }
/*   //For attacks
   if (GetNode ()->GetId () == 0)
     height = 2 - m_minerGeneratedBlocks;

   if (GetNode ()->GetId () == 0)
   {
	if (height == 1)
      parentBlockMinerId = -1;
    else
	  parentBlockMinerId = 0;
   } */


  if (m_fixedBlockSize > 0)
    m_nextBlockSize = m_fixedBlockSize;
  else
  {
    m_nextBlockSize = m_blockSizeDistribution(m_generator) * 1000;	// *1000 because the m_blockSizeDistribution returns KBytes

    if (m_cryptocurrency == BITCOIN)
    {
      // The block size is linearly dependent on the averageBlockGenIntervalSeconds
      if(m_nextBlockSize < m_maxBlockSize - m_headersSizeBytes)
        m_nextBlockSize = m_nextBlockSize*m_averageBlockGenIntervalSeconds / m_realAverageBlockGenIntervalSeconds
                        + m_headersSizeBytes;
      else
        m_nextBlockSize = m_nextBlockSize*m_averageBlockGenIntervalSeconds / m_realAverageBlockGenIntervalSeconds;
    }
  }

  if (m_nextBlockSize < m_averageTransactionSize)
    m_nextBlockSize = m_averageTransactionSize + m_headersSizeBytes;

  Block newBlock (height, minerId, parentBlockMinerId, m_nextBlockSize,
                  currentTime, currentTime, Ipv4Address("127.0.0.1"));

  //std::cout << m_blockBroadcastType << " : " << m_protocolType << " : " << m_blockTorrent << " :  !!!!!!!!!!! ";
  switch(m_blockBroadcastType)
  {
    case STANDARD:
    {
      rapidjson::Value value;
      rapidjson::Value array(rapidjson::kArrayType);
      rapidjson::Value blockInfo(rapidjson::kObjectType);

      value.SetString("block"); //Remove
      inv.AddMember("type", value, inv.GetAllocator());

      if (m_protocolType == STANDARD_PROTOCOL)
      {
        if (!m_blockTorrent)
        {
          value = COMP;
          inv.AddMember("message", value, inv.GetAllocator());

          value.SetString(blockHash.c_str(), blockHash.size(), inv.GetAllocator());
          array.PushBack(value, inv.GetAllocator());

          inv.AddMember("inv", array, inv.GetAllocator());
        }
        else
        {
          value = EXT_INV;
          inv.AddMember("message", value, inv.GetAllocator());
          value.SetString(blockHash.c_str(), blockHash.size(), inv.GetAllocator());
          blockInfo.AddMember("hash", value, inv.GetAllocator ());

	        value = newBlock.GetBlockSizeBytes ();
          blockInfo.AddMember("size", value, inv.GetAllocator ());

          value = true;
          blockInfo.AddMember("fullBlock", value, inv.GetAllocator ());

          array.PushBack(blockInfo, inv.GetAllocator());
          inv.AddMember("inv", array, inv.GetAllocator());
        }
      }
      else if (m_protocolType == SENDHEADERS)
      {

        value = newBlock.GetBlockHeight ();
        blockInfo.AddMember("height", value, inv.GetAllocator ());

        value = newBlock.GetMinerId ();
        blockInfo.AddMember("minerId", value, inv.GetAllocator ());

        value = newBlock.GetParentBlockMinerId ();
        blockInfo.AddMember("parentBlockMinerId", value, inv.GetAllocator ());

        value = newBlock.GetBlockSizeBytes ();
        blockInfo.AddMember("size", value, inv.GetAllocator ());

        value = newBlock.GetTimeCreated ();
        blockInfo.AddMember("timeCreated", value, inv.GetAllocator ());

        value = newBlock.GetTimeReceived ();
        blockInfo.AddMember("timeReceived", value, inv.GetAllocator ());

        if (!m_blockTorrent)
        {
          value = HEADERS;
          inv.AddMember("message", value, inv.GetAllocator());
        }
        else
        {
          value = EXT_HEADERS;
          inv.AddMember("message", value, inv.GetAllocator());

          value = true;
          blockInfo.AddMember("fullBlock", value, inv.GetAllocator ());
        }

        array.PushBack(blockInfo, inv.GetAllocator());
        inv.AddMember("blocks", array, inv.GetAllocator());
      }
      break;
    }
}

  m_blockchain.AddBlock(newBlock);

  // Stringify the DOM
  rapidjson::StringBuffer invInfo;
  rapidjson::Writer<rapidjson::StringBuffer> invWriter(invInfo);
  inv.Accept(invWriter);

  rapidjson::StringBuffer blockInfo;
  rapidjson::Writer<rapidjson::StringBuffer> blockWriter(blockInfo);
  block.Accept(blockWriter);

  int count = 0;

  //std::cout << "!!!!! going into mineblock loop and switch !!!!! " << m_blockBroadcastType << "\n";
  //std::cout << " packet info " << invInfo.GetString() << "\n";

  //for (std::vector<Ipv4Address>::const_iterator i = m_peersAddresses.begin(); i != m_peersAddresses.end(); ++i, ++count)
  //{
    Ipv4Address i = m_peersAddresses[0];
    if(GetNode()->GetId() < m_leaderID)
      i = m_peersAddresses[m_leaderID-1];
    else
      i = m_peersAddresses[m_leaderID];

    const uint8_t delimiter[] = "#";

    //if(GetNode()->GetId() == 0)
      //std::cout << GetNode()->GetId() << " packet info to " << i << " : " << invInfo.GetString() << Simulator::Now().GetSeconds() << "\n";

        m_peersSockets[i]->Send (reinterpret_cast<const uint8_t*>(invInfo.GetString()), invInfo.GetSize(), 0);
        m_peersSockets[i]->Send (delimiter, 1, 0);

  //m_messageCount++;

  //std::cout << "Current node " << minerId << " message count " << m_messageCount << " \n";
  if(m_blockCount <= m_maxNumBlocks)
  {
    ScheduleNextMiningEvent ();
  }
}

void
Consens::ReceivedHigherBlock(const Block &newBlock)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_WARN("Bitcoin miner " << GetNode ()->GetId () << " added a new block in the m_blockchain with higher height: " << newBlock);
  Simulator::Cancel (m_nextMiningEvent);
  //ScheduleNextMiningEvent ();
}

void
Consens::ReceivedCompMessage(int consenBlock)
{
  //Check to see if the compMessage was for the current block or previous block
  m_nodeCompCount++;

  if(GetNode()->GetId()==m_leaderID)
  {
    //std::cout << "Miner " << GetNode()->GetId() << " Recieved a comp message " << m_nodeCompCount << " : " << Simulator::Now().GetSeconds() << "\n";
  }

  if(GetNode()->GetId() == (m_leaderID) && m_blockCount <= m_maxNumBlocks && consenBlock >= m_maxNumBlocks)
  {
   ScheduleNextMiningEvent();
  }
  return;
}

void
Consens::ReceivedStartMessage(void)
{

  if(GetNode()->GetId() == 0)
  {
    //std::cout << "Miner " << GetNode()->GetId() << " Recieved a start message " << m_nodeCompCount << " : " << Simulator::Now().GetSeconds() << "\n";
  }
  m_blockCount++;
  m_messageCount = 0;
  m_consensState = 0;
  ScheduleNextMiningEvent();
  return;
}

void
Consens::ReceivedProcMessage(void)
{
  //m_messageCount++;

  if(GetNode()->GetId() == m_leaderID)
    //std::cout << GetNode()->GetId() << " Miner Recieved a proc message " << m_messageCount << " : " << Simulator::Now().GetSeconds() << "\n";
  //if(m_blockCount <= m_maxNumBlocks)
  //{
    if(m_consensState == 1)
    {
      //m_consensState = 0;
    }
      ScheduleNextMiningEvent();
  //}
  return;
}

void
Consens::BlockWritten(void)
{
  //std::cout << " we made it to Block Written " << GetNode()->GetId() << "\n";
  //m_blockCount++;
  m_messageCount=0;
  return;
}

void
Consens::SendBlock(std::string packetInfo, Ptr<Socket> to)
{
  NS_LOG_FUNCTION (this);

  NS_LOG_INFO ("SendBlock: At time " << Simulator::Now ().GetSeconds ()
               << "s bitcoin miner " << GetNode ()->GetId () << " send "
               << packetInfo << " to " << to);

  rapidjson::Document d;

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

  d.Parse(packetInfo.c_str());
  d.Accept(writer);

/*   if (d["type"] != "compressed-block")
    m_sendBlockTimes.erase(m_sendBlockTimes.begin()); */
  SendMessage(NO_MESSAGE, BLOCK, d, to);
  m_nodeStats->blockSentBytes -= m_bitcoinMessageHeader + d["blocks"][0]["size"].GetInt();
}
} // Namespace ns3


static double GetWallTime()
{
    struct timeval time;
    if (gettimeofday(&time,NULL)){
        //  Handle error
        return 0;
    }
    return (double)time.tv_sec + (double)time.tv_usec * .000001;
}
