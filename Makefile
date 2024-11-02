CC:=gcc
CFLAGS:=-Werror -Wall -Wextra -O2 
.o: .c
	gcc -C $(CFLAGS) $< -o $@
wav2png: wav2png.c stb_image_write.o dr_wav.o
	gcc $(CFLAGS) $^ -o $@ -lm
