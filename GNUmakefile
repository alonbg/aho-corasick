~ := acism
include $(word 1, ${RULES} rules.mk)

#---------------- PRIVATE VARS:
acism.x         = acism_x acism_mmap_x

#---------------- PUBLIC (see rules.mk):
all             : libacism.a
test            : acism_t.pass
install         : libacism.a  acism.h
clean           += *.tmp

#---------------- PRIVATE RULES:
libacism.a      : acism.o  acism_create.o  acism_dump.o  acism_file.o msutil.o
acism_t.pass    : ${acism.x}  words
${acism.x}      : libacism.a tap.o

# vim: set nowrap :
