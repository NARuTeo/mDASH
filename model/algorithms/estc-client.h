/*
 * estc-client.h
 *
 *  Created on: May 22, 2018
 *      Author: Dong
 */

#ifndef ESTC_CLIENT_H_
#define ESTC_CLIENT_H_

#include <ns3/dash-client.h>
namespace ns3
{

  class EstcClient : public DashClient
  {
  public:
    static TypeId
    GetTypeId(void);

    EstcClient();

    virtual
    ~EstcClient();

    virtual void
    CalcNextSegment(uint32_t currRate, uint32_t & nextRate, double aveRate);

  private:
    bool
    UnderFlow(uint32_t bitRate);

    bool
    OverFlow(uint32_t bitRate);

    void
    SleepWhile();

    int e_count;
    Time e_minDt;
    Time e_maxDt;

    int sleep_count;
    bool fast_start;
  };

} /* namespace ns3 */

#endif /* ESTC_CLIENT_H_ */
