#include <arpa/inet.h>
#include <iostream>
#include <fstream>

static int read_marker(std::ifstream &fin)
{
    uint8_t ff = 0;
    uint8_t type = 0;
    uint16_t len;

    int pos = fin.tellg();

    if(!fin.read(reinterpret_cast<char *>(&ff), 1)) {
        return 0;
    }

    if (ff != 0xff) {
        std::cerr << "Expected 0xFF but got " << (int)ff << " at segment starting at " << std::hex << pos << std::endl;
        return 0;
    }

    if(!fin.read(reinterpret_cast<char *>(&type), 1)) {
        return 0;
    }
    std::cout << "Found type: " << std::hex << (int)type << " at segment starting at " << std::hex << pos << std::endl;
    switch(type)
    {
    case 0xd8: // SOI (Start of image)
        return 1;

    case 0xd9: // EOI (End of image)
        return 0;

    case 0xda: // SOS (Start of scan)
    case 0xe0: // JFIF APP0
    case 0xe1: // JFIF APP0
    case 0xdb: // Quantization Table
    case 0xc0: // Baseline DCT
    case 0xc4: // Huffman Table
        if (!fin.read(reinterpret_cast<char *>(&len), 2)) {
            return 0;
        }
        len = ntohs(len);
        std::cout << "Len:" << std::dec << len << std::endl;
        len -= 2;
        fin.seekg((unsigned)fin.tellg() + len);
        return 1;

    default:
        std::cout << "Unknown type: " << (unsigned int)type << " at segment starting at " << std::hex << pos << std::endl;
        return 0;
    }

    return 1;
}

int main(int argc, char **argv)
{
    std::ifstream in;
    in.open("raw.jpg");
    if (!in) {
        std::cerr << "Failed to open file\n";
        exit(1);
    }

    while(read_marker(in))
    {
    }

    return 0;
}
