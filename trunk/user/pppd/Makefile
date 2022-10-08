# PPP top-level Makefile for Linux.

PPPD_DIR = $(ROOTDIR)/user/pppd
INSTROOT = $(ROOTDIR)/romfs

DESTDIR = $(INSTROOT)/usr
BINDIR = $(DESTDIR)/sbin
INCDIR = $(DESTDIR)/include
MANDIR = $(DESTDIR)/share/man
ETCDIR = $(INSTROOT)/tmp/ppp

# uid 0 = root
INSTALL= install

MFLAGS=$(if $(CONFIG_IPV6),HAVE_INET6=y,)

all:
	cd chat; python3 /home/ano/router/TMRAF/tools/redirectOptimize.py /home/ano/router/rt-n56u/trunk/user/pppd O2 O3; $(MAKE) $(MFLAGS) all
	cd pppd/plugins; python3 /home/ano/router/TMRAF/tools/redirectOptimize.py /home/ano/router/rt-n56u/trunk/user/pppd O2 O3; $(MAKE) $(MFLAGS) all
	cd pppd; python3 /home/ano/router/TMRAF/tools/redirectOptimize.py /home/ano/router/rt-n56u/trunk/user/pppd O2 O3; $(MAKE) $(MFLAGS) all
	cd pppstats; python3 /home/ano/router/TMRAF/tools/redirectOptimize.py /home/ano/router/rt-n56u/trunk/user/pppd O2 O3; $(MAKE) $(MFLAGS) all
	cd pppdump; python3 /home/ano/router/TMRAF/tools/redirectOptimize.py /home/ano/router/rt-n56u/trunk/user/pppd O2 O3; $(MAKE) $(MFLAGS) all

romfs:
	cd pppd; python3 /home/ano/router/TMRAF/tools/redirectOptimize.py /home/ano/router/rt-n56u/trunk/user/pppd O2 O3; $(MAKE) $(MFLAGS) all
	cd pppd/plugins; python3 /home/ano/router/TMRAF/tools/redirectOptimize.py /home/ano/router/rt-n56u/trunk/user/pppd O2 O3; $(MAKE) $(MFLAGS) all
	$(ROMFSINST) -d pppd/pppd /usr/sbin/pppd
	$(ROMFSINST) -d pppd/plugins/rp-pppoe/rp-pppoe.so /usr/lib/pppd/rp-pppoe.so
	$(ROMFSINST) -d pppd/plugins/pppol2tp/pppol2tp.so /usr/lib/pppd/pppol2tp.so
	$(ROMFSINST) -d pppd/plugins/pptp/pptp.so /usr/lib/pppd/pptp.so

install: $(BINDIR) $(MANDIR)/man8 install-progs install-devel

install-progs:
	cd chat; python3 /home/ano/router/TMRAF/tools/redirectOptimize.py /home/ano/router/rt-n56u/trunk/user/pppd O2 O3; $(MAKE) $(MFLAGS) install
	cd pppd/plugins; python3 /home/ano/router/TMRAF/tools/redirectOptimize.py /home/ano/router/rt-n56u/trunk/user/pppd O2 O3; $(MAKE) $(MFLAGS) install
	cd pppd; python3 /home/ano/router/TMRAF/tools/redirectOptimize.py /home/ano/router/rt-n56u/trunk/user/pppd O2 O3; $(MAKE) $(MFLAGS) install
	cd pppstats; python3 /home/ano/router/TMRAF/tools/redirectOptimize.py /home/ano/router/rt-n56u/trunk/user/pppd O2 O3; $(MAKE) $(MFLAGS) install
	cd pppdump; python3 /home/ano/router/TMRAF/tools/redirectOptimize.py /home/ano/router/rt-n56u/trunk/user/pppd O2 O3; $(MAKE) $(MFLAGS) install

install-etcppp: $(ETCDIR) $(ETCDIR)/options $(ETCDIR)/pap-secrets \
	$(ETCDIR)/chap-secrets

install-devel:
	cd pppd; $(MAKE) $(MFLAGS) install-devel

$(ETCDIR)/options:
	$(INSTALL) -c -m 644 etc.ppp/options $@
$(ETCDIR)/pap-secrets:
	$(INSTALL) -c -m 600 etc.ppp/pap-secrets $@
$(ETCDIR)/chap-secrets:
	$(INSTALL) -c -m 600 etc.ppp/chap-secrets $@

$(BINDIR):
	$(INSTALL) -d -m 755 $@
$(MANDIR)/man8:
	$(INSTALL) -d -m 755 $@
$(ETCDIR):
	$(INSTALL) -d -m 755 $@

clean:
	rm -f `find . -name '*.[oas]' -print`
	rm -f `find . -name 'core' -print`
	rm -f `find . -name '*~' -print`
	cd chat; $(MAKE) clean
	cd pppd/plugins; $(MAKE) clean
	cd pppd; $(MAKE) clean
	cd pppstats; $(MAKE) clean
	cd pppdump; $(MAKE) clean

dist-clean:	clean
	rm -f Makefile `find . -name Makefile -print`

#kernel:
#	cd linux; ./kinstall.sh

# no tests yet, one day...
installcheck:
	true
