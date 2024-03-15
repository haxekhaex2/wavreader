# wavreader
This is a small command line utility I wrote to generate Famitracker Namco 163 instrument files out of .wav files.

### Usage:
``wavreader -i "input.wav" -o "output.fti" -s 64 -c 16 -l 100``

The above command splits the audio file into 64 regions, each 100 samples long, and generates out of them an N163 instrument file with 16 waveforms.

### Options:
``-i`` input .wav file path.

``-o`` output .fti file path.

``-s`` size of generated N163 waveform.

``-c`` count of regions to split the .wav file into.

``-l`` length in samples of each region each waveform will sample from.

``-e`` if set, extend the start positions of each slice to minimize the unused space at the end.
