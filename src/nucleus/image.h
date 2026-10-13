// NC_STACK_image: bitmap created from an image file.
#ifndef IMAGE_H_INCLUDED
#define IMAGE_H_INCLUDED

#include "nucleus/bitmap.h"

class NC_STACK_image: public NC_STACK_bitmap
{
public:
    virtual size_t Init(IDVList &stak) override;
    virtual rsrc * rsrc_func64(IDVList &stak) override;

    static bool SavePng(ResBitmap *bmp, const std::string &fname);

    NC_STACK_image();
    virtual ~NC_STACK_image();

    virtual const std::string ClassName() const override {
        return __ClassName;
    };


    //Data
public:
    static constexpr const char * __ClassName = "image.class";
};

#endif // ILBM_H_INCLUDED
