CC = gcc

all:
	$(CC) audio_test.c \
	../hal/src/audio_hal.c \
	-I../hal/include \
	-I../tinyalsa/include \
	-L../tinyalsa/src \
	-ltinyalsa \
	-o audio_test

run:
	LD_LIBRARY_PATH=../tinyalsa/src ./audio_test

clean:
	rm -f audio_test