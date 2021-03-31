ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

bin_PROGRAMS = sim6809

sim6809_LDADD = $(ALSA_LIBS) $(UTIL_LIBS)
sim6809_SOURCES = console.c dis6809.c emu6809.c inst6809.c int6809.c memory.c misc.c miscutils.c intel.c motorola.c raw.c ../hardware/hardware.c ../hardware/m6850.c ../hardware/m6840.c ../hardware/m6820.c ../hardware/r6522.c ../hardware/r6532.c
