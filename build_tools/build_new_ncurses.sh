#!/usr/bin/env sh

checkdl(){ sh -c "$@ http://invisible-island.net 1>&2" 2>/dev/null; }
nodl(){ echo 'Error: cannot find usable wget or curl.'; exit 1 ; }
DLTOOL="curl -Ls" && checkdl "$DLTOOL" || DLTOOL="wget -qO-" && checkdl "$DLTOOL" || nodl

BASEDIR=$PWD; OURTMP="$(mktemp -d /tmp/fish-ncurses-XXXXXX)" && cd "$OURTMP"
mkdir -p src; cd src

echo 'Downloading latest ncurses development source...'
$DLTOOL http://invisible-island.net/datafiles/current/ncurses.tar.gz | gzip -dc |
 tar --strip-components 1 -x

echo 'Downloading latest terminfo...'
$DLTOOL http://invisible-island.net/datafiles/current/terminfo.src.gz |
  gzip -dc > misc/terminfo.src

echo 'Downloading extra terminfo...'
$DLTOOL https://raw2.github.com/thestinger/termite/master/termite.terminfo >> misc/terminfo.src

echo 'Applying patch...'
echo 'index: ncurses/tinfo/alloc_entry.c ncurses/tinfo/alloc_entry.c
--- ncurses/tinfo/alloc_entry.c
+++ ncurses/tinfo/alloc_entry.c
@@ -63,0 +64,2 @@
+    unsigned i;
+
@@ -75,0 +78,24 @@
+#if NCURSES_XNAMES
+    tp->num_Booleans = BOOLCOUNT;
+    tp->num_Numbers = NUMCOUNT;
+    tp->num_Strings = STRCOUNT;
+    tp->ext_Booleans = 0;
+    tp->ext_Numbers = 0;
+    tp->ext_Strings = 0;
+#endif
+    if (tp->Booleans == 0)
+	TYPE_MALLOC(NCURSES_SBOOL, BOOLCOUNT, tp->Booleans);
+    if (tp->Numbers == 0)
+	TYPE_MALLOC(short, NUMCOUNT, tp->Numbers);
+    if (tp->Strings == 0)
+	TYPE_MALLOC(char *, STRCOUNT, tp->Strings);
+
+    for_each_boolean(i, tp)
+	tp->Booleans[i] = FALSE;
+
+    for_each_number(i, tp)
+	tp->Numbers[i] = ABSENT_NUMERIC;
+
+    for_each_string(i, tp)
+	tp->Strings[i] = ABSENT_STRING;
+
@@ -77,2 +102,0 @@
-
-    _nc_init_termtype(tp);' | patch -p0 || echo "Patch failed. Not needed any more? ..."
echo 'Configuring ncurses...'
mkdir $OURTMP/stage; cd $OURTMP/stage; cp -R $OURTMP/src/* .; ./configure --prefix=$OURTMP/stage1-install --without-manpages --disable-getcap --disable-lp64 --disable-rpath-hack --disable-safe-sprintf --disable-termcap --enable-colorfgbg --enable-ext-colors --enable-ext-funcs --enable-ext-mouse --enable-hashmap --enable-home-terminfo --enable-interop --enable-largefile --enable-no-padding --enable-overwrite --enable-pc-files --enable-pthreads-eintr --enable-scroll-hints --enable-sigwinch --enable-sp-funcs --enable-symlinks --enable-tcap-names --enable-term-driver --enable-tparm-varargs --enable-weak-symbols --enable-wgetch-events --enable-widec --enable-xmc-glitch --with-curses-h --with-cxx --with-cxx-binding --with-manpage-format=formatted,uncompressed --with-manpage-renames --with-manpage-symlinks --with-manpage-tbl --with-pkg-config --with-pthread --with-xterm-new --without-ada --without-debug --without-dlsym --without-getcap --without-getcap-cache --without-gpm --without-libtool --without-rcs-ids --without-tests --disable-leaks
make; make install; cd $OURTMP; export PATH=$OURTMP/stage1-install/bin:$PATH; cp -R src/* .
echo 'Building standalone ncurses...'
./configure --disable-database --disable-db-install --disable-getcap --disable-lp64 --disable-rpath-hack --disable-safe-sprintf --disable-termcap --enable-colorfgbg --enable-ext-colors --enable-ext-funcs --enable-ext-mouse --enable-hashmap --enable-home-terminfo --enable-interop --enable-largefile --enable-no-padding --enable-overwrite --enable-pc-files --enable-pthreads-eintr --enable-scroll-hints --enable-sigwinch --enable-sp-funcs --enable-symlinks --enable-tcap-names --enable-term-driver --enable-tparm-varargs --enable-weak-symbols --enable-wgetch-events --enable-widec --enable-xmc-glitch --with-curses-h --with-cxx --with-cxx-binding --with-default-terminfo-dir= --with-manpage-format=formatted,uncompressed --with-manpage-renames --with-manpage-symlinks --with-manpage-tbl --with-pkg-config --with-pthread --with-terminfo-dirs= --with-xterm-new --without-ada --without-debug --without-dlsym --without-getcap --without-getcap-cache --without-gpm --without-libtool --without-progs --without-rcs-ids --without-termpath --without-tests --disable-leaks --with-fallbacks=ansi-generic,ansi-mini,color_xterm,dtterm,dumb,Eterm,Eterm-256color,Eterm-88color,eterm-color,gnome,gnome-256color,guru,hurd,iTerm.app,konsole,konsole-16color,konsole-256color,konsole-base,konsole-linux,konsole-solaris,konsole-vt100,kterm,kterm-color,linux,linux-16color,linux-basic,mac,mlterm,mlterm-256color,mrxvt,mrxvt-256color,mterm,mterm-ansi,mvterm,nsterm,nsterm-16color,nsterm-256color,pty,putty,putty-256color,putty-vt100,rxvt,rxvt-16color,rxvt-256color,rxvt-88color,rxvt-basic,rxvt-color,screen,screen-16color,screen-256color,simpleterm,st-16color,st-256color,st52,st52-color,stv52,tt,tt52,unknown,vt100,vt102,vte,vte-256color,xterm,xterm-16color,xterm-256color,xterm-88color,xterm-basic,xterm-bold,xterm-color,xterm-utf8,xterm-vt220,xterm-vt52,xterm1,xtermc,xtermm,xterm-termite &&
 make || echo 'Building ncurses failed.'

echo "Successfully built ncurses with built-in terminfo."

cp -R lib include $BASEDIR; rm -rf $OURTMP; cd $BASEDIR/include
ln -s curses.h ncurses.h; ln -s . ncurses

# All possible fallbacks:
#--with-fallbacks=aaa,aas1901,abm80,abm85,act4,act5,addrinfo,adds980,adm5,aepro,aixterm,aixterm-16color,aj510,aj830,altos7,amiga,amiga-vnc,ampex175,ampex210,ampex80,annarbor4080,ansi,ansi-color-2-emx,ansi-color-3-emx,ansi-emx,ansi-generic,ansi-mini,ansi-mtabs,ansi.sys,ansi.sysk,ansi77,apollo,apollo_15P,apollo_19L,apollo_color,apple-soroc,apple-uterm,apple-videx,apple-videx2,apple-videx3,apple-vm80,apple2e,apple80p,appleII,appleIIgs,arm100,aterm,avatar,avt,aws,awsc,bantam,basis,beacon,beehive,beterm,bg1.25,bg1.25nv,bg1.25rv,bg2.0,bg2.0rv,bitgraph,blit,bobcat,bq300,bsdos-pc-nobold,bsdos-sparc,bterm,c100,c108,ca22851,cbblit,cbunix,cci,cdc456,cdc721,cdc721-esc,cdc721ll,cdc752,cdc756,cg7900,cit101,cit101e,cit500,cit80,citoh,citoh-comp,citoh-elite,citoh-pica,citoh-prop,coco3,color_xterm,commodore,cons25,cons25-debian,cons25l1,cons25r,cons30,cons43,cons50,cons50l1,cons50r,cons60,cons60l1,cons60r,contel300,contel301,cops10,crt,cs10,ct8500,ctrm,cyb110,cyb83,cygwin,cygwinB19,cygwinDBG,d132,d200,d210,d211,d216-unix,d217-unix,d220,d230c,d400,d410,d412-unix,d413-unix,d414-unix,d430c-dg-ccc,d430c-unix,d430c-unix-25-ccc,d430c-unix-ccc,d430c-unix-s-ccc,d430c-unix-sr-ccc,d430c-unix-w-ccc,d470c,d555,d577,d578,d800,ddr,dec-vt100,dec-vt220,decansi,delta,dg-generic,dg200,dg210,dg211,dg450,dg460-ansi,dg6053,diablo1620,diablo1640,digilog,djgpp,djgpp203,djgpp204,dku7003,dku7202,dm1520,dm2500,dm3025,dm3045,dm80,dmchat,dmterm,dp3360,dp8242,dt100,dt110,dt80-sas,dtc300s,dtc382,dtterm,dumb,dw1,dw2,dw3,dw4,dwk,elks,elks-ansi,elks-glasstty,elks-vt52,emu,emx-base,env230,ep40,ep48,ergo4000,esprit,Eterm,Eterm-256color,Eterm-88color,eterm-color,ex155,excel62,f100,f110,f1720,f200,f200vi,falco,fos,fox,gator,gator-52t,gigi,glasstty,gnome,gnome-256color,gnome-fc5,gnome-rh62,gnome-rh72,gnome-rh80,gnome-rh90,go140,go225,graphos,gs6300,gsi,gt40,gt42,guru,guru-nctxt,h19,h19k,ha8675,ha8686,hazel,hds200,hirez100,hmod1,hp110,hp150,hp2,hp236,hpansi,hpex,hpgeneric,hpsub,hpterm,hpterm-color,hurd,hz1000,i100,i400,ibcs2,ibm-apl,ibmaed,ibmapa8c,ibmega,ibmmono,ibmpc3,ibmvga,icl6404,ifmr,ims-ansi,ims950,infoton,interix,intertube,intertube2,intext,intext2,iris-ansi,iris-color,iTerm.app,jaixterm,kaypro,kermit,kon,konsole,konsole-16color,konsole-256color,konsole-base,konsole-linux,konsole-solaris,konsole-vt100,konsole-xf3x,konsole-xf4x,kt7,kt7ix,kterm,kterm-color,kvt,lft,linux,linux-16color,linux-basic,linux-koi8,linux-koi8r,linux-lat,linux-nic,linux2.2,linux2.6,linux2.6.26,linux3.0,lisa,lisaterm,liswb,ln03,lpr,luna,m2-nam,mac,mach,mach-bold,mach-color,mach-gnu,mach-gnu-color,mai,masscomp,masscomp1,masscomp2,megatek,memhp,mgr,mgr-linux,mgr-sun,mgt,mgterm,microb,mime,mime2a,mime314,mime3a,mime3ax,minitel1,minitel1b,minix,minix-1.7,minix-3.0,mlterm,mlterm-256color,mm340,modgraph,modgraph2,modgraph48,mono-emx,morphos,mrxvt,mrxvt-256color,ms-vt-utf8,ms-vt100,ms-vt100-color,msk227,msk22714,msk227am,mt70,mterm,mterm-ansi,MtxOrb,MtxOrb162,MtxOrb204,mvterm,nansi.sys,nansi.sysk,ncsa,ncsa-vt220,ndr9500,nec5520,news-29-euc,news-29-sjis,news-33-euc,news-33-sjis,news-42-euc,news-42-sjis,news-unk,news28,news29,next,nextshell,northstar,nsterm,nsterm-16color,nsterm-256color,nsterm-acs,nsterm-bce,nsterm-c-acs,nsterm-c-s-acs,nsterm-m-acs,nsterm-m-s-acs,nsterm-s-acs,nwp511,nwp512,nwp513,nwp517,oblit,oc100,ofcons,oldpc3,oldsun,omron,origpc3,osborne,osexec,otek4112,otek4115,owl,p19,p8gl,pc-coherent,pc-minix,pc-venix,pc3,pc6300plus,pcansi,pccon,pccon0,pccons,pcix,pckermit,pckermit120,pcplot,pcvt25,pcvt25-color,pcvt28,pcvt35,pcvt40,pcvt43,pcvt50,pcvtXX,pe1251,pe7000c,pe7000m,pilot,pmcons,prism12,prism14,prism2,prism4,prism5,prism7,prism8,prism9,pro350,ps300,psterm,psterm-fast,pt100,pt210,pt250,pty,putty,putty-256color,putty-sco,putty-vt100,qansi,qdss,qnx,qnxm,qnxt,qnxt2,qnxtmono,qume5,qvt101,qvt102,qvt103,qvt203,rbcomm,rbcomm-nam,rca,rcons,rcons-color,regent,regent100,rt6221,rxvt,rxvt-16color,rxvt-256color,rxvt-88color,rxvt-basic,rxvt-color,rxvt-cygwin,rxvt-cygwin-native,rxvt-xpm,sb1,sb2,sbi,scanset,scoansi,screen,screen-16color,screen-16color-bce,screen-256color,screen-256color-bce,screen-bce,screen-bce.Eterm,screen-bce.gnome,screen-bce.konsole,screen-bce.linux,screen-bce.mlterm,screen-bce.mrxvt,screen-bce.rxvt,screen.Eterm,screen.gnome,screen.konsole,screen.linux,screen.mlterm,screen.mrxvt,screen.rxvt,screen.teraterm,screen.vte,screen.xterm-xfree86,screen2,screen3,screwpoint,scrhp,sibo,simpleterm,simterm,soroc120,soroc140,st-16color,st-256color,st52,st52-color,stv52,sun,sun-cgsix,sun-color,sun-type4,superbee-xsb,superbeeic,superbrain,swtp,synertek,t10,t1061,t1061f,t16,t3700,t3800,tab132,tandem6510,tandem653,tek,teraterm2.3,teraterm4.59,terminator,terminet1200,ti700,ti916,ti924,ti926,ti928,ti931,ti_ansi,trs16,trs2,ts100,ts100-ctxt,tt,tt52,tty33,tty37,tty40,tty43,tw100,tw52,tws-generic,tws2102-sna,tws2103,tws2103-sna,uniterm,unknown,uts30,uwin,v3220,v5410,vanilla,vc303,vc303a,vc404,vc414,vc415,versaterm,vi200,vi300,vi50,vi500,vi50adm,vi55,vi550,vi603,viewpoint,vip,vip-H,visa50,vp60,vp90,vremote,vsc,vt100,vt102,vt125,vt131,vt132,vt220,vt320,vt340,vt400,vt420,vt50,vt50h,vt510,vt52,vt520,vt525,vt61,vte,vte-256color,vwmterm,wsiris,wsvt25,wy100,wy120,wy160,wy185,wy30,wy325,wy350,wy370,wy50,wy50-wvb,wy520,wy60,wy75,wy75-wvb,wy75ap,wy85,x10term,x68k,xerox1720,xerox820,xfce,xiterm,xnuppc,xtalk,xterm,xterm-16color,xterm-256color,xterm-88color,xterm-basic,xterm-bold,xterm-color,xterm-nic,xterm-noapp,xterm-pcolor,xterm-sco,xterm-sun,xterm-utf8,xterm-vt220,xterm-vt52,xterm-xfree86,xterm1,xtermc,xtermm,xterms-sun,z100,z29,z29a,z340,z340-nam,zen30,zen50,ztx
