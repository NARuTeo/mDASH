/*
 * estc-client.cc
 *
 *  Created on: May 22, 2018
 *      Author: Dong
 */

#include "estc-client.h"
#include <ns3/log.h>
#include <ns3/simulator.h>
#include <ns3/dash-client.h>
#include <unistd.h>

NS_LOG_COMPONENT_DEFINE("EstcClient");

namespace ns3
{
  NS_OBJECT_ENSURE_REGISTERED(EstcClient);

  TypeId
  EstcClient::GetTypeId(void)
  {
    static TypeId tid =
        TypeId("ns3::EstcClient").SetParent<DashClient>().AddConstructor<
            EstcClient>();
    return tid;
  }

  EstcClient::EstcClient(): e_minDt(Seconds(12)), e_maxDt(Seconds(30)), sleep_count(0), fast_start(false)
  {
    // TODO Auto-generated constructor stub

  }

  EstcClient::~EstcClient()
  {
    // TODO Auto-generated destructor stub
  }

  void
  EstcClient::CalcNextSegment(uint32_t currRate, uint32_t & nextRate,
      double aveRate)
  {
    uint32_t rates[] =
      { 100000, 200000, 300000, 400000, 500000, 600000, 700000, 900000, 1000000, 1200000,
          1500000, 2000000, 2500000, 3000000, 3500000, 4000000, 4500000, 5000000, 5500000, 6000000 };

    int e_o = 5;

    // simple fast start logic. Copy form panda.
    int i=0;
    while (currRate != rates[i])
        i++;
    if (m_bitrateEstimate > currRate && fast_start)
    {
          nextRate = rates[i+1];
    }
    if (nextRate == 2000000 && fast_start)
    {
        fast_start = false;
    }

    if (!fast_start)
    {
        if (UnderFlow(rates[i]))
        {
            sleep_count = 0;
            estc_sleep = false;
            double minR = (m_currDt.GetSeconds()-e_minDt.GetSeconds())*m_bitrateEstimate / 2;
            int j=0;
            if (minR <= rates[0])
                nextRate = rates[0];
            else
            {
                while (rates[j] < minR)
                    j++;
                nextRate = rates[j-1];
            }
            e_count = 0;
            std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id << " ########## end a ########" << m_currDt.GetSeconds() << std::endl;
        }
        // !!! have not consider the situation: i+1>rates_size.
        else if (!UnderFlow(rates[i+1]) && !OverFlow(rates[i+1]) && e_count >= e_o && m_bitrateEstimate>rates[i+1])
        {
            sleep_count = 0;
            estc_sleep = false;
            nextRate = rates[i+1];
            e_count = 0;
            m_idleTime = Seconds(0);
            std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id << " ########## end b ########" << std::endl;
        }
        else if (OverFlow(rates[i]))
        {
            sleep_count++;
            double etf = currRate*2 / m_bitrateEstimate;
            int sleep_time;
            if (2-etf > 0)
                sleep_time = sleep_count*(2-etf);
            else
                sleep_time = 0;
            m_idleTime = Seconds(sleep_time);
            //m_idleTime = Seconds(1*(m_currDt.GetSeconds()-Seconds(25)));
            //Simulator::Schedule(m_idleTime, &EstcClient::SleepWhile, this);
            std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id << " ########## end c ########" << m_idleTime.GetSeconds() << std::endl;
        }
        else
        {
            sleep_count = 0;
            estc_sleep = false;
            nextRate = currRate;
            e_count++;
            m_idleTime = Seconds(0);
            std::cout << Simulator::Now().GetSeconds() << " Node: " << m_id << " ########## end d ########" << std::endl;
        }
    }

  }

  void
  EstcClient::SleepWhile()
  {
      // do nothing
  }

  bool
  EstcClient::UnderFlow(uint32_t bitRate)
  {
      double etf = bitRate*2 / m_bitrateEstimate;
      if (etf >= m_currDt.GetSeconds()-e_minDt.GetSeconds())
        return true;
      else
        return false;
  }

  bool
  EstcClient::OverFlow(uint32_t bitRate)
  {
      double etf = bitRate*2 / m_bitrateEstimate;
      if (2 >= e_maxDt.GetSeconds()-m_currDt.GetSeconds()+etf)
        return true;
      else
        return false;
  }








} /* namespace ns3 */
