// NC_STACK_embed: reader of resources embedded in SET.BAS archives.
#ifndef EMBED_H_INCLUDED
#define EMBED_H_INCLUDED

#include "nucleus/nucleas.h"
#include <deque>

class NC_STACK_embed: public NC_STACK_nucleus
{
public:
    virtual size_t Init(IDVList &) override;
    virtual size_t Deinit() override;
    virtual size_t LoadingFromIFF(IFFile **file) override;
    virtual size_t SavingIntoIFF(IFFile **file) override;

    NC_STACK_embed() {};
    virtual ~NC_STACK_embed() {};

    virtual const std::string ClassName() const override {
        return __ClassName;
    };

    //Data
    static constexpr const char * __ClassName = "embed.class";

    std::deque<NC_STACK_rsrc *> _resources;
};

#endif // EMBED_H_INCLUDED
