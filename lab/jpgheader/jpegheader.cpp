#include <arpa/inet.h>
#include <iostream>
#include <fstream>

static int read_marker(std::ifstream &fin)
{
    uint8_t ff = 0;
    uint8_t type = 0;
    uint16_t len;
    static int saved_type = -1;

    int pos = fin.tellg();

    if (saved_type == -1)  {
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
    }
    else {
        pos -= 2; // The FF for this type was actually found during the entropy_data scan.
        type = saved_type;
        saved_type = -1;
    }

    std::cout << "Found type: " << std::hex << (int)type << " at segment starting at " << std::hex << pos << std::endl;
    bool read_entropy_data = false;     // Some segmens are followed by entropy data. FF are escaped as FF 00.
    switch(type)
    {
    case 0xd8: // SOI (Start of image)
        return 1;

    case 0xd9: // EOI (End of image)
        return 0;

    case 0xda: // SOS (Start of scan)
        read_entropy_data = true;
        // Fall through

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

        if (read_entropy_data) {
            // Scan pass the entropy_data section.
            // Handle special case for 0xFF:
            // 0xFF 0x00 is an escaped FF for the section.
            // 0xFF 0xFF is padding.
            // 0xFF <any> means a new segment of 0xFF <type>
            enum { NORMAL, FOUND_FF } state = NORMAL;
            while(fin)
            {
                if (!fin.read(reinterpret_cast<char *>(&ff), 1)) {
                    return 0;
                }
                switch(state)
                {
                case NORMAL:
                    if (ff == 0xFF) {
                        state = FOUND_FF;
                    }
                    continue;

                case FOUND_FF:
                    if (ff == 0) {
                        state = NORMAL;
                    }
                    else if (ff == 0xFF) {
                        state = FOUND_FF;
                    }
                    else {
                        saved_type = ff;
                        return 1;
                    }
                }
            }
        }

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
