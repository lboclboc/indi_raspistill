#include <stdio.h>
#include <arpa/inet.h>
#include <iostream>
#include <fstream>
#include <stdexcept>

class RawStreamReceiver
{
public:
    class Exception : std::runtime_error
    {
    public: Exception(char *text) : std::runtime_error(text) {}
    };

    RawStreamReceiver();

    void accept_byte(uint8_t byte);

private:
    enum class State {
        WANT_FF,
        WANT_TYPE,
        WANT_S1,
        WANT_S2,
        WANT_SKIP,
        ENTROPY_WANT_SKIP,
        ENTROPY_DATA_WANT_S1,
        ENTROPY_DATA_WANT_S1,
        WANT_ENTROPY_DATA,
        END_OF_JPEG,
        INVALID} state {State::WANT_FF};
    int pos {0};
    int s1 {0}; // Length field, first byte MSB.
    int s2 {0}; // Length field, second byte LSB.
    int skip_bytes {0}; //Counter for skipping bytes.
};

/**
 * @brief RawStreamReceiver::accept_byte Accepts next byte of input and sets state accordingly.
 * @param byte Next byte to handle.
 * @return
 */
void RawStreamReceiver::accept_byte(uint8_t byte)
{
    uint8_t ff = 0;
    uint8_t type = 0;
    uint16_t len;
    static int saved_type = -1;

    switch(state)
    {
    case State::END_OF_JPEG:
    case State::INVALID:
        throw Exception("State invalid");
        return;

    case State::WANT_FF:
        if (byte != 0xFF) {
            throw Exception("Expected 0xFF");
        }
        state = State::WANT_TYPE;
        return;

    case State::WANT_ENTROPY_DATA:
        if (byte == 0xFF) {
            state = State:

        }
        return;

    case State::WANT_TYPE:
        switch(byte)
        {
        case 0xd8: // SOI (Start of image)
            state = State::WANT_FF;
            return;

        case 0xd9: // EOI (End of image)
            state = State::END_OF_JPEG;
            return;

        case 0xda: // SOS (Start of scan)
        case 0xdb: // Quantization Table
        case 0xc0: // Baseline DCT
        case 0xc4: // Huffman Table
            state = State::ENTROPY_DATA_WANT_S1;
            return;

        case 0xe0: // JFIF APP0
        case 0xe1: // JFIF APP0
            state = State:WANT_S1;
            return;
        }

    default:
        fprintf(stderr, "%s(%s): Unknown internal state.\n", __FILE__, __func__);
        state = State::INVALID;
        throw Exception("Internal error, unknown state");
    }


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
