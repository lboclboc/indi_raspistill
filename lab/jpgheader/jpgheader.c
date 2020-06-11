#include <stdio.h>
#include <jpeglib.h>

#if 0
struct jpeg_source_mgr {
  const JOCTET *next_input_byte; /* => next byte to read from buffer */
  size_t bytes_in_buffer;       /* # of bytes remaining in buffer */

  void (*init_source) (j_decompress_ptr cinfo);
  boolean (*fill_input_buffer) (j_decompress_ptr cinfo);
  void (*skip_input_data) (j_decompress_ptr cinfo, long num_bytes);
  boolean (*resync_to_restart) (j_decompress_ptr cinfo, int desired);
  void (*term_source) (j_decompress_ptr cinfo);
};
#endif

void init_source(j_decompress_ptr cinfo)
{
}

boolean fill_input_buffer(j_decompress_ptr cinfo)
{
}

void skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
}

boolean resync_to_restart(j_decompress_ptr cinfo, int desired)
{
}

void term_source(j_decompress_ptr cinfo)
{
}

struct jpeg_source_mgr source = {
        .next_input_byte = 0,
        .bytes_in_buffer = 0,
        .init_source = init_source,
        .fill_input_buffer = fill_input_buffer,
        .skip_input_data = skip_input_data,
        .resync_to_restart = resync_to_restart,
        .term_source = term_source
        };

static struct jpeg_decompress_struct info = {
  .src = &source
};

int main(int argc, char **argv)
{
    printf("Starting\n");        
    jpeg_read_header(&info, TRUE);
}
