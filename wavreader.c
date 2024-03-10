#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>

/* Loads a wave file into memory, writing a pointer to an array of samples into
**samplev**, and the size of that array into **samplec**. The user is
resposible for freeing the memory when done.

Returns 0 on success, 1 if an stdlib function has failed and errno was set, and
2 if the format of the .wav file is invalid. */
static int load_waveform(const char * path, int * samplec, uint16_t * *
samplev);

/* Print the leftmost nibble of **length** **wavev** samples in sequence. */
static int print_hex(unsigned int wavec, uint16_t * wavev, int length);

/* Print a graph **length** **wavev** samples. */
static int print_graph(unsigned int wavec, uint16_t * wavev, int length);

/* Available options:
	-i specify input filepath.
	-s specify length of printed hex sequence.
	-c specify amount of slices to chop the file into.
	-l specify length in simples of each slice.
	-n specify instrument number. */
int main(int argc, char * * argv){
	int error = EXIT_FAILURE;
	char * path = NULL;
	int size = 64; /* The length in nibbles of each waveform. */
	int count = 1; /* Amount of waveforms to slice the file into. */
	int length = 0; /* The length in audio samples of each slice, or
	zero if each slice expands to maximum. */
	int number = 0; /* Instrument number. */
	
	unsigned int wavec;
	uint16_t * wavev;

	/* Parse options. */
	for(int option = 0; (option = getopt(argc, argv, "i:s:c:l:n:m")) != -1;){
		switch(option){
			case 'i':
				size_t optarg_length = strlen(optarg);
				path = malloc(optarg_length + 1);
				if(!path) goto EXIT;
				strcpy(path, optarg);
				break;
			case 'n':
				errno = 0;
				number = (int) strtol(optarg, NULL, 0);
				if(errno) goto EXIT;
				break;
			case 's':
				errno = 0;
				size = (int) strtol(optarg, NULL, 0);
				if(errno) goto EXIT;
				break;
			case 'c':
				errno = 0;
				count = (int) strtol(optarg, NULL, 0);
				if(errno) goto EXIT;
				break;
			case 'l':
				errno = 0;
				length = (int) strtol(optarg, NULL, 0);
				if(errno) goto EXIT;
				break;
			default: /* '?' */
				fprintf(stderr, "Usage: %s [-i <input filepath>] [-s <length "
				"of generated waveforms>] [-c <amount of slices>] [-l "
				"<samples per slice>] [-n <instrument number>] [-m]\n",
				argv[0]);
				error = EXIT_SUCCESS;
				goto EXIT;
		}
	}
	
	/* Print a message if no filename was provided. */
	if(!path){
		fprintf(stderr, "No filenames were provided. Specify an input file "
		"with -i.\n");
		error = EXIT_SUCCESS;
		goto EXIT;
	}
	
	/* Load waveform. */
	do{
		int load_error = load_waveform(path, &wavec, &wavev);
		if(load_error == 1){
			if(errno == ENOENT)
				fprintf(stderr, "Nonexistent file: %s\n", path);
			else
				perror(NULL);
			goto EXIT;
		}
		if(load_error == 2){
			fprintf(stderr, "Unsupported WAV file format: %s\n", path);
			goto EXIT;
		}
	}while(0);
	
	do{
		if(!length) length = wavec / count;
		
		for(int slice_index = 0; slice_index < count; slice_index++){
			print_graph(length, wavev + (wavec * slice_index / count) - (length
			* slice_index / count), size);
			print_hex(length, wavev + (wavec * slice_index / count) - (length *
			slice_index / count), size);
			printf("\n");
		}
		
		printf("TEXT FILE EXPORT:\n");
		/* Data items: instrument number, volume, arppegio, pitch, hi-pitch,
		duty, waveform length, waveform position, waveform count, name in
		quotes. */
		printf("INSTN163 %i -1 -1 -1 -1 -1 %i 0 %i \"New Instrument\"\n",
		number, size, count);
		for(int slice_index = 0; slice_index < count; slice_index++){
			printf("N163WAVE %i %i : ", number, slice_index);
			print_hex(length, wavev + (wavec * slice_index / count) - (length *
			slice_index / count), size);
			
		}
	} while(0);
	
	error = EXIT_SUCCESS;

	FREE_WAVEFORM:
	free(wavev);
	EXIT:
	return error;
}

static int print_hex(unsigned int wavec, uint16_t * wavev, int length){
	const char * hex [16] = {"0 ", "1 ", "2 ", "3 ", "4 ", "5 ", "6 ", "7 ", "8 ", "9 ",
	"10", "11", "12", "13", "14", "15"};
	for(int index = 0; index < length; index++){
		int wave_index = wavec * index / length;
		printf("%s", hex[(unsigned int) ((wavev[wave_index] & ~0x0FFF) >> 12) % 16]);
		if(index < length - 1) printf(" "); else printf("\n");
	}
}

static int print_graph(unsigned int wavec, uint16_t * wavev, int length){
	for(int y = 15; y >= 0; y--){
		for(int index = 0; index < length; index++){
			int wave_index = wavec * index / length;
			if((wavev[wave_index] & ~0x0FFF) >> 12 > y){
				printf("\e[37;47m   \e[0m");
			} else {
				printf("   ");
			}
		}
		printf("\n");
	}
	
}

static int load_waveform(const char * path, int * samplec, uint16_t * * samplev){
/* Takes a pointer and reads four bytes following that pointer as though it were
a little-endian unsigned 32-bit integer. */
#define READ_UINT32(buf) ((uint32_t) *((uint8_t *) (buf)) + (uint32_t) (*( \
(unsigned char *) (buf) + 1) << 8) + (uint32_t) (*((unsigned char *) (buf) + \
2) << 16) + (uint32_t) (*((unsigned char *) (buf) + 3) << 24))
#define READ_UINT16(buf) ((uint16_t) *((uint8_t *) buf) + (uint16_t) (*( \
(unsigned char *) (buf) + 1) << 8))
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
	if(READ_UINT32(&buffer[0]) != 0x46464952) {printf("foo\n"); error = 2; goto CLOSE;}
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
	
	/* Allocate data array. */
	int samples_length = data_length / sizeof(uint16_t);
	uint16_t * samples = malloc(data_length);
	if(!samples){error = 1; goto CLOSE;}
	
	/* Load raw bytes into samples array. */
	if(fread(samples, sizeof(char), data_length, file) != data_length){
		error = 1;
		goto DEALLOC;
	}
	
	/* Reinterpret each pair of bytes as an unsigned short. */
	for(unsigned int index = 0; index < data_length / sizeof(uint16_t); index++){
		samples[index] = READ_UINT16(samples + index) + UINT16_MAX / 2;
	}

	/* Write and return. */
	*samplec = samples_length;
	*samplev = samples;
	return 0;

	DEALLOC:
	free(samples);
	CLOSE:
	if(fclose(file)){
		perror("load_waveform");
		exit(1);
	}
	return error;
#undef READ_UINT32
#undef READ_UINT16
} /*}}}*/
