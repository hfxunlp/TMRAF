SRC_NAME=pptpd-1.3.3

PPPD_DIR=$(ROOTDIR)/user/pppd
PPPD_VER=2.4.7

CFLAGS  += -O3
CFLAGS  += -ffunction-sections -fdata-sections
LDFLAGS += -Wl,--gc-sections

all: config_test
	python3 /home/ano/router/TMRAF/tools/redirectOptimize.py /home/ano/router/rt-n56u/trunk/user/accel-pptpd O2 O3
	$(MAKE) -C $(SRC_NAME)

config_test:
	( if [ -f ./config_done ]; then \
		echo "the same configuration"; \
	else \
		make configure && touch config_done; \
	fi )

configure:
	( cd $(SRC_NAME) ; \
	export ac_cv_header_libintl_h=no ; \
	export ac_cv_lib_intl_gettext=no ; \
	./configure \
		KDIR=$(ROOTDIR)/$(LINUXDIR) \
		PPPDIR=$(PPPD_DIR) \
		PPPVER=$(PPPD_VER) \
		--prefix=/usr \
		--enable-bcrelay \
		--host=$(HOST_TARGET) \
		--build=$(HOST_BUILD) ; \
	)

clean:
	if [ -f $(SRC_NAME)/Makefile ] ; then \
		$(MAKE) -C $(SRC_NAME) distclean ; \
	fi ; \
	rm -f pptpd
	rm -f pptpctrl
	rm -f bcrelay
	rm -f config_done

romfs:
	cp $(SRC_NAME)/pptpd .
	cp $(SRC_NAME)/pptpctrl .
	cp $(SRC_NAME)/bcrelay .
	$(STRIP) pptpd
	$(STRIP) pptpctrl
	$(STRIP) bcrelay
	$(ROMFSINST) /usr/sbin/pptpd
	$(ROMFSINST) /usr/sbin/pptpctrl
	$(ROMFSINST) /usr/sbin/bcrelay
