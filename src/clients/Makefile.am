#Eat bajs und die der bajs-death.

adirs = lib cli 
if HAVE_GTK2
bdirs = $(adirs) gtk2
else
bdirs = $(adirs)
endif
if HAVE_SDL
dirs = $(bdirs) sdl-vis
else
dirs = $(bdirs)
endif

SUBDIRS=$(dirs)
