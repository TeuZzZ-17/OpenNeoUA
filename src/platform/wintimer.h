// NC_STACK_wintimer: timer based on the operating system clock.
#ifndef WINTIMER_H_INCLUDED
#define WINTIMER_H_INCLUDED

#include "platform/itimer.h"

class NC_STACK_wintimer: public NC_STACK_itimer
{
public:
    virtual int itimer_func64() override;

    NC_STACK_wintimer() {};
    virtual ~NC_STACK_wintimer() {};

    virtual const std::string ClassName() const override {
        return __ClassName;
    };

    //Data
    static constexpr const char * __ClassName = "wintimer.class";

public:
    uint64_t ticks = 0; // Absolute time of the previous call, in 1/1024 s.
};

#endif // WINTIMER_H_INCLUDED
