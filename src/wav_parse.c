#include "wav_parse.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// Convert 32-bit unsigned little-endian value to big-endian from byte array
static inline uint32_t little2big_u32(uint8_t const* data) {
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

// Convert 16-bit unsigned little-endian value to big-endian from byte array
static inline uint16_t little2big_u16(uint8_t const* data) {
    return data[0] | (data[1] << 8);
}

// Copy n bytes from source to destination and terminate the destination with
// null character. Destination must be at least (amount + 1) bytes big to
// account for null character.
static inline void bytes_to_string(uint8_t const* source,
    char* destination,
    size_t amount) {

    memcpy(destination, source, amount);
    destination[amount] = '\0';
}

// Parse the header of WAV file and return WAVFile structure with header and
// pointer to data
/* ~~~~~~~~~~~~~~~~~ WAV DATA ORDER ~~~~~~~~~~~~~~~~~ 
1 – 4	    “RIFF”	Marks the file as a riff file. Characters are each 1 byte long.
5 – 8	    File size (integer)	Size of the overall file – 8 bytes, in bytes (32-bit integer).
9 -12   	“WAVE”	File Type Header. For our purposes, it always equals “WAVE”.
13-16	    “fmt “	Format chunk marker. Includes trailing null
17-20   	16	Length of format data as listed above
21-22	    1	Type of format (1 is PCM) – 2 byte integer
23-24   	2	Number of Channels – 2 byte integer
25-28	    44100	Sample Rate – 32 byte integer. Common values are 44100 (CD), 48000 (DAT).
29-32   	176400	(Sample Rate * BitsPerSample * Channels) / 8.
33-34	    4	(BitsPerSample * Channels) / 8.1 – 8 bit mono2 – 8 bit stereo/16 bit mono4 – 16 bit stereo
35-36   	16	Bits per sample
37+ -40+	“data”	“data” chunk header. Marks the beginning of the data section.
next 4 bits File size (data) Size of the data section.
*/
WAVFile_t WAV_ParseFileData(uint8_t const* data) {
    WAVFile_t file;
    uint8_t const* data_ptr = data;

    bytes_to_string(data_ptr, file.header.file_id, 4);
    data_ptr += 4;

    file.header.file_size = little2big_u32(data_ptr);
    data_ptr += 4;

    bytes_to_string(data_ptr, file.header.format, 4);
    data_ptr += 4;

    bytes_to_string(data_ptr, file.header.subchunk_id, 4);
    data_ptr += 4;

    file.header.subchunk_size = little2big_u32(data_ptr);
    data_ptr += 4;

    file.header.audio_format = little2big_u16(data_ptr);
    data_ptr += 2;

    file.header.number_of_channels = little2big_u16(data_ptr);
    data_ptr += 2;

    file.header.sample_rate = little2big_u32(data_ptr);
    data_ptr += 4;

    file.header.byte_rate = little2big_u32(data_ptr);
    data_ptr += 4;

    file.header.block_align = little2big_u16(data_ptr);
    data_ptr += 2;

    file.header.bits_per_sample = little2big_u16(data_ptr);
    data_ptr += 2;

    // Search for the "data" header
    while (!((*data_ptr == 'd') && (*(data_ptr+1) == 'a') &&
           (*(data_ptr+2) == 't') && (*(data_ptr+3) == 'a'))) {
        data_ptr++;
    }

    bytes_to_string(data_ptr, file.header.data_id, 4);
    data_ptr += 4;

    file.header.data_size = little2big_u32(data_ptr);
    data_ptr += 4;

    file.data = data_ptr;
    file.data_length = file.header.data_size;

    return file;
}