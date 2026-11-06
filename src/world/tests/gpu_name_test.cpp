// Checks the rule that decides whether a graphics chip name is a built-in one.
// Same text as GFXEngine::ReportGraphicsChip.
#include <cstdio>
#include <cctype>
#include <string>

static bool Integrated(const char *renderer)
{
    std::string name = renderer ? renderer : "";
    for (char &letter : name)
        letter = (char)std::tolower((unsigned char)letter);

    return (name.find("arc") == std::string::npos) &&
           (name.find("intel") != std::string::npos ||
            name.find("uhd graphics") != std::string::npos ||
            name.find("hd graphics") != std::string::npos ||
            name.find("iris") != std::string::npos ||
            name.find("radeon(tm) graphics") != std::string::npos ||
            name.find("vega") != std::string::npos ||
            name.find("llvmpipe") != std::string::npos ||
            name.find("softpipe") != std::string::npos ||
            name.find("microsoft basic") != std::string::npos);
}

int main()
{
    struct Case { const char *name; bool integrated; };
    const Case cases[] = {
        {"NVIDIA GeForce RTX 5070 Ti Laptop GPU/PCIe/SSE2", false},
        {"NVIDIA GeForce GTX 1060 6GB/PCIe/SSE2", false},
        {"AMD Radeon RX 7900 XTX", false},
        {"Radeon RX 580 Series", false},
        {"Intel(R) Arc(TM) A770 Graphics", false},
        {"Intel(R) Arc(TM) B580 Graphics", false},
        {"Intel(R) Graphics", true},
        {"Intel(R) UHD Graphics 770", true},
        {"Intel(R) Iris(R) Xe Graphics", true},
        {"AMD Radeon(TM) Graphics", true},
        {"AMD Radeon(TM) Vega 8 Graphics", true},
        {"llvmpipe (LLVM 15.0.6, 256 bits)", true},
        {"Microsoft Basic Render Driver", true},
        {nullptr, false},
    };

    int checks = 0, failures = 0;
    for (const Case &test : cases)
    {
        ++checks;
        if (Integrated(test.name) != test.integrated)
        {
            ++failures;
            std::printf("FAIL %s\n", test.name ? test.name : "(null)");
        }
    }
    std::printf("GPU NAME CHECKS %d FAILURES %d\n", checks, failures);
    return failures ? 1 : 0;
}
