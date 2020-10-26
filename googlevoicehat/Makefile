snd-soc-googlevoicehat-codec-objs := googlevoicehat-codec.o
snd-soc-rpi-simple-soundcard-objs := rpi-simple-soundcard.o
obj-m := snd-soc-googlevoicehat-codec.o
obj-m += snd-soc-rpi-simple-soundcard.o

KVERSION := $(shell uname -r)

all:
	$(MAKE) -C /lib/modules/$(KVERSION)/build M=$(PWD) modules

clean:
	$(MAKE) -C /lib/modules/$(KVERSION)/build M=$(PWD) clean

