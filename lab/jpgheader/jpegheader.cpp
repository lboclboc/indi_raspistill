#include <stdio.h>
#include <iostream>
#include <fstream>
#include <stdexcept>

/**
 * @brief The RawStreamReceiver class
 * Repsonsible for receiving a raw image from the MMAL subsystem. In this mode
 * the image consist of a normal JPEG-image, followed by a 32K broadcom header and then
 * the true raw data. This class accepts one byte at a time and spools past the JPEG header,
 * picks up the row pitch and then spools past the @BRCMo data.
 */
class RawStreamReceiver
{
public:
    class Exception : public std::runtime_error
    {
    public: Exception(const char *text) : std::runtime_error(text) {}
    };

    enum class State {
        WANT_FF,
        WANT_TYPE,
        WANT_S1,
        WANT_S2,
        SKIP_BYTES,
        ENTROPY_WANT_SKIP,
        ENTROPY_DATA_WANT_S1,
        ENTROPY_DATA_WANT_S2,
        ENTROPY_SKIP_BYTES,
        WANT_ENTROPY_DATA,
        ENTROPY_GOT_FF,
        END_OF_JPEG,
        INVALID} state {State::WANT_FF};

    RawStreamReceiver() {}

    void accept_byte(uint8_t byte);
    State getState() { return state; }
    int getPosition() { return pos; }

private:
    int pos {-1};
    int s1 {0}; // Length field, first byte MSB.
    int s2 {0}; // Length field, second byte LSB.
    int skip_bytes {0}; //Counter for skipping bytes.
    bool entropy_data_follows {false};
    int current_type {}; // For debugging only.
};

/**
 * @brief RawStreamReceiver::accept_byte Accepts next byte of input and sets state accordingly.
 * @param byte Next byte to handle.
 * @return
 */
void RawStreamReceiver::accept_byte(uint8_t byte)
{
    pos++;

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

    case State::WANT_S1:
        s1 = byte;
        state = State::WANT_S2;
        return;

    case State::WANT_S2:
        s2 = byte;
        state = State::SKIP_BYTES;
        skip_bytes = (s1 << 8) + s2 - 2;  // -2 since we already read S1 S2
        return;

    case State::SKIP_BYTES:
        skip_bytes--;
        if (skip_bytes == 0) {
            state = State::WANT_FF;
        }
        return;

    case State::ENTROPY_DATA_WANT_S1:
        s1 = byte;
        state = State::ENTROPY_DATA_WANT_S2;
        return;

    case State::ENTROPY_DATA_WANT_S2:
        s2 = byte;
        state = State::ENTROPY_SKIP_BYTES;
        skip_bytes = (s1 << 8) + s2 - 2;  // -2 since we already read S1 S2
        return;

    case State::ENTROPY_SKIP_BYTES:
        skip_bytes--;
        if (skip_bytes == 0) {
            state = State::WANT_ENTROPY_DATA;
        }
        return;

    case State::WANT_ENTROPY_DATA:
        if (byte == 0xFF) {
            state = State::ENTROPY_GOT_FF;
        }
        return;

    case State::ENTROPY_GOT_FF:
        if (byte == 0) {
            // Just an escaped 0
            state = State::WANT_ENTROPY_DATA;
            return;
        }
        else if (byte == 0xFF) {
            // Padding
            state = State::ENTROPY_GOT_FF;
            return;
        }
        // If not FF00 and FFFF then we got a real segment type now.
        // FALL THROUGH
        state = State::WANT_TYPE;

    case State::WANT_TYPE:
        current_type = byte;
        switch(byte)
        {
        case 0xd8: // SOI (Start of image)
            state = State::WANT_FF;
            return;

        case 0xd9: // EOI (End of image)
            state = State::END_OF_JPEG;
            return;

        case 0xda: // SOS (Start of scan)
        case 0xc0: // Baseline DCT
        case 0xc4: // Huffman Table
            state = State::ENTROPY_DATA_WANT_S1;
            return;

        case 0xdb: // Quantization Table
        case 0xe0: // JFIF APP0
        case 0xe1: // JFIF APP0
            state = State::WANT_S1;
            return;

        default:
            throw Exception("Unknown JPEG segment type.");
            return;
        }

    default:
        fprintf(stderr, "%s(%s): Unknown internal state.\n", __FILE__, __func__);
        state = State::INVALID;
        throw Exception("Internal error, unknown state");
    }
}

int main(int argc, char **argv)
{
    uint8_t byte;
    std::ifstream fin;
    fin.open("raw.jpg");
    if (!fin) {
        std::cerr << "Failed to open file\n";
        exit(1);
    }
    RawStreamReceiver receiver;

    try
    {
        while(fin.read(reinterpret_cast<char *>(&byte), 1))
        {
            receiver.accept_byte(byte);
            if (receiver.getState() == RawStreamReceiver::State::END_OF_JPEG) {
                fprintf(stderr, "End of jpeg at pos %d\n", receiver.getPosition());
                return 0;
            }
        }
        fprintf(stderr, "Never found end of jpeg. Pos=%d\n", receiver.getPosition());
    }
    catch(RawStreamReceiver::Exception e)
    {
        fprintf(stderr, "Caught exception %s\n", e.what());
    }

    return 0;
}
