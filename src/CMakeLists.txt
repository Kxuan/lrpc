add_library(lrpc SHARED lrpc.c lrpc.h method.c method.h interface.c interface.h socket.c socket.h)
install(TARGETS lrpc DESTINATION lib)