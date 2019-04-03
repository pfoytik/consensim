/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
#include <iostream>
#include "ns3/simulator.h"
#include "ns3/nstime.h"
#include "ns3/command-line.h"
#include "ns3/double.h"
#include "ns3/random-variable-stream.h"
using namespace ns3;
namespace {
  
class MyModel
{
public:
  void Start (void);
private:
  void HandleEvent (double eventValue);
};
void
MyModel::Start (void)
{
std::cout << "event time is : " << Simulator::Now().GetSeconds() << "\n";

  Simulator::Schedule (Seconds (0.5),
                       &MyModel::HandleEvent,
                       this, Simulator::Now ().GetSeconds ());

//std::cout << "event time is : " << Simulator::Now().GetSeconds() << "\n";

  Simulator::Schedule (Seconds (0.5),
                       &MyModel::HandleEvent,
                       this, Simulator::Now ().GetSeconds ());

}
void
MyModel::HandleEvent (double value)
{
std::cout << "event time is : " << Simulator::Now().GetSeconds() << "\n";

  std::cout << "Member method received event at "
            << Simulator::Now ().GetSeconds ()
            << "s started at " << value << "s" << std::endl;
}
static void
ExampleFunction (MyModel *model)
{
std::cout << "event time is : " << Simulator::Now().GetSeconds() << "\n";

  std::cout << "ExampleFunction received event at "
            << Simulator::Now ().GetSeconds () << "s" << std::endl;
  model->Start ();
}
static void
ExampleFunction2 (MyModel *model)
{
std::cout << "event time is : " << Simulator::Now().GetSeconds() << "\n";

  std::cout << "ExampleFunction 2 received event at "
            << Simulator::Now ().GetSeconds () << "s" << std::endl;
  model->Start ();
}
static void
RandomFunction (void)
{
	std::cout << "event time is : " << Simulator::Now().GetSeconds() << "\n";

  std::cout << "RandomFunction received event at "
            << Simulator::Now ().GetSeconds () << "s" << std::endl;
}
static void
CancelledEvent (void)
{
  std::cout << "I should never be called... " << std::endl;
}
}  // unnamed namespace
int main (int argc, char *argv[])
{
  double newTime = 19;
  CommandLine cmd;
  cmd.Parse (argc, argv);
  MyModel model;
 
  //model.Start();
   
  Ptr<UniformRandomVariable> v = CreateObject<UniformRandomVariable> ();
  v->SetAttribute ("Min", DoubleValue (10));
  v->SetAttribute ("Max", DoubleValue (20));
  std::cout << "event time is : " << Simulator::Now().GetSeconds() << "\n";
  Simulator::Schedule (Seconds (newTime), &ExampleFunction, &model);
  //newTime+=0.5;
  
  //std::cout << "event time is : " << Simulator::Now().GetSeconds() << "\n";

  Simulator::Schedule (Seconds (5), &ExampleFunction2, &model);
  //std::cout << "event time is : " << Simulator::Now().GetSeconds() << "\n";

  Simulator::Schedule (Seconds (v->GetValue ()), &RandomFunction);
  //std::cout << "event time is : " << Simulator::Now().GetSeconds() << "\n";

  EventId id = Simulator::Schedule (Seconds (30.0), &CancelledEvent);
  //std::cout << "event time is : " << Simulator::Now().GetSeconds() << "\n";

  Simulator::Cancel (id);
  Simulator::Run ();
  Simulator::Destroy ();
}
