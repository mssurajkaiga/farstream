plugin_LTLIBRARIES = libfsmsnconference.la


# First build a convenience lib with the common stuff so its not built twice

noinst_LTLIBRARIES = libfsmsnconference-convenience.la

libfsmsnconference_convenience_la_SOURCES = \
	fs-msn-cam-recv-conference.c \
	fs-msn-cam-send-conference.c \
	fs-msn-participant.c \
	fs-msn-session.c \
	fs-msn-connection.c \
	fs-msn-stream.c 

noinst_HEADERS = \
	fs-msn-cam-recv-conference.h \
	fs-msn-cam-send-conference.h \
	fs-msn-conference.h \
	fs-msn-participant.h \
	fs-msn-session.h \
	fs-msn-connection.h  \
	fs-msn-stream.h 


AM_CFLAGS = \
	$(FS_INTERNAL_CFLAGS) \
	$(FS_CFLAGS) \
	$(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_CFLAGS) \
	$(NICE_CFLAGS) 

# Build the main plugin

libfsmsnconference_la_SOURCES = \
	fs-msn-conference.c

libfsmsnconference_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libfsmsnconference_la_LIBADD = \
	libfsmsnconference-convenience.la \
	$(top_builddir)/farstream/libfarstream-@FS_APIVERSION@.la \
	$(FS_LIBS) \
	$(GST_BASE_LIBS) \
	$(GST_LIBS) \
	$(NICE_LIBS)



# Optionally build a library to allow building the gtk-doc

if ENABLE_GTK_DOC
noinst_LTLIBRARIES += libfsmsnconference_doc.la

nodist_libfsmsnconference_doc_la_CFLAGS = -DBUILD_GTK_DOC

nodist_libfsmsnconference_doc_la_SOURCES = $(libfsmsnconference_la_SOURCES)


libfsmsnconference_doc_la_LIBADD = libfsmsnconference-convenience.la \
	$(libfsmsnconference_la_LIBADD)

CLEANFILES = $(BUILT_SOURCES)

endif

