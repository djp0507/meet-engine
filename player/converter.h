#ifndef CONVERTER_H
#define CONVERTER_H

int64_t m3u8_get_last_timestamp();
int m3u8_get_last_seq_id();
int m3u8_create_list(char* inputfile, char* outputfile);
int m3u8_convert_ts(int64_t starttimestamp, int startsequence);

#endif
