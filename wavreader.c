#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>

/* Loads a .wav file at **path** into a newly allocated array of floats at
 * **wavev** of a length written to **wavec**. This may only load a mono-
 * channeled, 16-bit, linear-pcm .wav file.
 *
 * Returns 0 on success, 1 if a standard library operation failed, in which case
 * errno will not be modified after the failing call, and 2 if the format of the
 * .wav file is invalid. */
static int load_waveform(const char * path, unsigned int wavec, float * wavev);

/* For each given float ranging from 0 to 1, multiply it by 16 and return the
 * floored result. */
static int print_hex(unsigned int wavec, float * wavev);


void main(int argc, char * * argv){
	int option;
	unsigned int path_count = 0;
	char * * paths = NULL;
	unsigned int size = 64;

	/* Parse options. */
	while((option = getopt(argc, argv, "i:s:")) != -1){
		unsigned int optarg_length;
		switch(option){
		case 'i':
			printf("%c", '\0');
			char * * temp = realloc(paths, path_count + 1);
			if(!temp) goto FREE;
			paths = temp;
			optarg_length = strlen(optarg);
			char * copy = malloc(optarg_length + 1);
			if(!copy) goto FREE;
			strcpy(copy, optarg);
			paths[path_count] = copy;
			path_count++;
			break;
		case 's':
			errno = 0;
			size = (unsigned int) strtol(optarg, NULL, 0);
			if(errno) goto FREE;
			break;
		default: /* '?' */
			fprintf(stderr, "Usage: %s [-i input] [-s]\n", argv[0]);
		}
	}
	
	/* Print a message if no filenames were provided. */
	if(path_count < 1){
		fprintf(stderr, "No filenames were provided. Specify one or more input "
		"files with -i.\n");
		goto FREE;
	}

	/* Load waveforms. */
	float * * waveforms = malloc(sizeof(float) * path_count);
	int success = 1;
	if(!waveforms) goto FREE;
	for(int index = 0; index < path_count; index++) waveforms[index] = NULL;
	for(int index = 0; index < path_count; index++){
		waveforms[index] = malloc(sizeof(float) * size);
		if(!waveforms[index]) goto FREE_WAVEFORMS;
		int error;
		if((error = load_waveform(paths[index], size, waveforms[
		index]))){
			if(error == 1)
			if(errno == ENOENT) fprintf(stderr, "Nonexistent file: "
			"%s\n", paths[index]);
			if(error == 2) fprintf(stderr, "Invalid WAV file: %s\n",
			paths[index]);
			success = 0;
		}
		print_hex(size, waveforms[index]);
	}
	if(!success) goto FREE_WAVEFORMS;

	FREE_WAVEFORMS:
	for(int index = 0; index < path_count; index++) free(waveforms[index]);
	free(waveforms);

	FREE:
	for(unsigned int path_index = 0; path_index < path_count; path_index++){
		free(paths[path_index]);
	}
	free(paths);
}

static int print_hex(unsigned int wavec, float * wavev){
	const char * hex [16] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
	"10", "11", "12", "13", "14", "15"};
	for(int index = 0; index < wavec; index++){
		printf("%s", hex[(unsigned int) (wavev[index] * 16 + 8) % 16]);
		if(index < wavec - 1) printf(" "); else printf("\n");
	}
}

static int load_waveform(const char * path, unsigned int wavec, float * /*{{{*/
wavev){
/* Takes a pointer and reads four bytes following that pointer as though it were
 * a little-endian unsigned 32-bit integer. */
#define READ_UINT32(buf) ((uint32_t) *((uint8_t *) buf) + (uint32_t) (*( \
(unsigned char *) buf + 1) << 8) + (uint32_t) (*((unsigned char *) buf + 2) \
<< 16) + (uint32_t) (*((unsigned char *) buf + 3) << 24))
#define READ_UINT16(buf) ((uint16_t) *((uint8_t *) buf) + (uint16_t) (*( \
(unsigned char *) buf + 1) << 8))
	int error = 0;
	char buffer [41];

	/* Open file. */
	FILE * file = fopen(path, "r");
	if(!file) return 1;

	/* Find file size. */
	long size;
	if(fseek(file, 0, SEEK_END) == -1) {error = 1; goto CLOSE;}
	if((size = ftell(file)) == -1) {error = 1; goto CLOSE;}
	if(fseek(file, 0, SEEK_SET) == -1) {error = 1; goto CLOSE;}

	/* Read the RIFF header. */
	long chunk_size;
	if(size < 8) {error = 2; goto CLOSE;}
	if(!fgets(buffer, 9, file)) {error = 1; goto CLOSE;}
	if(READ_UINT32(&buffer[0]) != 0x46464952) {error = 2; goto CLOSE;}
	chunk_size = READ_UINT32(&buffer[4]);

	/* Assert that the RIFF chunk has a WAVE identifier. */
	if(chunk_size < 4) {error = 2; goto CLOSE;}
	if(!fgets(buffer, 5, file)) {error = 1; goto CLOSE;}
	if(READ_UINT32(&buffer[0]) != 0x45564157) {error = 2; goto CLOSE;}

	/* Read the format header. */
	long fmt_chunk_size;
	if(chunk_size < 12) {error = 2; goto CLOSE;}
	if(!fgets(buffer, 9, file)) {error = 1; goto CLOSE;}
	if(READ_UINT32(&buffer[0]) != 0x20746d66) {error = 2; goto CLOSE;}
	fmt_chunk_size = READ_UINT32(&buffer[4]);

	/* Read the format. */
	if(fmt_chunk_size < 16) {error = 2; goto CLOSE;}
	if(!fgets(buffer, fmt_chunk_size + 1, file)) {error = 1; goto CLOSE;}
	int fmt_code = READ_UINT16(&buffer[0]);
	int channels = READ_UINT16(&buffer[2]);
	long sample_rate = READ_UINT32(&buffer[4]);
	long byte_rate = READ_UINT32(&buffer[8]);
	int align = READ_UINT16(&buffer[12]);
	int bits_per_sample = READ_UINT16(&buffer[14]);
	if(fmt_code != 1) {error = 2; goto CLOSE;}
	if(channels != 1) {error = 2; goto CLOSE;}
	if(align != 2) {error = 2; goto CLOSE;}
	if(bits_per_sample != 16) {error = 2; goto CLOSE;}

	/* Read the data. */
	long data_length;
	long data_start;
	if(!fgets(buffer, 9, file)) {error = 1; goto CLOSE;}
	if(READ_UINT32(&buffer[0]) != 0x61746164) {error = 2; goto CLOSE;}
	data_length = READ_UINT32(&buffer[4]);
	if(!data_length) {error = 2; goto CLOSE;}
	if((data_start = ftell(file)) == -1) {error = 1; goto CLOSE;}

	/* Write samples to wavev. */
	for(unsigned int index = 0; index < wavec; index++){
		long datum_index = index * data_length / wavec;
		datum_index &= ~0x1;
		datum_index += data_start;
		if(fseek(file, datum_index, SEEK_SET) == -1) {error = 1;
		goto CLOSE;}
		if(!fgets(buffer, 3, file)) {error = 1; goto CLOSE;}
		wavev[index] = (float) (int16_t) READ_UINT16(&buffer[0]) /
		UINT16_MAX;
	}

	/* Two bytes per sample. */
	error = 0;

	CLOSE:
	if(fclose(file)){
		perror("load_waveform");
		exit(1);
	}
	return error;
#undef READ_UINT32
} /*}}}*/
