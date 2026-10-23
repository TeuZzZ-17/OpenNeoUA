// Optional level locations. No world or rendering dependencies.
#ifndef WORLD_LOCATIONS_H_INCLUDED
#define WORLD_LOCATIONS_H_INCLUDED
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace World {
// LDF files may use UTF-8 or the game's original Windows-1252 encoding.
inline std::string LocationDisplayName(const std::string &text)
{
    bool utf8=true;
    for (size_t i=0; i<text.size();) {
        const unsigned char c=text[i++];
        if (c<128) continue;
        const int n=c>=0xc2 && c<=0xdf ? 1 : c>=0xe0 && c<=0xef ? 2 : c>=0xf0 && c<=0xf4 ? 3 : -1;
        if (n<0 || i+n>text.size()) { utf8=false; break; }
        const unsigned char first=text[i];
        if ((c==0xe0 && first<0xa0) || (c==0xed && first>=0xa0) ||
            (c==0xf0 && first<0x90) || (c==0xf4 && first>=0x90)) { utf8=false; break; }
        for (int j=0; j<n; ++j) if ((static_cast<unsigned char>(text[i++])&0xc0)!=0x80) utf8=false;
        if (!utf8) break;
    }
    if (utf8) return text;
    static const uint16_t special[32]={0x20ac,0xfffd,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,
        0x2c6,0x2030,0x160,0x2039,0x152,0xfffd,0x17d,0xfffd,0xfffd,0x2018,0x2019,0x201c,
        0x201d,0x2022,0x2013,0x2014,0x2dc,0x2122,0x161,0x203a,0x153,0xfffd,0x17e,0x178};
    std::string out;
    for (unsigned char c:text) {
        const unsigned cp=c>=0x80 && c<0xa0 ? special[c-0x80] : c;
        if (cp<128) out+=char(cp);
        else if (cp<2048) { out+=char(0xc0|(cp>>6)); out+=char(0x80|(cp&63)); }
        else { out+=char(0xe0|(cp>>12)); out+=char(0x80|((cp>>6)&63)); out+=char(0x80|(cp&63)); }
    }
    return out;
}
struct LocationPoint { double x = 0, z = 0; };
struct LocationColorRgb { uint8_t r = 240, g = 244, b = 233; };
inline std::string NormalizeLocationColor(const std::string &text)
{
    std::string name;
    name.reserve(text.size());
    for (unsigned char c:text) if (!std::isspace(c)) name+=char(std::tolower(c));
    static const char *const names[]={"white","yellow","orange","red","pink","purple",
        "blue","cyan","green","lime","gray"};
    for (const char *candidate:names) if (name==candidate) return name;
    return "white";
}
inline LocationColorRgb LocationTextColor(const std::string &text)
{
    const std::string name=NormalizeLocationColor(text);
    if (name=="yellow") return {255,221,87};
    if (name=="orange") return {255,155,74};
    if (name=="red") return {255,105,105};
    if (name=="pink") return {255,140,200};
    if (name=="purple") return {190,130,255};
    if (name=="blue") return {110,170,255};
    if (name=="cyan") return {100,225,235};
    if (name=="green") return {110,225,130};
    if (name=="lime") return {195,235,100};
    if (name=="gray") return {185,195,205};
    return {240,244,233};
}
struct Location {
    int id = 0;
    std::string name, color="white", sound;
    int fadeIn = 450, hold = 2800, fadeOut = 750;
    std::vector<LocationPoint> points;
    double minX=-std::numeric_limits<double>::infinity(), maxX=std::numeric_limits<double>::infinity();
    double minZ=-std::numeric_limits<double>::infinity(), maxZ=std::numeric_limits<double>::infinity();
};

inline void PrepareLocationBounds(Location &v)
{
    if (v.points.empty()) return;
    v.minX=v.maxX=v.points[0].x;
    v.minZ=v.maxZ=v.points[0].z;
    for (const auto &p:v.points) {
        v.minX=std::min(v.minX,p.x); v.maxX=std::max(v.maxX,p.x);
        v.minZ=std::min(v.minZ,p.z); v.maxZ=std::max(v.maxZ,p.z);
    }
}

inline double LocationCross(const LocationPoint &a, const LocationPoint &b, const LocationPoint &c)
{ return (b.x-a.x)*(c.z-a.z)-(b.z-a.z)*(c.x-a.x); }
inline bool LocationOnEdge(const LocationPoint &a, const LocationPoint &b, const LocationPoint &p)
{
    return std::abs(LocationCross(a,b,p)) <= 1e-7 &&
        p.x >= std::min(a.x,b.x)-1e-7 && p.x <= std::max(a.x,b.x)+1e-7 &&
        p.z >= std::min(a.z,b.z)-1e-7 && p.z <= std::max(a.z,b.z)+1e-7;
}
inline bool LocationContains(const Location &v, double x, double z)
{
    if (x<v.minX-1e-7 || x>v.maxX+1e-7 || z<v.minZ-1e-7 || z>v.maxZ+1e-7) return false;
    bool inside = false;
    for (size_t i=0; i<v.points.size(); ++i) {
        const auto &a=v.points[i], &b=v.points[(i+1)%v.points.size()];
        if (LocationOnEdge(a,b,{x,z})) return true;
        if ((a.z>z)!=(b.z>z) && x<(b.x-a.x)*(z-a.z)/(b.z-a.z)+a.x) inside=!inside;
    }
    return inside;
}
inline bool ValidLocation(const Location &v)
{
    if (v.id<1 || v.name.empty() || v.name.find_first_of("\r\n;")!=std::string::npos ||
        v.color!=NormalizeLocationColor(v.color) ||
        v.fadeIn<0 || v.fadeIn>10000 || v.fadeOut<0 || v.fadeOut>10000 ||
        v.hold<0 || v.hold>60000 || v.points.size()<3 || v.points.size()>256) return false;
    // UTF-8 continuation bytes do not count as separate characters.
    size_t chars=0;
    for (unsigned char c:LocationDisplayName(v.name)) if ((c&0xc0)!=0x80) ++chars;
    if (chars>96) return false;
    double area=0;
    const size_t n=v.points.size();
    for (size_t i=0; i<n; ++i) {
        const auto &a=v.points[i], &b=v.points[(i+1)%n];
        if (!std::isfinite(a.x) || !std::isfinite(a.z)) return false;
        area+=a.x*b.z-b.x*a.z;
        for (size_t j=i+1; j<n; ++j) {
            const auto &c=v.points[j], &d=v.points[(j+1)%n];
            if (a.x==c.x && a.z==c.z) return false;
            if (j==i+1 || (i==0 && j==n-1)) continue;
            if ((LocationCross(a,b,c)*LocationCross(a,b,d)<0 &&
                 LocationCross(c,d,a)*LocationCross(c,d,b)<0) ||
                LocationOnEdge(a,b,c) || LocationOnEdge(a,b,d) ||
                LocationOnEdge(c,d,a) || LocationOnEdge(c,d,b)) return false;
        }
    }
    return std::abs(area)>1e-7;
}
inline float LocationOpacity(const Location &v, int64_t elapsed)
{
    const int total=v.fadeIn+v.hold+v.fadeOut;
    if (elapsed<0 || elapsed>=total) return 0;
    if (v.fadeIn && elapsed<v.fadeIn) return float(elapsed)/v.fadeIn;
    if (elapsed<v.fadeIn+v.hold) return 1;
    return v.fadeOut ? float(total-elapsed)/v.fadeOut : 0;
}
// Only the currently controlled unit participates; later polygons take priority.
inline int FindLocation(const std::vector<Location> &values, double x, double z)
{
    for (int i=int(values.size())-1; i>=0; --i)
        if (LocationContains(values[i],x,z)) return i;
    return -1;
}
struct LocationEntryState {
    int current=-1, notice=-1;
    bool suppressInitial=false;
    int64_t started=0;
    bool update(int next, int64_t now) {
        if (suppressInitial) { current=next; suppressInitial=false; return false; }
        if (next==current) return false;
        current=next;
        if (next<0) return false;
        notice=next; started=now; return true;
    }
};
}
#endif
