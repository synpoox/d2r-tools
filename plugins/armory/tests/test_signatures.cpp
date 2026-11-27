#include "../src/native_signatures.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>

using armory::native::ResolveUnique;
void Check(bool ok) { if (!ok) std::abort(); }
int main(int argc, char** argv) {
    const uint8_t data[]{0xAB, 0xCD, 0xEF, 0xAB, 0x12, 0xEF};
    Check(ResolveUnique(data, "AB CD EF") == data);
    Check(ResolveUnique(data, "AB ?? EF") == nullptr);
    Check(ResolveUnique(data, "CD ?? AB") == data + 1);
    Check(ResolveUnique(data, "12 EF") == data + 4);
    Check(ResolveUnique(data, "00") == nullptr);
    Check(ResolveUnique(data, "AB ") == nullptr);
    Check(ResolveUnique(data, "??") == nullptr);
    Check(ResolveUnique(data, "") == nullptr);
    Check(ResolveUnique(data, "XX") == nullptr);
    // Optional local dump smoke test: use the executable section extracted by the researcher.
    if (argc == 2) {
        std::ifstream input(argv[1], std::ios::binary);
        Check(static_cast<bool>(input));
        const std::vector<uint8_t> text(std::istreambuf_iterator<char>{input}, {});
        for (const auto& sig : armory::native::Signatures) {
            Check(ResolveUnique(text, sig.pattern) != nullptr);
        }
    }
    std::cout << "Armory signature checks passed\n";
}
